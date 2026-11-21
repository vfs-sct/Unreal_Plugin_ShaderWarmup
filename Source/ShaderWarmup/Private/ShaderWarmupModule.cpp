#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "GameDelegates.h"
#include "Misc/PackageName.h"
#include "ShaderWarmupSettings.h"
#endif

/** Makes sure the chosen screen widget and next level are packaged even though only Project Settings point at them. */
class FShaderWarmupModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		CookHandle = FGameDelegates::Get().GetModifyCookDelegate().AddLambda(
			[](TConstArrayView<const ITargetPlatform*>, TArray<FName>& PackagesToCook, TArray<FName>&)
			{
				const UShaderWarmupSettings* Settings = GetDefault<UShaderWarmupSettings>();
				const FName WidgetPackage = Settings->WidgetClass.ToSoftObjectPath().GetLongPackageFName();
				const FName LevelPackage = Settings->NextLevel.ToSoftObjectPath().GetLongPackageFName();
				// A missing widget (e.g. the old name WBP_ShaderWarmup_Default) is skipped: the plugin's own screen is cooked anyway.
				if (!WidgetPackage.IsNone() && FPackageName::DoesPackageExist(WidgetPackage.ToString())) { PackagesToCook.AddUnique(WidgetPackage); }
				if (!LevelPackage.IsNone()) { PackagesToCook.AddUnique(LevelPackage); }
			});
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		FGameDelegates::Get().GetModifyCookDelegate().Remove(CookHandle);
#endif
	}

private:
#if WITH_EDITOR
	FDelegateHandle CookHandle;
#endif
};

IMPLEMENT_MODULE(FShaderWarmupModule, ShaderWarmup)
