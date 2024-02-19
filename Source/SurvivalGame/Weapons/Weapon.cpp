#include "Weapon.h"
#include "SurvivalGame.h"

#include "Player/SurvivalPlayerController.h"
#include "Player/SurvivalCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InventoryComponent.h"
#include "Curves/CurveVector.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

#include "Net/UnrealNetwork.h"
#include "Items/EquippableItem.h"
#include "Items/AmmoItem.h"
#include "Items/WeaponItem.h"
#include "DrawDebugHelpers.h"

#include "Components/SkeletalMeshComponent.h"

static const FName NAME_MuzzleSocket("Muzzle");

AWeapon::AWeapon()
{
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->bReceivesDecals = false;
	WeaponMesh->SetCollisionObjectType(ECC_WorldDynamic);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	RootComponent = WeaponMesh;

	bLoopedMuzzleFX = false;
	bLoopedFireAnim = false;
	bPlayingFireAnim = false;
	bIsEquipped = false;
	bWantsToFire = false;
	bPendingReload = false;
	bPendingEquip = false;
	CurrentState = EWeaponState::Idle;
	AttachSocket1P = FName("GripPoint");
	AttachSocket3P = FName("GripPoint");

	CurrentAmmoInClip = 0;
	BurstCounter = 0;
	LastFireTime = 0.0f;

	ADSTime = 0.5f;
	RecoilResetSpeed = 5.f;
	RecoilSpeed = 10.f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeapon, PawnOwner);

	DOREPLIFETIME_CONDITION(AWeapon, CurrentAmmoInClip, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWeapon, BurstCounter, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeapon, bPendingReload, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AWeapon, Item, COND_InitialOnly);
}

void AWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		PawnOwner = Cast<ASurvivalCharacter>(GetOwner());
	}
}


void AWeapon::Destroyed()
{
	Super::Destroyed();

	StopSimulatingWeaponFire();
}

void AWeapon::UseClipAmmo()
{
	if (HasAuthority())
	{
		--CurrentAmmoInClip;
	}
}

void AWeapon::ConsumeAmmo(const int32 Amount)
{
	if (HasAuthority() && PawnOwner)
	{
		if (UInventoryComponent* Inventory = PawnOwner->PlayerInventory)
		{
			if (UItem* AmmoItem = Inventory->FindItemByClass(WeaponConfig.AmmoClass))
			{
				Inventory->ConsumeItem(AmmoItem, Amount);
			}
		}
	}
}

void AWeapon::ReturnAmmoToInventory()
{
	if (HasAuthority())
	{
		if (PawnOwner && CurrentAmmoInClip > 0)
		{
			if (UInventoryComponent* Inventory = PawnOwner->PlayerInventory)
			{
				Inventory->TryAddItemFromClass(WeaponConfig.AmmoClass, CurrentAmmoInClip);
			}
		}
	}
}


void AWeapon::OnEquip()
{
	AttachMeshToPawn();

	bPendingEquip = true;
	DetermineWeaponState();

	OnEquipFinished();


	if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		PlayWeaponSound(EquipSound);
	}
}

void AWeapon::OnEquipFinished()
{
	AttachMeshToPawn();

	bIsEquipped = true;
	bPendingEquip = false;

	DetermineWeaponState();

	if (PawnOwner)
	{
		if (PawnOwner->IsLocallyControlled() &&
			CanReload())
		{
			StartReload();
		}
	}
}


void AWeapon::OnUnEquip()
{
	bIsEquipped = false;
	StopFire();

	if (bPendingReload)
	{
		StopWeaponAnimation(ReloadAnim);
		bPendingReload = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
		GetWorldTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
	}

	if (bPendingEquip)
	{
		StopWeaponAnimation(EquipAnim);
		bPendingEquip = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
	}

	ReturnAmmoToInventory();
	DetermineWeaponState();
}

bool AWeapon::IsEquipped() const
{
	return bIsEquipped;
}

bool AWeapon::IsAttachedToPawn() const
{
	return bIsEquipped || bPendingEquip;
}


void AWeapon::StartFire()
{
	if (!HasAuthority())
	{
		ServerStartFire();
	}

	if (!bWantsToFire)
	{
		bWantsToFire = true;
		DetermineWeaponState();
	}
}

void AWeapon::StopFire()
{
	if (!HasAuthority() && PawnOwner && PawnOwner->IsLocallyControlled())
	{
		ServerStopFire();
	}

	if (bWantsToFire)
	{
		bWantsToFire = false;
		DetermineWeaponState();
	}
}

void AWeapon::StartReload(bool bFromReplication /*= false*/)
{
	if (!bFromReplication && !HasAuthority())
	{
		ServerStartReload();
	}

	if (bFromReplication || CanReload())
	{
		bPendingReload = true;
		DetermineWeaponState();

		float AnimDuration = PlayWeaponAnimation(ReloadAnim);
		if (AnimDuration <= 0.0f)
		{
			AnimDuration = .5f;
		}

		GetWorldTimerManager().SetTimer(TimerHandle_StopReload, this, &AWeapon::StopReload, AnimDuration, false);
		if (HasAuthority())
		{
			GetWorldTimerManager().SetTimer(TimerHandle_ReloadWeapon, this, &AWeapon::ReloadWeapon, FMath::Max(0.1f, AnimDuration - 0.1f), false);
		}

		if (PawnOwner && PawnOwner->IsLocallyControlled())
		{
			PlayWeaponSound(ReloadSound);
		}
	}
}

void AWeapon::StopReload()
{
	if (CurrentState == EWeaponState::Reloading)
	{
		bPendingReload = false;
		DetermineWeaponState();
		StopWeaponAnimation(ReloadAnim);
	}
}

void AWeapon::ReloadWeapon()
{
	const int32 ClipDelta = FMath::Min(WeaponConfig.AmmoPerClip - CurrentAmmoInClip, GetCurrentAmmo());

	if (ClipDelta > 0)
	{
		CurrentAmmoInClip += ClipDelta;
		ConsumeAmmo(ClipDelta);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Didnt have enough ammo for a reload"));
	}
}

bool AWeapon::CanFire() const
{
	bool bCanFire = PawnOwner != nullptr;
	bool bStateOKToFire = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
	return ((bCanFire == true) && (bStateOKToFire == true) && (bPendingReload == false));
}

bool AWeapon::CanReload() const
{
	bool bCanReload = PawnOwner != nullptr;
	bool bGotAmmo = (CurrentAmmoInClip < WeaponConfig.AmmoPerClip) && (GetCurrentAmmo() > 0);
	bool bStateOKToReload = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
	return ((bCanReload == true) && (bGotAmmo == true) && (bStateOKToReload == true));
}

EWeaponState AWeapon::GetCurrentState() const
{
	return CurrentState;
}

int32 AWeapon::GetCurrentAmmo() const
{
	if (PawnOwner)
	{
		if (UInventoryComponent* Inventory = PawnOwner->PlayerInventory)
		{
			if (UItem* Ammo = Inventory->FindItemByClass(WeaponConfig.AmmoClass))
			{
				return Ammo->GetQuantity();
			}
		}
	}

	return 0;
}

int32 AWeapon::GetCurrentAmmoInClip() const
{
	return CurrentAmmoInClip;
}

int32 AWeapon::GetAmmoPerClip() const
{
	return WeaponConfig.AmmoPerClip;
}

USkeletalMeshComponent* AWeapon::GetWeaponMesh() const
{
	return WeaponMesh;
}

class ASurvivalCharacter* AWeapon::GetPawnOwner() const
{
	return PawnOwner;
}

void AWeapon::SetPawnOwner(ASurvivalCharacter* SurvivalCharacter)
{
	if (PawnOwner != SurvivalCharacter)
	{
		SetInstigator(SurvivalCharacter);
		PawnOwner = SurvivalCharacter;
		SetOwner(SurvivalCharacter);
	}
}

float AWeapon::GetEquipStartedTime() const
{
	return EquipStartedTime;
}

float AWeapon::GetEquipDuration() const
{
	return EquipDuration;
}

void AWeapon::ClientStartReload_Implementation()
{
	StartReload();
}

void AWeapon::ServerStartFire_Implementation()
{
	StartFire();
}

bool AWeapon::ServerStartFire_Validate()
{
	return true;
}

void AWeapon::ServerStopFire_Implementation()
{
	StopFire();
}

bool AWeapon::ServerStopFire_Validate()
{
	return true;
}

void AWeapon::ServerStartReload_Implementation()
{
	StartReload();
}

bool AWeapon::ServerStartReload_Validate()
{
	return true;
}

void AWeapon::ServerStopReload_Implementation()
{
	StopReload();
}

bool AWeapon::ServerStopReload_Validate()
{
	return true;
}

void AWeapon::OnRep_HitNotify()
{
	SimulateInstantHit(HitNotify);
}

void AWeapon::OnRep_PawnOwner()
{

}

void AWeapon::OnRep_BurstCounter()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (BurstCounter > 0)
		{
			SimulateWeaponFire();
		}
		else
		{
			StopSimulatingWeaponFire();
		}
	}
}

void AWeapon::OnRep_Reload()
{
	if (bPendingReload)
	{
		StartReload();
	}
	else
	{
		StopReload();
	}
}

void AWeapon::SimulateWeaponFire()
{
	if (CurrentState != EWeaponState::Firing)
	{
		return;
	}

	if (MuzzleParticles)
	{
		if (!bLoopedMuzzleFX || MuzzlePSC == NULL)
		{
			if ((PawnOwner != NULL) && (PawnOwner->IsLocallyControlled() == true))
			{
				AController* PlayerCon = PawnOwner->GetController();
				if (PlayerCon != NULL)
				{
					WeaponMesh->GetSocketLocation(MuzzleAttachPoint);
					MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleParticles, WeaponMesh, MuzzleAttachPoint);
					MuzzlePSC->bOwnerNoSee = false;
					MuzzlePSC->bOnlyOwnerSee = true;
				}
			}
			else
			{
				MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleParticles, WeaponMesh, MuzzleAttachPoint);
			}
		}
	}


	if (!bLoopedFireAnim || !bPlayingFireAnim)
	{
		FWeaponAnim AnimToPlay = FireAnim;
		PlayWeaponAnimation(FireAnim);
		bPlayingFireAnim = true;
	}

	if (bLoopedFireSound)
	{
		if (FireAC == NULL)
		{
			FireAC = PlayWeaponSound(FireLoopSound);
		}
	}
	else
	{
		PlayWeaponSound(FireSound);
	}

	ASurvivalPlayerController* PC = (PawnOwner != NULL) ? Cast<ASurvivalPlayerController>(PawnOwner->Controller) : NULL;
	if (PC != NULL && PC->IsLocalController())
	{
		if (RecoilCurve)
		{
			const FVector2D RecoilAmount(RecoilCurve->GetVectorValue(FMath::RandRange(0.f, 1.f)).X, RecoilCurve->GetVectorValue(FMath::RandRange(0.f, 1.f)).Y);
			PC->ApplyRecoil(RecoilAmount, RecoilSpeed, RecoilResetSpeed);
		}

		if (FireCameraShake != NULL)
		{
			PC->ClientPlayCameraShake(FireCameraShake, 1);
		}
		if (FireForceFeedback != NULL)
		{
			FForceFeedbackParameters FFParams;
			FFParams.Tag = "Weapon";
			PC->ClientPlayForceFeedback(FireForceFeedback, FFParams);
		}
	}
}


void AWeapon::StopSimulatingWeaponFire()
{
	if (bLoopedMuzzleFX)
	{
		if (MuzzlePSC != NULL)
		{
			MuzzlePSC->DeactivateSystem();
			MuzzlePSC = NULL;
		}
		if (MuzzlePSCSecondary != NULL)
		{
			MuzzlePSCSecondary->DeactivateSystem();
			MuzzlePSCSecondary = NULL;
		}
	}

	if (bLoopedFireAnim && bPlayingFireAnim)
	{
		StopWeaponAnimation(FireAimingAnim);
		StopWeaponAnimation(FireAnim);
		bPlayingFireAnim = false;
	}

	if (FireAC)
	{
		FireAC->FadeOut(0.1f, 0.0f);
		FireAC = NULL;

		PlayWeaponSound(FireFinishSound);
	}
}

void AWeapon::HandleReFiring()
{
	UWorld* MyWorld = GetWorld();

	float SlackTimeThisFrame = FMath::Max(0.0f, (MyWorld->TimeSeconds - LastFireTime) - WeaponConfig.TimeBetweenShots);

	if (bAllowAutomaticWeaponCatchup)
	{
		TimerIntervalAdjustment -= SlackTimeThisFrame;
	}

	HandleFiring();
}

void AWeapon::HandleFiring()
{

	if ((CurrentAmmoInClip > 0) && CanFire())
	{
		if (GetNetMode() != NM_DedicatedServer)
		{
			SimulateWeaponFire();
		}

		if (PawnOwner && PawnOwner->IsLocallyControlled())
		{
			FireShot();
			BurstCounter++;
			OnRep_BurstCounter();
		}
	}
	else if (CanReload())
	{
		StartReload();
	}
	else if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		if (GetCurrentAmmo() == 0 && !bRefiring)
		{
			PlayWeaponSound(OutOfAmmoSound);
			ASurvivalPlayerController* MyPC = Cast<ASurvivalPlayerController>(PawnOwner->Controller);
		}
		if (BurstCounter > 0)
		{
			OnBurstFinished();
		}
	}

	if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		if(!HasAuthority())
		{
			ServerHandleFiring();
		}
		else
		{
			const bool bShouldUpdateAmmo = (CurrentAmmoInClip > 0 && CanFire());

			if (bShouldUpdateAmmo)
			{
				UseClipAmmo();
				BurstCounter++;
				OnRep_BurstCounter();
			}
		}

		if (CurrentAmmoInClip <= 0 && CanReload())
		{
			StartReload();
		}

		bRefiring = (CurrentState == EWeaponState::Firing && WeaponConfig.TimeBetweenShots > 0.0f);
		if (bRefiring)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AWeapon::HandleReFiring, FMath::Max<float>(WeaponConfig.TimeBetweenShots + TimerIntervalAdjustment, SMALL_NUMBER), false);
			TimerIntervalAdjustment = 0.f;
		}
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
}

void AWeapon::OnBurstStarted()
{
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (LastFireTime > 0 && WeaponConfig.TimeBetweenShots > 0.0f &&
		LastFireTime + WeaponConfig.TimeBetweenShots > GameTime)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AWeapon::HandleFiring, LastFireTime + WeaponConfig.TimeBetweenShots - GameTime, false);
	}
	else
	{
		HandleFiring();
	}
}

void AWeapon::OnBurstFinished()
{
	BurstCounter = 0;
	OnRep_BurstCounter();

	if (GetNetMode() != NM_DedicatedServer)
	{
		StopSimulatingWeaponFire();
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_HandleFiring);
	bRefiring = false;

	TimerIntervalAdjustment = 0.0f;
}


void AWeapon::SetWeaponState(EWeaponState NewState)
{
	const EWeaponState PrevState = CurrentState;

	if (PrevState == EWeaponState::Firing && NewState != EWeaponState::Firing)
	{
		OnBurstFinished();
	}

	CurrentState = NewState;

	if (PrevState != EWeaponState::Firing && NewState == EWeaponState::Firing)
	{
		OnBurstStarted();
	}
}


void AWeapon::DetermineWeaponState()
{
	EWeaponState NewState = EWeaponState::Idle;

	if (bIsEquipped)
	{
		if (bPendingReload)
		{
			if (CanReload() == false)
			{
				NewState = CurrentState;
			}
			else
			{
				NewState = EWeaponState::Reloading;
			}
		}
		else if ((bPendingReload == false) && (bWantsToFire == true) && (CanFire() == true))
		{
			NewState = EWeaponState::Firing;
		}
	}
	else if (bPendingEquip)
	{
		NewState = EWeaponState::Equipping;
	}

	SetWeaponState(NewState);
}

void AWeapon::AttachMeshToPawn()
{
	if (PawnOwner)
	{
		if (const USkeletalMeshComponent* PawnMesh = PawnOwner->GetMesh())
		{
			const FName AttachSocket = PawnOwner->IsLocallyControlled() ? AttachSocket1P : AttachSocket3P;
			AttachToComponent(PawnOwner->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
		}
	}
}

UAudioComponent* AWeapon::PlayWeaponSound(USoundCue* Sound)
{
	UAudioComponent* AC = NULL;
	if (Sound && PawnOwner)
	{
		AC = UGameplayStatics::SpawnSoundAttached(Sound, PawnOwner->GetRootComponent());
	}

	return AC;
}

float AWeapon::PlayWeaponAnimation(const FWeaponAnim& Animation)
{
	float Duration = 0.0f;
	if (PawnOwner)
	{
		UAnimMontage* UseAnim = PawnOwner->IsLocallyControlled() ? Animation.Pawn1P : Animation.Pawn3P;
		if (UseAnim)
		{
			Duration = PawnOwner->PlayAnimMontage(UseAnim);
		}
	}

	return Duration;
}

void AWeapon::StopWeaponAnimation(const FWeaponAnim& Animation)
{
	if (PawnOwner)
	{
		UAnimMontage* UseAnim = PawnOwner->IsLocallyControlled() ? Animation.Pawn1P : Animation.Pawn3P;
		if (UseAnim)
		{
			PawnOwner->StopAnimMontage(UseAnim);
		}
	}
}

FVector AWeapon::GetCameraAim() const
{
	ASurvivalPlayerController* const PlayerController = GetInstigator() ? Cast<ASurvivalPlayerController>(GetInstigator()->Controller) : NULL;
	FVector FinalAim = FVector::ZeroVector;

	if (PlayerController)
	{
		FVector CamLoc;
		FRotator CamRot;
		PlayerController->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		FinalAim = GetInstigator()->GetBaseAimRotation().Vector();
	}

	return FinalAim;
}


void AWeapon::SimulateInstantHit(const FVector& Origin)
{
	if (!GetInstigator())
	{
		UE_LOG(LogTemp, Warning, TEXT("Weapon did not have instigator"));
		return;
	}

	const FVector StartTrace = Origin;
	const FVector AimRot = GetInstigator()->GetBaseAimRotation().Vector();
	const FVector EndTrace = (AimRot * HitScanConfig.Distance) + StartTrace;

	FHitResult WeaponHit = WeaponTrace(StartTrace, EndTrace);

	SpawnImpactEffects(WeaponHit);
}

void AWeapon::SpawnImpactEffects(const FHitResult& Impact)
{
	if (ImpactParticles)
	{
		if (Impact.bBlockingHit)
		{
			if (Impact.GetActor() != nullptr && Impact.GetActor() == UGameplayStatics::GetPlayerPawn(this, 0))
			{
				return;
			}

			FHitResult UseImpact = Impact;

			if (!Impact.Component.IsValid())
			{
				const FVector StartTrace = Impact.ImpactPoint + Impact.ImpactNormal * 10.0f;
				const FVector EndTrace = Impact.ImpactPoint - Impact.ImpactNormal * 10.0f;
				FHitResult Hit = WeaponTrace(StartTrace, EndTrace);
				UseImpact = Hit;
			}

			FTransform const SpawnTransform(Impact.ImpactNormal.Rotation(), Impact.ImpactPoint);

			UParticleSystem* ImpactEffect = ImpactParticles;
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactEffect, SpawnTransform);
		}
	}

}

void AWeapon::DealDamage(const FHitResult& Impact, const FVector& ShootDir)
{
	if (ASurvivalCharacter* OwnerCharacter = Cast<ASurvivalCharacter>(GetOwner()))
	{

		if (AActor* HitActor = Impact.GetActor())
		{
			float DamageAmount = HitScanConfig.Damage;

			FPointDamageEvent PointDmg;
			PointDmg.DamageTypeClass = HitScanConfig.DamageType;
			PointDmg.HitInfo = Impact;
			PointDmg.ShotDirection = ShootDir;
			PointDmg.Damage = DamageAmount;

			HitActor->TakeDamage(PointDmg.Damage, PointDmg, OwnerCharacter->Controller, this);
		}
	}
}

FHitResult AWeapon::WeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const
{
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, GetInstigator());
	TraceParams.bReturnPhysicalMaterial = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, COLLISION_WEAPON, TraceParams);

	return Hit;
}

void AWeapon::FireShot()
{
	if (ASurvivalCharacter* OwnerCharacter = Cast<ASurvivalCharacter>(GetOwner()))
	{
		if (GetNetMode() != NM_DedicatedServer && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
		{
			if (ASurvivalPlayerController* OwnerController = Cast<ASurvivalPlayerController>(OwnerCharacter->GetController()))
			{
				FVector AimLoc;
				FRotator AimRot;

				OwnerController->GetPlayerViewPoint(AimLoc, AimRot);

				const FVector StartTrace = AimLoc;
				const FVector EndTrace = (AimRot.Vector() * HitScanConfig.Distance) + AimLoc;
				const FVector ShootDir = AimRot.Vector();

				FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, GetInstigator());
				TraceParams.bReturnPhysicalMaterial = true;

				FHitResult WeaponHit = WeaponTrace(StartTrace, EndTrace);
				ProcessInstantHit(WeaponHit, StartTrace, ShootDir);
			}
		}
	}
}

void AWeapon::ProcessInstantHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir)
{
	if (ASurvivalCharacter* OwnerCharacter = Cast<ASurvivalCharacter>(GetOwner()))
	{
		if (OwnerCharacter->IsLocallyControlled() && GetNetMode() == NM_Client)
		{
			if (Impact.GetActor() && Impact.GetActor()->GetRemoteRole() == ROLE_Authority)
			{
				ServerNotifyHit(Impact, ShootDir);
			}
			else if (Impact.GetActor() == NULL)
			{
				if (Impact.bBlockingHit)
				{
					ServerNotifyHit(Impact, ShootDir);
				}
				else
				{
					ServerNotifyMiss(ShootDir);
				}
			}
		}
	}

	ProcessInstantHit_Confirmed(Impact, Origin, ShootDir);
}

void AWeapon::ProcessInstantHit_Confirmed(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir)
{
	if (HasAuthority())
	{
		DealDamage(Impact, ShootDir);

		HitNotify = Origin;

		if (Impact.GetActor() && Impact.GetActor()->IsA<ASurvivalCharacter>())
		{
			if (ASurvivalCharacter* OwnerCharacter = Cast<ASurvivalCharacter>(GetOwner()))
			{
				if (ASurvivalPlayerController* OwnerController = OwnerCharacter ? Cast<ASurvivalPlayerController>(OwnerCharacter->GetController()) : nullptr)
				{
					OwnerController->ClientShotHitConfirmed();
				}
			}
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		SpawnImpactEffects(Impact);
	}
}

void AWeapon::ServerNotifyMiss_Implementation(FVector_NetQuantizeNormal ShootDir)
{

}

void AWeapon::ServerNotifyHit_Implementation(const FHitResult& Impact, FVector_NetQuantizeNormal ShootDir)
{
	if (GetInstigator() && (Impact.GetActor() || Impact.bBlockingHit))
	{
		const FVector Origin = WeaponMesh ? WeaponMesh->GetSocketLocation(NAME_MuzzleSocket) : FVector();
		const FVector ViewDir = (Impact.Location - Origin).GetSafeNormal();

		const float ViewDotHitDir = FVector::DotProduct(GetInstigator()->GetViewRotation().Vector(), ViewDir);
		if (true)
		{
			if (Impact.GetActor() == NULL)
			{
				if (Impact.bBlockingHit)
				{
					ProcessInstantHit_Confirmed(Impact, Origin, ShootDir);
				}
			}
			else if (Impact.GetActor()->IsRootComponentStatic() || Impact.GetActor()->IsRootComponentStationary())
			{
				ProcessInstantHit_Confirmed(Impact, Origin, ShootDir);
			}
			else
			{
				const FBox HitBox = Impact.GetActor()->GetComponentsBoundingBox();
				FVector BoxExtent = 0.5 * (HitBox.Max - HitBox.Min);
				BoxExtent *= HitScanConfig.ClientSideHitLeeway;
				BoxExtent.X = FMath::Max(20.0f, BoxExtent.X);
				BoxExtent.Y = FMath::Max(20.0f, BoxExtent.Y);
				BoxExtent.Z = FMath::Max(20.0f, BoxExtent.Z);

				const FVector BoxCenter = (HitBox.Min + HitBox.Max) * 0.5;
				if (FMath::Abs(Impact.Location.Z - BoxCenter.Z) < BoxExtent.Z &&
					FMath::Abs(Impact.Location.X - BoxCenter.X) < BoxExtent.X &&
					FMath::Abs(Impact.Location.Y - BoxCenter.Y) < BoxExtent.Y)
				{
					ProcessInstantHit_Confirmed(Impact, Origin, ShootDir);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("%s Rejected client side hit of %s (outside bounding box tolerance)"), *GetNameSafe(this), *GetNameSafe(Impact.GetActor()));
				}
			}
		}
		else if (false)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s Rejected client side hit of %s (facing too far from the hit direction)"), *GetNameSafe(this), *GetNameSafe(Impact.GetActor()));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s Rejected client side hit of %s"), *GetNameSafe(this), *GetNameSafe(Impact.GetActor()));
		}
	}
}

void AWeapon::ServerHandleFiring_Implementation()
{
	const bool bShouldUpdateAmmo = (CurrentAmmoInClip > 0 && CanFire());

	HandleFiring();

	if (bShouldUpdateAmmo)
	{
		UseClipAmmo();
		BurstCounter++;
		OnRep_BurstCounter();
	}
}

bool AWeapon::ServerHandleFiring_Validate()
{
	return true;
}