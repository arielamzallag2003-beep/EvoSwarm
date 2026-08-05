// Copyright Evoswarm.
//
// Per-world simulation services that must run OUTSIDE Mass processing: spawning boids
// and food, flushing queued births from the reproduction processor, and maintaining the
// food field. Also the registry mapping species index -> render ISM. Ticking here (not
// inside a processor) keeps all structural entity changes off the Mass execution path.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h" // Latest include order no longer pulls this in via MassEntityTypes.h
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmSimSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UAudioComponent;
class USoundWaveProcedural;
struct FMassEntityManager;

/** A queued offspring, produced by the reproduction processor, born on the next sim tick. */
struct FBoidBirthRequest
{
	FBoidSpeciesSharedFragment Shared;
	FBoidGenome Genome;
	FVector Position = FVector::ZeroVector;
	int32 Generation = 0;
};

/** Per-species live readout, refreshed each frame by the stats processor and shown on the HUD. */
struct FSpeciesLiveStats
{
	FString Name;
	FLinearColor Color = FLinearColor::White;
	int32 Count = 0;
	// Population-averaged genome values (let you watch evolution drift).
	float AvgHP = 0.f;
	float AvgWalkSpeed = 0.f;
	float AvgPerception = 0.f;
	float AvgDiet = 0.f;
	float AvgLifespan = 0.f;
	float AvgDamage = 0.f;
	float AvgArmor = 0.f;
	float AvgMutationRate = 0.f;
	float AvgGeneration = 0.f;
	int32 MaxGeneration = 0;
	/** Recent population samples (ring buffer) for the HUD sparkline; oldest first. */
	TArray<int32> PopHistory;
};

UCLASS()
class EVOSWARM_API UEvoswarmSimSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Setup, called by the game mode ---
	// Boids are rendered in diet-colour buckets (one instanced mesh per bucket), so an
	// individual's colour reflects its own genome rather than a fixed per-species colour.
	void RegisterBoidBucketISM(int32 Bucket, UInstancedStaticMeshComponent* ISM);
	UInstancedStaticMeshComponent* GetBoidBucketISM(int32 Bucket) const;
	int32 NumBoidBuckets() const { return BoidBucketISM.Num(); }

	int32 NumSpecies() const { return SpeciesStats.Num(); }

	void RegisterFoodISM(UInstancedStaticMeshComponent* ISM) { FoodISM = ISM; }
	UInstancedStaticMeshComponent* GetFoodISM() const { return FoodISM; }

	void RegisterCarcassISM(UInstancedStaticMeshComponent* ISM) { CarcassISM = ISM; }
	UInstancedStaticMeshComponent* GetCarcassISM() const { return CarcassISM; }

	// --- Live stats (for the HUD) ---
	void SetSpeciesInfo(int32 SpeciesIndex, const FString& Name, const FLinearColor& Color);
	const TArray<FSpeciesLiveStats>& GetSpeciesStats() const { return SpeciesStats; }
	TArray<FSpeciesLiveStats>& GetSpeciesStatsMutable() { return SpeciesStats; }
	int32 GetFoodCount() const { return FoodCount; }

	/** Current population of a species (from the stats processor; ~1 frame stale). */
	int32 GetSpeciesCount(int32 SpeciesIndex) const { return SpeciesStats.IsValidIndex(SpeciesIndex) ? SpeciesStats[SpeciesIndex].Count : 0; }

	/** Enable food regrowth + birth flushing. The game mode calls this once the world is built. */
	void StartSimulation() { bRunning = true; }

	// --- Debug visualisation (toggled by a key) ---
	void ToggleDebugDraw() { bDebugDraw = !bDebugDraw; }
	bool IsDebugDraw() const { return bDebugDraw; }
	/** Définit directement le mode de debug (0 à 4) */
	UFUNCTION(BlueprintCallable, Category = "Evoswarm|Debug")
	void SetDebugMode(int32 NewMode);

	/** Récupère le mode de debug actuel */
	UFUNCTION(BlueprintPure, Category = "Evoswarm|Debug")
	int32 GetDebugMode() const { return CurrentDebugMode; }

	// --- Spawning (safe: invoked outside Mass processing) ---
	FMassEntityHandle SpawnBoid(const FBoidSpeciesSharedFragment& Shared, const FBoidGenome& Genome, const FVector& Position, int32 Generation = 0);
	FMassEntityHandle SpawnFood(const FVector& Position, EFoodType Type = EFoodType::Plant, float Energy = -1.f);

	// --- Called from processors during the frame (counter / queue only, no structural change) ---
	void RequestBirth(const FBoidSpeciesSharedFragment& Shared, const FBoidGenome& Genome, const FVector& Position, int32 ChildGeneration);
	void RequestCarcass(const FVector& Position, float Energy);
	void NotifyFoodConsumed() { FoodCount = FMath::Max(0, FoodCount - 1); }

	float GetElapsedTime() const { return ElapsedTime; }

	// --- UTickableWorldSubsystem ---
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bRunning; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UEvoswarmSimSubsystem, STATGROUP_Tickables); }

private:
	FMassEntityManager* GetEntityManager() const;
	void FlushBirths();
	void FlushCarcasses();
	void RegrowFood();
	FVector RandomArenaPoint();

	// --- Reproduction feedback: a quick expanding ring + a soft, throttled blip ---
	void OnReproduction(const FVector& Position);
	void DrawBirthFlashes(float DeltaTime);
	void PlayBirthBlip();

	struct FBirthFlash { FVector Position = FVector::ZeroVector; float Age = 0.f; };
	TArray<FBirthFlash> BirthFlashes;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BirthAudio;
	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> BirthSound;
	float LastBirthSfxTime = -10.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BoidBucketISM;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> FoodISM;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> CarcassISM;

	TArray<FBoidBirthRequest> PendingBirths;
	TArray<TPair<FVector, float>> PendingCarcasses;
	TArray<FSpeciesLiveStats> SpeciesStats;
	void SampleHistory();

	FRandomStream Rng = FRandomStream(1337);
	int32 FoodCount = 0;
	bool bRunning = false;
	bool bDebugDraw = false;

	float ElapsedTime = 0.f;       // seconds since the sim started
	float HistoryTimer = 0.f;      // accumulator for population sampling
	
	/** Mode de debug actif (0: Off, 1: Comportements/FSM, 2: Espèces/Perceptions, 3: ..., 4: ...) */
	int32 CurrentDebugMode = 0;
};
