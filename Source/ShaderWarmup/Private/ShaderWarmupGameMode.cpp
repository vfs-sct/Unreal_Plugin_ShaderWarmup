#include "ShaderWarmupGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ShaderWarmupSettings.h"
#include "ShaderWarmupSubsystem.h"
#include "ShaderWarmupWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogShaderWarmup, Log, All);

AShaderWarmupGameMode::AShaderWarmupGameMode()
{
	DefaultPawnClass = nullptr; // nothing to control on the boot screen
}

void AShaderWarmupGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetShowMouseCursor(false);
	}

	if (UShaderWarmupSubsystem* Warmup = GetGameInstance()->GetSubsystem<UShaderWarmupSubsystem>())
	{
		Warmup->StartWarmup(bOpenNextLevelWhenDone);
	}

	// With nothing to compile the warm-up is done in a fraction of a second and the screen never appears.
	// Otherwise it appears after Show Screen After, or later if work only turns up later (checked every 0.1 s).
	const float Delay = GetDefault<UShaderWarmupSettings>()->ShowScreenAfterSeconds;
	if (Delay <= 0.f)
	{
		ShowScreen();
	}
	else
	{
		GetWorldTimerManager().SetTimer(ShowScreenTimer, this, &AShaderWarmupGameMode::ShowScreenIfBusy, 0.1f, true, Delay);
	}
}

void AShaderWarmupGameMode::ShowScreenIfBusy()
{
	const UShaderWarmupSubsystem* Warmup = GetGameInstance()->GetSubsystem<UShaderWarmupSubsystem>();
	if (!Warmup || !Warmup->IsRunning())
	{
		GetWorldTimerManager().ClearTimer(ShowScreenTimer); // finished (or never started): no screen
		return;
	}
	// Still scanning / loading, or shaders left. (Compiling with 0 left = only the final quiet wait.)
	const bool bBusy = Warmup->GetPhase() != EShaderWarmupPhase::CompilingShaders || Warmup->GetShadersRemaining() > 0;
	if (bBusy)
	{
		GetWorldTimerManager().ClearTimer(ShowScreenTimer);
		ShowScreen();
	}
}

void AShaderWarmupGameMode::ShowScreen()
{
	UClass* WidgetClass = GetDefault<UShaderWarmupSettings>()->LoadWidgetClass();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (WidgetClass && PC)
	{
		if (UUserWidget* Screen = CreateWidget<UUserWidget>(PC, WidgetClass))
		{
			Screen->AddToViewport();
			UE_LOG(LogShaderWarmup, Display, TEXT("Shader warm-up: showing the screen (%s)"), *WidgetClass->GetName());
		}
	}
}
