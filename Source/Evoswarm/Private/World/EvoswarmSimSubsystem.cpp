// Copyright Evoswarm.

#include "EvoswarmSimSubsystem.h"
#include "EvoswarmTuning.h"
#include "EvoswarmTerrain.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityBuilder.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "DrawDebugHelpers.h"

FMassEntityManager* UEvoswarmSimSubsystem::GetEntityManager() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UMassEntitySubsystem* Sys = World->GetSubsystem<UMassEntitySubsystem>())
		{
			return &Sys->GetMutableEntityManager();
		}
	}
	return nullptr;
}

void UEvoswarmSimSubsystem::RegisterBoidBucketISM(int32 Bucket, UInstancedStaticMeshComponent* ISM)
{
	if (Bucket < 0)
	{
		return;
	}
	if (Bucket >= BoidBucketISM.Num())
	{
		BoidBucketISM.SetNum(Bucket + 1);
	}
	BoidBucketISM[Bucket] = ISM;
}

UInstancedStaticMeshComponent* UEvoswarmSimSubsystem::GetBoidBucketISM(int32 Bucket) const
{
	return BoidBucketISM.IsValidIndex(Bucket) ? BoidBucketISM[Bucket] : nullptr;
}

void UEvoswarmSimSubsystem::SetSpeciesInfo(int32 SpeciesIndex, const FString& Name, const FLinearColor& Color)
{
	if (SpeciesIndex < 0)
	{
		return;
	}
	if (SpeciesIndex >= SpeciesStats.Num())
	{
		SpeciesStats.SetNum(SpeciesIndex + 1);
	}
	SpeciesStats[SpeciesIndex].Name = Name;
	SpeciesStats[SpeciesIndex].Color = Color;
	EnsureKillMatrixSize();
}

void UEvoswarmSimSubsystem::EnsureKillMatrixSize()
{
	// Square matrix, re-laid-out when the side length changes (only ever at setup, and species
	// count is tiny, so the naive copy is fine).
	const int32 N = SpeciesStats.Num();
	const int32 Wanted = N * N;
	if (KillMatrix.Num() == Wanted)
	{
		return;
	}
	const int32 OldN = FMath::FloorToInt(FMath::Sqrt(static_cast<float>(KillMatrix.Num())));
	TArray<int32> Rebuilt;
	Rebuilt.SetNumZeroed(Wanted);
	for (int32 K = 0; K < OldN && K < N; ++K)
	{
		for (int32 V = 0; V < OldN && V < N; ++V)
		{
			Rebuilt[K * N + V] = KillMatrix[K * OldN + V];
		}
	}
	KillMatrix = MoveTemp(Rebuilt);
}

int32 UEvoswarmSimSubsystem::GetKillCount(int32 Killer, int32 Victim) const
{
	const int32 N = SpeciesStats.Num();
	const int32 Index = Killer * N + Victim;
	return KillMatrix.IsValidIndex(Index) ? KillMatrix[Index] : 0;
}

void UEvoswarmSimSubsystem::NotifyPlantEaten(int32 SpeciesIndex, float EnergyGained)
{
	FoodCount = FMath::Max(0, FoodCount - 1);
	if (SpeciesStats.IsValidIndex(SpeciesIndex))
	{
		FTrophicLedger& L = SpeciesStats[SpeciesIndex].Trophic;
		L.PlantEnergy += EnergyGained;
		++L.PlantsEaten;
	}
}

void UEvoswarmSimSubsystem::NotifyMeatEaten(int32 SpeciesIndex, float EnergyGained)
{
	if (SpeciesStats.IsValidIndex(SpeciesIndex))
	{
		SpeciesStats[SpeciesIndex].Trophic.MeatEnergy += EnergyGained;
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogEvoswarmSim, Log, All);

FMassEntityHandle UEvoswarmSimSubsystem::SpawnBoid(const FBoidSpeciesSharedFragment& Shared, const FBoidGenome& Genome, const FVector& Position, int32 Generation)
{
	FMassEntityManager* EM = GetEntityManager();
	if (!EM)
	{
		return FMassEntityHandle();
	}

	// The builder defines composition (tags, shared value, which fragments exist). Per-entity
	// fragment VALUES are written straight into chunk memory below, because Mass copies builder
	// values via CopyScriptStruct (reflection only) which would drop our non-UPROPERTY data.
	UE::Mass::FEntityBuilder Builder = EM->MakeEntityBuilder();
	Builder.Add<FBoidTag>();
	Builder.Add<FBoidSpeciesSharedFragment>(Shared); // shared, deduped by value -> one archetype per species
	Builder.Add<FTransformFragment>();
	Builder.Add<FMassVelocityFragment>();
	Builder.Add<FMassForceFragment>();
	Builder.Add<FBoidGenomeFragment>();
	Builder.Add<FBoidStateFragment>();

	const FMassEntityHandle Handle = Builder.Commit();
	if (!Handle.IsValid())
	{
		return Handle;
	}

	const FVector P(Position.X, Position.Y, Evo::SurfaceZ(Position.X, Position.Y) + Evo::GroundOffset);
	EM->GetFragmentDataChecked<FTransformFragment>(Handle).GetMutableTransform().SetLocation(P);
	EM->GetFragmentDataChecked<FBoidGenomeFragment>(Handle).Genome = Genome;

	// Kick off with a small random cruise velocity so the swarm looks alive on frame 1.
	const FVector InitDir = FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	EM->GetFragmentDataChecked<FMassVelocityFragment>(Handle).Value = InitDir * Evo::WalkSpeed(Genome);

	FBoidStateFragment& State = EM->GetFragmentDataChecked<FBoidStateFragment>(Handle);
	State.CurrentHP = Evo::MaxHP(Genome);
	State.CurrentStamina = Evo::MaxStamina(Genome);
	State.CurrentHunger = Evo::MaxHunger(Genome) * 0.5f; // start half-hungry so they forage right away
	State.Age = 0.f;
	State.ReproCooldown = Evo::ReproCooldown(Genome) * Rng.FRandRange(0.5f, 1.5f);
	State.Generation = Generation;

	return Handle;
}

FMassEntityHandle UEvoswarmSimSubsystem::SpawnFood(const FVector& Position, EFoodType Type, float Energy)
{
	FMassEntityManager* EM = GetEntityManager();
	if (!EM)
	{
		return FMassEntityHandle();
	}

	UE::Mass::FEntityBuilder Builder = EM->MakeEntityBuilder();
	Builder.Add<FFoodTag>();
	Builder.Add<FTransformFragment>();
	Builder.Add<FFoodFragment>();

	const FMassEntityHandle Handle = Builder.Commit();
	if (Handle.IsValid())
	{
		// Sit on the surface (carcasses a touch higher so they read above the ground).
		const float ZOffset = (Type == EFoodType::Carcass) ? 25.f : 10.f;
		const FVector P(Position.X, Position.Y, Evo::TerrainHeight(Position.X, Position.Y) + ZOffset);
		EM->GetFragmentDataChecked<FTransformFragment>(Handle).GetMutableTransform().SetLocation(P);

		FFoodFragment& F = EM->GetFragmentDataChecked<FFoodFragment>(Handle);
		F.Type = Type;
		F.Energy = (Energy >= 0.f) ? Energy : Evo::FoodEnergy;

		if (Type == EFoodType::Plant)
		{
			++FoodCount; // only the maintained plant field is counted/targeted
		}
	}
	return Handle;
}

void UEvoswarmSimSubsystem::RequestBirth(const FBoidSpeciesSharedFragment& Shared, const FBoidGenome& Genome, const FVector& Position, int32 ChildGeneration)
{
	PendingBirths.Add(FBoidBirthRequest{ Shared, Genome, Position, ChildGeneration });
}

void UEvoswarmSimSubsystem::RequestCarcass(const FVector& Position, float Energy)
{
	PendingCarcasses.Add(TPair<FVector, float>(Position, Energy));
}

FVector UEvoswarmSimSubsystem::RandomArenaPoint()
{
	return FVector(
		Rng.FRandRange(-Evo::ArenaHalfExtent, Evo::ArenaHalfExtent),
		Rng.FRandRange(-Evo::ArenaHalfExtent, Evo::ArenaHalfExtent),
		0.f);
}

void UEvoswarmSimSubsystem::FlushBirths()
{
	for (const FBoidBirthRequest& Req : PendingBirths)
	{
		if (SpawnBoid(Req.Shared, Req.Genome, Req.Position, Req.Generation).IsValid())
		{
			// Count only real spawns, attributed to the child's species.
			if (SpeciesStats.IsValidIndex(Req.Shared.SpeciesIndex))
			{
				++SpeciesStats[Req.Shared.SpeciesIndex].TotalBirths;
			}
			OnReproduction(Req.Position); // flash + soft blip where the pair bred
		}
	}
	PendingBirths.Reset();
}

void UEvoswarmSimSubsystem::FlushCarcasses()
{
	for (const TPair<FVector, float>& Req : PendingCarcasses)
	{
		SpawnFood(Req.Key, EFoodType::Carcass, Req.Value);
	}
	PendingCarcasses.Reset();
}

void UEvoswarmSimSubsystem::RegrowFood()
{
	int32 Attempts = 0;
	int32 Spawned = 0;
	// Biome-weighted: plants concentrate where FoodMultiplier is high (grassland/forest).
	while (FoodCount < Evo::FoodTargetCount && Spawned < Evo::FoodSpawnPerTick && Attempts < Evo::FoodSpawnPerTick * 6)
	{
		++Attempts;
		const FVector P = RandomArenaPoint();
		if (Evo::TerrainHeight(P.X, P.Y) < Evo::SeaLevel + Evo::BeachBand)
		{
			continue; // no plants in the water (would lure herbivores in to drown)
		}
		const FBiomeParams Biome = Evo::GetBiomeParams(Evo::BiomeAt(P.X, P.Y));
		const float Accept = FMath::Clamp(Biome.FoodMultiplier / 1.7f, 0.f, 1.f);
		if (Rng.FRand() > Accept)
		{
			continue; // rejected: sparse biome
		}
		SpawnFood(P, EFoodType::Plant);
		++Spawned;
	}
}

void UEvoswarmSimSubsystem::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	FlushBirths();
	FlushCarcasses();
	RegrowFood();
	DrawBirthFlashes(DeltaTime);

	ElapsedTime += DeltaTime;
	HistoryTimer += DeltaTime;
	if (HistoryTimer >= Evo::StatsSampleInterval)
	{
		HistoryTimer = 0.f;
		SampleHistory();
		DetectEvents();
	}
}

void UEvoswarmSimSubsystem::SampleHistory()
{
	auto Push = [](TArray<int32>& History, int32 Value)
		{
			History.Add(Value);
			if (History.Num() > Evo::StatsHistorySamples)
			{
				History.RemoveAt(0);
			}
		};
	auto PushF = [](TArray<float>& History, float Value)
		{
			History.Add(Value);
			if (History.Num() > Evo::StatsHistorySamples)
			{
				History.RemoveAt(0);
			}
		};

	for (FSpeciesLiveStats& S : SpeciesStats)
	{
		Push(S.PopHistory, S.Count);

		// Births/deaths in this interval = cumulative counters minus the previous sample.
		const int32 DeathsNow = S.TotalDeaths();
		Push(S.BirthHistory, S.TotalBirths - S.LastSampledBirths);
		Push(S.DeathHistory, DeathsNow - S.LastSampledDeaths);
		S.LastSampledBirths = S.TotalBirths;
		S.LastSampledDeaths = DeathsNow;

		// Energy throughput this interval (food-web arrow widths).
		FTrophicLedger& L = S.Trophic;
		PushF(L.PlantEnergyHistory, L.PlantEnergy - L.LastSampledPlantEnergy);
		PushF(L.MeatEnergyHistory, L.MeatEnergy - L.LastSampledMeatEnergy);
		L.LastSampledPlantEnergy = L.PlantEnergy;
		L.LastSampledMeatEnergy = L.MeatEnergy;

		// One point on the whole-run record. TraitNow was filled by the stats processor
		// earlier this frame, so this is a straight copy - no second pass over the entities.
		FSpeciesTimeSample Sample;
		Sample.Time = ElapsedTime;
		Sample.Count = static_cast<float>(S.Count);
		Sample.BirthRate = Evo::RatePerMinute(S.BirthHistory);
		Sample.DeathRate = Evo::RatePerMinute(S.DeathHistory);
		Sample.AvgGeneration = S.AvgGeneration;
		for (int32 I = 0; I < NumBoidStats; ++I)
		{
			Sample.Traits[I] = S.TraitNow[I];
		}
		S.Timeline.Push(Sample);
	}
}

void UEvoswarmSimSubsystem::CycleAnalyticsPage(int32 Direction)
{
	const int32 Next = (static_cast<int32>(AnalyticsPage) + Direction + NumAnalyticsPages) % NumAnalyticsPages;
	AnalyticsPage = static_cast<EAnalyticsPage>(Next);
}

void UEvoswarmSimSubsystem::CycleAnalyticsSelection(int32 Direction)
{
	// [ and ] mean "previous / next" in whatever the visible page is selecting.
	if (AnalyticsPage == EAnalyticsPage::Scatter)
	{
		AnalyticsScatterIndex = (AnalyticsScatterIndex + Direction + Evo::NumScatterPresets) % Evo::NumScatterPresets;
	}
	else
	{
		AnalyticsTraitIndex = (AnalyticsTraitIndex + Direction + NumBoidStats) % NumBoidStats;
	}
}

void UEvoswarmSimSubsystem::ExportAnalyticsCsv()
{
	TArray<Evo::FCsvSpeciesInput> Inputs;
	Inputs.Reserve(SpeciesStats.Num());
	for (const FSpeciesLiveStats& S : SpeciesStats)
	{
		Evo::FCsvSpeciesInput& In = Inputs.AddDefaulted_GetRef();
		In.Name = S.Name;
		In.Timeline = &S.Timeline;
		In.Trophic = &S.Trophic;
	}

	FString Error;
	const FString Dir = Evo::ExportRunToCsv(Inputs, KillMatrix, Error);
	if (Dir.IsEmpty())
	{
		LogEvent(FString::Printf(TEXT("CSV export failed: %s"), *Error), FLinearColor(0.9f, 0.4f, 0.4f));
	}
	else
	{
		LogEvent(FString::Printf(TEXT("CSV exported to %s"), *Dir), FLinearColor(0.5f, 0.85f, 0.95f));
	}
}

void UEvoswarmSimSubsystem::NotifyDeath(int32 SpeciesIndex, EDeathCause Cause)
{
	if (SpeciesStats.IsValidIndex(SpeciesIndex) && Cause < EDeathCause::Count)
	{
		++SpeciesStats[SpeciesIndex].Deaths[static_cast<int32>(Cause)];
	}
}

void UEvoswarmSimSubsystem::NotifyKill(int32 KillerSpeciesIndex, int32 VictimSpeciesIndex)
{
	if (SpeciesStats.IsValidIndex(KillerSpeciesIndex))
	{
		++SpeciesStats[KillerSpeciesIndex].TotalKillsMade;
	}
	if (SpeciesStats.IsValidIndex(VictimSpeciesIndex))
	{
		// Every kill drops a carcass, so this doubles as the carcass source count.
		++SpeciesStats[VictimSpeciesIndex].Trophic.CarcassesDropped;
	}
	const int32 Index = KillerSpeciesIndex * SpeciesStats.Num() + VictimSpeciesIndex;
	if (KillMatrix.IsValidIndex(Index))
	{
		++KillMatrix[Index];
	}
	NotifyDeath(VictimSpeciesIndex, EDeathCause::Predation);
}

void UEvoswarmSimSubsystem::LogEvent(const FString& Text, const FLinearColor& Color)
{
	EventLog.Add(FSimEvent{ ElapsedTime, Text, Color });
	if (EventLog.Num() > Evo::EventLogMaxEntries)
	{
		EventLog.RemoveAt(0);
	}
	const int32 Mins = FMath::FloorToInt(ElapsedTime / 60.f);
	const int32 Secs = FMath::FloorToInt(ElapsedTime) % 60;
	UE_LOG(LogEvoswarmSim, Log, TEXT("[%d:%02d] %s"), Mins, Secs, *Text);
}

void UEvoswarmSimSubsystem::DetectEvents()
{
	// Runs every stats sample (0.5 s): pure integer/float comparisons over per-species data.
	for (FSpeciesLiveStats& S : SpeciesStats)
	{
		const bool bAlive = S.Count > 0;

		// --- Extinction: had a living population, now zero ---
		if (S.bWasAlive && !bAlive)
		{
			S.bWasAlive = false;
			LogEvent(FString::Printf(TEXT("%s went EXTINCT (peak gen %d, %d born, %d starved, %d eaten)"),
				*S.Name, S.MaxGeneration, S.TotalBirths,
				S.DeathCount(EDeathCause::Starvation), S.DeathCount(EDeathCause::Predation)),
				FLinearColor(0.95f, 0.35f, 0.35f));
		}
		else if (bAlive)
		{
			S.bWasAlive = true;
		}

		if (!bAlive)
		{
			continue; // the remaining events only make sense for living species
		}

		// --- Generation milestone: every GenMilestoneStep generations ---
		if (S.MaxGeneration >= S.LastMilestoneGen + Evo::GenMilestoneStep)
		{
			S.LastMilestoneGen = (S.MaxGeneration / Evo::GenMilestoneStep) * Evo::GenMilestoneStep;
			LogEvent(FString::Printf(TEXT("%s reached generation %d"), *S.Name, S.LastMilestoneGen),
				FLinearColor(0.40f, 0.70f, 1.0f));
		}

		// --- Population collapse: pop fell below CollapseFraction of what it was WindowSec ago ---
		const int32 WindowSamples = FMath::RoundToInt(Evo::CollapseWindowSec / Evo::StatsSampleInterval);
		if (S.PopHistory.Num() > WindowSamples)
		{
			const int32 Then = S.PopHistory[S.PopHistory.Num() - 1 - WindowSamples];
			if (Then >= Evo::CollapseMinPop
				&& S.Count < static_cast<int32>(Then * Evo::CollapseFraction)
				&& ElapsedTime - S.LastCollapseLogTime > Evo::CollapseCooldownSec)
			{
				S.LastCollapseLogTime = ElapsedTime;
				LogEvent(FString::Printf(TEXT("%s population collapsing: %d -> %d in %.0f s"),
					*S.Name, Then, S.Count, Evo::CollapseWindowSec),
					FLinearColor(0.95f, 0.62f, 0.25f));
			}
		}

		// --- Diet class shift (with hysteresis so a border species doesn't flip-flop) ---
		int32 NewClass = S.DietClass;
		if (S.AvgDiet > Evo::DietCarnThreshold + Evo::DietClassHysteresis)
		{
			NewClass = 2;
		}
		else if (S.AvgDiet < Evo::DietHerbThreshold - Evo::DietClassHysteresis)
		{
			NewClass = 0;
		}
		else if (S.AvgDiet > Evo::DietHerbThreshold + Evo::DietClassHysteresis
			&& S.AvgDiet < Evo::DietCarnThreshold - Evo::DietClassHysteresis)
		{
			NewClass = 1;
		}

		if (!S.bDietClassInit)
		{
			// First classification is silent: the starting diet isn't an "event".
			S.bDietClassInit = true;
			S.DietClass = (S.AvgDiet > Evo::DietCarnThreshold) ? 2 : (S.AvgDiet < Evo::DietHerbThreshold) ? 0 : 1;
		}
		else if (NewClass != S.DietClass)
		{
			S.DietClass = NewClass;
			const TCHAR* ClassWord = (NewClass == 0) ? TEXT("herbivorous") : (NewClass == 2) ? TEXT("carnivorous") : TEXT("omnivorous");
			LogEvent(FString::Printf(TEXT("%s turned %s (avg diet %.2f)"), *S.Name, ClassWord, S.AvgDiet),
				Evo::DietColor(S.AvgDiet));
		}
	}
}

void UEvoswarmSimSubsystem::OnReproduction(const FVector& Position)
{
	BirthFlashes.Add(FBirthFlash{ Position, 0.f });
	PlayBirthBlip();
}

void UEvoswarmSimSubsystem::DrawBirthFlashes(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		BirthFlashes.Reset();
		return;
	}

	for (int32 I = BirthFlashes.Num() - 1; I >= 0; --I)
	{
		FBirthFlash& F = BirthFlashes[I];
		F.Age += DeltaTime;
		const float T = F.Age / Evo::BirthFlashDuration;
		if (T >= 1.f)
		{
			BirthFlashes.RemoveAtSwap(I);
			continue;
		}

		// Expanding ring that fades out: a bright, friendly spawn pop.
		const FVector Centre = F.Position + FVector(0, 0, Evo::DebugZLift);
		const float Radius = FMath::Lerp(15.f, Evo::BirthFlashRadius, T);
		const float Fade = 1.f - T;
		const FColor Ring(
			static_cast<uint8>(255 * Fade),
			static_cast<uint8>(170 * Fade + 40 * (1.f - Fade)),
			static_cast<uint8>(230 * Fade), 255);
		DrawDebugCircle(World, Centre, Radius, 24, Ring, false, -1.f, 0, 3.f * Fade + 0.5f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);
		// A small upward spark for a touch of life.
		DrawDebugLine(World, Centre, Centre + FVector(0, 0, 60.f * Fade), Ring, false, -1.f, 0, 2.f);
	}
}

void UEvoswarmSimSubsystem::PlayBirthBlip()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Throttle so a baby boom doesn't turn into a machine-gun of blips.
	if (ElapsedTime - LastBirthSfxTime < Evo::BirthSfxMinInterval)
	{
		return;
	}
	LastBirthSfxTime = ElapsedTime;

	// Lazily build a procedural sound + a 2D audio channel — no imported asset needed.
	if (!BirthSound)
	{
		BirthSound = NewObject<USoundWaveProcedural>(this);
		BirthSound->SetSampleRate(44100);
		BirthSound->NumChannels = 1;
		BirthSound->Duration = INDEFINITELY_LOOPING_DURATION; // a persistent stream we feed on demand
		BirthSound->bLooping = false;
	}
	if (!BirthAudio)
	{
		BirthAudio = NewObject<UAudioComponent>(this);
		BirthAudio->bAutoActivate = false;
		BirthAudio->bAllowSpatialization = false;
		BirthAudio->bIsUISound = true;
		BirthAudio->SetVolumeMultiplier(Evo::BirthSfxVolume);
		BirthAudio->SetSound(BirthSound);
		BirthAudio->RegisterComponentWithWorld(World);
		BirthAudio->Play();
	}

	// Synthesize a soft, warm "bloop": low sine, smooth fade-in (no click), gentle decay,
	// with a touch of a fifth above for a mellow bell rather than a beep.
	constexpr int32 SampleRate = 44100;
	constexpr float DurationSec = 0.20f;
	constexpr float AttackSec = 0.015f;                 // smooth onset kills the click
	const int32 NumSamples = static_cast<int32>(SampleRate * DurationSec);
	TArray<int16> Pcm;
	Pcm.SetNumUninitialized(NumSamples);
	for (int32 S = 0; S < NumSamples; ++S)
	{
		const float Time = static_cast<float>(S) / SampleRate;
		const float Attack = FMath::Clamp(Time / AttackSec, 0.f, 1.f);   // fade in
		const float Decay = FMath::Exp(-Time * 11.f);                    // soft tail
		const float Env = Attack * Decay;
		const float Freq = 392.f - 22.f * (Time / DurationSec);          // warm, drifting gently down
		const float Tone = FMath::Sin(2.f * PI * Freq * Time)
			+ 0.3f * FMath::Sin(2.f * PI * Freq * 1.5f * Time);          // quiet harmonic for warmth
		const float Sample = Tone * Env * 0.4f;
		Pcm[S] = static_cast<int16>(FMath::Clamp(Sample, -1.f, 1.f) * 32767.f);
	}

	BirthSound->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
}

void UEvoswarmSimSubsystem::SetDebugMode(int32 NewMode)
{
	// On s'assure que le mode reste bien compris entre 0 et 4.
	CurrentDebugMode = FMath::Clamp(NewMode, 0, 4);

	UE_LOG(LogTemp, Log, TEXT("Evoswarm debug mode set to %d"), CurrentDebugMode);
}