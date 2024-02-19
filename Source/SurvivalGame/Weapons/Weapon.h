#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalDamageTypes.h"
#include "Weapon.generated.h"

class UAnimMontage;
class ASurvivalCharacter;
class UAudioComponent;
class UParticleSystemComponent;
class UCameraShake;
class UForceFeedbackEffect;
class USoundCue;

UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	Idle,
	Firing,
	Reloading,
	Equipping
};

USTRUCT(BlueprintType)
struct FWeaponData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Ammo)
	int32 AmmoPerClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Ammo)
	TSubclassOf<class UAmmoItem> AmmoClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = WeaponStat)
	float TimeBetweenShots;

	FWeaponData()
	{
		AmmoPerClip = 20;
		TimeBetweenShots = 0.2f;
	}
};

USTRUCT()
struct FWeaponAnim
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	UAnimMontage* Pawn1P;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	UAnimMontage* Pawn3P;
};

USTRUCT(BlueprintType)
struct FHitScanConfiguration
{
	GENERATED_BODY()

		FHitScanConfiguration()
	{
		Distance = 10000.f;
		Damage = 25.f;
		Radius = 0.f;
		DamageType = UWeaponDamage::StaticClass();
		ClientSideHitLeeway = 300.f;
	}

	UPROPERTY(EditDefaultsOnly, Category = "Trace Info")
	TMap<FName, float> BoneDamageModifiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Info")
	float Distance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Info")
	float Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Info")
	float Radius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Info")
	float ClientSideHitLeeway;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Info")
	TSubclassOf<UDamageType> DamageType;

};

UCLASS()
class SURVIVALGAME_API AWeapon : public AActor
{
	GENERATED_BODY()
	
	friend class ASurvivalCharacter;

public:	
	AWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

protected:
	void UseClipAmmo();

	void ConsumeAmmo(const int32 Amount = 1);

	void ReturnAmmoToInventory();

	virtual void OnEquip();
	virtual void OnEquipFinished();
	virtual void OnUnEquip();

	bool IsEquipped() const;
	bool IsAttachedToPawn() const;

	virtual void StartFire();
	virtual void StopFire();
	virtual void StartReload(bool bFromReplication = false);
	virtual void StopReload();
	virtual void ReloadWeapon();

	UFUNCTION(reliable, client)
	void ClientStartReload();

	bool CanFire() const;
	bool CanReload() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	EWeaponState GetCurrentState() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetCurrentAmmo() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetCurrentAmmoInClip() const;

	int32 GetAmmoPerClip() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	class ASurvivalCharacter* GetPawnOwner() const;

	void SetPawnOwner(ASurvivalCharacter* SurvivalCharacter);

	float GetEquipStartedTime() const;

	float GetEquipDuration() const;

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Transient)
	class UWeaponItem* Item;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_PawnOwner)
	class ASurvivalCharacter* PawnOwner;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Config)
	FWeaponData WeaponConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Config)
	FHitScanConfiguration HitScanConfig;

public:
	UPROPERTY(EditAnywhere, Category = Components)
	USkeletalMeshComponent* WeaponMesh;

protected:
	UPROPERTY(Transient)
	float TimerIntervalAdjustment;

	UPROPERTY(Config)
	bool bAllowAutomaticWeaponCatchup = true;

	UPROPERTY(Transient)
	UAudioComponent* FireAC;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	FName MuzzleAttachPoint;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	FName AttachSocket1P;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	FName AttachSocket3P;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	UParticleSystem* MuzzleParticles;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	UParticleSystem* ImpactParticles;

	UPROPERTY(Transient)
	UParticleSystemComponent* MuzzlePSC;

	UPROPERTY(Transient)
	UParticleSystemComponent* MuzzlePSCSecondary;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	TSubclassOf<UCameraShake> FireCameraShake;

	UPROPERTY(EditDefaultsOnly, Category = Weapon)
	float ADSTime;

	UPROPERTY(EditDefaultsOnly, Category = Recoil)
	class UCurveVector* RecoilCurve;

	UPROPERTY(EditDefaultsOnly, Category = Recoil)
	float RecoilSpeed;

	UPROPERTY(EditDefaultsOnly, Category = Recoil)
	float RecoilResetSpeed;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	UForceFeedbackEffect *FireForceFeedback;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* FireSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* FireLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* FireFinishSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* OutOfAmmoSound;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* ReloadSound;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim ReloadAnim;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	USoundCue* EquipSound;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim EquipAnim;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim FireAnim;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	FWeaponAnim FireAimingAnim;

	UPROPERTY(EditDefaultsOnly, Category = Effects)
	uint32 bLoopedMuzzleFX : 1;

	UPROPERTY(EditDefaultsOnly, Category = Sound)
	uint32 bLoopedFireSound : 1;

	UPROPERTY(EditDefaultsOnly, Category = Animation)
	uint32 bLoopedFireAnim : 1;

	uint32 bPlayingFireAnim : 1;
	uint32 bIsEquipped : 1;
	uint32 bWantsToFire : 1;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_Reload)
	uint32 bPendingReload : 1;

	uint32 bPendingEquip : 1;
	uint32 bRefiring;

	EWeaponState CurrentState;

	float LastFireTime;
	float EquipStartedTime;
	float EquipDuration;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_HitNotify)
	FVector HitNotify;

	UFUNCTION()
	void OnRep_HitNotify();

	UPROPERTY(Transient, Replicated)
	int32 CurrentAmmoInClip;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
	int32 BurstCounter;

	FTimerHandle TimerHandle_OnEquipFinished;
	FTimerHandle TimerHandle_StopReload;
	FTimerHandle TimerHandle_ReloadWeapon;
	FTimerHandle TimerHandle_HandleFiring;

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStartFire();

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStopFire();

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStartReload();

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStopReload();

	UFUNCTION()
	void OnRep_PawnOwner();

	UFUNCTION()
	void OnRep_BurstCounter();

	UFUNCTION()
	void OnRep_Reload();

	virtual void SimulateWeaponFire();

	virtual void StopSimulatingWeaponFire();

	virtual void FireShot();

	UFUNCTION(reliable, server, WithValidation)
	void ServerHandleFiring();

	void HandleReFiring();

	void HandleFiring();

	virtual void OnBurstStarted();

	virtual void OnBurstFinished();

	void SetWeaponState(EWeaponState NewState);

	void DetermineWeaponState();

	void AttachMeshToPawn();


	UAudioComponent* PlayWeaponSound(USoundCue* Sound);


	float PlayWeaponAnimation(const FWeaponAnim& Animation);
	void StopWeaponAnimation(const FWeaponAnim& Animation);

	FVector GetCameraAim() const;

	void SimulateInstantHit(const FVector& Origin);
	void SpawnImpactEffects(const FHitResult& Impact);

public:

	void DealDamage(const FHitResult& Impact, const FVector& ShootDir);

	FHitResult WeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const;

	void ProcessInstantHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir);

	UFUNCTION(Reliable, Server)
	void ServerNotifyHit(const FHitResult& Impact, FVector_NetQuantizeNormal ShootDir);

	UFUNCTION(Unreliable, Server)
	void ServerNotifyMiss(FVector_NetQuantizeNormal ShootDir);

	void ProcessInstantHit_Confirmed(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir);
};
