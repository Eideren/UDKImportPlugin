#pragma once

#include "T3DParser.h"

class T3DLevelParser;

class T3DMaterialInstanceConstantParser : public T3DParser
{
public:
	T3DMaterialInstanceConstantParser(T3DLevelParser * ParentParser, const FString &RelDirectory);
	UMaterialInstanceConstant * ImportT3DFile(const FString &FileName, const FRequirement &req);

private:
	T3DLevelParser * LevelParser;

	// T3D Parsing
	UMaterialInstanceConstant * ImportMaterialInstanceConstant(const FRequirement &req);
	UMaterialInstanceConstant * MaterialInstanceConstant;
	bool IsParameter(const FString &Key, int32 &index, FString &Value);

	FGuid GetGuid(UMaterialInstanceConstant* inst, FName name)
	{
		FGuid guid;
		auto material_parent = Cast<UMaterial>(inst->Parent);
		//FName nameWithQuotes = FName("\""+ name.ToString()+"\"");
		for (int32 CheckIdx = 0; CheckIdx < material_parent->Expressions.Num(); CheckIdx++)
		{
			auto exp = Cast<UMaterialExpressionParameter>(material_parent->Expressions[CheckIdx]);
			if (exp && name == exp->ParameterName)
			{
				return exp->ExpressionGUID;
			}

			auto texSample = Cast<UMaterialExpressionTextureSampleParameter>(material_parent->Expressions[CheckIdx]);
			if (texSample && name == texSample->ParameterName)
			{
				return texSample->ExpressionGUID;
			}
		}
		return guid;
	}

	void SetSwitchParameter(UMaterialInstanceConstant* MaterialInstance, FMaterialParameterInfo pInfo, bool SwitchEnabled, bool SwitchValue)
	{
		return;
		bool valid = true;
		FStaticParameterSet StaticParameterSet = MaterialInstance->GetStaticParameters();

		bool isExisting = false;
		for (FStaticSwitchParameter& parameter : StaticParameterSet.StaticSwitchParameters)
		{
			if (parameter.bOverride && parameter.ParameterInfo.Name == pInfo.Name)
			{
				parameter.bOverride = SwitchEnabled;
				parameter.Value = SwitchValue;
				isExisting = true;
				break;
			}
		}

		if (!isExisting)
		{
			FStaticSwitchParameter SwitchParameter;
			SwitchParameter.ParameterInfo = pInfo;
			SwitchParameter.Value = SwitchValue;

			SwitchParameter.bOverride = SwitchEnabled;
			StaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);
		}


		MaterialInstance->UpdateStaticPermutation(StaticParameterSet);
	}
};