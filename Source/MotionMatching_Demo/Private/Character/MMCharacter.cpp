// Character/.cpp - COMPLETE FIXED VERSION
#include "Character/MMCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"

AMMCharacter::AMMCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMMCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Add mapping context for local players
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (DefaultIMC)
                {
                    Subsys->AddMappingContext(DefaultIMC, IMCPriority);
                }
            }
        }
    }
}

void AMMCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (ensureMsgf(IA_Move != nullptr, TEXT("AMMCharacter: IA_Move is null")))
        {
            EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMMCharacter::OnMoveTriggered);
            EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &AMMCharacter::OnMoveCompleted);
            EIC->BindAction(IA_Move, ETriggerEvent::Canceled,  this, &AMMCharacter::OnMoveCompleted);
        }

        if (ensureMsgf(IA_Look != nullptr, TEXT("AMMCharacter: IA_Look is null")))
        {
            EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMMCharacter::OnLookTriggered);
            EIC->BindAction(IA_Look, ETriggerEvent::Completed, this, &AMMCharacter::OnLookCompleted);
            EIC->BindAction(IA_Look, ETriggerEvent::Canceled,  this, &AMMCharacter::OnLookCompleted);
        }
    }
}

void AMMCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
    CurrentMoveInput = Value.Get<FVector2D>();
    
    // Apply movement using standard character movement
    if (Controller && CurrentMoveInput.Size() > 0.01f)
    {
        const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
        
        // Get forward and right vectors
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        
        // Apply movement - STANDARD UE5 CONVENTION
        AddMovementInput(ForwardDirection, CurrentMoveInput.Y);
        AddMovementInput(RightDirection, CurrentMoveInput.X);
    }
}

void AMMCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
    CurrentMoveInput = FVector2D::ZeroVector;
}

void AMMCharacter::OnLookTriggered(const FInputActionValue& Value)
{
    CurrentLookInput = Value.Get<FVector2D>();
    
    if (Controller)
    {
        // This is the CORRECT way - let Unreal handle camera rotation
        AddControllerYawInput(CurrentLookInput.X);
        AddControllerPitchInput(-CurrentLookInput.Y);
    }
}

void AMMCharacter::OnLookCompleted(const FInputActionValue& Value)
{
    CurrentLookInput = FVector2D::ZeroVector;
}

FRotator AMMCharacter::GetControlYawRotation() const
{
    // Return pure control rotation without mesh compensation
    // Mesh compensation should be applied contextually, not globally
    const float BaseYaw = Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
    return FRotator(0.f, BaseYaw, 0.f);
}

float AMMCharacter::GetMeshYawOffset() const
{
    if (const USkeletalMeshComponent* SkeletalMesh = GetMesh())
    {
        return FMath::UnwindDegrees(SkeletalMesh->GetRelativeRotation().Yaw);
    }
    return 0.f;
}


