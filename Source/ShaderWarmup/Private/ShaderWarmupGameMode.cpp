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

	// The screen only appears for real compiling (checked every 0.1 s), so a launch with everything compiled
	// goes straight to Next Level without it.
	CompilingSeconds = 0.f;
	if (GetDefault<UShaderWarmupSettings>()->ShowScreenAfterSeconds <= 0.f)
	{
		ShowScreen();
	}
	else
	{
		GetWorldTimerManager().SetTimer(ShowScreenTimer, this, &AShaderWarmupGameMode::ShowScreenIfBusy, 0.1f, true);
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
	// Loading doesn't count, only shaders / PSOs left to compile, and it must last: quick checks of PSOs the
	// graphics driver already has come and go within a moment.
	CompilingSeconds = Warmup->GetShadersRemaining() > 0 ? CompilingSeconds + 0.1f : 0.f;
	if (CompilingSeconds >= GetDefault<UShaderWarmupSettings>()->ShowScreenAfterSeconds)
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
			if (UShaderWarmupSubsystem* Warmup = GetGameInstance()->GetSubsystem<UShaderWarmupSubsystem>())
			{
				Warmup->NoteScreenShown();
			}
			UE_LOG(LogShaderWarmup, Display, TEXT("Shader warm-up: showing the screen (%s)"), *WidgetClass->GetName());
		}
	}
}
