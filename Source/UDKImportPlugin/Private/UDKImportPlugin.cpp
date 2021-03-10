// Copyright Epic Games, Inc. All Rights Reserved.

#include "UDKImportPlugin.h"
#include "UDKImportPluginEdMode.h"
#include "LevelEditor.h"
#include "SUDKImportScreen.h"

#define LOCTEXT_NAMESPACE "FUDKImportPluginModule"

const FName FUDKImportPluginModule::UDKImportPluginTabName(TEXT("UDKImportPlugin"));

void FUDKImportPluginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorModeRegistry::Get().RegisterMode<FUDKImportPluginEdMode>(FUDKImportPluginEdMode::EM_UDKImportPluginEdModeId, LOCTEXT("UDKImportPluginEdModeName", "UDKImportPluginEdMode"), FSlateIcon(), true);
	if (!IsRunningCommandlet())
	{
		// Add menu option for UDK import
		MainMenuExtender = MakeShareable(new FExtender);
		MainMenuExtender->AddMenuExtension("FileProject", EExtensionHook::After, NULL, FMenuExtensionDelegate::CreateRaw(this, &FUDKImportPluginModule::AddSummonUDKImportMenuExtension));
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
	}
}

void FUDKImportPluginModule::AddSummonUDKImportMenuExtension(FMenuBuilder& MenuBuilder)
{
	FSlateIcon* IconBrush = new FSlateIcon();/*FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tutorials")*/
	MenuBuilder.AddMenuEntry(LOCTEXT("UDKImportMenuEntryTitle", "UDK Import"),
		LOCTEXT("UDKImportMenuEntryToolTip", "Opens up a tool for importing UDK assets."),
		*IconBrush,
		FUIAction(FExecuteAction::CreateRaw(this, &FUDKImportPluginModule::SummonUDKImport)));
}

void FUDKImportPluginModule::SummonUDKImport()
{
	const FText UDKImportWindowTitle = LOCTEXT("UDKImportWindowTitle", "UDK importation tool, based on Speedy37's UDKImportPlugin");
	
	TSharedPtr<SWindow> UDKImportWindow =
		SNew(SWindow)
		.Title(UDKImportWindowTitle)
		.ClientSize(FVector2D(600.f, 150.f))
		.SupportsMaximize(false).SupportsMinimize(false)
		.SizingRule(ESizingRule::Autosized)
		[
			SNew(SUDKImportScreen)
		];

	FSlateApplication::Get().AddWindow(UDKImportWindow.ToSharedRef());
}


void FUDKImportPluginModule::ShutdownModule()
{
	// Unregister the tab spawner
	FGlobalTabmanager::Get()->UnregisterTabSpawner(UDKImportPluginTabName);
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FEditorModeRegistry::Get().UnregisterMode(FUDKImportPluginEdMode::EM_UDKImportPluginEdModeId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUDKImportPluginModule, UDKImportPlugin)