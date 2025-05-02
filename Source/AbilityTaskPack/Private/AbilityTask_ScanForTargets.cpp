// Copyright (c) Jared Taylor


#include "AbilityTask_ScanForTargets.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "TargetingSystem/TargetingPreset.h"
#include "TargetingSystem/TargetingSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_ScanForTargets)


UAbilityTask_ScanForTargets::UAbilityTask_ScanForTargets(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	Priority = 1;

	Preset = nullptr;
	SourceActor = nullptr;
	Origin = FVector::ZeroVector;
	MaxRate = 0.05f;
	DurationType = EScanOverlapDuration::Ability;
	Duration = 0.f;
	bAsync = false;
	
	bIsFinished = false;
	bAwaitingCallback = false;
	ElapsedTime = 0.f;
	TimeSinceLastScan = 0.f;
}

UAbilityTask_ScanForTargets* UAbilityTask_ScanForTargets::ScanForTargets(UGameplayAbility* OwningAbility,
	const UTargetingPreset* TargetingPreset, const TObjectPtr<AActor>& InSourceActor, FVector WorldOrigin,
	float MaxScanRate, EScanOverlapDuration InDurationType, float InMaxDuration, bool bUpdateAsync)
{
	auto* MyTask = NewAbilityTask<UAbilityTask_ScanForTargets>(OwningAbility);
	MyTask->Preset = TargetingPreset;
	MyTask->SourceActor = InSourceActor;
	MyTask->Origin = WorldOrigin;
	MyTask->MaxRate = MaxScanRate;
	MyTask->Duration = InMaxDuration;
	MyTask->DurationType = InDurationType;
	MyTask->bAsync = bUpdateAsync;

	// Bind to OnFinished then call SharedInitAndApply() after creating this task
	// Otherwise the task will not run
	
	return MyTask;
}

UAbilityTask_ScanForTargets* UAbilityTask_ScanForTargets::K2_ScanForTargets(UGameplayAbility* OwningAbility,
	const UTargetingPreset* TargetingPreset, AActor* InSourceActor, FVector WorldOrigin, float MaxScanRate,
	EScanOverlapDuration InDurationType, float MaxDuration, bool bUpdateAsync)
{
	UAbilityTask_ScanForTargets* MyTask = ScanForTargets(OwningAbility, TargetingPreset, InSourceActor,
		 WorldOrigin, MaxScanRate, InDurationType, MaxDuration, bUpdateAsync);

	MyTask->SharedInitAndApply();
	return MyTask;
}

void UAbilityTask_ScanForTargets::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);
	SharedInitAndApply();
}

void UAbilityTask_ScanForTargets::SharedInitAndApply()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAbilityTask_ScanForTargets::SharedInitAndApply);
	
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && ASC->AbilityActorInfo->MovementComponent.IsValid() && Preset)
	{
		ElapsedTime = 0.f;
		TimeSinceLastScan = 0.f;

		const UTargetingSubsystem* TargetSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UTargetingSubsystem>();
		if (!TargetSubsystem)
		{
			ABILITY_LOG(Warning, TEXT("UHonorAbilityTask_ScanForTargets called in Ability %s with null TargetingSubsystem; Task Instance Name %s."), 
				Ability ? *Ability->GetName() : TEXT("NULL"), 
				*InstanceName.ToString());
			EndTask();
			return;
		}

		if (!Preset)
		{
			ABILITY_LOG(Warning, TEXT("UHonorAbilityTask_ScanForTargets called in Ability %s with null TargetingPreset; Task Instance Name %s."), 
				Ability ? *Ability->GetName() : TEXT("NULL"), 
				*InstanceName.ToString());
			EndTask();
			return;
		}

		if (!Preset->GetTargetingTaskSet())
		{
			ABILITY_LOG(Warning, TEXT("UHonorAbilityTask_ScanForTargets called in Ability %s with null TargetingTaskSet; Task Instance Name %s."), 
				Ability ? *Ability->GetName() : TEXT("NULL"), 
				*InstanceName.ToString());
			EndTask();
			return;
		}

		if (Preset->GetTargetingTaskSet()->Tasks.IsEmpty())
		{
			ABILITY_LOG(Warning, TEXT("UHonorAbilityTask_ScanForTargets called in Ability %s with empty TargetingTaskSet; Task Instance Name %s."), 
				Ability ? *Ability->GetName() : TEXT("NULL"), 
				*InstanceName.ToString());
			EndTask();
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UHonorAbilityTask_FaceTowards called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
		
		EndTask();
	}
}

void UAbilityTask_ScanForTargets::OnScanTaskReachedEnd(AActor* Avatar)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAbilityTask_ScanForTargets::OnScanTaskReachedEnd);
	
	bIsFinished = true;
	if (!bIsSimulating)
	{
		Avatar->ForceNetUpdate();
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast();
		}
		EndTask();
	}
}

void UAbilityTask_ScanForTargets::TickTask(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAbilityTask_ScanForTargets::TickTask);
	
	if (bIsFinished)
	{
		return;
	}

	AActor* Avatar = GetAvatarActor();
	if (!Avatar || !SourceActor.IsValid())
	{
		bIsFinished = true;
		EndTask();
		return;
	}

	ElapsedTime += DeltaTime;

	// Already waiting for a callback
	if (bAwaitingCallback)
	{
		return;
	}

	// Rate throttling
	if (MaxRate > 0.f)
	{
		TimeSinceLastScan += DeltaTime;
		if (TimeSinceLastScan < MaxRate)
		{
			return;
		}
	}

	// End if duration has elapsed
	if (DurationType == EScanOverlapDuration::Duration && ElapsedTime >= Duration)
	{
		OnScanTaskReachedEnd(Avatar);
		return;
	}

	UTargetingSubsystem* TargetSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UTargetingSubsystem>();
	if (!TargetSubsystem)
	{
		OnScanTaskReachedEnd(Avatar);
		return;
	}

	if (!Preset || !Preset->GetTargetingTaskSet() || Preset->GetTargetingTaskSet()->Tasks.IsEmpty())
	{
		OnScanTaskReachedEnd(Avatar);
		return;
	}

	// Scan for targets
	const FTargetingRequestHandle Handle = TargetSubsystem->MakeTargetRequestHandle(Preset,
		FTargetingSourceContext { SourceActor.Get() });

	bAwaitingCallback = true;

	if (bAsync)
	{
		FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(Handle);
		AsyncTaskData.bReleaseOnCompletion = true;

		TargetSubsystem->StartAsyncTargetingRequestWithHandle(Handle,
			FTargetingRequestDelegate::CreateUObject(this, &ThisClass::OnTargetingRequestComplete));
	}
	else
	{
		TargetSubsystem->ExecuteTargetingRequestWithHandle(Handle,
			FTargetingRequestDelegate::CreateUObject(this, &ThisClass::OnTargetingRequestComplete));
	}
}

void UAbilityTask_ScanForTargets::OnTargetingRequestComplete(FTargetingRequestHandle TargetingHandle)
{
	bAwaitingCallback = false;
	if (!TargetingHandle.IsValid())
	{
		return;
	}
	
	// Process results
	int32 NumResults = 0;
	if (const FTargetingDefaultResultsSet* Results = FTargetingDefaultResultsSet::Find(TargetingHandle))
	{
		NumResults = Results->TargetResults.Num();
		if (NumResults > 0 && ShouldBroadcastAbilityTaskDelegates())
		{
			OnTargetsFound.Broadcast(Results->TargetResults);
		}
	}

	// Check if we should end the task
	switch (DurationType)
	{
	case EScanOverlapDuration::Ability:
		break;
	case EScanOverlapDuration::Duration:
		break;
	case EScanOverlapDuration::TargetFound:
		if (NumResults > 0)
		{
			OnScanTaskReachedEnd(GetAvatarActor());
		}
		break;
	case EScanOverlapDuration::NoTargetFound:
		if (NumResults == 0)
		{
			OnScanTaskReachedEnd(GetAvatarActor());
		}
		break;
	case EScanOverlapDuration::Once:
		OnScanTaskReachedEnd(GetAvatarActor());
		break;
	}
}

void UAbilityTask_ScanForTargets::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UAbilityTask_ScanForTargets::OnDestroy(bool bInOwnerFinished)
{
	if (!bIsFinished && ShouldBroadcastAbilityTaskDelegates())
	{
		bIsFinished = true;
		OnFinished.Broadcast();
	}
	
	Super::OnDestroy(bInOwnerFinished);
}
