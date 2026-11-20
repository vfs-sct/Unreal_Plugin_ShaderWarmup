#include "ShaderWarmupWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture.h"

#define LOCTEXT_NAMESPACE "ShaderWarmup"

UShaderWarmupWidget::UShaderWarmupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PercentFormat = LOCTEXT("PercentFormat", "{Percent}%");
	ScanningText = LOCTEXT("Scanning", "Getting ready…");
	LoadingText = LOCTEXT("Loading", "Loading materials…");
	CompilingText = LOCTEXT("Compiling", "Compiling shaders… {Left} left");
	AlmostReadyText = LOCTEXT("AlmostReady", "Almost ready…");
	FinishedText = LOCTEXT("Finished", "Ready");
}

void UShaderWarmupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyLook();

	if (IsDesignTime())
	{
		// Designer preview: show a part-full bar and example texts.
		DisplayedProgress = TargetProgress = PreviewProgress;
		ShowBar(PreviewProgress);
		ShowStatus(1204, EShaderWarmupPhase::CompilingShaders);
		ShowTip(0);
	}
}

void UShaderWarmupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	DisplayedProgress = TargetProgress = 0.f;
	TipIndex = 0;
	TipTimer = 0.f;
	ShowBar(0.f);
	ShowStatus(0, EShaderWarmupPhase::ScanningAssets);
	ShowTip(0);

	UGameInstance* GI = GetGameInstance();
	Subsystem = GI ? GI->GetSubsystem<UShaderWarmupSubsystem>() : nullptr;
	if (Subsystem)
	{
		Subsystem->OnProgress.AddDynamic(this, &UShaderWarmupWidget::HandleProgress);
		Subsystem->OnFinished.AddDynamic(this, &UShaderWarmupWidget::HandleFinished);
		if (Subsystem->IsRunning())
		{
			HandleProgress(Subsystem->GetProgress(), Subsystem->GetShadersRemaining(), Subsystem->GetPhase());
		}
	}
}

void UShaderWarmupWidget::NativeDestruct()
{
	if (Subsystem)
	{
		Subsystem->OnProgress.RemoveDynamic(this, &UShaderWarmupWidget::HandleProgress);
		Subsystem->OnFinished.RemoveDynamic(this, &UShaderWarmupWidget::HandleFinished);
		Subsystem = nullptr;
	}
	Super::NativeDestruct();
}

void UShaderWarmupWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!FMath::IsNearlyEqual(DisplayedProgress, TargetProgress))
	{
		DisplayedProgress = BarSmoothing > 0.f
			? FMath::FInterpTo(DisplayedProgress, TargetProgress, InDeltaTime, BarSmoothing)
			: TargetProgress;
		if (FMath::Abs(TargetProgress - DisplayedProgress) < 0.001f)
		{
			DisplayedProgress = TargetProgress;
		}
		ShowBar(DisplayedProgress);
		OnWarmupProgress(DisplayedProgress, LastShaders, LastPhase);
	}

	if (Tips.Num() > 1)
	{
		TipTimer += InDeltaTime;
		if (TipTimer >= SecondsPerTip)
		{
			TipTimer = 0.f;
			ShowTip(TipIndex + 1);
		}
	}
}

void UShaderWarmupWidget::HandleProgress(float Progress, int32 ShadersRemaining, EShaderWarmupPhase Phase)
{
	TargetProgress = Progress;
	LastShaders = ShadersRemaining;
	LastPhase = Phase;
	if (Phase == EShaderWarmupPhase::Finished)
	{
		DisplayedProgress = 1.f; // the end is never smoothed: the level is about to change
		ShowBar(1.f);
	}
	ShowStatus(ShadersRemaining, Phase);
	OnWarmupProgress(DisplayedProgress, ShadersRemaining, Phase);
}

void UShaderWarmupWidget::HandleFinished()
{
	OnWarmupFinished();
}

void UShaderWarmupWidget::ApplyLook()
{
	if (TitleImage)
	{
		FSlateBrush Brush = TitleImage->GetBrush();
		if (TitleImageAsset)
		{
			Brush.SetResourceObject(TitleImageAsset);
		}
		if (TitleImageHeight > 0.f)
		{
			// Keep the picture's shape: the height is fixed, the width follows.
			const UTexture* Texture = Cast<UTexture>(Brush.GetResourceObject());
			const float TexWidth = Texture ? Texture->GetSurfaceWidth() : 0.f;
			const float TexHeight = Texture ? Texture->GetSurfaceHeight() : 0.f;
			float Aspect = 4.f; // no picture yet: a wide placeholder box in the Designer
			if (TexWidth > 0.f && TexHeight > 0.f)
			{
				Aspect = TexWidth / TexHeight;
			}
			else if (Brush.GetResourceObject() && Brush.ImageSize.X > 0.f && Brush.ImageSize.Y > 0.f)
			{
				Aspect = Brush.ImageSize.X / Brush.ImageSize.Y; // a material: keep the Designer's shape
			}
			Brush.ImageSize = FVector2D(TitleImageHeight * Aspect, TitleImageHeight);
		}
		TitleImage->SetBrush(Brush);
		if (!IsDesignTime() && !TitleImage->GetBrush().GetResourceObject())
		{
			// No picture chosen: hide it instead of drawing a white box. (The Designer keeps showing the slot.)
			TitleImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (Background && BackgroundImage)
	{
		Background->SetBrushResourceObject(BackgroundImage);
	}
	if (ProgressBar && bOverrideBarColor)
	{
		ProgressBar->SetFillColorAndOpacity(BarColor);
	}
}

void UShaderWarmupWidget::ShowBar(float Value)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Value);
	}
	if (PercentText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Percent"), FText::AsNumber(FMath::FloorToInt(FMath::Clamp(Value, 0.f, 1.f) * 100.f)));
		PercentText->SetText(FText::Format(PercentFormat, Args));
	}
}

void UShaderWarmupWidget::ShowStatus(int32 ShadersRemaining, EShaderWarmupPhase Phase)
{
	if (!StatusText)
	{
		return;
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Left"), FText::AsNumber(ShadersRemaining));
	Args.Add(TEXT("Assets"), FText::AsNumber(Subsystem ? Subsystem->GetAssetCount() : 0));

	const FText* Pattern = &ScanningText;
	switch (Phase)
	{
	case EShaderWarmupPhase::LoadingAssets:    Pattern = &LoadingText; break;
	case EShaderWarmupPhase::CompilingShaders: Pattern = ShadersRemaining > 0 ? &CompilingText : &AlmostReadyText; break;
	case EShaderWarmupPhase::Finished:         Pattern = &FinishedText; break;
	default: break;
	}
	StatusText->SetText(FText::Format(*Pattern, Args));
}

void UShaderWarmupWidget::ShowTip(int32 Index)
{
	if (!TipText)
	{
		return;
	}
	if (Tips.Num() == 0)
	{
		TipText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	TipIndex = Index % Tips.Num();
	TipText->SetVisibility(ESlateVisibility::HitTestInvisible);
	TipText->SetText(Tips[TipIndex]);
}

#undef LOCTEXT_NAMESPACE
