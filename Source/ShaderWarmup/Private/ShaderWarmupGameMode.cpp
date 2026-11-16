#include "ShaderWarmupGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ShaderWarmupSettings.h"
#include "ShaderWarmupSubsystem.h"
#include "ShaderWarmupWidget.h"

AShaderWarmupGameMode::AShaderWarmupGameMode()
{
	DefaultPawnClass = nullptr; // nothing to control on the boot screen
}

void AShaderWarmupGameMode::BeginPlay()
{
	Super::BeginPlay();

	const UShaderWarmupSettings* Settings = GetDefault<UShaderWarmupSettings>();
	UClass* WidgetClass = Settings->WidgetClass.LoadSynchronous();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (WidgetClass && PC)
	{
		if (UUserWidget* Screen = CreateWidget<UUserWidget>(PC, WidgetClass))
		{
			Screen->AddToViewport();
		}
		PC->SetShowMouseCursor(false);
	}

	if (UShaderWarmupSubsystem* Warmup = GetGameInstance()->GetSubsystem<UShaderWarmupSubsystem>())
	{
		Warmup->StartWarmup(bOpenNextLevelWhenDone);
	}
}
