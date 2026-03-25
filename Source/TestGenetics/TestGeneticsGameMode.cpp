// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestGeneticsGameMode.h"
#include "Monster.h"

class AMyGameMode : public AGameModeBase
{
	
};

void ATestGeneticsGameMode::BeginPlay()
{
	Super::BeginPlay();

	SpawnMonster();
}

void ATestGeneticsGameMode::SpawnMonster()
{
	// GetWorld()->SpawnActor<AMonster>(
	// 	AMonster,
	// 	Location,
	// 	FRotator::ZeroRotator
	// );
	printf("test");

}