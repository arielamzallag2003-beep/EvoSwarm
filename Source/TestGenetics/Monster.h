// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TestGeneticsGameMode.h"
#include "GameFramework/Actor.h"
#include "Monster.generated.h"

UCLASS()
class TESTGENETICS_API AMonster : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	EShape shape;
	float speed;
	float health;
	float size;
	FLinearColor color;
	float hunger;
	AMonster();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
