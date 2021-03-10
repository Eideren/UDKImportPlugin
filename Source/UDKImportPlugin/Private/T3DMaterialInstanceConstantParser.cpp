#include "T3DMaterialInstanceConstantParser.h"
#include "UDKImportPluginPrivatePCH.h"
#include "T3DLevelParser.h"

T3DMaterialInstanceConstantParser::T3DMaterialInstanceConstantParser(T3DLevelParser * ParentParser, const FString &RelDirectory) : T3DParser(ParentParser->SourcePath, ParentParser->DestPath)
{
	this->LevelParser = ParentParser;
	this->RelDirectory = RelDirectory;
	this->MaterialInstanceConstant = NULL;
}

UMaterialInstanceConstant* T3DMaterialInstanceConstantParser::ImportT3DFile(const FString &FileName, const FRequirement &req)
{
	FString MaterialT3D;
	if (FFileHelper::LoadFileToString(MaterialT3D, *FileName))
	{
		ResetParser(MaterialT3D);
		MaterialT3D.Empty();
		return ImportMaterialInstanceConstant(req);
	}

	return NULL;
}

UMaterialInstanceConstant*  T3DMaterialInstanceConstantParser::ImportMaterialInstanceConstant(const FRequirement &req)
{
	FString ClassName, Name, Value;
	int32 ParameterIndex;

	ensure(NextLine());
	ensure(IsBeginObject(ClassName));
	ensure(ClassName == TEXT("MaterialInstanceConstant"));
	ensure(GetOneValueAfter(TEXT(" Name="), Name));

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UMaterialInstanceConstantFactoryNew* MaterialFactory = NewObject<UMaterialInstanceConstantFactoryNew>(UMaterialInstanceConstantFactoryNew::StaticClass());
	FString ObjectPath = GetPathToUAsset(*req.RelDirectory, *req.Name);
	MaterialInstanceConstant = LoadObject<UMaterialInstanceConstant>(NULL, *ObjectPath, NULL, LOAD_NoWarn | LOAD_Quiet);
	if (MaterialInstanceConstant == NULL)
	{
		MaterialInstanceConstant = (UMaterialInstanceConstant*)AssetToolsModule.Get().CreateAsset(Name, GetPathToDirectory(req.RelDirectory), UMaterialInstanceConstant::StaticClass(), MaterialFactory);
	}
	else
	{
		MaterialInstanceConstant->TextureParameterValues.Empty();
		MaterialInstanceConstant->ScalarParameterValues.Empty();
		MaterialInstanceConstant->VectorParameterValues.Empty();
		MaterialInstanceConstant->VectorParameterValues.Empty();
	}
	if (MaterialInstanceConstant == NULL)
	{
		return NULL;
	}

	MaterialInstanceConstant->Modify();

	while (NextLine() && IgnoreSubObjects() && !IsEndObject())
	{
		if (IsBeginObject(ClassName))
		{
			JumpToEnd();
		}
		else if (IsParameter(TEXT("TextureParameterValues"), ParameterIndex, Value))
		{
			if (ParameterIndex >= MaterialInstanceConstant->TextureParameterValues.Num())
				MaterialInstanceConstant->TextureParameterValues.SetNum(ParameterIndex + 1);

			FTextureParameterValue &Parameter = MaterialInstanceConstant->TextureParameterValues[ParameterIndex];
			if (GetOneValueAfter(TEXT("ParameterName="), Value))
			{
				Value.RemoveFromStart("\"");
				Value.RemoveFromEnd("\"");
				Parameter.ParameterInfo.Name = *Value;
			}
			if (GetOneValueAfter(TEXT("ParameterValue="), Value))
			{
				FRequirement Requirement;
				if (ParseRessourceUrl(Value, Requirement))
				{
					LevelParser->AddRequirement(Requirement, UObjectDelegate::CreateRaw(LevelParser, &T3DLevelParser::SetTextureParameterValue, MaterialInstanceConstant, ParameterIndex));
				}
				else
				{
					UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to parse ressource url : %s"), *Value);
				}
			}
			SetSwitchParameter(MaterialInstanceConstant, Parameter.ParameterInfo, true, true);
		}
		else if (IsParameter(TEXT("ScalarParameterValues"), ParameterIndex, Value))
		{
			if (ParameterIndex >= MaterialInstanceConstant->ScalarParameterValues.Num())
				MaterialInstanceConstant->ScalarParameterValues.SetNum(ParameterIndex + 1);

			FScalarParameterValue &Parameter = MaterialInstanceConstant->ScalarParameterValues[ParameterIndex];
			if (GetOneValueAfter(TEXT("ParameterName="), Value))
			{
				Value.RemoveFromStart("\"");
				Value.RemoveFromEnd("\"");
				Parameter.ParameterInfo.Name = *Value;
			}
			if (GetOneValueAfter(TEXT("ParameterValue="), Value))
				Parameter.ParameterValue = FCString::Atoi(*Value);
		}
		else if (IsParameter(TEXT("VectorParameterValues"), ParameterIndex, Value))
		{
			if (ParameterIndex >= MaterialInstanceConstant->VectorParameterValues.Num())
				MaterialInstanceConstant->VectorParameterValues.SetNum(ParameterIndex + 1);

			FVectorParameterValue &Parameter = MaterialInstanceConstant->VectorParameterValues[ParameterIndex];
			if (GetOneValueAfter(TEXT("ParameterName="), Value))
			{
				Value.RemoveFromStart("\"");
				Value.RemoveFromEnd("\"");
				Parameter.ParameterInfo.Name = *Value;
			}
			if (GetOneValueAfter(TEXT("ParameterValue="), Value))
				Parameter.ParameterValue.InitFromString(Value);
		}
		else if (GetProperty(TEXT("Parent="), Value))
		{
			FRequirement Requirement;
			if (ParseRessourceUrl(Value, Requirement))
			{
				LevelParser->AddRequirement(Requirement, UObjectDelegate::CreateRaw(LevelParser, &T3DLevelParser::SetParent, MaterialInstanceConstant));
			}
			else
			{
				UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to parse ressource url : %s"), *Value);
			}
		}
		else
		{
			// Could be important
			UE_LOG(UDKImportPluginLog, Warning, TEXT("Unimplemented handling for %s"), *Line);
		}
	}

	/*
	for (int32 CheckIdx = 0; CheckIdx < MaterialInstanceConstant->TextureParameterValues.Num(); CheckIdx++)
	{
		auto param = &MaterialInstanceConstant->TextureParameterValues[CheckIdx];
		param->ExpressionGUID = GetGuid(MaterialInstanceConstant, param->ParameterInfo.Name);
	}
	for (int32 CheckIdx = 0; CheckIdx < MaterialInstanceConstant->VectorParameterValues.Num(); CheckIdx++)
	{
		auto param = &MaterialInstanceConstant->VectorParameterValues[CheckIdx];
		param->ExpressionGUID = GetGuid(MaterialInstanceConstant, param->ParameterInfo.Name);
	}
	for (int32 CheckIdx = 0; CheckIdx < MaterialInstanceConstant->ScalarParameterValues.Num(); CheckIdx++)
	{
		auto param = &MaterialInstanceConstant->ScalarParameterValues[CheckIdx];
		param->ExpressionGUID = GetGuid(MaterialInstanceConstant, param->ParameterInfo.Name);
	}*/

	// Enable all statics as I don't know how to query for these
	/*FStaticParameterSet CompositedStaticParameters;
	MaterialInstanceConstant->GetStaticParameterValues(CompositedStaticParameters);
	for (int32 CheckIdx = 0; CheckIdx < CompositedStaticParameters.StaticSwitchParameters.Num(); CheckIdx++)
	{
		FStaticSwitchParameter& SwitchParam = CompositedStaticParameters.StaticSwitchParameters[CheckIdx];
		SwitchParam.bOverride = true;
		SwitchParam.Value = true;
	}

	MaterialInstanceConstant->UpdateStaticPermutation(CompositedStaticParameters);
	*/
	MaterialInstanceConstant->Modify(); // Might force save changes to materials

	return MaterialInstanceConstant;
}

bool T3DMaterialInstanceConstantParser::IsParameter(const FString &Key, int32 &index, FString &Value)
{
	const TCHAR* Stream = *Line;

	if (FParse::Command(&Stream, *Key) && *Stream == TCHAR('('))
	{
		++Stream;
		index = FCString::Atoi(Stream);
		while (FChar::IsAlnum(*Stream))
		{
			++Stream;
		}
		if (*Stream == TCHAR(')'))
		{
			++Stream;
			if (*Stream == TCHAR('='))
			{
				++Stream;
				Value = Stream;
				return true;
			}
		}
	}

	return false;
}
