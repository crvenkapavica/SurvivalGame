#include "SurvivalCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Camera/CameraComponent.h"
#include "../Player/SurvivalPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/InteractionComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "../Items/EquippableItem.h"
#include "../Items/GearItem.h"
#include "../Items/WeaponItem.h"
#include "../Items/ThrowableItem.h"
#include "Materials/MaterialInstance.h"
#include "../World/Pickup.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/DamageType.h"
#include "../Weapons/SurvivalDamageTypes.h"
#include "../Weapons/ThrowableWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "../SurvivalGame.h"
#include "../Weapons/Weapon.h"
#include "../Widgets/SurvivalHUD.h"

#define LOCTEXT_NAMESPACE "SurvivalCharacter"

static FName NAME_AimDownSightsSocket("ADSSocket");

ASurvivalCharacter::ASurvivalCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArmComponent");
	SpringArmComponent->SetupAttachment(GetMesh(), FName("CameraSocket"));
	SpringArmComponent->TargetArmLength = 0.f;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	CameraComponent->SetupAttachment(SpringArmComponent);
	CameraComponent->bUsePawnControlRotation = true;

	HelmetMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Helmet, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HelmetMesh")));
	ChestMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Chest, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh")));
	LegsMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Legs, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMesh")));
	FeetMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Feet, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FeetMesh")));
	VestMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Vest, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VestMesh")));
	HandsMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Hands, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandsMesh")));
	BackpackMesh = PlayerMeshes.Add(EEquippableSlot::EIS_Backpack, CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BackpackMesh")));

	for (auto& Kvp : PlayerMeshes)
	{
		USkeletalMeshComponent* MeshComponent = Kvp.Value;
		MeshComponent->SetupAttachment(GetMesh());
		MeshComponent->SetMasterPoseComponent(GetMesh());
	}

	PlayerMeshes.Add(EEquippableSlot::EIS_Head, GetMesh());
	GetMesh()->SetOwnerNoSee(true);

	PlayerInventory = CreateDefaultSubobject<UInventoryComponent>("PlayerInventory");
	PlayerInventory->SetCapacity(20);
	PlayerInventory->SetWeightCapacity(80.f);
	PlayerInventory->OnItemAdded.AddDynamic(this, &ASurvivalCharacter::ItemAddedToInventory);
	PlayerInventory->OnItemRemoved.AddDynamic(this, &ASurvivalCharacter::ItemRemovedFromInventory);

	LootPlayerInteraction = CreateDefaultSubobject<UInteractionComponent>("PlayerInteraction");
	LootPlayerInteraction->InteractableActionText = LOCTEXT("LootPlayerText", "Loot");
	LootPlayerInteraction->InteractableNameText = LOCTEXT("LootPlayerName", "Player");
	LootPlayerInteraction->SetupAttachment(GetRootComponent());
	LootPlayerInteraction->SetActive(false, true);
	LootPlayerInteraction->bAutoActivate = false;

	USceneComponent* InventoryCaptureRoot = CreateDefaultSubobject<USceneComponent>("InventoryCaptureRoot");
	InventoryCaptureRoot->SetupAttachment(GetCapsuleComponent());
	InventoryCaptureRoot->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));

	InventoryPlayerCapture = CreateDefaultSubobject<USceneCaptureComponent2D>("InventoryPlayerCapture");
	InventoryPlayerCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	InventoryPlayerCapture->SetupAttachment(InventoryCaptureRoot);
	InventoryPlayerCapture->SetRelativeLocation(FVector(-1100.f, 0.f, 0.f));

	InteractionCheckFrequency = 0.f;
	InteractionCheckDistance = 1000.f;

	MaxHealth = 100.f;
	Health = MaxHealth;

	MeleeAttackDistance = 150.f;
	MeleeAttackDamage = 20.f;

	SprintSpeed = GetCharacterMovement()->MaxWalkSpeed * 1.3f;
	WalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	bIsAiming = false;

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	SetReplicateMovement(true);
	SetReplicates(true);
	bAlwaysRelevant = true;
}

void ASurvivalCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocallyControlled())
	{
		CameraComponent->DestroyComponent();
		SpringArmComponent->DestroyComponent();
		InventoryPlayerCapture->DestroyComponent();
	}
	if (InventoryPlayerCapture)
	{
		InventoryPlayerCapture->ShowOnlyActors.Add(this);
	}

	LootPlayerInteraction->OnInteract.AddDynamic(this, &ASurvivalCharacter::BeginLootingPlayer);

	if (APlayerState* PS = GetPlayerState())
	{
		LootPlayerInteraction->SetInteractableNameText(FText::FromString(PS->GetPlayerName()));
	}
	for (auto& PlayerMesh : PlayerMeshes)
	{
		NakedMeshes.Add(PlayerMesh.Key, PlayerMesh.Value->SkeletalMesh);
	}
}

void ASurvivalCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASurvivalCharacter, bSprinting);
	DOREPLIFETIME(ASurvivalCharacter, EquippedWeapon);
	DOREPLIFETIME(ASurvivalCharacter, Killer);

	DOREPLIFETIME_CONDITION(ASurvivalCharacter, LootSource, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ASurvivalCharacter, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ASurvivalCharacter, bIsAiming, COND_SkipOwner);
}

bool ASurvivalCharacter::IsInteracting() const
{
	return GetWorldTimerManager().IsTimerActive(TimerHandle_Interact);
}

float ASurvivalCharacter::GetRemainingInteractTime() const
{
	return GetWorldTimerManager().GetTimerRemaining(TimerHandle_Interact);
}

void ASurvivalCharacter::UseItem(class UItem* Item)
{
	if (!HasAuthority() && Item)
	{
		ServerUseItem(Item);
	}

	if (HasAuthority())
	{
		if (PlayerInventory && !PlayerInventory->FindItem(Item))
		{
			return;
		}
	}

	if (Item)
	{
		Item->OnUse(this);
		Item->Use(this);
	}
}

void ASurvivalCharacter::DropItem(class UItem* Item, const int32 Quantity)
{
	if (PlayerInventory && Item && PlayerInventory->FindItem(Item))
	{
		if (!HasAuthority())
		{
			ServerDropItem(Item, Quantity);
			return;
		}

		if (HasAuthority())
		{
			const int32 ItemQuantity = Item->GetQuantity();
			const int32 DroppedQuantity = PlayerInventory->ConsumeItem(Item, Quantity);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.bNoFail = true;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			FVector SpawnLocation = GetActorLocation();
			SpawnLocation.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

			FTransform SpawnTransform(GetActorRotation(), SpawnLocation);

			ensure(PickupClass);

			if (APickup* Pickup = GetWorld()->SpawnActor<APickup>(PickupClass, SpawnTransform, SpawnParams))
			{
				Pickup->InitializePickup(Item->GetClass(), DroppedQuantity);
			}
		}
	}
}

void ASurvivalCharacter::ItemAddedToInventory(class UItem* Item)
{}

void ASurvivalCharacter::ItemRemovedFromInventory(class UItem* Item)
{}

bool ASurvivalCharacter::EquipItem(class UEquippableItem* Item)
{
	EquippedItems.Add(Item->Slot, Item);
	OnEquippedItemsChanged.Broadcast(Item->Slot, Item);
	return true;
}

bool ASurvivalCharacter::UnEquipItem(class UEquippableItem* Item)
{
	if (Item)
	{
		if (EquippedItems.Contains(Item->Slot))
		{
			if (Item == *EquippedItems.Find(Item->Slot))
			{
				EquippedItems.Remove(Item->Slot);
				OnEquippedItemsChanged.Broadcast(Item->Slot, nullptr);
				return true;
			}
		}
	}
	return false;
}

void ASurvivalCharacter::EquipGear(class UGearItem* Gear)
{
	if (USkeletalMeshComponent* GearMesh = GetSlotSkeletalMeshComponent(Gear->Slot))
	{
		GearMesh->SetSkeletalMesh(Gear->Mesh);
		GearMesh->SetMaterial(GearMesh->GetMaterials().Num() - 1, Gear->MaterialInstance);
	}
}

void ASurvivalCharacter::UnEquipGear(const EEquippableSlot Slot)
{
	if (USkeletalMeshComponent* EquippableMesh = GetSlotSkeletalMeshComponent(Slot))
	{
		if (USkeletalMesh* BodyMesh = *NakedMeshes.Find(Slot))
		{
			EquippableMesh->SetSkeletalMesh(BodyMesh);
			for (int32 i = 0; i < BodyMesh->Materials.Num(); ++i)
			{
				if (BodyMesh->Materials.IsValidIndex(i))
				{
					EquippableMesh->SetMaterial(i, BodyMesh->Materials[i].MaterialInterface);
				}
			}
		}
		else
		{
			EquippableMesh->SetSkeletalMesh(nullptr);
		}
	}
}

void ASurvivalCharacter::EquipWeapon(class UWeaponItem* WeaponItem)
{
	if (WeaponItem && WeaponItem->WeaponClass && HasAuthority())
	{
		if (EquippedWeapon)
		{
			UnEquipWeapon();
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.bNoFail = true;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.Owner = SpawnParams.Instigator = this;

		if (AWeapon* Weapon = GetWorld()->SpawnActor<AWeapon>(WeaponItem->WeaponClass, SpawnParams))
		{
			Weapon->Item = WeaponItem;

			EquippedWeapon = Weapon;
			OnRep_EquippedWeapon();

			Weapon->OnEquip();
		}
	}
}

void ASurvivalCharacter::UnEquipWeapon()
{
	if (HasAuthority() && EquippedWeapon)
	{
		EquippedWeapon->OnUnEquip();
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
		OnRep_EquippedWeapon();
	}
}

class USkeletalMeshComponent* ASurvivalCharacter::GetSlotSkeletalMeshComponent(const EEquippableSlot Slot)
{
	if (PlayerMeshes.Contains(Slot))
	{
		return *PlayerMeshes.Find(Slot);
	}
	return nullptr;
}

class UThrowableItem* ASurvivalCharacter::GetThrowable() const
{
	UThrowableItem* EquippedThrowable = nullptr;

	if (EquippedItems.Contains(EEquippableSlot::EIS_Throwable))
	{
		EquippedThrowable = Cast<UThrowableItem>(*EquippedItems.Find(EEquippableSlot::EIS_Throwable));
	}

	return EquippedThrowable;
}

void ASurvivalCharacter::UseThrowable()
{
	if (CanUseThrowable())
	{
		if (UThrowableItem* Throwable = GetThrowable())
		{
			if (HasAuthority())
			{
				SpawnThrowable();

				if (PlayerInventory)
				{
					PlayerInventory->ConsumeItem(Throwable, 1);
				}
			}
			else
			{
				if (Throwable->GetQuantity() <= 1)
				{
					EquippedItems.Remove(EEquippableSlot::EIS_Throwable);
					OnEquippedItemsChanged.Broadcast(EEquippableSlot::EIS_Throwable, nullptr);
				}

				PlayAnimMontage(Throwable->ThrowableTossAnimation);

				ServerUseThrowable();
			}
		}
	}
}

void ASurvivalCharacter::SpawnThrowable()
{
	if (HasAuthority())
	{
		if (UThrowableItem* CurrentThrowable = GetThrowable())
		{
			if (CurrentThrowable->ThrowableClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = SpawnParams.Instigator = this;
				SpawnParams.bNoFail = true;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				FVector EyesLoc;
				FRotator EyesRot;

				GetController()->GetPlayerViewPoint(EyesLoc, EyesRot);

				EyesLoc = (EyesRot.Vector() * 20.f) + EyesLoc;

				if (AThrowableWeapon* ThrowableWeapon = GetWorld()->SpawnActor<AThrowableWeapon>(CurrentThrowable->ThrowableClass, FTransform(EyesRot, EyesLoc), SpawnParams))
				{
					MulticastPlayThrowableTossFX(CurrentThrowable->ThrowableTossAnimation);
				}
			}
		}
	}
}

bool ASurvivalCharacter::CanUseThrowable() const
{
	return GetThrowable() != nullptr && GetThrowable()->ThrowableClass != nullptr;
}

void ASurvivalCharacter::MulticastPlayThrowableTossFX_Implementation(UAnimMontage* MontageToPlay)
{
	if (GetNetMode() != NM_DedicatedServer && !IsLocallyControlled())
	{
		PlayAnimMontage(MontageToPlay);
	}
}

void ASurvivalCharacter::ServerUseThrowable_Implementation()
{
	UseThrowable();
}

void ASurvivalCharacter::ServerDropItem_Implementation(class UItem* Item, const int32 Quantity)
{
	DropItem(Item, Quantity);
}

bool ASurvivalCharacter::ServerDropItem_Validate(class UItem* Item, const int32 Quantity)
{
	return true;
}

void ASurvivalCharacter::ServerUseItem_Implementation(class UItem* Item)
{
	UseItem(Item);
}

bool ASurvivalCharacter::ServerUseItem_Validate(class UItem* Item)
{
	return true;
}

float ASurvivalCharacter::ModifyHealth(const float Delta)
{
	const float OldHealth = Health;

	Health = FMath::Clamp<float>(Health + Delta, 0.f, MaxHealth);

	return Health - OldHealth;
}

void ASurvivalCharacter::OnRep_Health(float OldHealth)
{
	OnHealthModified(Health - OldHealth);
}

void ASurvivalCharacter::OnRep_EquippedWeapon()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->OnEquip();
	}
}

void ASurvivalCharacter::StartFire()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->StartFire();
	}
	else
	{
		BeginMeleeAttack();
	}
}

void ASurvivalCharacter::StopFire()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->StopFire();
	}
}

void ASurvivalCharacter::BeginMeleeAttack()
{
	if (GetWorld()->TimeSince(LastMeleeAttackTime) > MeleeAttackMontage->GetPlayLength())
	{
		FHitResult Hit;
		FCollisionShape Shape = FCollisionShape::MakeSphere(15.f);

		FVector StartTrace = CameraComponent->GetComponentLocation();
		FVector EndTrace = (CameraComponent->GetComponentRotation().Vector() * MeleeAttackDistance) + StartTrace;

		FCollisionQueryParams QueryParams = FCollisionQueryParams("MeleeSweep", false, this);

		PlayAnimMontage(MeleeAttackMontage);

		if (GetWorld()->SweepSingleByChannel(Hit, StartTrace, EndTrace, FQuat(), COLLISION_WEAPON, Shape, QueryParams))
		{
			if (ASurvivalCharacter* HitPlayer = Cast<ASurvivalCharacter>(Hit.GetActor()))
			{
				if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController()))
				{
					PC->ClientShotHitConfirmed();
				}
			}
		}

		ServerProcessMeleeHit(Hit);

		LastMeleeAttackTime = GetWorld()->GetTimeSeconds();
	}
}

void ASurvivalCharacter::MulticastPlayMeleeFX_Implementation()
{
	if (!IsLocallyControlled())
	{
		PlayAnimMontage(MeleeAttackMontage);
	}
}


void ASurvivalCharacter::ServerProcessMeleeHit_Implementation(const FHitResult& MeleeHit)
{
	if (GetWorld()->TimeSince(LastMeleeAttackTime) > MeleeAttackMontage->GetPlayLength() && (GetActorLocation() - MeleeHit.ImpactPoint).Size() <= MeleeAttackDistance)
	{
		MulticastPlayMeleeFX();

		UGameplayStatics::ApplyPointDamage(MeleeHit.GetActor(), MeleeAttackDamage, (MeleeHit.TraceStart - MeleeHit.TraceEnd).GetSafeNormal(), MeleeHit, GetController(), this, UMeleeDamage::StaticClass());
	}
	LastMeleeAttackTime = GetWorld()->GetTimeSeconds();
}

void ASurvivalCharacter::Suicide(struct FDamageEvent const& DamageEvent, const AActor* DamageCauser)
{
	Killer = this;
	OnRep_Killer();
}

void ASurvivalCharacter::KilledByPlayer(struct FDamageEvent const& DamageEvent, class ASurvivalCharacter* Character, const AActor* DamageCauser)
{
	Killer = Character;
	OnRep_Killer();
}

void ASurvivalCharacter::OnRep_Killer()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	GetMesh()->SetOwnerNoSee(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	SetReplicateMovement(false);

	LootPlayerInteraction->Activate();

	TArray<UEquippableItem*> EquippedInvItems;
	EquippedItems.GenerateValueArray(EquippedInvItems);

	for (auto& Equippable : EquippedInvItems)
	{
		Equippable->SetEquipped(false);
	}

	if (IsLocallyControlled())
	{
		SpringArmComponent->TargetArmLength = 500.f;
		SpringArmComponent->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		bUseControllerRotationPitch = true;

		if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController()))
		{
			PC->Died(Killer);
		}
	}
}

bool ASurvivalCharacter::CanSprint() const
{
	return !IsAiming();
}

void ASurvivalCharacter::StartSprinting()
{
	SetSprinting(true);
}

void ASurvivalCharacter::StopSprinting()
{
	SetSprinting(false);
}

void ASurvivalCharacter::SetSprinting(const bool bNewSprinting)
{
	if ((bNewSprinting && !CanSprint()) || bNewSprinting == bSprinting)
	{
		return;
	}

	if (!HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}

	bSprinting = bNewSprinting;

	GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

void ASurvivalCharacter::ServerSetSprinting_Implementation(const bool bNewSprinting)
{
	SetSprinting(bNewSprinting);
}

bool ASurvivalCharacter::ServerSetSprinting_Validate(const bool bNewSprinting)
{
	return true;
}

void ASurvivalCharacter::StartCrouching()
{
	Crouch();
}

void ASurvivalCharacter::StopCrouching()
{
	UnCrouch();
}

void ASurvivalCharacter::MoveForward(float Val)
{
	if (Val != 0.f)
	{
		AddMovementInput(GetActorForwardVector(), Val);
	}
}

void ASurvivalCharacter::MoveRight(float Val)
{
	if (Val != 0.f)
	{
		AddMovementInput(GetActorRightVector(), Val);
	}
}

void ASurvivalCharacter::LookUp(float Val)
{
	if (Val != 0.f)
	{
		AddControllerPitchInput(Val);
	}
}

void ASurvivalCharacter::Turn(float Val)
{
	if (Val != 0.f)
	{
		AddControllerYawInput(Val);
	}
}

bool ASurvivalCharacter::CanAim() const
{
	return EquippedWeapon != nullptr;
}

void ASurvivalCharacter::StartAiming()
{
	if (CanAim())
	{
		SetAiming(true);
	}
}

void ASurvivalCharacter::StopAiming()
{
	SetAiming(false);
}

void ASurvivalCharacter::SetAiming(const bool bNewAiming)
{
	if ((bNewAiming && !CanAim()) || bNewAiming == bIsAiming)
	{
		return;
	}

	if (!HasAuthority())
	{
		ServerSetAiming(bNewAiming);
	}

	bIsAiming = bNewAiming;
}

void ASurvivalCharacter::ServerSetAiming_Implementation(const bool bNewAiming)
{
	SetAiming(bNewAiming);
}

void ASurvivalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (GetWorld()->TimeSince(InteractionData.LastInteractionCheckTime) > InteractionCheckFrequency)
	{
		PerformInteractionCheck();
	}

	if (IsLocallyControlled())
	{
		const float DesiredFOV = IsAiming() ? 70.f : 100.f;
		CameraComponent->SetFieldOfView(FMath::FInterpTo(CameraComponent->FieldOfView, DesiredFOV, DeltaTime, 10.f));

		if (EquippedWeapon)
		{
			const FVector ADSLocation = EquippedWeapon->GetWeaponMesh()->GetSocketLocation(NAME_AimDownSightsSocket);
			const FVector DefaultCameraLocation = GetMesh()->GetSocketLocation(FName("CameraSocket"));

			const FVector CameraLoc = bIsAiming ? ADSLocation : DefaultCameraLocation;

			const float InterpSpeed = FVector::Dist(ADSLocation, DefaultCameraLocation) / EquippedWeapon->ADSTime;
			CameraComponent->SetWorldLocation(FMath::VInterpTo(CameraComponent->GetComponentLocation(), CameraLoc, DeltaTime, InterpSpeed));
		}
	}

}

void ASurvivalCharacter::Restart()
{
	Super::Restart();

	if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController()))
	{
		if (ASurvivalHUD* HUD = Cast<ASurvivalHUD>(PC->GetHUD()))
		{
			HUD->CreateGameplayWidget();
		}
	}
}

float ASurvivalCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

	if (!IsAlive())
	{
		return 0.f;
	}

	const float DamageDealt = ModifyHealth(-Damage);
	
	UE_LOG(LogTemp, Warning, TEXT("Health: %f"), Health);

	if (Health <= 0.f)
	{
		if (ASurvivalCharacter* KillerCharacter = Cast<ASurvivalCharacter>(DamageCauser->GetOwner()))
		{
			KilledByPlayer(DamageEvent, KillerCharacter, DamageCauser);
		}
		else
		{
			Suicide(DamageEvent, DamageCauser);
		}
	}

	return DamageDealt;
}

void ASurvivalCharacter::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);

	if (EquippedWeapon)
	{
		EquippedWeapon->SetActorHiddenInGame(bNewHidden);
	}
}

void ASurvivalCharacter::SetLootSource(class UInventoryComponent* NewLootSource)
{
	if (NewLootSource && NewLootSource->GetOwner())
	{
		NewLootSource->GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ASurvivalCharacter::OnLootSourceOwnerDestroyed);
	}

	if (HasAuthority())
	{
		if (NewLootSource)
		{
			if (ASurvivalCharacter* Character = Cast<ASurvivalCharacter>(NewLootSource->GetOwner()))
			{
				Character->SetLifeSpan(120.f);
			}
		}

		LootSource = NewLootSource;
		OnRep_LootSource();
	}
	else
	{
		ServerSetLootSource(NewLootSource);
	}
}

bool ASurvivalCharacter::IsLooting() const
{
	return LootSource != nullptr;
}

void ASurvivalCharacter::BeginLootingPlayer(class ASurvivalCharacter* Character)
{
	if (Character)
	{
		Character->SetLootSource(PlayerInventory);
	}
}

void ASurvivalCharacter::OnLootSourceOwnerDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority() && LootSource && DestroyedActor == LootSource->GetOwner())
	{
		ServerSetLootSource(nullptr);
	}
}

void ASurvivalCharacter::OnRep_LootSource()
{
	if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			if (ASurvivalHUD* HUD = Cast<ASurvivalHUD>(PC->GetHUD()))
			{
				if(LootSource)
				{
					HUD->OpenLootWidget();
				}
				else
				{
					HUD->CloseLootWidget();
				}
			}
		}
	}
}

void ASurvivalCharacter::ServerSetLootSource_Implementation(class UInventoryComponent* NewLootSource)
{
	SetLootSource(NewLootSource);
}

bool ASurvivalCharacter::ServerSetLootSource_Validate(class UInventoryComponent* NewLootSource)
{
	return true;
}

void ASurvivalCharacter::LootItem(class UItem* ItemToGive)
{
	if (HasAuthority())
	{
		if (PlayerInventory && LootSource && ItemToGive && LootSource->HasItem(ItemToGive->GetClass(), ItemToGive->GetQuantity()))
		{
			const FItemAddResult AddResult = PlayerInventory->TryAddItem(ItemToGive);

			if (AddResult.AmountGiven > 0)
			{
				LootSource->ConsumeItem(ItemToGive, AddResult.AmountGiven);
			}
			else
			{
				if (ASurvivalPlayerController* PC = Cast<ASurvivalPlayerController>(GetController()))
				{
					PC->ClientShowNotification(AddResult.ErrorText);
				}
			}
		}
	}
	else
	{
		ServerLootItem(ItemToGive);
	}
}

void ASurvivalCharacter::ServerLootItem_Implementation(class UItem* ItemToLoot)
{
	LootItem(ItemToLoot);
}

bool ASurvivalCharacter::ServerLootItem_Validate(class UItem* ItemToLoot)
{
	return true;
}

void ASurvivalCharacter::PerformInteractionCheck()
{

	if (GetController() == nullptr)
	{
		return;
	}
	
	InteractionData.LastInteractionCheckTime = GetWorld()->GetTimeSeconds();

	FVector EyesLoc;
	FRotator EyesRot;

	GetController()->GetPlayerViewPoint(EyesLoc, EyesRot);

	FVector TraceStart = EyesLoc;
	FVector TraceEnd = (EyesRot.Vector() * InteractionCheckDistance) + TraceStart;
	FHitResult TraceHit;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(TraceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		if (TraceHit.GetActor())
		{
			if (UInteractionComponent* InteractionComponent = Cast<UInteractionComponent>(TraceHit.GetActor()->GetComponentByClass(UInteractionComponent::StaticClass())))
			{
				float Distance = (TraceStart - TraceHit.ImpactPoint).Size();
				if (InteractionComponent != GetInteractable() && Distance <= InteractionComponent->InteractionDistance)
				{
					FoundNewInteractable(InteractionComponent);
				}
				else if (Distance > InteractionComponent->InteractionDistance && GetInteractable())
				{
					CouldntFindInteractable();
				}
				return;
			}
		}
	}

	CouldntFindInteractable();

}

void ASurvivalCharacter::CouldntFindInteractable()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_Interact))
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_Interact);
	}

	if (UInteractionComponent* Interactable = GetInteractable())
	{
		Interactable->EndFocus(this);

		if (InteractionData.bInteractHeld)
		{
			EndInteract();
		}
	}

	InteractionData.ViewedInteractionComponent = nullptr;
}

void ASurvivalCharacter::FoundNewInteractable(UInteractionComponent* Interactable)
{
	EndInteract();

	if (UInteractionComponent* OldInteractable = GetInteractable())
	{
		OldInteractable->EndFocus(this);
	}

	InteractionData.ViewedInteractionComponent = Interactable;
	Interactable->BeginFocus(this);

}

void ASurvivalCharacter::BeginInteract()
{
	if (!HasAuthority())
	{
		ServerBeginInteract();
	}

	if (HasAuthority())
	{
		PerformInteractionCheck();
	}

	InteractionData.bInteractHeld = true;

	if (UInteractionComponent* Interactable = GetInteractable())
	{
		Interactable->BeginInteract(this);

		if (FMath::IsNearlyZero(Interactable->InteractionTime))
		{
			Interact();
		}
		else
		{
			GetWorldTimerManager().SetTimer(TimerHandle_Interact, this, &ASurvivalCharacter::Interact, Interactable->InteractionTime, false);
		}
	}
}

void ASurvivalCharacter::EndInteract()
{
	if (!HasAuthority())
	{
		ServerEndInteract();
	}

	InteractionData.bInteractHeld = false;

	GetWorldTimerManager().ClearTimer(TimerHandle_Interact);

	if (UInteractionComponent* Interactable = GetInteractable())
	{
		Interactable->EndInteract(this);
	}
}

void ASurvivalCharacter::Interact()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_Interact);

	if (UInteractionComponent* Interactable = GetInteractable())
	{
		Interactable->Interact(this);
	}
}

void ASurvivalCharacter::ServerEndInteract_Implementation()
{
	EndInteract();
}

bool ASurvivalCharacter::ServerEndInteract_Validate()
{
	return true;
}

void ASurvivalCharacter::ServerBeginInteract_Implementation()
{
	BeginInteract();
}

bool ASurvivalCharacter::ServerBeginInteract_Validate()
{
	return true;
}

void ASurvivalCharacter::StartReload()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->StartReload();
	}
}

void ASurvivalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ASurvivalCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ASurvivalCharacter::StopFire);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ASurvivalCharacter::StartAiming);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ASurvivalCharacter::StopAiming);
	PlayerInputComponent->BindAction("Throw", IE_Pressed, this, &ASurvivalCharacter::UseThrowable);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASurvivalCharacter::BeginInteract);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &ASurvivalCharacter::EndInteract);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ASurvivalCharacter::StartCrouching);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ASurvivalCharacter::StopCrouching);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ASurvivalCharacter::StartSprinting);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ASurvivalCharacter::StopSprinting);

	PlayerInputComponent->BindAxis("MoveForward", this, &ASurvivalCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASurvivalCharacter::MoveRight);
}

#undef LOCTEXT_NAMESPACE