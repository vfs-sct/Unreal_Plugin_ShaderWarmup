#include "ShaderWarmupSubsystem.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PipelineStateCache.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"
#include "DynamicRHI.h"
#include "RHIGlobals.h"
#include "ShaderWarmupSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogShaderWarmup, Log, All);

// Less compiling than this in total counts as "nothing to compile": the graphics driver's quick checks of PSOs it
// already has.
static constexpr float NothingToCompileSeconds = 1.f;

FString UShaderWarmupSubsystem::CompiledMarkerPath()
{
	return FPaths::ProjectSavedDir() / TEXT("ShaderWarmup") / TEXT("AlreadyCompiled.txt");
}

FString UShaderWarmupSubsystem::CompiledMarkerKey()
{
	// Changes with a new build (exe and content files), a new engine, another graphics card, driver or graphics API.
	FString Key = FString::Printf(TEXT("%s|%s|%s|%s|%u|%u|%s"),
		FApp::GetBuildVersion(), *FEngineVersion::Current().ToString(), GDynamicRHI ? GDynamicRHI->GetName() : TEXT(""),
		*GRHIAdapterName, GRHIVendorId, GRHIDeviceId, *GRHIAdapterUserDriverVersion);
	IFileManager& Files = IFileManager::Get();
	Key += FString::Printf(TEXT("|exe %lld"), Files.GetTimeStamp(FPlatformProcess::ExecutablePath()).GetTicks());
	const FString PaksDir = FPaths::ProjectContentDir() / TEXT("Paks");
	TArray<FString> Paks;
	Files.FindFiles(Paks, *PaksDir, nullptr);
	Paks.Sort();
	for (const FString& Pak : Paks)
	{
		const FString Path = PaksDir / Pak;
		Key += FString::Printf(TEXT("|%s %lld %lld"), *Pak, Files.FileSize(*Path), Files.GetTimeStamp(*Path).GetTicks());
	}
	return Key;
}

int32 UShaderWarmupSubsystem::GetShaderCompilesRemaining()
{
	return GShaderCompilingManager ? FMath::Max(0, GShaderCompilingManager->GetNumRemainingJobs()) : 0;
}

int32 UShaderWarmupSubsystem::GetPipelineCompilesRemaining()
{
	int64 Count = FShaderPipelineCache::NumPrecompilesRemaining();
	if (PipelineStateCache::IsPSOPrecachingEnabled())
	{
		Count += PipelineStateCache::NumActivePrecacheRequests();
	}
	return (int32)FMath::Clamp<int64>(Count, 0, MAX_int32);
}

void UShaderWarmupSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	ReleaseAssets();
	SetFastMode(false);
	Super::Deinitialize();
}

void UShaderWarmupSubsystem::StartWarmup(bool bOpenNextLevelWhenDone)
{
	if (IsRunning())
	{
		return;
	}

	const UShaderWarmupSettings* Settings = GetDefault<UShaderWarmupSettings>();
	bOpenNextLevel = bOpenNextLevelWhenDone;
	Progress = 0.f;
	ElapsedSeconds = 0.f;
	QuietTime = 0.f;
	ShadersRemaining = 0;
	MostShadersSeen = 0;
	AssetCount = 0;
	HoldSecondsLeft = 0.f;
	CompilingSeconds = 0.f;
	bScreenShown = false;
	Phase = EShaderWarmupPhase::ScanningAssets;

	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UShaderWarmupSubsystem::Tick));
	}

	// -SkipShaderWarmup on the command line, or Play In Editor with the skip option on: go straight on.
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const bool bPIESkip = Settings->bSkipInPlayInEditor && World && World->IsPlayInEditor();
	if (bPIESkip || FParse::Param(FCommandLine::Get(), TEXT("SkipShaderWarmup")))
	{
		Log(TEXT("skipped"));
		Finish(false);
		return;
	}

#if !WITH_EDITOR
	// The last launch had nothing to compile and nothing has changed since: go straight on.
	FString LastKey;
	if (Settings->bSkipWhenAlreadyCompiled && FFileHelper::LoadFileToString(LastKey, *CompiledMarkerPath())
		&& LastKey == CompiledMarkerKey())
	{
		Log(TEXT("already compiled on this PC (nothing to compile last time), skipped"));
		Finish(false);
		return;
	}
#endif

	if (Settings->bFastPipelineCompile)
	{
		SetFastMode(true);
	}
	Log(TEXT("started, waiting for the asset scan"));
}

void UShaderWarmupSubsystem::CancelWarmup()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	ReleaseAssets();
	SetFastMode(false);
	Phase = EShaderWarmupPhase::NotStarted;
}

void UShaderWarmupSubsystem::BeginLoading()
{
	const UShaderWarmupSettings* Settings = GetDefault<UShaderWarmupSettings>();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInterface")));
	if (Settings->bIncludeParticleEffects)
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("ParticleSystem")));
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Niagara"), TEXT("NiagaraSystem")));
	}
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	for (const FDirectoryPath& Folder : Settings->FoldersToWarmUp)
	{
		FString Path = Folder.Path;
		Path.RemoveFromEnd(TEXT("/"));
		if (!Path.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*Path));
		}
	}
	if (Filter.PackagePaths.Num() == 0)
	{
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
	}

	TArray<FAssetData> Assets;
	IAssetRegistry::GetChecked().GetAssets(Filter, Assets);

	TArray<FSoftObjectPath> Paths;
	Paths.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		Paths.Add(Asset.GetSoftObjectPath());
	}
	AssetCount = Paths.Num();
	if (AssetCount > 0)
	{
		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Paths, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority, true);
	}

	Phase = EShaderWarmupPhase::LoadingAssets;
	UE_LOG(LogShaderWarmup, Display, TEXT("Shader warm-up: loading %d assets, %d shader jobs queued"),
		AssetCount, GetShaderCompilesRemaining() + GetPipelineCompilesRemaining());
}

bool UShaderWarmupSubsystem::Tick(float DeltaTime)
{
	const UShaderWarmupSettings* Settings = GetDefault<UShaderWarmupSettings>();

	// Finished: hold the full bar for a moment, then open the next level.
	if (Phase == EShaderWarmupPhase::Finished)
	{
		HoldSecondsLeft -= DeltaTime;
		if (HoldSecondsLeft <= 0.f)
		{
			TickHandle.Reset();
			if (bOpenNextLevel)
			{
				OpenNextLevel();
			}
			return false; // stop ticking
		}
		return true;
	}

	ElapsedSeconds += DeltaTime;
	if (ElapsedSeconds >= Settings->MaxSeconds)
	{
		UE_LOG(LogShaderWarmup, Warning, TEXT("Shader warm-up: gave up after %.0f s (Max Seconds), %d shaders still queued"),
			ElapsedSeconds, ShadersRemaining);
		Finish(true);
		return true;
	}

	// Uncooked runs scan content at startup. Searching before the scan ends finds nothing.
	if (Phase == EShaderWarmupPhase::ScanningAssets)
	{
		if (IAssetRegistry::GetChecked().IsLoadingAssets())
		{
			OnProgress.Broadcast(Progress, 0, Phase);
			return true;
		}
		BeginLoading();
	}

	const float LoadFraction = LoadHandle.IsValid() ? LoadHandle->GetProgress() : 1.f;
	const bool bLoaded = LoadFraction >= 1.f;

	ShadersRemaining = GetShaderCompilesRemaining() + GetPipelineCompilesRemaining();
	MostShadersSeen = FMath::Max(MostShadersSeen, ShadersRemaining);
	if (ShadersRemaining > 0)
	{
		CompilingSeconds += DeltaTime;
	}

	float CompileFraction = MostShadersSeen > 0 ? 1.f - (float)ShadersRemaining / (float)MostShadersSeen : (bLoaded ? 1.f : 0.f);
	if (!bLoaded)
	{
		CompileFraction = FMath::Min(CompileFraction, LoadFraction); // more work can still arrive while loading
	}

	const float Share = FMath::Clamp(Settings->LoadingShare, 0.f, 1.f);
	const float Raw = Share * LoadFraction + (1.f - Share) * CompileFraction;
	Progress = FMath::Max(Progress, FMath::Min(Raw, 0.99f)); // never goes backwards; 100% only at the end

	Phase = bLoaded ? EShaderWarmupPhase::CompilingShaders : EShaderWarmupPhase::LoadingAssets;

	QuietTime = (bLoaded && ShadersRemaining == 0) ? QuietTime + DeltaTime : 0.f;

	LogTimer -= DeltaTime;
	if (Settings->bVerboseLog && LogTimer <= 0.f)
	{
		LogTimer = 0.5f;
		UE_LOG(LogShaderWarmup, Log, TEXT("Shader warm-up: %.0f%%, load %.0f%%, shaders left %d (most %d)"),
			Progress * 100.f, LoadFraction * 100.f, ShadersRemaining, MostShadersSeen);
	}

	OnProgress.Broadcast(Progress, ShadersRemaining, Phase);

	if (QuietTime >= Settings->QuietSeconds && ElapsedSeconds >= Settings->MinSeconds)
	{
		Finish(true);
	}
	return true;
}

void UShaderWarmupSubsystem::Finish(bool bRememberResult)
{
	UE_LOG(LogShaderWarmup, Display, TEXT("Shader warm-up: done after %.1f s, %d assets, most shader jobs seen %d, compiling %.1f s"),
		ElapsedSeconds, AssetCount, MostShadersSeen, CompilingSeconds);

#if !WITH_EDITOR
	// Remember whether this launch had anything to compile, so the next one can skip the warm-up.
	if (bRememberResult)
	{
		if (CompilingSeconds < NothingToCompileSeconds)
		{
			FFileHelper::SaveStringToFile(CompiledMarkerKey(), *CompiledMarkerPath());
		}
		else
		{
			IFileManager::Get().Delete(*CompiledMarkerPath(), false, false, true);
		}
	}
#endif

	Progress = 1.f;
	ShadersRemaining = 0;
	Phase = EShaderWarmupPhase::Finished;
	// The full bar is only held when there is a screen to show it; otherwise go straight on.
	HoldSecondsLeft = bScreenShown ? GetDefault<UShaderWarmupSettings>()->FinishedHoldSeconds : 0.f;
	ReleaseAssets();
	SetFastMode(false);

	OnProgress.Broadcast(Progress, 0, Phase);
	OnFinished.Broadcast();

	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UShaderWarmupSubsystem::Tick));
	}
}

void UShaderWarmupSubsystem::OpenNextLevel()
{
	const TSoftObjectPtr<UWorld>& Next = GetDefault<UShaderWarmupSettings>()->NextLevel;
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (Next.IsNull())
	{
		UE_LOG(LogShaderWarmup, Warning,
			TEXT("Shader warm-up: no Next Level set. Pick one in Project Settings > Plugins > Shader Warmup."));
		return;
	}
	if (World)
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(World, Next);
	}
}

void UShaderWarmupSubsystem::ReleaseAssets()
{
	if (LoadHandle.IsValid())
	{
		LoadHandle->ReleaseHandle();
		LoadHandle.Reset();
	}
}

void UShaderWarmupSubsystem::SetFastMode(bool bFast)
{
	if (bFast == bFastModeOn)
	{
		return;
	}
	bFastModeOn = bFast;
	FShaderPipelineCache::SetBatchMode(bFast ? FShaderPipelineCache::BatchMode::Fast : FShaderPipelineCache::BatchMode::Background);
	PipelineStateCache::PrecachePSOsBoostToHighestPriority(bFast);
}

void UShaderWarmupSubsystem::Log(const TCHAR* What) const
{
	UE_LOG(LogShaderWarmup, Display, TEXT("Shader warm-up: %s"), What);
}
