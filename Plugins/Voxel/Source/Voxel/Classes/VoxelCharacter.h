// Copyright 2018 Phyronnaz

#pragma once

#include "GameFramework/Character.h"
#include "VoxelInvokerComponent.h"
#include "VoxelCharacter.generated.h"

UCLASS(config=Game, BlueprintType)
class VOXEL_API AVoxelCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** Sets the component the Character is walking on, used by CharacterMovement walking movement to be able to follow dynamic objects. */
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor = true) override;
};