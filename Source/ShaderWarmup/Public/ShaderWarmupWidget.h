#pragma once

#include "Blueprint/UserWidget.h"
#include "ShaderWarmupSubsystem.h"
#include "ShaderWarmupWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * Parent class for the warm-up screen. You never need to write graph code:
 *
 *  1. Details panel: change Title Image, Background Image, Bar Color, Tips and the texts under "Shader Warmup".
 *  2. Designer: move and restyle anything. Widgets with these exact names update by themselves
 *     (all optional, delete what you don't want):
 *       ProgressBar (Progress Bar)   PercentText (Text)   StatusText (Text)
 *       TitleImage (Image)           TipText (Text)       Background (Image)
 *  3. Your own animations: implement the events "On Warmup Progress" / "On Warmup Finished" in the graph.
 */
UCLASS(Abstract, Blueprintable)
class SHADERWARMUP_API UShaderWarmupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---- Look (Details panel) ----

	/**
	 * Shown in the widget named TitleImage (e.g. your game's logo). A texture or a material.
	 * Leave empty to keep the Designer's brush; with no picture at all, TitleImage is hidden in the game.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Look", meta = (DisplayName = "Title Image", AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> TitleImageAsset;

	/** Height of TitleImage. The width follows the picture's shape. 0 = use the Designer's image size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Look", meta = (ClampMin = "0"))
	float TitleImageHeight = 120.f;

	/** Shown in the widget named Background. A texture or a material. Leave empty to keep the Designer's brush. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Look", meta = (AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> BackgroundImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Look", meta = (InlineEditConditionToggle))
	bool bOverrideBarColor = false;

	/** Fill colour of the widget named ProgressBar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Look", meta = (EditCondition = "bOverrideBarColor"))
	FLinearColor BarColor = FLinearColor(0.21f, 0.54f, 0.87f, 1.f);

	/** How fast the bar glides to the real value. 0 = jump straight there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Look", meta = (ClampMin = "0"))
	float BarSmoothing = 8.f;

	/** Progress shown in the Designer preview only (0..1), so you can see the bar while styling. */
	UPROPERTY(EditAnywhere, Category = "Shader Warmup|Look", meta = (ClampMin = "0", ClampMax = "1"))
	float PreviewProgress = 0.64f;

	// ---- Text (Details panel) ----

	/** {Percent} = 0..100. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	FText PercentFormat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	FText ScanningText;

	/** {Assets} = number of materials and effects being loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	FText LoadingText;

	/** {Left} = shaders still to compile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	FText CompilingText;

	/** Everything is loaded and compiled; waiting a moment in case more work turns up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	FText AlmostReadyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	FText FinishedText;

	/** Lines shown one after another in the widget named TipText. Empty = TipText is hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text")
	TArray<FText> Tips;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Warmup|Text", meta = (ClampMin = "0.5", Units = "s"))
	float SecondsPerTip = 4.f;

	// ---- Events for your own graph code ----

	/** Every frame while warming up. Progress 0..1 (already smoothed). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shader Warmup")
	void OnWarmupProgress(float Progress, int32 ShadersRemaining, EShaderWarmupPhase Phase);

	/** Once, when everything is ready (just before the next level opens). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shader Warmup")
	void OnWarmupFinished();

	/** The smoothed 0..1 value the bar shows. */
	UFUNCTION(BlueprintPure, Category = "Shader Warmup")
	float GetDisplayedProgress() const { return DisplayedProgress; }

	UShaderWarmupWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Shader Warmup", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "Shader Warmup", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PercentText;

	UPROPERTY(BlueprintReadOnly, Category = "Shader Warmup", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Shader Warmup", meta = (BindWidgetOptional))
	TObjectPtr<UImage> TitleImage;

	UPROPERTY(BlueprintReadOnly, Category = "Shader Warmup", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TipText;

	UPROPERTY(BlueprintReadOnly, Category = "Shader Warmup", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Background;

private:
	UFUNCTION()
	void HandleProgress(float Progress, int32 ShadersRemaining, EShaderWarmupPhase Phase);

	UFUNCTION()
	void HandleFinished();

	void ApplyLook();
	void ShowBar(float Value);
	void ShowStatus(int32 ShadersRemaining, EShaderWarmupPhase Phase);
	void ShowTip(int32 Index);

	UPROPERTY(Transient)
	TObjectPtr<UShaderWarmupSubsystem> Subsystem;

	float TargetProgress = 0.f;
	float DisplayedProgress = 0.f;
	int32 LastShaders = 0;
	EShaderWarmupPhase LastPhase = EShaderWarmupPhase::NotStarted;
	int32 TipIndex = 0;
	float TipTimer = 0.f;
};
