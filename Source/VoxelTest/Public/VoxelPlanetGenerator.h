// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "VoxelWorldGenerator.h"
#include "VoxelPlanetGenerator.generated.h"

/**
* Sphere World
*/
UCLASS(Blueprintable)
class VOXELTEST_API UVoxelPlanetGenerator : public UVoxelWorldGenerator
{
	GENERATED_BODY()

public:
	UVoxelPlanetGenerator();

	// Radius of the sphere in world space
	UPROPERTY(EditAnywhere)
		float Radius;

	// Material of the sphere
	UPROPERTY(EditAnywhere)
		FVoxelMaterial Material;

	// If true, sphere is a hole in a full world
	UPROPERTY(EditAnywhere)
		bool InverseOutsideInside;

	//~ Begin UVoxelWorldGenerator Interface
	TSharedRef<FVoxelWorldGeneratorInstance> GetWorldGenerator() override;
	//~ End UVoxelWorldGenerator Interface
};

class VOXELTEST_API FVoxelPlanetGeneratorInstance : public FVoxelWorldGeneratorInstance
{
public:
	FVoxelPlanetGeneratorInstance(float Radius, const FVoxelMaterial& Material, bool bInverseOutsideInside);

	//~ Begin FVoxelWorldGeneratorInstance Interface
	void GetValuesAndMaterialsAndVoxelTypes(float Values[], FVoxelMaterial Materials[], FVoxelType VoxelTypes[], const FIntVector& Start, const FIntVector& StartIndex, int Step, const FIntVector& Size, const FIntVector& ArraySize) const override;
	void SetVoxelWorld(const AVoxelWorld* VoxelWorld) override;
	FVector GetUpVector(int X, int Y, int Z) const override;
	//~ End FVoxelWorldGeneratorInstance Interface

private:
	const float Radius;
	const FVoxelMaterial Material;
	const bool bInverseOutsideInside;

private:
	float LocalRadius;
};