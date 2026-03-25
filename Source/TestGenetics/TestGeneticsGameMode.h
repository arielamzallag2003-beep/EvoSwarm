// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TestGeneticsGameMode.generated.h"

UENUM(BlueprintType)
enum class EShape : uint8
{
				//nom dans l'éditeur
	None    UMETA(DisplayName = "Undefined"),
	Sphere,
	Cube,
	Cone
};

/**
 *  Simple Game Mode for a top-down perspective game
 *  Sets the default gameplay framework classes
 *  Check the Blueprint derived class for the set values
 */
UCLASS(abstract)
class ATestGeneticsGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	virtual void BeginPlay() override;

	void SpawnMonster();
};



