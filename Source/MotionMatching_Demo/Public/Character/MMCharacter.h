// Character/.h - COMPLETE FIXED VERSION
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MMCharacter.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class MOTIONMATCHING_DEMO_API AMMCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMMCharacter(const FObjectInitializer& ObjectInitializer);

    /** Optional mapping context to add on BeginPlay (local players). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MM|Input")
    UInputMappingContext* DefaultIMC = nullptr;

    /** Priority for DefaultIMC. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MM|Input")
    int32 IMCPriority = 0;

    /** Axis2D action for movement (Enhanced Input). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MM|Input")
    UInputAction* IA_Move = nullptr;

    /** Axis2D action for look (X=Yaw, Y=Pitch). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MM|Input")
    UInputAction* IA_Look = nullptr;

    /** Latest cached movement input (X=Right, Y=Forward). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MM|Input")
    FVector2D CurrentMoveInput = FVector2D::ZeroVector;

    /** Latest cached look input (X=YawDelta, Y=PitchDelta). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MM|Input")
    FVector2D CurrentLookInput = FVector2D::ZeroVector;

    /**
     * Get control rotation for trajectory purposes (no mesh compensation)
     */
    UFUNCTION(BlueprintPure, Category="MM|Camera")
    FRotator GetControlYawRotation() const;
    
    /**
     * Get mesh yaw offset if needed for specific calculations
     */
    UFUNCTION(BlueprintPure, Category="MM|Camera")
    float GetMeshYawOffset() const;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UFUNCTION()
    void OnMoveTriggered(const struct FInputActionValue& Value);

    UFUNCTION()
    void OnMoveCompleted(const struct FInputActionValue& Value);

    UFUNCTION()
    void OnLookTriggered(const struct FInputActionValue& Value);

    UFUNCTION()
    void OnLookCompleted(const struct FInputActionValue& Value);
};