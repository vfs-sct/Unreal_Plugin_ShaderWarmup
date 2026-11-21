#pragma once

#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ShaderWarmupGameMode.generated.h"

/**
 * Game mode of the L_ShaderWarmup map: no pawn, starts the warm-up and shows the warm-up screen from Project Settings
 * (only once shaders have been compiling non-stop for Show Screen After).
 * Want the screen in your own boot level instead? Set this as that level's GameMode Override.
 */
UCLASS()
class SHADERWARMUP_API AShaderWarmupGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShaderWarmupGameMode();

	/** Open Project Settings > Shader Warmup > Next Level when done. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shader Warmup")
	bool bOpenNextLevelWhenDone = true;

protected:
	virtual void BeginPlay() override;

private:
	void ShowScreen();
	void ShowScreenIfBusy();

	FTimerHandle ShowScreenTimer;
	float CompilingSeconds = 0.f;
};
