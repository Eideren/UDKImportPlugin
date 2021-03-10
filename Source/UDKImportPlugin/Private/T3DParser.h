#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
//#define LOCTEXT_NAMESPACE "UDKImportPlugin"

DECLARE_LOG_CATEGORY_EXTERN(UDKImportPluginLog, Log, All);
DECLARE_DELEGATE_OneParam(UObjectDelegate, UObject*);

static void OpenMsgDialog(FString f)
{
	FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(f));
}

class T3DParser
{
public:
	struct FRequirement
	{
		FString Type;

		FString RelDirectory;

		FString Name;

		FString OriginalUrl;
		
		FString Url;

		FRequirement()
		{}

		bool operator==(const FRequirement& second)
		{
			return this->Type == second.Type 
			&& this->RelDirectory == second.RelDirectory
			&& this->Name == second.Name
			&& this->OriginalUrl == second.OriginalUrl
			&& this->Url == second.Url;
		}
		/*friend  uint32 GetTypeHash(const FRequirement& Other)
		{
			return (uint32)0;
		}*/

		FORCEINLINE friend uint32 GetTypeHash(const T3DParser::FRequirement& R)
		{
			return FCrc::Strihash_DEPRECATED(*(R.Url));
		}
	};
protected:
	static float UnrRotToDeg;
	static float IntensityMultiplier;

	T3DParser(const FString &SourcePath, const FString &DestPath);

	int32 StatusNumerator, StatusDenominator;

	FString SourcePath, DestPath;

	/// Ressources requirements
	TMap<FRequirement, TArray<UObjectDelegate> > Requirements;
	TMap<FRequirement, UObject*> FixedRequirements;
	void AddRequirement(const FString &UDKRequiredObjectName, UObjectDelegate Action);
	void FixRequirement(const FString &UDKRequiredObjectName, UObject * Object);
	bool FindRequirement(const FString &UDKRequiredObjectName, UObject * &Object);
	void AddRequirement(const FRequirement &Requirement, UObjectDelegate Action);
	void FixRequirement(const FRequirement &Requirement, UObject * Object);
	bool FindRequirement(const FRequirement &Requirement, UObject * &Object);
	void PrintMissingRequirements();

	/// Line parsing
	int32 LineIndex, ParserLevel;
	TArray<FString> Lines;
	FString Line, RelDirectory;
	void ResetParser(const FString &Content);
	bool NextLine();
	bool IgnoreSubs();
	bool IgnoreSubObjects();
	void JumpToEnd();

	/// Line content parsing
	bool IsBeginObject(FString &Class);
	bool IsEndObject();
	bool IsProperty(FString &PropertyName, FString &Value);
	bool IsActorLocation(AActor * Actor);
	bool IsActorRotation(AActor * Actor);
	bool IsActorScale(AActor * Actor);
	bool IsActorProperty(AActor * Actor);

	/// Value parsing
	bool GetOneValueAfter(const FString &Key, FString &Value, int32 maxindex = MAX_int32);
	bool GetProperty(const FString &Key, FString &Value);
	bool ParseUDKRotation(const FString &InSourceString, FRotator &Rotator);
	bool ParseFVector(const TCHAR* Stream, FVector& Value);
	bool ParseRessourceUrl(const FString &Url, FRequirement &Requirement);

	FString GetPathToT3D(FString relDirectory, FString Name)
	{
		return SourcePath / relDirectory.Replace(_T("."), _T("/")) / Name + ".T3D";
	}

	FString GetPathToUAsset(FString relDirectory, FString Name)
	{
		return GetPathToDirectory(relDirectory) / Name + "." + Name;
	}

	FString GetPathToDirectory(FString relDirectory)
	{
		return "/Game" / DestPath / relDirectory.Replace( _T("."), _T("/") );
	}

private:
	void ParseRessourceUrl(const FString &Url, FString &relDirectory, FString &Name);
	bool ParseRessourceUrl(const FString &Url, FString &Type, FString &relDirectory, FString &Name);
};

FORCEINLINE bool operator==(const T3DParser::FRequirement& A, const T3DParser::FRequirement& B)
{
	return A.Url == B.Url;
}

FORCEINLINE bool T3DParser::ParseRessourceUrl(const FString &Url, FRequirement &Requirement)
{
	Requirement.OriginalUrl = Url;
	if (ParseRessourceUrl(Url, Requirement.Type, Requirement.RelDirectory, Requirement.Name))
	{
		Requirement.Url = FString::Printf(TEXT("%s'%s.%s'"), *Requirement.Type, *Requirement.RelDirectory, *Requirement.Name);
		return true;
	}
	return false;
}

FORCEINLINE bool T3DParser::GetProperty(const FString &Key, FString &Value)
{
	return GetOneValueAfter(Key, Value, 0);
}
