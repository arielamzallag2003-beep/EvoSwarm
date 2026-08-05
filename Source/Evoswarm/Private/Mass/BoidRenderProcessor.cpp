// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmSimSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace
{
	// Push transforms to an ISM. Fast path (no buffer realloc) when the instance count is
	// unchanged since last frame — only fully rebuilds when boids were born/died/eaten.
	void SyncInstances(UInstancedStaticMeshComponent* ISM, const TArray<FTransform>& Transforms)
	{
		if (!ISM)
		{
			return;
		}
		if (Transforms.Num() == 0 && ISM->GetInstanceCount() == 0)
		{
			return; // nothing to do (common for sparsely-populated appearance buckets)
		}
		if (Transforms.Num() > 0 && ISM->GetInstanceCount() == Transforms.Num())
		{
			ISM->BatchUpdateInstancesTransforms(0, Transforms, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
		}
		else
		{
			ISM->ClearInstances();
			if (Transforms.Num() > 0)
			{
				ISM->AddInstances(Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
			}
		}
	}
}

UBoidRenderProcessor::UBoidRenderProcessor()
	: EntityQuery(*this)
	, FoodQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // touches scene components
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidMovementProcessor"));
}

void UBoidRenderProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);

	FoodQuery.AddTagRequirement<FFoodTag>(EMassFragmentPresence::All);
	FoodQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FoodQuery.AddRequirement<FFoodFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidRenderProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}

	// --- Boids: binned by diet (colour) with per-individual size (from the genome) ---
	const int32 NumBuckets = Sim->NumBoidBuckets();
	PerBucketTransforms.SetNum(NumBuckets);
	for (TArray<FTransform>& Arr : PerBucketTransforms)
	{
		Arr.Reset();
	}

	EntityQuery.ForEachEntityChunk(Context, [this, NumBuckets](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidGenome& G = Gen[It].Genome;
			const int32 Bucket = Evo::AppearanceBucket(G);
			if (Bucket < 0 || Bucket >= NumBuckets)
			{
				continue;
			}
			FTransform T = Xf[It].GetTransform();
			T.SetScale3D(Evo::BodyScale(G)); // bulk from HP, length from speed, width from armour
			PerBucketTransforms[Bucket].Add(T);
		}
	});

	for (int32 Bucket = 0; Bucket < NumBuckets; ++Bucket)
	{
		SyncInstances(Sim->GetBoidBucketISM(Bucket), PerBucketTransforms[Bucket]);
	}

	// --- Food: plants and carcasses into their own ISMs ---
	FoodTransforms.Reset();
	CarcassTransforms.Reset();
	FoodQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FFoodFragment> Food = Context.GetFragmentView<FFoodFragment>();
		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			if (Food[It].Type == EFoodType::Carcass)
			{
				FTransform T = Xf[It].GetTransform();
				T.SetRotation(Evo::MeshStandUp()); // imported meshes are Y-up
				T.SetScale3D(FVector(Evo::CarcassMeshScale));
				CarcassTransforms.Add(T);
			}
			else
			{
				FTransform T = Xf[It].GetTransform();
				T.SetRotation(Evo::MeshStandUp());
				T.SetScale3D(FVector(Evo::FoodMeshScale));
				FoodTransforms.Add(T);
			}
		}
	});

	SyncInstances(Sim->GetFoodISM(), FoodTransforms);
	SyncInstances(Sim->GetCarcassISM(), CarcassTransforms);
}
