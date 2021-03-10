#include "SUDKImportScreen.h"
#include "UDKImportPluginPrivatePCH.h"
#include "T3DLevelParser.h"

#define LOCTEXT_NAMESPACE "UDKImportScreen"

DEFINE_LOG_CATEGORY(LogUDKImportPlugin);

FText EUDKImportMode::ToName(const Type ExportType)
{
	switch (ExportType)
	{
	case Map: return LOCTEXT("UDKImportMode_Name_LVL", "Level");
	case StaticMesh: return LOCTEXT("UDKImportMode_Name_SM", "Static Mesh");
	case Material: return LOCTEXT("UDKImportMode_Name_M", "Material");
	case MaterialInstanceConstant: return LOCTEXT("UDKImportMode_Name_MIC", "Material Instance Constant");

	default: return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

FText EUDKImportMode::ToDescription(const Type ExportType)
{
	switch (ExportType)
	{
	case Map: return LOCTEXT("UDKImportMode_Desc_LVL", "Import the requested map");
	case StaticMesh: return LOCTEXT("UDKImportMode_Desc_SM", "Import StaticMeshes (whole package or just one)");
	case Material: return LOCTEXT("UDKImportMode_Desc_M", "Import Materials (whole package or just one)");
	case MaterialInstanceConstant: return LOCTEXT("UDKImportMode_Desc_MIC", "Import MaterialInstances (whole package or just one)");

	default: return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

void SUDKImportScreen::ExportType_OnSelectionChanged(TSharedPtr<EUDKImportMode::Type> NewExportMode, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ExportMode = *NewExportMode;
	}
}

TSharedRef<SWidget> SUDKImportScreen::ExportType_OnGenerateWidget(TSharedPtr<EUDKImportMode::Type> InExportMode) const
{
	return SNew(STextBlock)
		.Text(EUDKImportMode::ToName(*InExportMode))
		.ToolTipText(EUDKImportMode::ToDescription(*InExportMode));
}

FText SUDKImportScreen::ExportType_GetSelectedText() const
{
	return EUDKImportMode::ToName(ExportMode);
}

SUDKImportScreen::SUDKImportScreen()
	: ExportMode(EUDKImportMode::Material)
{

}

SUDKImportScreen::~SUDKImportScreen()
{

}

void SUDKImportScreen::Construct(const FArguments& Args)
{	
	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExportType", "Type"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TmpDirLabel", "Import path"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DestDirLabel", "Destination path"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(2.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			[
				SAssignNew(ExportTypeComboBox, SComboBox< TSharedPtr<EUDKImportMode::Type> >)
				.OptionsSource(&ExportTypeOptionsSource)
				.OnSelectionChanged(this, &SUDKImportScreen::ExportType_OnSelectionChanged)
				.OnGenerateWidget(this, &SUDKImportScreen::ExportType_OnGenerateWidget)
				[
					SNew(STextBlock)
					.Text(this, &SUDKImportScreen::ExportType_GetSelectedText)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			[
				SAssignNew(SSourcePath, SEditableTextBox)
				.Text(FText::FromString(TEXT("C:/Materials")))
				.ToolTipText(LOCTEXT("ExportTmpFolder", "An existing directory containing assets you want to import, for levels you must name that file PersistentLevel.T3D"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			[
				SAssignNew(SDestPath, SEditableTextBox)
				.Text(FText::FromString(TEXT("Original/")))
				.ToolTipText(LOCTEXT("DestFolder", "A directory within this unreal project to find references and import those assets into"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("Run", "Run"))
				.OnClicked(this, &SUDKImportScreen::OnRun)
			]
		]
	];

	ExportTypeOptionsSource.Reset(5);
	//ExportTypeOptionsSource.Add(MakeShareable(new EUDKImportMode::Type(EUDKImportMode::Map)));
	//ExportTypeOptionsSource.Add(MakeShareable(new EUDKImportMode::Type(EUDKImportMode::StaticMesh)));
	ExportTypeOptionsSource.Add(MakeShareable(new EUDKImportMode::Type(EUDKImportMode::Material)));
	//ExportTypeOptionsSource.Add(MakeShareable(new EUDKImportMode::Type(EUDKImportMode::MaterialInstanceConstant)));/*Instance constants can be imported through 'Material'*/
}

FReply SUDKImportScreen::OnRun()
{
	FString SourcePath = SSourcePath.Get()->GetText().ToString();
	SourcePath = SourcePath.Replace(_TEXT("\\"), _TEXT("/"));
	SourcePath.RemoveFromEnd("/");
	FString DestPath = SDestPath.Get()->GetText().ToString();
	DestPath = DestPath.Replace(_TEXT("\\"), _TEXT("/"));
	DestPath.RemoveFromEnd("/");
	DestPath.RemoveFromStart("/");
	
	switch (ExportMode)
	{
	case EUDKImportMode::Map:
	{
		T3DLevelParser Parser(SourcePath, DestPath);
		Parser.ImportLevel();
		break;
	}
	case EUDKImportMode::StaticMesh:
	{
		T3DLevelParser Parser(SourcePath, DestPath);
		Parser.ImportStaticMesh();
		break;
	}
	case EUDKImportMode::Material:
	{
		T3DLevelParser Parser(SourcePath, DestPath);
		Parser.ImportMaterial();
		break;
	}
	case EUDKImportMode::MaterialInstanceConstant:
	{
		T3DLevelParser Parser(SourcePath, DestPath);
		Parser.ImportMaterialInstanceConstant();
		break;
	}
	}
	
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE