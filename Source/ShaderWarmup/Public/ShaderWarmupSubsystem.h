#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShaderWarmupSubsystem.generated.h"

struct FStreamableHandle;

/** What the warm-up is doing right now. */
UENUM(BlueprintType)
enum class EShaderWarmupPhase : uint8
{
	NotStarted,
	/** Waiting for the asset registry to finish scanning content (editor / uncooked runs only). */
	ScanningAssets,
	/** Loading materials and effects. */
	LoadingAssets,
	/** Assets are loaded; waiting for shader compiles and pipeline states. */
	CompilingShaders,
	Finished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShaderWarmupProgressSignature, float, Progress, int32, ShadersRemaining, EShaderWarmupPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShaderWarmupFinishedSignature);

/**
 * Runs the warm-up: loads every material / effect under the chosen folders, watches the shader and pipeline queues,
 * reports progress, then opens the next level. Lives on the Game Instance, so it works with any game mode.
 */
UCLASS()
class SHADERWARMUP_API UShaderWarmupSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Starts the warm-up. bOpenNextLevelWhenDone = open Settings > Next Level at the end. Does nothing if already running. */
	UFUNCTION(BlueprintCallable, Category = "Shader Warmup")
	void StartWarmup(bool bOpenNextLevelWhenDone = true);

	/** Stops right away and lets the loaded assets go. Does not open the next level. */
	UFUNCTION(BlueprintCallable, Category = "Shader Warmup")
	void CancelWarmup();

	/** Called when the warm-up screen appears. Only then is the full bar held (Finished Hold Seconds) at the end. */
	void NoteScreenShown() { bScreenShown = true; }

	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	bool IsRunning() const { return Phase != EShaderWarmupPhase::NotStarted && Phase != EShaderWarmupPhase::Finished; }

	/** 0..1, never goes backwards. */
	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	float GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	EShaderWarmupPhase GetPhase() const { return Phase; }

	/** Shader compile jobs + pipeline states still queued. */
	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	int32 GetShadersRemaining() const { return ShadersRemaining; }

	/** Materials and effects the warm-up is loading. */
	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	int32 GetAssetCount() const { return AssetCount; }

	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	float GetElapsedSeconds() const { return ElapsedSeconds; }

	/** Called every frame while running (and once more at 1.0). */
	UPROPERTY(BlueprintAssignable, Category = "Shader Warmup")
	FShaderWarmupProgressSignature OnProgress;

	/** Called once, just before the next level opens. */
	UPROPERTY(BlueprintAssignable, Category = "Shader Warmup")
	FShaderWarmupFinishedSignature OnFinished;

	/** Shader compile jobs still queued (0 in a packaged game: shaders come pre-compiled there). */
	UFUNCTION(BlueprintPure, Category = "Shader Warmup|Low Level")
	static int32 GetShaderCompilesRemaining();

	/** Pipeline states (PSOs) still being compiled: bundled PSO cache + PSO precaching for loaded assets. */
	UFUNCTION(BlueprintPure, Category = "Shader Warmup|Low Level")
	static int32 GetPipelineCompilesRemaining();

private:
	bool Tick(float DeltaTime);
	void BeginLoading();
	void Finish(bool bRememberResult);
	static FString CompiledMarkerPath();
	static FString CompiledMarkerKey();
	void OpenNextLevel();
	void ReleaseAssets();
	void SetFastMode(bool bFast);
	void Log(const TCHAR* What) const;

	FTSTicker::FDelegateHandle TickHandle;
	TSharedPtr<FStreamableHandle> LoadHandle;
	float HoldSecondsLeft = 0.f;
	float LogTimer = 0.f;

	EShaderWarmupPhase Phase = EShaderWarmupPhase::NotStarted;
	bool bOpenNextLevel = true;
	bool bFastModeOn = false;
	float Progress = 0.f;
	float ElapsedSeconds = 0.f;
	float QuietTime = 0.f;
	int32 ShadersRemaining = 0;
	int32 MostShadersSeen = 0;
	int32 AssetCount = 0;
	float CompilingSeconds = 0.f; // total time with shaders / PSOs left to compile
	bool bScreenShown = false;
};
