#include "ShaderWarmupSettings.h"

#include "ShaderWarmupWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogShaderWarmup, Log, All);

static const TCHAR* DefaultWidgetPath = TEXT("/ShaderWarmup/WBP_ShaderWarmup.WBP_ShaderWarmup_C");

UShaderWarmupSettings::UShaderWarmupSettings()
{
	WidgetClass = TSoftClassPtr<UShaderWarmupWidget>(FSoftObjectPath(DefaultWidgetPath));

	FDirectoryPath Game;
	Game.Path = TEXT("/Game");
	FoldersToWarmUp.Add(Game);
}

UClass* UShaderWarmupSettings::LoadWidgetClass() const
{
	if (WidgetClass.IsNull())
	{
		return nullptr;
	}
	if (UClass* Class = WidgetClass.LoadSynchronous())
	{
		return Class;
	}
	// e.g. a project that saved the old name WBP_ShaderWarmup_Default in its settings
	UE_LOG(LogShaderWarmup, Warning, TEXT("Widget Class '%s' can't be loaded; using %s instead."),
		*WidgetClass.ToString(), DefaultWidgetPath);
	return TSoftClassPtr<UShaderWarmupWidget>(FSoftObjectPath(DefaultWidgetPath)).LoadSynchronous();
}
