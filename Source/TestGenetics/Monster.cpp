// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster.h"

// Sets default values
AMonster::AMonster()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMonster::BeginPlay()
{
	Super::BeginPlay();
	shape = static_cast<EShape>(FMath::RandRange(0, 3));
	speed = FMath::FRandRange(0.5f, 2.0f);
	health = FMath::FRandRange(1.0f, 10.0f);
	size = FMath::FRandRange(0.5f, 2.0f);
	color = FLinearColor(
		FMath::FRand(),
		FMath::FRand(),
		FMath::FRand(),
		1.0f
		);
	hunger = FMath::FRandRange(0.0f, 1.0f);
}

// Called every frame
void AMonster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

