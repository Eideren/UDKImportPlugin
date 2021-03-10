#include "T3DLevelParser.h"
#include "UDKImportPluginPrivatePCH.h"
#include "Editor/UnrealEd/Public/BSPOps.h"
#include "Runtime/Engine/Public/ComponentReregisterContext.h"
#include "Runtime/Engine/Classes/Sound/SoundNode.h"
#include "T3DMaterialParser.h"
#include "T3DMaterialInstanceConstantParser.h"

#define LOCTEXT_NAMESPACE "UDKImportPlugin"

T3DLevelParser::T3DLevelParser(const FString &SourcePath, const FString &DestPath) : T3DParser(SourcePath, DestPath)
{
	this->World = NULL;
}

template<class T>
T * T3DLevelParser::SpawnActor()
{
	if (World == NULL)
	{
		FLevelEditorModule & LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		World = LevelEditorModule.GetFirstLevelEditor().Get()->GetWorld();
	}
	ensure(World != NULL);

	return World->SpawnActor<T>();
}

void T3DLevelParser::ImportLevel()
{
	GWarn->BeginSlowTask(LOCTEXT("StatusBeginLevel", "Importing requested level"), true, false);
	StatusNumerator = 0;
	StatusDenominator = 12;

	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("LoadUDKLevelT3D", "Loading UDK Level informations"));
	{
		FString UdkLevelT3D;
		if (!FFileHelper::LoadFileToString(UdkLevelT3D, *(SourcePath / TEXT("PersistentLevel.T3D"))))
		{
			GWarn->EndSlowTask();
			return;
		}

		ResetParser(UdkLevelT3D);
		RelDirectory = _T("");
	}

	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ParsingUDKLevelT3D", "Parsing UDK Level informations"));
	ImportLevelInternal();

	ResolveRequirements();
	GWarn->EndSlowTask();
}

void T3DLevelParser::ImportStaticMesh()
{
	ImportRessource(EExportType::StaticMesh);
}

void T3DLevelParser::ImportMaterial()
{
	ImportRessource(EExportType::Material);
}

void T3DLevelParser::ImportMaterialInstanceConstant()
{
	ImportRessource(EExportType::MaterialInstanceConstant);
}

void T3DLevelParser::ImportRessource(EExportType::Type Type)
{
	StatusNumerator = 0;
	StatusDenominator = 8;

	GWarn->BeginSlowTask(LOCTEXT("StatusBeginMaterialRessouces", "Scanning files"), true, false);
	ExportPackageToRequirements(Type);

	ResolveRequirements();
	GWarn->EndSlowTask();
}

FString T3DLevelParser::RessourceTypeFor(EExportType::Type Type)
{
	switch (Type)
	{
	case EExportType::Material: return TEXT("Material");
	case EExportType::StaticMesh: return TEXT("StaticMesh");
	case EExportType::MaterialInstanceConstant: return TEXT("MaterialInstanceConstant");
	case EExportType::Texture2D: return TEXT("Texture2D");
	default: return TEXT("Unknow");
	}
}

void T3DLevelParser::ExportPackageToRequirements(EExportType::Type Type)
{
	FString RessourceType = RessourceTypeFor(Type);

	TArray<FString> FileNames;
	IFileManager::Get().FindFilesRecursive(FileNames, *SourcePath, _T("*.T3D"), true, false);
	for (int32 NameIdx = 0; NameIdx < FileNames.Num(); NameIdx++)
	{
		FString relPath = FileNames[NameIdx];
		relPath.RemoveFromEnd(".T3D", ESearchCase::IgnoreCase);
		relPath.RemoveFromStart((SourcePath + "/"), ESearchCase::IgnoreCase);
		relPath = relPath.Replace(_T("/"), _T("."));
		AddRequirement(FString::Printf(TEXT("%s'%s'"), *RessourceType, *relPath), UObjectDelegate());
	}
}

void T3DLevelParser::ResolveRequirements()
{
	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ExportMaterialAssets", "Parsing materials"));
	ExportMaterialAssets();

	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ExportMaterialInstanceConstantAssets", "Parsing material instances"));
	ExportMaterialInstanceConstantAssets();
	
	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ResolvingLinks", "Fixing reference with existing assets"));
	//UTexture2D * DefaultTexture2D = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	for (auto Iter = Requirements.CreateConstIterator(); Iter; ++Iter)
	{
		const FRequirement &Requirement = Iter.Key();

		FString ObjectPath = GetPathToUAsset(Requirement.RelDirectory, Requirement.Name);
		UObject * asset = NULL;

		if (Requirement.Type == TEXT("StaticMesh"))
		{
			asset = LoadObject<UStaticMesh>(NULL, *ObjectPath);
		}
		else if (Requirement.Type == TEXT("Material") || Requirement.Type == TEXT("DecalMaterial"))
		{
			asset = LoadObject<UMaterial>(NULL, *ObjectPath);
		}
		else if (Requirement.Type.StartsWith(TEXT("Texture")))
		{
			asset = LoadObject<UTexture>(NULL, *ObjectPath);
			/*if (!asset)
			{
				FixRequirement(Requirement, DefaultTexture2D); // Using default texture
			}*/
		}

		if (asset)
		{
			FixRequirement(Requirement, asset);
		}
	}


	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ResolvingLinks2", "Update existing assets"));

	// make sure that any static meshes, etc using this material will stop using the FMaterialResource of the original 
	// material, and will use the new FMaterialResource created when we make a new UMaterial in place
	FGlobalComponentReregisterContext RecreateComponents;

	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ResolvingLinks3", "Compile materials"));
	PostEditChangeFor(TEXT("Material"));
	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ResolvingLinks4", "Compile material instances"));
	PostEditChangeFor(TEXT("MaterialInstanceConstant"));
	GWarn->StatusUpdate(++StatusNumerator, StatusDenominator, LOCTEXT("ResolvingLinks5", "Post edit for meshes"));
	PostEditChangeFor(TEXT("StaticMesh"));

	PrintMissingRequirements();
	int i = 0;
	for (auto Iter = Requirements.CreateConstIterator(); Iter; ++Iter)
	{
		const FRequirement& Requirement = Iter.Key();

		FString ObjectPath = GetPathToUAsset(Requirement.RelDirectory, Requirement.Name);
		LogContent += FString::Printf(TEXT("Failed to find %s '%s'\n"), *Requirement.Type, *ObjectPath);
		i++;
	}
	if (LogContent != TEXT(""))
		OpenMsgDialog(LogContent);
}

void T3DLevelParser::PostEditChangeFor(const FString &Type)
{
	for (auto Iter = FixedRequirements.CreateIterator(); Iter; ++Iter)
	{
		const FRequirement &Requirement = Iter.Key();
		if (Requirement.Type == Type)
		{
			if (Requirement.Name == TEXT("MI_StargateSupport_Base"))
			{
				UE_LOG(UDKImportPluginLog, Warning, TEXT("Test Me : %s"), *Requirement.Url);
			}
			UObject * Object = Iter.Value();
			if (Object)
			{
				Object->PostEditChange();
			}
		}
	}
}

void T3DLevelParser::ExportMaterialInstanceConstantAssets()
{
	bool bRequiresAnotherLoop = false;

	do
	{
		bRequiresAnotherLoop = false;
		for (auto Iter = Requirements.CreateConstIterator(); Iter; ++Iter)
		{
			const FRequirement &Requirement = Iter.Key();

			if (Requirement.Type == TEXT("MaterialInstanceConstant"))
			{
				FString ObjectPath = GetPathToUAsset(*Requirement.RelDirectory, *Requirement.Name);
				UMaterialInstanceConstant * MaterialInstanceConstant = NULL; //LoadObject<UMaterialInstanceConstant>(NULL, *ObjectPath, NULL, LOAD_NoWarn | LOAD_Quiet);
				if (!MaterialInstanceConstant)
				{
					T3DMaterialInstanceConstantParser MaterialInstanceConstantParser(this, Requirement.RelDirectory);
					MaterialInstanceConstant = MaterialInstanceConstantParser.ImportT3DFile(GetPathToT3D(Requirement.RelDirectory, Requirement.Name), Requirement);
				}

				if (MaterialInstanceConstant)
				{
					bRequiresAnotherLoop = true;
					FixRequirement(Requirement, MaterialInstanceConstant);
				}
				else
				{
					UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to import : %s"), *Requirement.Url);
				}
			}
		}
	} while (bRequiresAnotherLoop);
}

void T3DLevelParser::ExportMaterialAssets()
{
	for (auto Iter = Requirements.CreateIterator(); Iter; ++Iter)
	{
		FRequirement &Requirement = Iter.Key();

		if (Requirement.Type == TEXT("Material"))
		{
			FString ObjectPath = GetPathToUAsset(*Requirement.RelDirectory, *Requirement.Name);
			UMaterial * Material = NULL;//LoadObject<UMaterial>(NULL, *ObjectPath, NULL, LOAD_NoWarn | LOAD_Quiet);
			if (!Material)
			{
				T3DMaterialParser MaterialParser(this, Requirement.RelDirectory);
				Material = MaterialParser.ImportMaterialT3DFile(GetPathToT3D(Requirement.RelDirectory, Requirement.Name), Requirement);
			}

			if (Material)
			{
				FixRequirement(Requirement, Material);
			}
		}
	}
}

void T3DLevelParser::ImportLevelInternal()
{
	FString Class;

	ensure(NextLine());
	ensure(Line.Equals(TEXT("Begin Object Class=Level Name=PersistentLevel")));

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class))
		{
			UObject * Object = 0;
			if (Class.Equals(TEXT("StaticMeshActor")))
				ImportStaticMeshActor();
			else if (Class.Equals(TEXT("Brush")))
				ImportBrush();
			else if (Class.Equals(TEXT("PointLight")))
				ImportPointLight();
			else if (Class.Equals(TEXT("SpotLight")))
				ImportSpotLight();
			else
				JumpToEnd();
		}
	}
}

void T3DLevelParser::ImportBrush()
{
	FString Value, Class, Name;
	ABrush * Brush = SpawnActor<ABrush>();
	Brush->BrushType = Brush_Add;
	UModel* Model = NewObject<UModel>(Brush, NAME_None, RF_Transactional);
	Model->Initialize(Brush, 1);

	while (NextLine() && !IsEndObject())
	{
		if (Line.StartsWith(TEXT("Begin Brush ")))
		{
			while (NextLine() && !Line.StartsWith(TEXT("End Brush")))
			{
				if (Line.StartsWith(TEXT("Begin PolyList")))
				{
					ImportPolyList(Model->Polys);
				}
			}
		}
		else if (GetProperty(TEXT("CsgOper="), Value))
		{
			if (Value.Equals(TEXT("CSG_Subtract")))
			{
				Brush->BrushType = Brush_Subtract;
			}
		}
		else if (IsActorLocation(Brush) || IsActorProperty(Brush))
		{
			continue;
		}
		else if (Line.StartsWith(TEXT("Begin "), ESearchCase::CaseSensitive))
		{
			JumpToEnd();
		}
	}
	
	Model->Modify();
	Model->BuildBound();

	Brush->GetBrushComponent()->Brush = Brush->Brush;
	Brush->PostEditImport();
	Brush->PostEditChange();
}

void T3DLevelParser::ImportPolyList(UPolys * Polys)
{
	FString Texture;
	while (NextLine() && !Line.StartsWith(TEXT("End PolyList")))
	{
		if (Line.StartsWith(TEXT("Begin Polygon ")))
		{
			bool GotBase = false;
			FPoly Poly;
			if (GetOneValueAfter(TEXT(" Texture="), Texture))
			{
				AddRequirement(FString::Printf(TEXT("Material'%s'"), *Texture), UObjectDelegate::CreateRaw(this, &T3DLevelParser::SetPolygonTexture, Polys, Polys->Element.Num()));
			}
			FParse::Value(*Line, TEXT("LINK="), Poly.iLink);
			Poly.PolyFlags &= ~PF_NoImport;

			while (NextLine() && !Line.StartsWith(TEXT("End Polygon")))
			{
				const TCHAR* Str = *Line;
				if (FParse::Command(&Str, TEXT("ORIGIN")))
				{
					GotBase = true;
					ParseFVector(Str, Poly.Base);
				}
				else if (FParse::Command(&Str, TEXT("VERTEX")))
				{
					FVector TempVertex;
					ParseFVector(Str, TempVertex);
					new(Poly.Vertices) FVector(TempVertex);
				}
				else if (FParse::Command(&Str, TEXT("TEXTUREU")))
				{
					ParseFVector(Str, Poly.TextureU);
				}
				else if (FParse::Command(&Str, TEXT("TEXTUREV")))
				{
					ParseFVector(Str, Poly.TextureV);
				}
				else if (FParse::Command(&Str, TEXT("NORMAL")))
				{
					ParseFVector(Str, Poly.Normal);
				}
			}
			if (!GotBase)
				Poly.Base = Poly.Vertices[0];
			if (Poly.Finalize(NULL, 1) == 0)
				new(Polys->Element)FPoly(Poly);
		}
	}
}

void T3DLevelParser::ImportPointLight()
{
	FString Value, Class;
	APointLight* PointLight = SpawnActor<APointLight>();

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class))
		{
			if (Class.Equals(TEXT("SpotLightComponent")))
			{
				while (NextLine() && IgnoreSubs() && !IsEndObject())
				{
					if (GetProperty(TEXT("Radius="), Value))
					{
						PointLight->PointLightComponent->AttenuationRadius = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("Brightness="), Value))
					{
						PointLight->PointLightComponent->Intensity = FCString::Atof(*Value) * IntensityMultiplier;
					}
					else if (GetProperty(TEXT("LightColor="), Value))
					{
						FColor Color;
						Color.InitFromString(Value);
						PointLight->PointLightComponent->LightColor = Color;
					}
				}
			}
			else
			{
				JumpToEnd();
			}
		}
		else if (IsActorLocation(PointLight) || IsActorRotation(PointLight) || IsActorProperty(PointLight))
		{
			continue;
		}
	}
	PointLight->PostEditChange();
}

void T3DLevelParser::ImportSpotLight()
{
	FVector DrawScale3D(1.0,1.0,1.0);
	FRotator Rotator(0.0, 0.0, 0.0);
	FString Value, Class, Name;
	ASpotLight* SpotLight = SpawnActor<ASpotLight>();

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class))
		{
			if (Class.Equals(TEXT("SpotLightComponent")))
			{
				while (NextLine() && IgnoreSubs() && !IsEndObject())
				{
					if (GetProperty(TEXT("Radius="), Value))
					{
						SpotLight->SpotLightComponent->AttenuationRadius = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("InnerConeAngle="), Value))
					{
						SpotLight->SpotLightComponent->InnerConeAngle = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("OuterConeAngle="), Value))
					{
						SpotLight->SpotLightComponent->OuterConeAngle = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("Brightness="), Value))
					{
						SpotLight->SpotLightComponent->Intensity = FCString::Atof(*Value) * IntensityMultiplier;
					}
					else if (GetProperty(TEXT("LightColor="), Value))
					{
						FColor Color;
						Color.InitFromString(Value);
						SpotLight->SpotLightComponent->LightColor = Color;
					}
				}
			}
			else
			{
				JumpToEnd();
			}
		}
		else if (IsActorLocation(SpotLight) || IsActorProperty(SpotLight))
		{
			continue;
		}
		else if (GetProperty(TEXT("Rotation="), Value))
		{
			ensure(ParseUDKRotation(Value, Rotator));
		}
		else if (GetProperty(TEXT("DrawScale3D="), Value))
		{
			ensure(DrawScale3D.InitFromString(Value));
		}
	}

	// Because there is people that does this in UDK...
	SpotLight->SetActorRotation((DrawScale3D.X * Rotator.Vector()).Rotation());
	SpotLight->PostEditChange();
}

void T3DLevelParser::ImportStaticMeshActor()
{
	FString Value, Class;
	FVector PrePivot;
	bool bPrePivotFound = false;
	AStaticMeshActor * StaticMeshActor = SpawnActor<AStaticMeshActor>();

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class))
		{
			if (Class.Equals(TEXT("StaticMeshComponent")))
			{
				while (NextLine() && !IsEndObject())
				{
					if (GetProperty(TEXT("StaticMesh="), Value))
					{
						AddRequirement(Value, UObjectDelegate::CreateRaw(this, &T3DLevelParser::SetStaticMesh, StaticMeshActor->GetStaticMeshComponent()));
					}
				}
			}
			else
			{
				JumpToEnd();
			}
		}
		else if (IsActorLocation(StaticMeshActor) || IsActorRotation(StaticMeshActor) || IsActorScale(StaticMeshActor) || IsActorProperty(StaticMeshActor))
		{
			continue;
		}
		else if (GetProperty(TEXT("PrePivot="), Value))
		{
			ensure(PrePivot.InitFromString(Value));
			bPrePivotFound = true;
		}
	}

	if (bPrePivotFound)
	{
		PrePivot = StaticMeshActor->GetActorRotation().RotateVector(PrePivot);
		StaticMeshActor->SetActorLocation(StaticMeshActor->GetActorLocation() - PrePivot);
	}
	StaticMeshActor->PostEditChange();
}

USoundCue * T3DLevelParser::ImportSoundCue()
{
	USoundCue * SoundCue = 0;
	FString Value;

	while (NextLine())
	{
		if (GetProperty(TEXT("SoundClass="), Value))
		{
			// TODO
		}
		else if (GetProperty(TEXT("FirstNode="), Value))
		{
			AddRequirement(Value, UObjectDelegate::CreateRaw(this, &T3DLevelParser::SetSoundCueFirstNode, SoundCue));
		}
	}

	return SoundCue;
}

void T3DLevelParser::SetPolygonTexture(UObject * Object, UPolys * Polys, int32 index)
{
	Polys->Element[index].Material = Cast<UMaterialInterface>(Object);
}

void T3DLevelParser::SetStaticMesh(UObject * Object, UStaticMeshComponent * StaticMeshComponent)
{
	FProperty* ChangedProperty = FindFProperty<FProperty>(UStaticMeshComponent::StaticClass(), "StaticMesh");
	UStaticMesh * StaticMesh = Cast<UStaticMesh>(Object);
	StaticMeshComponent->PreEditChange(ChangedProperty);

	StaticMeshComponent->SetStaticMesh(StaticMesh);

	FPropertyChangedEvent PropertyChangedEvent(ChangedProperty);
	StaticMeshComponent->PostEditChangeProperty(PropertyChangedEvent);
}

void T3DLevelParser::SetSoundCueFirstNode(UObject * Object, USoundCue * SoundCue)
{
	SoundCue->FirstNode = Cast<USoundNode>(Object);
}

void T3DLevelParser::SetStaticMeshMaterial(UObject * Material, FString StaticMeshUrl, int32 MaterialIdx)
{
	AddRequirement(StaticMeshUrl, UObjectDelegate::CreateRaw(this, &T3DLevelParser::SetStaticMeshMaterialResolved, Material, MaterialIdx));
}

void T3DLevelParser::SetStaticMeshMaterialResolved(UObject * Object, UObject * Material, int32 MaterialIdx)
{
	UStaticMesh * StaticMesh = Cast<UStaticMesh>(Object);

	check(StaticMesh->RenderData);
	FMeshSectionInfo Info = StaticMesh->GetSectionInfoMap().Get(0, MaterialIdx);

	if (MaterialIdx >= StaticMesh->StaticMaterials.Num())
		StaticMesh->StaticMaterials.SetNum(MaterialIdx + 1);
	Info.MaterialIndex = MaterialIdx;
	StaticMesh->GetSectionInfoMap().Set(0, MaterialIdx, Info);
	StaticMesh->SetMaterial(MaterialIdx, Cast<UMaterialInterface>(Material));

	StaticMesh->Modify();
	StaticMesh->PostEditChange();
}

void T3DLevelParser::SetTexture(UObject * Object, UMaterialExpressionTextureBase * MaterialExpression)
{
	UTexture * Texture = Cast<UTexture>(Object);
	MaterialExpression->Texture = Texture;
	switch (Texture->CompressionSettings)
	{
	case TC_Normalmap:
		MaterialExpression->SamplerType = SAMPLERTYPE_Normal;
		break;
	case TC_Grayscale:
		MaterialExpression->SamplerType = SAMPLERTYPE_Grayscale;
		break;
	case TC_Masks:
		MaterialExpression->SamplerType = SAMPLERTYPE_Masks;
		break;
	case TC_Alpha:
		MaterialExpression->SamplerType = SAMPLERTYPE_Alpha;
		break;
	default:
		MaterialExpression->SamplerType = SAMPLERTYPE_Color;
		break;
	}
}

void T3DLevelParser::SetParent(UObject * Object, UMaterialInstanceConstant * MaterialInstanceConstant)
{
	UMaterialInterface * MaterialInterface = Cast<UMaterialInterface>(Object);
	MaterialInstanceConstant->Parent = MaterialInterface;
}

void T3DLevelParser::SetTextureParameterValue(UObject * Object, UMaterialInstanceConstant * MaterialInstanceConstant, int32 ParameterIndex)
{
	UTexture * Texture = Cast<UTexture>(Object);
	//auto name = MaterialInstanceConstant->TextureParameterValues[ParameterIndex].ParameterInfo.Name;
	MaterialInstanceConstant->TextureParameterValues[ParameterIndex].ParameterValue = Texture;
	//MaterialInstanceConstant->SetTextureParameterValueEditorOnly(MaterialInstanceConstant->TextureParameterValues[ParameterIndex].ParameterInfo, Texture);
	//SetSwitchParameter(MaterialInstanceConstant, name, true, true);
	//MaterialInstanceConstant->OverrideTexture(MaterialInstanceConstant->TextureParameterValues[ParameterIndex].ParameterValue, Texture, ERHIFeatureLevel::Type::SM5);
	//MaterialInstanceConstant->OverrideTextureParameterDefault(MaterialInstanceConstant->TextureParameterValues[ParameterIndex].ParameterInfo, Texture, true, ERHIFeatureLevel::Type.SM5);
}
