﻿#include "T3DParser.h"
#include "UDKImportPluginPrivatePCH.h"

DEFINE_LOG_CATEGORY(UDKImportPluginLog);

float T3DParser::UnrRotToDeg = 0.00549316540360483;
float T3DParser::IntensityMultiplier = 5000;

T3DParser::T3DParser(const FString &SourcePath, const FString &DestPath)
{
	this->SourcePath = SourcePath;
	this->DestPath = DestPath;
}

inline bool IsWhitespace(TCHAR c) 
{ 
	return c == LITERAL(TCHAR, ' ') || c == LITERAL(TCHAR, '\t') || c == LITERAL(TCHAR, '\r');
}

void T3DParser::ResetParser(const FString &Content)
{
	LineIndex = 0;
	ParserLevel = 0;
	Content.ParseIntoArray(Lines, TEXT("\n"), true);
}

bool T3DParser::NextLine()
{
	if (LineIndex < Lines.Num())
	{
		int32 Start, End;
		const FString &String = Lines[LineIndex];
		Start = 0;
		End = String.Len();

		// Trimming
		while (Start < End && IsWhitespace(String[Start]))
		{
			++Start;
		}

		while (End > Start && IsWhitespace(String[End-1]))
		{
			--End;
		}

		Line = String.Mid(Start, End - Start);
		++LineIndex;
		return true;
	}
	return false;
}

bool T3DParser::IgnoreSubObjects()
{
	while (Line.StartsWith(TEXT("Begin Object "), ESearchCase::CaseSensitive))
	{
		JumpToEnd();
		if (!NextLine())
			return false;
	}

	return true;
}

bool T3DParser::IgnoreSubs()
{
	while (Line.StartsWith(TEXT("Begin "), ESearchCase::CaseSensitive))
	{
		JumpToEnd();
		if (!NextLine())
			return false;
	}

	return true;
}

void T3DParser::JumpToEnd()
{
	int32 Level = 1;
	while (NextLine())
	{
		if (Line.StartsWith(TEXT("Begin "), ESearchCase::CaseSensitive))
		{
			++Level;
		}
		else if (Line.StartsWith(TEXT("End "), ESearchCase::CaseSensitive))
		{
			--Level;
			if (Level == 0)
				break;
		}
	}
}

bool T3DParser::IsBeginObject(FString &Class)
{
	if (Line.StartsWith(TEXT("Begin Object "), ESearchCase::CaseSensitive))
	{
		GetOneValueAfter(TEXT(" Class="), Class);
		return true;
	}
	return false;
}

bool T3DParser::IsEndObject()
{
	return Line.Equals(TEXT("End Object"));
}

bool T3DParser::GetOneValueAfter(const FString &Key, FString &Value, int32 maxindex)
{
	int32 start = Line.Find(Key, ESearchCase::CaseSensitive, ESearchDir::FromStart, 0);
	if (start != -1 && start <= maxindex)
	{
		start += Key.Len();

		const TCHAR * Buffer = *Line + start;
		if (*Buffer == TCHAR('"'))
		{
			++start;
			++Buffer;
			bool Escaping = false;
			while (*Buffer && (*Buffer != TCHAR('"') || Escaping))
			{
				if (Escaping)
					Escaping = false;
				else if (*Buffer == TCHAR('\\'))
					Escaping = true;
				++Buffer;
			}
		}
		else if(*Buffer == TCHAR('('))
		{
			++Buffer;
			int Level = 1;
			while (*Buffer && Level != 0)
			{
				if (*Buffer == TCHAR('('))
					++Level;
				else if (*Buffer == TCHAR(')'))
					--Level;
				++Buffer;
			}
		}
		else
		{
			while (*Buffer && *Buffer != TCHAR(' ') && *Buffer != TCHAR(',') && *Buffer != TCHAR(')'))
			{
				++Buffer;
			}
		}
		Value = Line.Mid(start, Buffer - *Line - start);

		return true;
	}
	return false;
}

void T3DParser::AddRequirement(const FString &UDKRequiredObjectName, UObjectDelegate Action)
{
	FRequirement Requirement;
	if (!ParseRessourceUrl(UDKRequiredObjectName, Requirement))
	{
		UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to parse ressource url : %s"), *UDKRequiredObjectName);
		return;
	}
	AddRequirement(Requirement, Action);
}

void T3DParser::AddRequirement(const FRequirement &Requirement, UObjectDelegate Action)
{
	UObject ** pObject = FixedRequirements.Find(Requirement);
	if (pObject != NULL)
	{
		Action.ExecuteIfBound(*pObject);
		return;
	}

	FString ObjectPath = GetPathToUAsset(Requirement.RelDirectory, Requirement.Name);
	UObject * asset = NULL;

	if (Requirement.Type == TEXT("StaticMesh"))
		asset = LoadObject<UStaticMesh>(NULL, *ObjectPath);
	/*else if (Requirement.Type == TEXT("Material") || Requirement.Type == TEXT("DecalMaterial"))
		asset = LoadObject<UMaterial>(NULL, *ObjectPath);*/
	else if (Requirement.Type.StartsWith(TEXT("Texture")))
		asset = LoadObject<UTexture>(NULL, *ObjectPath);

	
	TArray<UObjectDelegate> * pActions = Requirements.Find(Requirement);
	if (pActions != NULL)
	{
		pActions->Add(Action);
	}
	else
	{
		TArray<UObjectDelegate> Actions;
		Actions.Add(Action);
		Requirements.Add(Requirement, Actions);
	}

	if (asset)
	{
		FixRequirement(Requirement, asset);
	}
}

void T3DParser::FixRequirement(const FString &UDKRequiredObjectName, UObject * Object)
{
	FRequirement Requirement;
	if (!ParseRessourceUrl(UDKRequiredObjectName, Requirement))
	{
		UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to parse ressource url : %s"), *UDKRequiredObjectName);
		return;
	}
	FixRequirement(Requirement, Object);
}

void T3DParser::FixRequirement(const FRequirement &Requirement, UObject * Object)
{
	if (Object == NULL)
		return;

	FixedRequirements.Add(FRequirement(Requirement), Object);

	TArray<UObjectDelegate> * pActions = Requirements.Find(Requirement);
	if (pActions != NULL)
	{
		for (auto IterActions = pActions->CreateConstIterator(); IterActions; ++IterActions)
		{
			IterActions->ExecuteIfBound(Object);
		}
		Requirements.Remove(Requirement);
	}
}

bool T3DParser::FindRequirement(const FString &UDKRequiredObjectName, UObject * &Object)
{
	FRequirement Requirement;
	if (!ParseRessourceUrl(UDKRequiredObjectName, Requirement))
	{
		UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to parse ressource url : %s"), *UDKRequiredObjectName);
		return false;
	}
	return FindRequirement(Requirement, Object);
}

bool T3DParser::FindRequirement(const FRequirement &Requirement, UObject * &Object)
{
	UObject ** pObject = FixedRequirements.Find(Requirement);
	if (pObject != NULL)
	{
		Object = *pObject;
		return true;
	}

	return false;
}

void T3DParser::PrintMissingRequirements()
{
	for (auto Iter = Requirements.CreateConstIterator(); Iter; ++Iter)
	{
		const FRequirement &Requirement = Iter.Key();
		UE_LOG(UDKImportPluginLog, Warning, TEXT("Missing requirements : %s"), *Requirement.Url);
	}
}

bool T3DParser::ParseUDKRotation(const FString &InSourceString, FRotator &Rotator)
{
	int32 Pitch = 0;
	int32 Yaw = 0;
	int32 Roll = 0;

	const bool bSuccessful = FParse::Value(*InSourceString, TEXT("Pitch="), Pitch) && FParse::Value(*InSourceString, TEXT("Yaw="), Yaw) && FParse::Value(*InSourceString, TEXT("Roll="), Roll);

	Rotator.Pitch = Pitch * UnrRotToDeg;
	Rotator.Yaw = Yaw * UnrRotToDeg;
	Rotator.Roll = Roll * UnrRotToDeg;

	return bSuccessful;

}

bool T3DParser::ParseFVector(const TCHAR* Stream, FVector& Value)
{
	Value = FVector::ZeroVector;

	Value.X = FCString::Atof(Stream);
	Stream = FCString::Strchr(Stream, ',');
	if (!Stream)
	{
		return false;
	}

	Stream++;
	Value.Y = FCString::Atof(Stream);
	Stream = FCString::Strchr(Stream, ',');
	if (!Stream)
	{
		return false;
	}

	Stream++;
	Value.Z = FCString::Atof(Stream);

	return true;
}

bool T3DParser::IsProperty(FString &PropertyName, FString &Value)
{
	int32 Index;
	if (Line.FindChar('=', Index) && Index > 0)
	{
		PropertyName = Line.Mid(0, Index);
		Value = Line.Mid(Index + 1);
		return true;
	}

	return false;
}

bool T3DParser::IsActorLocation(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("Location="), Value))
	{
		FVector Location;
		ensure(Location.InitFromString(Value));
		Actor->SetActorLocation(Location);
		return true;
	}

	return false;
}

bool T3DParser::IsActorRotation(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("Rotation="), Value))
	{
		FRotator Rotator;
		ensure(ParseUDKRotation(Value, Rotator));
		Actor->SetActorRotation(Rotator);
		return true;
	}

	return false;
}

bool T3DParser::IsActorScale(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("DrawScale="), Value))
	{
		float DrawScale = FCString::Atof(*Value);
		Actor->SetActorScale3D(Actor->GetActorScale() * DrawScale);
		return true;
	}
	else if (GetProperty(TEXT("DrawScale3D="), Value))
	{
		FVector DrawScale3D;
		ensure(DrawScale3D.InitFromString(Value));
		Actor->SetActorScale3D(Actor->GetActorScale() * DrawScale3D);
		return true;
	}

	return false;
}

bool T3DParser::IsActorProperty(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("Layer="), Value))
	{
		Actor->Layers.Add(FName(*Value));
		return true;
	}

	return false;
}

void T3DParser::ParseRessourceUrl(const FString &Url, FString &relDirectory, FString &Name)
{
	int32 PackageIndex;

	PackageIndex = Url.Find(".", ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	if (PackageIndex == -1)
	{
		relDirectory = this->RelDirectory;
		Name = Url;
	}
	else
	{
		relDirectory = Url.Mid(0, PackageIndex);
		Name = Url.Mid(PackageIndex + 1);
	}
}

bool T3DParser::ParseRessourceUrl(const FString &Url, FString &Type, FString &relDirectory, FString &Name)
{
	int32 Index, PackageIndex;

	if (!Url.FindChar('\'', Index) || !Url.EndsWith(TEXT("'")))
		return false;

	Type = Url.Mid(0, Index);
	++Index;
	PackageIndex = Url.Find(".", ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	if (PackageIndex == -1)
	{
		// Package Name is the current Package
		relDirectory = this->RelDirectory;
		Name = Url.Mid(Index, Url.Len() - Index - 1);
	}
	else
	{
		relDirectory = Url.Mid(Index, PackageIndex - Index);
		Name = Url.Mid(PackageIndex + 1, Url.Len() - PackageIndex - 2);
	}

	return true;
}
