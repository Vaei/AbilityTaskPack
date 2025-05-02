// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_ScanForTargets.generated.h"

struct FTargetingRequestHandle;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScanTargetsFoundDelegate, const TArray<FTargetingDefaultResultData>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScanFinishedDelegate);

UENUM(BlueprintType)
enum class EScanOverlapDuration : uint8
{
	Ability			UMETA(ToolTip="Stop when ability ends"),
	Duration		UMETA(ToolTip="Stop when max duration ends"),
	TargetFound		UMETA(ToolTip="Stop when any target is found"),
	NoTargetFound	UMETA(ToolTip="Stop when no targets are found"),
	Once			UMETA(ToolTip="Stop when the task runs once"),
};

class UTargetingPreset;

/**
 * Find nearby objects of a specific type and callback
 * Useful for finding overlapping pawns to trigger root motion source tasks on them -- after a wait net sync!
 */
UCLASS()
class ABILITYTASKPACK_API UAbilityTask_ScanForTargets : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnScanTargetsFoundDelegate OnTargetsFound;

	UPROPERTY(BlueprintAssignable)
	FOnScanFinishedDelegate OnFinished;

	UAbilityTask_ScanForTargets(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	static UAbilityTask_ScanForTargets* ScanForTargets(UGameplayAbility* OwningAbility, const UTargetingPreset* TargetingPreset,
		const TObjectPtr<AActor>& InSourceActor, FVector WorldOrigin, float MaxScanRate = 0.05f,
		EScanOverlapDuration InDurationType = EScanOverlapDuration::Ability, float InMaxDuration = 0.f, bool bUpdateAsync = false);

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(DisplayName="Scan For Targets ()", Keywords="target,preset", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UAbilityTask_ScanForTargets* K2_ScanForTargets(UGameplayAbility* OwningAbility, const UTargetingPreset* TargetingPreset,
		AActor* InSourceActor, FVector WorldOrigin, float MaxScanRate = 0.05f,
		EScanOverlapDuration InDurationType = EScanOverlapDuration::Ability, float MaxDuration = 0.f, bool bUpdateAsync = false);

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;
	virtual void SharedInitAndApply();
	void OnScanTaskReachedEnd(AActor* Avatar);

	virtual void TickTask(float DeltaTime) override;
	virtual void OnTargetingRequestComplete(FTargetingRequestHandle TargetingHandle);

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	
protected:
	UPROPERTY()
	const UTargetingPreset* Preset;

	UPROPERTY()
	TWeakObjectPtr<AActor> SourceActor;

	UPROPERTY()
	FVector Origin;
	
	UPROPERTY()
	float MaxRate;

	UPROPERTY()
	float Duration;

	UPROPERTY()
	bool bAsync;

	UPROPERTY()
	EScanOverlapDuration DurationType;

	float ElapsedTime;
	float TimeSinceLastScan;
	
	bool bIsFinished;
	bool bAwaitingCallback;
};
