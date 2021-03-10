#include "T3DMaterialParser.h"
#include "UDKImportPluginPrivatePCH.h"
#include "T3DLevelParser.h"

T3DMaterialParser::T3DMaterialParser(T3DLevelParser * ParentParser, const FString &RelDirectory) : T3DParser(ParentParser->SourcePath, ParentParser->DestPath)
{
	this->LevelParser = ParentParser;
	this->RelDirectory = RelDirectory;
	this->Material = NULL;
}

UMaterial* T3DMaterialParser::ImportMaterialT3DFile(const FString &FileName, FRequirement &req)
{
	FString MaterialT3D;
	if (FFileHelper::LoadFileToString(MaterialT3D, *FileName))
	{
		ResetParser(MaterialT3D);
		MaterialT3D.Empty();
		return ImportMaterial(req);
	}

	return NULL;
}

UMaterial*  T3DMaterialParser::ImportMaterial(FRequirement &req)
{
	FString ClassName, Name, Value;
	UClass * Class;

	ensure(NextLine());
	ensure(IsBeginObject(ClassName));
	if (ClassName == TEXT("TextureCube"))
	{
		OpenMsgDialog(FString::Printf(TEXT("Trying to import %s as material is not supported (%s)"), *ClassName, *req.OriginalUrl));
		return NULL;
	}
	if (ClassName == TEXT("MaterialInstanceConstant"))
	{
		req.Type = TEXT("MaterialInstanceConstant");
		return NULL;
	}
	if (ClassName != TEXT("Material") && ClassName != TEXT("DecalMaterial"))
	{
		OpenMsgDialog(FString::Printf(TEXT("Trying to import %s as material is not supported (%s)"), *ClassName, *req.OriginalUrl));
		return NULL;
	}
	ensure(GetOneValueAfter(TEXT(" Name="), Name));

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>(UMaterialFactoryNew::StaticClass());
	FString path = GetPathToDirectory(req.RelDirectory) / Name;
	FString ObjectPath = GetPathToUAsset(*req.RelDirectory, *req.Name);
	Material = LoadObject<UMaterial>(NULL, *ObjectPath, NULL, LOAD_NoWarn | LOAD_Quiet);
	if (Material == NULL)
	{
		Material = (UMaterial*)AssetToolsModule.Get().CreateAsset(Name, GetPathToDirectory(req.RelDirectory), UMaterial::StaticClass(), MaterialFactory);
	}
	else
	{
		// We're overwriting it, empty data
		Material->EditorComments.Empty();
		Material->Expressions.Empty();
		FColorMaterialInput c;
		FScalarMaterialInput s;
		FVectorMaterialInput v;
		Material->BaseColor = c;
		Material->Specular = s;
		Material->Normal = v;
		Material->EmissiveColor = c;
		Material->Opacity = s;
		Material->OpacityMask = s;
	}
	if (Material == NULL)
	{
		UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to import : %s"), *req.Url);
		return NULL;
	}

	Material->Modify();
	bool isDecal = false;
	if(ClassName == TEXT("DecalMaterial"))
	{
		Material->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
		isDecal = true;
	}

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(ClassName))
		{
			FString previousClassName;
			bool remapped = false;
			if (ClassName == TEXT("MaterialExpressionReflectionVector"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionReflectionVectorWS");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionConstantClamp"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionClamp");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionCameraVector"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionCameraVectorWS");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionDestDepth"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionSceneDepth");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionMeshEmitterVertexColor"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionParticleColor");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionMeshSubUV"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionTextureSample");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionDestColor"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionSceneColor");
				remapped = true;
			}
			else if (ClassName == TEXT("MaterialExpressionLightVector"))
			{
				previousClassName = ClassName;
				ClassName = TEXT("MaterialExpressionMaterialFunctionCall");
				remapped = true;
			}
			else if (
				ClassName == TEXT("MaterialExpressionDepthBiasedAlpha") 
				|| ClassName == TEXT("MaterialExpressionDepthBiasedBlend")
				|| ClassName == TEXT("MaterialExpressionLensFlareRadialDistance")
				|| ClassName == TEXT("MaterialExpressionLensFlareIntensity")
				|| ClassName == TEXT("MaterialExpressionLensFlareOcclusion")
				|| ClassName == TEXT("MaterialExpressionTextureSampleParameterMovie"))
			{
				this->LevelParser->LogContent += FString::Printf(TEXT("Importer does not support material node type %s on %s, you will have to fix it manually\n"), *ClassName, *path);
				//JumpToEnd();
				continue;
			}

			if (ClassName == TEXT("MaterialExpressionFlipBookSample"))
			{
				Class = UMaterialExpressionTextureSample::StaticClass();
			}
			else
			{
				Class = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ClassName, true);
			}

			if (Class)
			{
				ensure(GetOneValueAfter(TEXT(" Name="), Name));

				FRequirement TextureRequirement;
				auto prevLineIndex = LineIndex;
				UMaterialExpression* MaterialExpression = ImportMaterialExpression(Class, TextureRequirement);

				if(previousClassName == TEXT("MaterialExpressionLightVector"))
				{
					auto funcCall = Cast<UMaterialExpressionMaterialFunctionCall>(MaterialExpression);
					UMaterialFunctionInterface* funcInterface = LoadObject<UMaterialFunctionInterface>(NULL, _TEXT("/Game/LightVectorProxy.LightVectorProxy"));
					funcCall->SetMaterialFunction(funcInterface);
				}
				
				if (TextureRequirement.Type == "TextureCube" && ClassName == TEXT("MaterialExpressionTextureSample"))
				{
					previousClassName = ClassName;
					ClassName = TEXT("MaterialExpressionTextureSampleParameterCube");
					remapped = true;
					Class = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ClassName, true);
					LineIndex = prevLineIndex;
					MaterialExpression = ImportMaterialExpression(Class, TextureRequirement);
				}

				UMaterialExpressionComment * MaterialExpressionComment = Cast<UMaterialExpressionComment>(MaterialExpression);
				if (MaterialExpressionComment)
				{
					Material->EditorComments.Add(MaterialExpressionComment);
					MaterialExpressionComment->MaterialExpressionEditorX -= MaterialExpressionComment->SizeX;
				}
				else if (MaterialExpression)
				{
					Material->Expressions.Add(MaterialExpression);
				}

				if (MaterialExpression)
				{
					if (remapped)
						FixRequirement(FString::Printf(TEXT("%s'%s'"), *previousClassName, *Name), MaterialExpression);
					FixRequirement(FString::Printf(TEXT("%s'%s'"), *ClassName, *Name), MaterialExpression);
				}

				if (ClassName == TEXT("MaterialExpressionFlipBookSample"))
				{
					ImportMaterialExpressionFlipBookSample((UMaterialExpressionTextureSample *)MaterialExpression, TextureRequirement);
				}
			}
			else
			{
				OpenMsgDialog(FString::Printf(TEXT("Couldn't find material node class for '%s' %s"), *ClassName, *req.OriginalUrl));
				JumpToEnd();
			}
		}
		else if (GetProperty(TEXT("DiffuseColor="), Value))
		{
			ImportExpression(&Material->BaseColor);
		}
		else if (GetProperty(TEXT("SpecularColor="), Value))
		{
			ImportExpression(&Material->Specular);
		}
		else if (GetProperty(TEXT("SpecularPower="), Value))
		{
			// TODO
		}
		else if (GetProperty(TEXT("Normal="), Value))
		{
			ImportExpression(&Material->Normal);
		}
		else if (GetProperty(TEXT("EmissiveColor="), Value))
		{
			ImportExpression(&Material->EmissiveColor);
		}
		else if (GetProperty(TEXT("Opacity="), Value))
		{
			ImportExpression(&Material->Opacity);
		}
		else if (GetProperty(TEXT("OpacityMask="), Value))
		{
			ImportExpression(&Material->OpacityMask);
		}
		else if (IsProperty(Name, Value) && Name != "PreviewMesh"/*I don't need it and it requires additional work to be supported*/)
		{
			FProperty* Property = FindFProperty<FProperty>(UMaterial::StaticClass(), *Name);
			if (Property)
			{
				Property->ImportText(*Value, Property->ContainerPtrToValuePtr<uint8>(Material), 0, Material);
			}
		}
	}

	if (isDecal)
	{
		Material->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
		Material->BlendMode = EBlendMode::BLEND_Translucent;
		Material->DecalBlendMode = EDecalBlendMode::DBM_DBuffer_Color; // Modulate is not supported with DBuffer decals and Forward Rendering, might as well push everything to color
	}

	Material->Modify(); // Might force save changes to materials

	PrintMissingRequirements();

	return Material;
}

UMaterialExpression* T3DMaterialParser::ImportMaterialExpression(UClass * Class, FRequirement &TextureRequirement)
{
	if (!Class->IsChildOf(UMaterialExpression::StaticClass()))
		return NULL;
	
	UMaterialExpression* MaterialExpression = NewObject<UMaterialExpression>(Material, Class);

	FString Value, Name, PropertyName, Type, PackageName;
	while (NextLine() && IgnoreSubs() && !IsEndObject())
	{
		if (GetProperty(TEXT("Texture="), Value))
		{
			if (ParseRessourceUrl(Value, TextureRequirement))
			{
				LevelParser->AddRequirement(TextureRequirement, UObjectDelegate::CreateRaw(LevelParser, &T3DLevelParser::SetTexture, (UMaterialExpressionTextureBase*)MaterialExpression));
			}
			else
			{
				UE_LOG(UDKImportPluginLog, Warning, TEXT("Unable to parse ressource url : %s"), *Value);
			}
		}
		else if (IsProperty(PropertyName, Value) 
			&& PropertyName != TEXT("Material")
			&& PropertyName != TEXT("ExpressionGUID")
			&& PropertyName != TEXT("ObjectArchetype")
			&& PropertyName != TEXT("bIsParameterExpression"))
		{
			if(PropertyName == "ParameterName")
			{
				Value.RemoveFromStart("\"");
				Value.RemoveFromEnd("\"");
			}
			if (Class->GetName() == TEXT("MaterialExpressionDesaturation") && PropertyName == TEXT("Percent"))
			{
				PropertyName = TEXT("Fraction");
			}
			else if (Class == UMaterialExpressionConstant4Vector::StaticClass())
			{
				if (PropertyName == TEXT("A"))
					((UMaterialExpressionConstant4Vector*)MaterialExpression)->Constant.A = FCString::Atof(*Value);
				else if (PropertyName == TEXT("B"))
					((UMaterialExpressionConstant4Vector*)MaterialExpression)->Constant.B = FCString::Atof(*Value);
				else if (PropertyName == TEXT("G"))
					((UMaterialExpressionConstant4Vector*)MaterialExpression)->Constant.G = FCString::Atof(*Value);
				else if (PropertyName == TEXT("R"))
					((UMaterialExpressionConstant4Vector*)MaterialExpression)->Constant.R = FCString::Atof(*Value);
			}
			else if (Class == UMaterialExpressionConstant3Vector::StaticClass())
			{
				if (PropertyName == TEXT("B"))
					((UMaterialExpressionConstant3Vector*)MaterialExpression)->Constant.B = FCString::Atof(*Value);
				else if (PropertyName == TEXT("G"))
					((UMaterialExpressionConstant3Vector*)MaterialExpression)->Constant.G = FCString::Atof(*Value);
				else if (PropertyName == TEXT("R"))
					((UMaterialExpressionConstant3Vector*)MaterialExpression)->Constant.R = FCString::Atof(*Value);
			}
			if (PropertyName == "EditorX" || PropertyName == "EditorY")
			{
				int32 p = FCString::Atoi(*Value);
				if (PropertyName == "EditorX")
					MaterialExpression->MaterialExpressionEditorX = p;
				else
					MaterialExpression->MaterialExpressionEditorY = p;
				continue;
			}

			FProperty* Property = FindFProperty<FProperty>(Class, *PropertyName);
			FStructProperty * StructProperty = CastField<FStructProperty>(Property);
			if (StructProperty && StructProperty->Struct->GetName() == TEXT("ExpressionInput"))
			{
				FExpressionInput * ExpressionInput = Property->ContainerPtrToValuePtr<FExpressionInput>(MaterialExpression);
				ImportExpression(ExpressionInput);
			}
			else if (Property)
			{
				Property->ImportText(*Value, Property->ContainerPtrToValuePtr<uint8>(MaterialExpression), 0, MaterialExpression);
			}
		}
	}
	MaterialExpression->Material = Material;
	MaterialExpression->MaterialExpressionEditorX = -MaterialExpression->MaterialExpressionEditorX;
	// Most nodes are more compact in older UE versions, increase distance between each nodes here
	MaterialExpression->MaterialExpressionEditorX += MaterialExpression->MaterialExpressionEditorX/2;

	return MaterialExpression;
}

void T3DMaterialParser::ImportMaterialExpressionFlipBookSample(UMaterialExpressionTextureSample * Expression, FRequirement &TextureRequirement)
{
	UMaterialExpressionMaterialFunctionCall * MEFunction = NewObject<UMaterialExpressionMaterialFunctionCall>(Material, UMaterialExpressionMaterialFunctionCall::StaticClass());
	MEFunction->Material = Material;
	UMaterialExpressionConstant * MECRows = NewObject<UMaterialExpressionConstant>(Material, UMaterialExpressionConstant::StaticClass());
	MECRows->Material = Material;
	UMaterialExpressionConstant * MECCols = NewObject<UMaterialExpressionConstant>(Material, UMaterialExpressionConstant::StaticClass());
	MECCols->Material = Material;
	MEFunction->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY;
	MECRows->MaterialExpressionEditorY = MEFunction->MaterialExpressionEditorY;
	MECCols->MaterialExpressionEditorY = MECRows->MaterialExpressionEditorY + 64;

	MEFunction->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX - 304;
	MECRows->MaterialExpressionEditorX = MEFunction->MaterialExpressionEditorX - 80;
	MECCols->MaterialExpressionEditorX = MECRows->MaterialExpressionEditorX;

	MECRows->bCollapsed = true;
	MECCols->bCollapsed = true;

	MEFunction->SetMaterialFunction(LoadObject<UMaterialFunction>(NULL, TEXT("/Engine/Functions/Engine_MaterialFunctions02/Texturing/FlipBook.FlipBook")));

	MEFunction->FunctionInputs[1].Input.Expression = MECRows;
	MEFunction->FunctionInputs[2].Input.Expression = MECCols;

	if (Expression->Coordinates.Expression)
	{
		MEFunction->FunctionInputs[4].Input.Expression = Expression->Coordinates.Expression;
	}

	FString ExportFolder;
	FString FileName = TextureRequirement.Name + TEXT(".T3D");
	//LevelParser->ExportPackage(TextureRequirement.RelDirectory, T3DLevelParser::EExportType::Texture2DInfo, ExportFolder);
	if (FFileHelper::LoadFileToString(Line, *(ExportFolder / FileName)))
	{
		FString Value;
		if (GetOneValueAfter(TEXT("HorizontalImages="), Value))
		{
			MECCols->R = FCString::Atof(*Value);
		}
		if (GetOneValueAfter(TEXT("VerticalImages="), Value))
		{
			MECRows->R = FCString::Atof(*Value);
		}
	}

	Expression->Coordinates.OutputIndex = 2;
	Expression->Coordinates.Expression = MEFunction;

	Material->Expressions.Add(MECRows);
	Material->Expressions.Add(MECCols);
	Material->Expressions.Add(MEFunction);
}

void T3DMaterialParser::ImportExpression(FExpressionInput * ExpressionInput)
{
	FString Value;
	if (GetOneValueAfter(TEXT(",Mask="), Value))
		ExpressionInput->Mask = FCString::Atoi(*Value);
	if (GetOneValueAfter(TEXT(",MaskR="), Value))
		ExpressionInput->MaskR = FCString::Atoi(*Value);
	if (GetOneValueAfter(TEXT(",MaskG="), Value))
		ExpressionInput->MaskG = FCString::Atoi(*Value);
	if (GetOneValueAfter(TEXT(",MaskB="), Value))
		ExpressionInput->MaskB = FCString::Atoi(*Value);
	if (GetOneValueAfter(TEXT(",MaskA="), Value))
		ExpressionInput->MaskA = FCString::Atoi(*Value);
	if (GetOneValueAfter(TEXT("(Expression="), Value))
		AddRequirement(Value, UObjectDelegate::CreateRaw(this, &T3DMaterialParser::SetExpression, ExpressionInput));
}

void T3DMaterialParser::SetExpression(UObject * Object, FExpressionInput * ExpressionInput)
{
	UMaterialExpression * Expression = Cast<UMaterialExpression>(Object);
	ExpressionInput->Expression = Expression;
	UMaterialExpressionTextureBase* asTex = Cast<UMaterialExpressionTextureBase>(Object);
	if(asTex && ExpressionInput->MaskA >= 1 && asTex->Texture->CompressionSettings == TC_Normalmap)
	{
		// Normalmaps compression do not keep the alpha channel while this node requires it, 
		// change normal map settings and sampler type to support alpha
		asTex->Texture->Modify();
		asTex->Texture->CompressionSettings = TC_Default;
		asTex->Texture->PostEditChange();
		asTex->Modify();
		asTex->SamplerType = SAMPLERTYPE_LinearColor;
		asTex->PostEditChange();
	}
}
