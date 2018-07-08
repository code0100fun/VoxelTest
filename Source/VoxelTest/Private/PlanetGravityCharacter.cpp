// Fill out your copyright notice in the Description page of Project Settings.

#include "PlanetGravityMovementComponent.h"
#include "PlanetGravityCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

APlanetGravityCharacter::APlanetGravityCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPlanetGravityMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}