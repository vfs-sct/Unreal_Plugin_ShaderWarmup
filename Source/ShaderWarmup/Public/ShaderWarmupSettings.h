#pragma once

#include "Engine/DeveloperSettings.h"
#include "ShaderWarmupSettings.generated.h"

class UShaderWarmupWidget;

/**
 * Project Settings > Plugins > Shader Warmup.
 * The only page students need: pick the level to open afterwards and (optionally) your own screen widget.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shader Warmup"))
class SHADERWARMUP_API UShaderWarmupSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UShaderWarmupSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** The level to open once the shaders are ready (usually your main menu). */
	UPROPERTY(Config, EditAnywhere, Category = "Basics", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> NextLevel;

	/** The screen to show. Make your own Widget Blueprint with "Shader Warmup Widget" as its parent class and pick it here. */
	UPROPERTY(Config, EditAnywhere, Category = "Basics")
	TSoftClassPtr<UShaderWarmupWidget> WidgetClass;

	/** Loads WidgetClass. If it's set but can't be loaded (renamed or deleted), the plugin's own WBP_ShaderWarmup is used instead. */
	UClass* LoadWidgetClass() const;

	/** Content folders whose materials and effects are loaded so their shaders compile. /Game = your whole project. */
	UPROPERTY(Config, EditAnywhere, Category = "Warm-up", meta = (ContentDir))
	TArray<FDirectoryPath> FoldersToWarmUp;

	/** Also load Niagara systems and Cascade particle systems (their materials compile too). */
	UPROPERTY(Config, EditAnywhere, Category = "Warm-up")
	bool bIncludeParticleEffects = true;

	/**
	 * Packaged games only: if the last launch on this PC had nothing to compile, and the game build, graphics card,
	 * driver and graphics API are the same, skip the warm-up and open Next Level straight away.
	 * (Editor and Standalone runs always warm up.)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Warm-up")
	bool bSkipWhenAlreadyCompiled = true;

	/**
	 * The screen only appears once shaders / PSOs have been compiling non-stop for this long. Loading alone never
	 * shows it, and neither do the graphics driver's quick checks of already-compiled PSOs, so a launch with
	 * everything compiled goes straight to Next Level without it. 0 = always show the screen.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float ShowScreenAfterSeconds = 1.f;

	/** The warm-up runs at least this long. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float MinSeconds = 0.1f;

	/** Safety net: move on after this long even if shaders are still compiling. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "1", Units = "s"))
	float MaxSeconds = 300.f;

	/** Nothing may be left to compile for this long before the warm-up counts as done (new jobs can still trickle in). */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float QuietSeconds = 0.1f;

	/** How long the full bar stays on screen before the next level opens. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float FinishedHoldSeconds = 0.1f;

	/** Share of the progress bar used by asset loading; the rest is shader compiling. */
	UPROPERTY(Config, EditAnywhere, Category = "Advanced", meta = (ClampMin = "0", ClampMax = "1"))
	float LoadingShare = 0.25f;

	/** Compile pipeline states (PSOs) as fast as possible while the screen is up. */
	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bFastPipelineCompile = true;

	/** When you press Play in the editor on the warm-up map, go straight to the next level (the editor already has its shaders). */
	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bSkipInPlayInEditor = false;

	/** Write progress lines to the log (LogShaderWarmup). */
	UPROPERTY(Config, EditAnywhere, Category = "Advanced")
	bool bVerboseLog = false;
};
