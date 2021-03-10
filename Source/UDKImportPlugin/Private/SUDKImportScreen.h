#pragma once
#include "UDKImportPluginPrivatePCH.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUDKImportPlugin, Warning, All);

struct EUDKImportMode
{
	enum Type
	{
		Map,
		StaticMesh,
		Material,
		MaterialInstanceConstant,

		InvalidOrMax
	};

	/**
	* @param ExportType - The value to get the text for.
	*
	* @return text representation of the specified EUDKImportMode value.
	*/
	static FText ToName(const Type ExportType);

	/**
	* @param ExportType - The value to get the text for.
	*
	* @return text representation with more detailed explanation of the specified EUDKImportMode value.
	*/
	static FText ToDescription(const Type ExportType);
};

class SUDKImportScreen : public SCompoundWidget
{
public:
	/** Default constructor. */
	SUDKImportScreen();

	/** Virtual destructor. */
	virtual ~SUDKImportScreen();

	SLATE_BEGIN_ARGS(SUDKImportScreen){}
	SLATE_END_ARGS()

	/** Widget constructor */
	void Construct(const FArguments& Args);

protected:
	/** Run the import tool */
	FReply OnRun();

	void ExportType_OnSelectionChanged(TSharedPtr<EUDKImportMode::Type> NewSortingMode, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> ExportType_OnGenerateWidget(TSharedPtr<EUDKImportMode::Type> InSortingMode) const;

	FText ExportType_GetSelectedText() const;

protected:
	/** Path to the temporary directory to use */
	TSharedPtr<SEditableTextBox> SSourcePath;

	/** Path to the temporary directory to use */
	TSharedPtr<SEditableTextBox> SDestPath;

	/** Path to the temporary directory to use */
	TSharedPtr<SComboBox< TSharedPtr<EUDKImportMode::Type> > > ExportTypeComboBox;

	TArray<TSharedPtr<EUDKImportMode::Type>> ExportTypeOptionsSource;

	EUDKImportMode::Type ExportMode;
};
