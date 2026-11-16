#include "ShaderWarmupSettings.h"

#include "ShaderWarmupWidget.h"

UShaderWarmupSettings::UShaderWarmupSettings()
{
	WidgetClass = TSoftClassPtr<UShaderWarmupWidget>(
		FSoftObjectPath(TEXT("/ShaderWarmup/WBP_ShaderWarmup_Default.WBP_ShaderWarmup_Default_C")));

	FDirectoryPath Game;
	Game.Path = TEXT("/Game");
	FoldersToWarmUp.Add(Game);
}
