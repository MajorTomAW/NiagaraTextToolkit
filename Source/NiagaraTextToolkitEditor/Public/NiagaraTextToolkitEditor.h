// Made by Lucian Tranc

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSlateStyleSet;

class FNiagaraTextToolkitEditorModule : public IModuleInterface
{
	TSharedPtr<FSlateStyleSet> StyleSet;

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};


