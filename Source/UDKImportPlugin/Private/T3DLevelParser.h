#pragma once

#include "T3DParser.h"

class T3DMaterialParser;
class T3DMaterialInstanceConstantParser;

class T3DLevelParser : public T3DParser
{
	friend class T3DMaterialParser;
	friend class T3DMaterialInstanceConstantParser;
public:
	T3DLevelParser(const FString &SourcePath, const FString &DestPath);
	void ImportLevel();
	void ImportStaticMesh();
	void ImportMaterial();
	void ImportMaterialInstanceConstant();
	FString LogContent = TEXT("");

private:
	// Export tools
	struct EExportType
	{
		enum Type
		{
			StaticMesh,
			Material,
			MaterialInstanceConstant,
			Texture2D,
			Texture2DInfo
		};
	};
	FString RessourceTypeFor(EExportType::Type Type);
	void ImportRessource(EExportType::Type Type);
	void ExportPackageToRequirements(EExportType::Type Type);

	/// Ressources requirements
	void ResolveRequirements();
	void ExportMaterialInstanceConstantAssets();
	void ExportMaterialAssets();
	void PostEditChangeFor(const FString &Type);

	/// Actor creation
	UWorld * World;
	template<class T>
	T * SpawnActor();

	/// Actor Importation
	void ImportLevelInternal();
	void ImportBrush();
	void ImportPolyList(UPolys * Polys);
	void ImportStaticMeshActor();
	void ImportPointLight();
	void ImportSpotLight();
	USoundCue * ImportSoundCue();

	/// Available ressource actions
	void SetStaticMesh(UObject * Object, UStaticMeshComponent * StaticMeshComponent);
	void SetPolygonTexture(UObject * Object, UPolys * Polys, int32 index);
	void SetSoundCueFirstNode(UObject * Object, USoundCue * SoundCue);
	void SetStaticMeshMaterial(UObject * Material, FString StaticMeshUrl, int32 MaterialIdx);
	void SetStaticMeshMaterialResolved(UObject * Object, UObject * Material, int32 MaterialIdx);
	void SetTexture(UObject * Object, UMaterialExpressionTextureBase * MaterialExpression);
	void SetParent(UObject * Object, UMaterialInstanceConstant * MaterialInstanceConstant);
	void SetTextureParameterValue(UObject * Object, UMaterialInstanceConstant * MaterialInstanceConstant, int32 ParameterIndex);
};
