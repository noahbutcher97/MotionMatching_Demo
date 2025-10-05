#include "Data/MMPrimaryStructs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"


// ============================================================================
// FRotationConfig Implementation - IMPROVED WITH ALL FIXES
// ============================================================================

void FRotationConfig::InitializeOnPlay(AActor* Owner, UCharacterMovementComponent* MoveComp)
{
    if (!Owner || !MoveComp) return;
    
    // Configure CMC based on initial settings
    ConfigureCMC(MoveComp);
    
    // Initialize rotation state
    CurrentYaw = Owner->GetActorRotation().Yaw;
    TargetYaw = CurrentYaw;
    RotationLag = 0.f;
    ActualAngularVelocity = 0.f;
    DesiredAngularVelocity = 0.f;
    bIsLagging = false;
    bIsRotationRateLimited = false;
    BlendAlpha = 0.0f;
    
    // Initialize alpha correction with improved parameters
    AlphaErrorAccumulator = 0.0f;
    LastAlphaPredictionError = 0.0f;
    
    // Initialize movement state with continuous blend
    MovementAlpha = 0.0f;
    if (Owner->GetWorld())
    {
        LastMovementTime = Owner->GetWorld()->GetTimeSeconds();
    }
    bWasMovingLastFrame = false;
    
    // Set initial mode tracking with improved transition
    LastRotationMode = GetCurrentModeAsInt();
    ModeTransitionAlpha = 1.0f;
    ModeTransitionStartRotation = Owner->GetActorQuat();
    ModeTransitionTargetYaw = CurrentYaw;
    ModeBlendAlpha = 1.0f;
    
    // Initialize raw input state
    RawInputForRotation = FVector2D::ZeroVector;
    
    UE_LOG(LogTemp, Warning, TEXT("Rotation System Initialized: %s"), *GetCurrentModeDescription());
}

void FRotationConfig::ConfigureCMC(UCharacterMovementComponent* MoveComp)
{
    if (!MoveComp) return;
    
    if (bUseCharacterMovementRotation)
    {
        // Re-enable CMC rotation systems
        bCompetingSystemsDisabled = false;
        
        // Configure CMC flags based on selected mode
        switch (CMCMode)
        {
        case ECMCRotationMode::ControlBased:
            MoveComp->bUseControllerDesiredRotation = true;
            MoveComp->bOrientRotationToMovement = false;
            break;
                
        case ECMCRotationMode::MovementBased:
            MoveComp->bOrientRotationToMovement = true;
            MoveComp->bUseControllerDesiredRotation = false;
            break;
        }
        
        // Store CMC rotation rate for predictions
        CMCRotationRateDegrees = MoveComp->RotationRate.Yaw;
    }
    else
    {
        // Disable CMC rotation for manual control
        MoveComp->bOrientRotationToMovement = false;
        MoveComp->bUseControllerDesiredRotation = false;
        bCompetingSystemsDisabled = true;
    }
}

void FRotationConfig::UpdateRotationState(float ControlYaw, float CameraAngularVelocity, float DeltaTime)
{
    // Update desired state
    DesiredAngularVelocity = CameraAngularVelocity;
    
    // Check for mode changes with improved transition
    const int32 CurrentMode = GetCurrentModeAsInt();
    if (CurrentMode != LastRotationMode)
    {
        // Store the target from the previous mode for smooth transition
        ModeTransitionTargetYaw = TargetYaw;
        ModeTransitionAlpha = 0.f;
        ModeBlendAlpha = 0.f;
        ModeTransitionStartRotation = FRotator(0, CurrentYaw, 0).Quaternion();
        LastRotationMode = CurrentMode;
        UE_LOG(LogTemp, Verbose, TEXT("Rotation Mode Changed to: %s"), *GetCurrentModeDescription());
    }
    
    // Update mode transition with smooth blending
    if (ModeTransitionAlpha < 1.0f)
    {
        ModeTransitionAlpha = FMath::Min(1.0f, ModeTransitionAlpha + DeltaTime / RotationSmoothingTime);
    }
    
    if (ModeBlendAlpha < 1.0f)
    {
        ModeBlendAlpha = FMath::Min(1.0f, ModeBlendAlpha + DeltaTime / (RotationSmoothingTime * 0.5f));
    }
    
    // Calculate rotation lag based on camera angular velocity
    const float CameraSpeed = FMath::Abs(CameraAngularVelocity);
    const float TargetLag = FMath::GetMappedRangeValueClamped(
        FVector2D(0.f, AdaptiveThresholdDegrees),
        FVector2D(0.f, 1.f),
        CameraSpeed
    );
    
    // Smooth lag transitions
    const float LagRate = TargetLag > RotationLag ? 5.f : 2.f;
    RotationLag = FMath::FInterpTo(RotationLag, TargetLag, DeltaTime, LagRate);
    
    // Update rate limiting detection
    const float CurrentRate = GetCurrentRotationRate();
    bIsRotationRateLimited = FMath::Abs(DesiredAngularVelocity) > CurrentRate;
    
    // Update lagging detection
    if (bUseCharacterMovementRotation)
    {
        bIsLagging = bIsRotationRateLimited;
    }
    else
    {
        const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
        bIsLagging = FMath::Abs(DeltaYaw) > AdaptiveThresholdDegrees * 0.5f;
    }
}

void FRotationConfig::UpdateMovementState(const FVector& Velocity, float CurrentTime)
{
    // IMPROVED: Use continuous movement alpha instead of binary state
    const float Speed = Velocity.Size();
    
    // Calculate target movement alpha with smooth transition zone
    float TargetMovementAlpha = FMath::GetRangePct(0.0f, MovementThreshold * 2.0f, Speed);
    TargetMovementAlpha = FMath::SmoothStep(0.0f, 1.0f, TargetMovementAlpha);
    
    // Interpolate movement alpha for smooth transitions
    const float AlphaInterpSpeed = TargetMovementAlpha > MovementAlpha ? 5.0f : 2.0f;
    MovementAlpha = FMath::FInterpTo(MovementAlpha, TargetMovementAlpha, 0.016f, AlphaInterpSpeed);
    
    // Update binary state for compatibility (with hysteresis)
    const bool bIsMoving = Speed > MovementThreshold;
    
    if (bIsMoving != bWasMovingLastFrame)
    {
        // Only transition after delay for idle, immediate for moving
        if (bIsMoving || (CurrentTime - LastMovementTime) > IdleTransitionDelay)
        {
            bWasMovingLastFrame = bIsMoving;
            if (bIsMoving) 
            {
                LastMovementTime = CurrentTime;
            }
        }
    }
    else if (bIsMoving)
    {
        LastMovementTime = CurrentTime;
    }
}

void FRotationConfig::UpdateAlphaCorrection(float ActualAlpha, float PredictedAlpha, float DeltaTime)
{
    // Calculate prediction error
    const float Error = ActualAlpha - PredictedAlpha;
    
    // IMPROVED: Update error accumulator with proper damping
    const float DecayRate = 2.0f;
    AlphaErrorAccumulator = FMath::FInterpTo(AlphaErrorAccumulator, Error, DeltaTime, DecayRate);
    
    // Store for next frame
    LastAlphaPredictionError = Error;
}

void FRotationConfig::ApplyAlphaCorrection(float DeltaTime)
{
    // IMPROVED: Apply alpha correction with limits and decay
    if (FMath::Abs(AlphaErrorAccumulator) > 0.01f)
    {
        // Calculate correction with rate limiting
        float Correction = AlphaErrorAccumulator * AlphaCorrectionRate;
        Correction = FMath::Clamp(Correction, -AlphaCorrectionMax, AlphaCorrectionMax);
        
        // Apply correction
        BlendAlpha = FMath::Clamp(BlendAlpha + Correction, 0.0f, 1.0f);
        
        // Decay accumulator to prevent oscillation
        AlphaErrorAccumulator *= AlphaCorrectionDecay;
    }
}

FVector FRotationConfig::CalculateWorldIntentFromRawInput(const FVector2D& RawInput, const FRotator& ControlRot)
{
    // Convert raw input to world space intent for immediate rotation response
    if (RawInput.Size() < 0.01f)
    {
        return FVector::ZeroVector;
    }
    
    const FRotator CameraRot(0, ControlRot.Yaw, 0);
    const FVector CameraForward = CameraRot.RotateVector(FVector::ForwardVector);
    const FVector CameraRight = CameraRot.RotateVector(FVector::RightVector);
    
    FVector2D NormalizedInput = RawInput;
    if (NormalizedInput.Size() > 1.0f)
    {
        NormalizedInput.Normalize();
    }
    
    FVector WorldIntent = (CameraForward * NormalizedInput.Y + CameraRight * NormalizedInput.X);
    WorldIntent.Z = 0.f;
    WorldIntent.Normalize();
    
    return WorldIntent;
}

FRotator FRotationConfig::CalculateTargetForMode(int32 ModeInt, const FRotator& ControlRot, 
                                                 const FVector& Velocity, const FVector& Acceleration,
                                                 const FVector& Position) const
{
    // Calculate target rotation for a specific mode (used during transitions)
    FRotator TargetRot = ControlRot;
    
    if (ModeInt < 10) // CMC modes
    {
        switch (static_cast<ECMCRotationMode>(ModeInt))
        {
            case ECMCRotationMode::MovementBased:
                if (Velocity.Size() > MovementThreshold)
                {
                    TargetRot = Velocity.GetSafeNormal().Rotation();
                }
                break;
                
            case ECMCRotationMode::ControlBased:
                TargetRot = ControlRot;
                break;
        }
    }
    else // Manual modes
    {
        switch (static_cast<EManualRotationMode>(ModeInt - 10))
        {
            case EManualRotationMode::Orbit:
                if (Velocity.Size() > MovementThreshold)
                {
                    TargetRot = Velocity.GetSafeNormal().Rotation();
                }
                break;
                
            case EManualRotationMode::Strafe:
                TargetRot = ControlRot;
                break;
                
            case EManualRotationMode::Dynamic:
            {
                // Simple linear blend
                if (Velocity.Size() > MovementThreshold)
                {
                    const FQuat OrbitTarget = Velocity.GetSafeNormal().Rotation().Quaternion();
                    const FQuat StrafeTarget = ControlRot.Quaternion();
                    const FQuat BlendedTarget = FQuat::Slerp(OrbitTarget, StrafeTarget, DynamicBlendWeight);
                    TargetRot = BlendedTarget.Rotator();
                }
                else
                {
                    TargetRot = ControlRot;
                }
                break;
            }
                
            case EManualRotationMode::LookAt:
                if (bHasLookAtTarget)
                {
                    const FVector ToTarget = LookAtTarget - Position;
                    if (ToTarget.SizeSquared() > FMath::Square(LookAtDeadZone))
                    {
                        TargetRot = ToTarget.Rotation();
                    }
                }
                else
                {
                    TargetRot = ControlRot;
                }
                break;
                
            case EManualRotationMode::Cinematic:
                // Simplified version for transition calculation
                TargetRot = ControlRot;
                break;
        }
    }
    
    // Ensure no vertical rotation
    TargetRot.Pitch = 0;
    TargetRot.Roll = 0;
    
    return TargetRot;
}

void FRotationConfig::UpdateManualRotation(AActor* Owner, const FRotator& ControlRotation, 
                                          const FVector& Velocity, const FVector2D& InputVector,
                                          const FVector& Acceleration, float DeltaTime)
{
    if (!Owner || bUseCharacterMovementRotation || !Owner->GetWorld()) return;

    // Ensure competing systems are disabled (only once)
    if (!bCompetingSystemsDisabled)
    {
        if (ACharacter* Char = Cast<ACharacter>(Owner))
        {
            if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
            {
                MoveComp->bOrientRotationToMovement = false;
                MoveComp->bUseControllerDesiredRotation = false;
            }
        }
        bCompetingSystemsDisabled = true;
    }

    // Update movement state with continuous blend
    const float CurrentTime = Owner->GetWorld()->GetTimeSeconds();
    UpdateMovementState(Velocity, CurrentTime);
    
    // Store raw input for rotation if enabled
    if (bUseRawInputForRotation)
    {
        RawInputForRotation = InputVector;
    }
    
    // Handle mode transitions with improved blending
    if (ModeBlendAlpha < 1.0f)
    {
        // Calculate targets for both old and new modes
        const FRotator OldModeTarget = CalculateTargetForMode(
            LastRotationMode, ControlRotation, Velocity, Acceleration, Owner->GetActorLocation());
        const FRotator NewModeTarget = CalculateTargetForMode(
            GetCurrentModeAsInt(), ControlRotation, Velocity, Acceleration, Owner->GetActorLocation());
        
        // Blend between mode targets
        const FQuat BlendedTarget = FQuat::Slerp(
            OldModeTarget.Quaternion(),
            NewModeTarget.Quaternion(),
            FMath::SmoothStep(0.f, 1.f, ModeBlendAlpha)
        );
        
        TargetYaw = BlendedTarget.Rotator().Yaw;
        
        // Apply rotation during transition
        const float TransitionRate = GetCurrentRotationRate() * 1.5f; // Boost rate during transition
        const FRotator CurrentRotation = Owner->GetActorRotation();
        const FRotator NewRotation = FMath::RInterpConstantTo(
            CurrentRotation, FRotator(0, TargetYaw, 0), DeltaTime, TransitionRate);
        Owner->SetActorRotation(NewRotation);
        CurrentYaw = NewRotation.Yaw;
        
        return; // Skip normal rotation processing during transition
    }

    const FRotator CurrentRotation = Owner->GetActorRotation();
    FRotator TargetRotation = CurrentRotation;
    float EffectiveRate = GetCurrentRotationRate();
    
    // NEW: Calculate world intent from raw input if enabled
    FVector WorldIntent = FVector::ZeroVector;
    if (bUseRawInputForRotation && RawInputForRotation.Size() > 0.01f)
    {
        WorldIntent = CalculateWorldIntentFromRawInput(RawInputForRotation, ControlRotation);
    }
    
    // Use continuous movement alpha for smoother transitions
    const float InterpSpeed = FMath::Lerp(IdleInterpolationSpeed, MovingInterpolationSpeed, MovementAlpha);

    // Process rotation based on mode with continuous movement blending
    if (MovementAlpha > 0.01f && Velocity.SizeSquared() > 1.0f)
    {
        switch (ManualMode)
        {
            case EManualRotationMode::Orbit:
            {
                // Pure orbit - always face velocity direction
                TargetRotation = Velocity.GetSafeNormal().Rotation();
                BlendAlpha = 0.0f;
                break;
            }

            case EManualRotationMode::Strafe:
            {
                // Pure strafe - always face camera direction
                TargetRotation = ControlRotation;
                BlendAlpha = 1.0f;
                break;
            }

            case EManualRotationMode::Dynamic:
            {
                // SIMPLIFIED: Pure linear blend between orbit and strafe
                const FQuat OrbitTarget = Velocity.GetSafeNormal().Rotation().Quaternion();
                const FQuat StrafeTarget = ControlRotation.Quaternion();
                
                // Use DynamicBlendWeight directly as the interpolation factor
                const FQuat BlendedTarget = FQuat::Slerp(OrbitTarget, StrafeTarget, DynamicBlendWeight);
                TargetRotation = BlendedTarget.Rotator();
                
                // Store blend state for AnimBP (now matches the weight exactly)
                BlendAlpha = DynamicBlendWeight;
                
                // Apply movement alpha to blend between idle and moving behavior
                if (MovementAlpha < 1.0f)
                {
                    const FQuat IdleTarget = ControlRotation.Quaternion();
                    const FQuat FinalTarget = FQuat::Slerp(IdleTarget, BlendedTarget, MovementAlpha);
                    TargetRotation = FinalTarget.Rotator();
                }
                break;
            }

            case EManualRotationMode::LookAt:
            {
                // Look at specific target if available
                if (bHasLookAtTarget)
                {
                    const FVector ToTarget = LookAtTarget - Owner->GetActorLocation();
                    if (ToTarget.SizeSquared() > FMath::Square(LookAtDeadZone))
                    {
                        TargetRotation = ToTarget.Rotation();
                        BlendAlpha = 1.0f; // Treat as strafe-like
                    }
                }
                else
                {
                    // Fallback to camera facing
                    TargetRotation = ControlRotation;
                    BlendAlpha = 1.0f;
                }
                break;
            }

            case EManualRotationMode::Cinematic:
            {
                // IMPROVED: Intelligent cinematic rotation with proper normalization
                
                // Check if we have valid weights
                const float TotalWeight = CinematicAccelerationInfluence + 
                                         CinematicVelocityInfluence + 
                                         CinematicControlInfluence;
                
                // FIX: Prevent division by zero
                if (TotalWeight < KINDA_SMALL_NUMBER)
                {
                    // Fallback to strafe mode behavior
                    TargetRotation = ControlRotation;
                    BlendAlpha = 1.0f;
                    break;
                }
                
                // Only engage cinematic rotation above minimum speed
                if (Velocity.Size() > CinematicMinSpeed)
                {
                    // Normalize weights
                    const float NormAccelWeight = CinematicAccelerationInfluence / TotalWeight;
                    const float NormVelWeight = CinematicVelocityInfluence / TotalWeight;
                    const float NormControlWeight = CinematicControlInfluence / TotalWeight;
                    
                    FQuat ResultQuat = FQuat::Identity;
                    float TotalAppliedWeight = 0.0f;
                    
                    // Velocity component
                    if (NormVelWeight > 0.001f)
                    {
                        const FQuat VelQuat = Velocity.GetSafeNormal().Rotation().Quaternion();
                        if (TotalAppliedWeight > 0.001f)
                        {
                            ResultQuat = FQuat::Slerp(ResultQuat, VelQuat, NormVelWeight / (TotalAppliedWeight + NormVelWeight));
                        }
                        else
                        {
                            ResultQuat = VelQuat;
                        }
                        TotalAppliedWeight += NormVelWeight;
                    }
                    
                    // Acceleration component (predictive) with raw input consideration
                    if (NormAccelWeight > 0.001f)
                    {
                        FVector PredictiveAccel = Acceleration;
                        
                        // Use raw input for more responsive rotation if enabled
                        if (bUseRawInputForRotation && WorldIntent.SizeSquared() > 0.1f)
                        {
                            // Blend between actual acceleration and input-based acceleration
                            const float InputResponseBlend = FMath::Clamp(RotationInputResponseTime * 10.f, 0.f, 1.f);
                            PredictiveAccel = FMath::Lerp(Acceleration, WorldIntent * Acceleration.Size(), InputResponseBlend);
                        }
                        
                        if (PredictiveAccel.SizeSquared() > 1.0f)
                        {
                            // Predict future position based on acceleration
                            const FVector PredictedVel = Velocity + PredictiveAccel * CinematicLookAheadTime;
                            const FQuat AccelQuat = PredictedVel.GetSafeNormal().Rotation().Quaternion();
                            
                            if (TotalAppliedWeight > 0.001f)
                            {
                                ResultQuat = FQuat::Slerp(ResultQuat, AccelQuat, NormAccelWeight / (TotalAppliedWeight + NormAccelWeight));
                            }
                            else
                            {
                                ResultQuat = AccelQuat;
                            }
                            TotalAppliedWeight += NormAccelWeight;
                        }
                    }
                    
                    // Control component
                    if (NormControlWeight > 0.001f)
                    {
                        const FQuat ControlQuat = ControlRotation.Quaternion();
                        if (TotalAppliedWeight > 0.001f)
                        {
                            ResultQuat = FQuat::Slerp(ResultQuat, ControlQuat, NormControlWeight / (TotalAppliedWeight + NormControlWeight));
                        }
                        else
                        {
                            ResultQuat = ControlQuat;
                        }
                        TotalAppliedWeight += NormControlWeight;
                    }
                    
                    // Apply smoothing
                    const FQuat CurrentQuat = CurrentRotation.Quaternion();
                    ResultQuat = FQuat::Slerp(CurrentQuat, ResultQuat, 1.0f - CinematicSmoothingFactor);
                    
                    TargetRotation = ResultQuat.Rotator();
                    
                    // Check for direction changes and boost rotation rate
                    const float VelDot = FVector::DotProduct(
                        Velocity.GetSafeNormal(),
                        CurrentQuat.GetForwardVector()
                    );
                    const float DirectionChangeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(VelDot, -1.f, 1.f)));
                    
                    if (DirectionChangeAngle > CinematicTurnBoostThreshold)
                    {
                        EffectiveRate *= CinematicTurnBoostMultiplier;
                    }
                    
                    // Check for excessive lag and force recovery
                    const float AngleDiff = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw);
                    if (FMath::Abs(AngleDiff) > CinematicMaxLagAngle)
                    {
                        EffectiveRate = RecoveryRateDegrees;
                    }
                    
                    // Update blend alpha based on control influence
                    BlendAlpha = FMath::FInterpTo(BlendAlpha, NormControlWeight, DeltaTime, InterpSpeed);
                }
                else
                {
                    // Below minimum speed, face camera with movement alpha blend
                    TargetRotation = ControlRotation;
                    BlendAlpha = FMath::FInterpTo(BlendAlpha, 1.0f, DeltaTime, InterpSpeed);
                }
                break;
            }
        }
    }
    else
    {
        // When idle or nearly idle, face camera for all modes
        TargetRotation = ControlRotation;
        BlendAlpha = FMath::FInterpTo(BlendAlpha, 1.0f, DeltaTime, IdleInterpolationSpeed);
    }

    // Apply alpha correction
    ApplyAlphaCorrection(DeltaTime);

    // Prevent vertical rotation
    TargetRotation.Pitch = 0;
    TargetRotation.Roll = 0;

    // Apply rotation with rate limiting
    const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, TargetRotation, DeltaTime, EffectiveRate);
    Owner->SetActorRotation(NewRotation);

    // Update state tracking
    CurrentYaw = NewRotation.Yaw;
    TargetYaw = TargetRotation.Yaw;
    ActualAngularVelocity = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, NewRotation.Yaw) / DeltaTime;
}

void FRotationConfig::HandleModeTransition(AActor* Owner, float DeltaTime)
{
    if (!Owner) return;
    
    // Check if we're in a mode transition
    if (ModeTransitionAlpha >= 1.0f)
    {
        return; // No transition needed
    }
    
    // Update transition progress
    ModeTransitionAlpha = FMath::Min(1.0f, ModeTransitionAlpha + DeltaTime / RotationSmoothingTime);
    
    // During transition, blend between the starting rotation and current target
    if (ModeTransitionAlpha < 1.0f)
    {
        const FQuat TargetRotation = FRotator(0, TargetYaw, 0).Quaternion();
        
        // Smooth blend from start to target
        const FQuat BlendedRotation = FQuat::Slerp(
            ModeTransitionStartRotation, 
            TargetRotation, 
            FMath::SmoothStep(0.f, 1.f, ModeTransitionAlpha)
        );
        
        // Apply blended rotation
        Owner->SetActorRotation(BlendedRotation);
        
        // Update current yaw
        CurrentYaw = BlendedRotation.Rotator().Yaw;
    }
}

float FRotationConfig::GetCurrentRotationRate() const
{
    if (bUseCharacterMovementRotation)
    {
        return CMCRotationRateDegrees;
    }
    
    // Apply mode-specific modifier to base rate
    return BaseRotationRate * GetModeSpecificRate(ManualMode);
}

float FRotationConfig::GetModeSpecificRate(EManualRotationMode Mode) const
{
    switch (Mode)
    {
        case EManualRotationMode::Orbit:
            return OrbitRateModifier;
        case EManualRotationMode::Strafe:
            return StrafeRateModifier;
        case EManualRotationMode::Dynamic:
            return DynamicRateModifier;
        case EManualRotationMode::LookAt:
            return LookAtRateModifier;
        case EManualRotationMode::Cinematic:
            return CinematicRateModifier;
        default:
            return 1.0f;
    }
}

float FRotationConfig::GetPredictionRotationRate(bool bIsDirectionChange) const
{
    float Rate = GetCurrentRotationRate();
    
    // Apply prediction bias
    Rate *= PredictionBias;
    
    // Boost rate for direction changes
    if (bIsDirectionChange)
    {
        Rate *= 1.2f;
    }
    
    // Use recovery rate if lagging
    if (bIsLagging && RotationLag > 0.5f)
    {
        Rate = FMath::Max(Rate, RecoveryRateDegrees);
    }
    
    return Rate;
}

int32 FRotationConfig::GetCurrentModeAsInt() const
{
    if (bUseCharacterMovementRotation)
    {
        return static_cast<int32>(CMCMode);
    }
    else
    {
        return 10 + static_cast<int32>(ManualMode); // Offset manual modes
    }
}

FString FRotationConfig::GetCurrentModeDescription() const
{
    if (bUseCharacterMovementRotation)
    {
        return (CMCMode == ECMCRotationMode::ControlBased) 
            ? FString::Printf(TEXT("CMC Control (Rate: %.0f°/s)"), CMCRotationRateDegrees)
            : FString::Printf(TEXT("CMC Movement (Rate: %.0f°/s)"), CMCRotationRateDegrees);
    }
    
    const float Rate = GetCurrentRotationRate();
    
    switch (ManualMode)
    {
        case EManualRotationMode::Orbit:
            return FString::Printf(TEXT("Orbit (Rate: %.0f°/s) - Face Velocity"), Rate);
            
        case EManualRotationMode::Strafe:
            return FString::Printf(TEXT("Strafe (Rate: %.0f°/s) - Face Camera"), Rate);
            
        case EManualRotationMode::Dynamic:
            return FString::Printf(TEXT("Dynamic (Rate: %.0f°/s, Blend: %.2f) - Simple Linear"), 
                Rate, DynamicBlendWeight);
            
        case EManualRotationMode::LookAt:
            return FString::Printf(TEXT("LookAt (Rate: %.0f°/s) %s"), 
                Rate, bHasLookAtTarget ? TEXT("[ACTIVE]") : TEXT("[NO TARGET]"));
            
        case EManualRotationMode::Cinematic:
            return FString::Printf(TEXT("Cinematic (Rate: %.0f°/s) - Multi-Factor Intelligent"), Rate);
            
        default:
            return TEXT("Unknown");
    }
}

bool FRotationConfig::HasModeChanged() const
{
    return ModeTransitionAlpha < 1.0f;
}
// ============================================================================
// FMotionData Implementation - IMPROVED WITH CACHED ACCELERATION
// ============================================================================

FMotionData::FMotionData()
{
    WriteIndex = 0;
    NumValidSamples = 0;
    CurrentComplexity = EMotionComplexity::Simple;
    ComplexityScore = 0.f;
    CachedAnalysisFrame = 0;
    LastSmoothedWorldIntent = FVector::ZeroVector;
    LastSmoothedAcceleration = FVector::ZeroVector;
    bSmoothedStateInitialized = false;
    CurrentUrgency = 0.f;
    LastStateChangeTime = 0.f;
    LastStateChangeFrame = 0;
    CurrentStateStartTime = 0.f;
}

void FMotionData::Initialize(const FMotionBufferConfig& InConfig)
{
    MotionBufferConfig = InConfig;
    
    const int32 BufferSize = FMath::Clamp(MotionBufferConfig.BufferSize, 20, 60);
    
    Samples.Reset();
    Samples.SetNum(BufferSize);
    
    for (auto& Sample : Samples)
    {
        Sample = FMotionSample();
    }
    
    if (MotionBufferConfig.bTrackImportance)
    {
        ImportanceData.Reset();
        ImportanceData.SetNum(BufferSize);
        for (auto& Importance : ImportanceData)
        {
            Importance = FSampleImportance();
        }
    }
    else
    {
        ImportanceData.Empty();
    }
    
    WriteIndex = 0;
    NumValidSamples = 0;
    CachedAnalysisFrame = 0;
    CurrentComplexity = EMotionComplexity::Simple;
    ComplexityScore = 0.f;
    ResetSmoothingState();
}

void FMotionData::InitializeDefault(int32 DesiredSize)
{
    FMotionBufferConfig DefaultConfig;
    DefaultConfig.BufferSize = FMath::Clamp(DesiredSize, 20, 60);
    DefaultConfig.bTrackImportance = false;
    Initialize(DefaultConfig);
}

void FMotionData::ResetSmoothingState() const
{
    LastSmoothedWorldIntent = FVector::ZeroVector;
    LastSmoothedAcceleration = FVector::ZeroVector;
    bSmoothedStateInitialized = false;
    CurrentUrgency = 0.f;
}

void FMotionData::AddSample(const FMotionSample& Sample)
{
    if (Samples.Num() == 0)
    {
        InitializeDefault(30);
    }
    
    if (!Sample.IsValid())
    {
        return;
    }
    
    const int32 BufferSize = Samples.Num();
    
    FMotionSample PreviousSample;
    bool bHasPrevious = NumValidSamples > 0 && GetSampleByAge(0, PreviousSample);
    
    Samples[WriteIndex] = Sample;
    
    if (MotionBufferConfig.bTrackImportance && ImportanceData.Num() == BufferSize)
    {
        if (bHasPrevious)
        {
            CalculateImportance(Sample, PreviousSample, ImportanceData[WriteIndex]);
        }
        else
        {
            ImportanceData[WriteIndex] = FSampleImportance();
            ImportanceData[WriteIndex].bIsKeyFrame = true;
            ImportanceData[WriteIndex].ImportanceScore = 2.0f;
            ImportanceData[WriteIndex].TimeAdded = Sample.TimeStamp;
        }
    }
    
    if (bHasPrevious)
    {
        UpdateComplexity(Sample, PreviousSample);
    }
    
    WriteIndex = (WriteIndex + 1) % BufferSize;
    NumValidSamples = FMath::Min(NumValidSamples + 1, BufferSize);
    
    CachedAnalysisFrame = 0;
    
    if (NumValidSamples > 1)
    {
        FTrajectoryConfig DefaultConfig;
        UpdateAdaptiveSmoothing(Sample, DefaultConfig);
    }
}

bool FMotionData::GetSampleByAge(int32 Age, FMotionSample& OutSample) const
{
    if (Age < 0 || Age >= NumValidSamples || Samples.Num() == 0)
    {
        return false;
    }
    
    int32 Index = (WriteIndex - 1 - Age + Samples.Num()) % Samples.Num();
    
    if (!Samples.IsValidIndex(Index))
    {
        return false;
    }
    
    OutSample = Samples[Index];
    return OutSample.IsValid();
}

FMotionSample FMotionData::GetSampleAtOffset(float OffsetSeconds) const
{
    if (IsEmpty())
    {
        return FMotionSample();
    }
    
    if (FMath::IsNearlyZero(OffsetSeconds))
    {
        return GetNewestSample();
    }
    
    const FMotionSample NewestSample = GetNewestSample();
    if (!NewestSample.IsValid())
    {
        return FMotionSample();
    }
    
    const float TargetTime = NewestSample.TimeStamp + OffsetSeconds;
    
    if (FMath::Abs(OffsetSeconds) < 1.0f)
    {
        const float SampleRate = 60.f; 
        const int32 ApproxAge = FMath::RoundToInt(-OffsetSeconds * SampleRate);
        
        if (ApproxAge >= 0 && ApproxAge < NumValidSamples)
        {
            FMotionSample Sample;
            if (GetSampleByAge(ApproxAge, Sample) && Sample.IsValid())
            {
                const float TimeDiff = FMath::Abs(Sample.TimeStamp - TargetTime);
                if (TimeDiff < 0.02f) 
                {
                    return Sample;
                }
            }
        }
    }
    
    if (OffsetSeconds < 0)
    {
        for (int32 i = 0; i < NumValidSamples - 1; i++)
        {
            FMotionSample S0, S1;
            if (GetSampleByAge(i, S0) && GetSampleByAge(i + 1, S1))
            {
                if (!S0.IsValid() || !S1.IsValid())
                {
                    continue;
                }
                
                if (TargetTime <= S0.TimeStamp && TargetTime >= S1.TimeStamp)
                {
                    const float TimeDelta = S0.TimeStamp - S1.TimeStamp;
                    if (FMath::Abs(TimeDelta) > KINDA_SMALL_NUMBER)
                    {
                        const float Alpha = (S0.TimeStamp - TargetTime) / TimeDelta;
                        return InterpolateSamples(S0, S1, Alpha);
                    }
                    return S0;
                }
            }
        }
        
        return GetOldestSample();
    }
    else
    {
        return GetNewestSample();
    }
}

void FMotionData::CalculateImportance(const FMotionSample& Current, const FMotionSample& Previous, FSampleImportance& OutImportance)
{
    OutImportance = FSampleImportance();
    OutImportance.TimeAdded = Current.TimeStamp;
    OutImportance.ImportanceScore = 1.0f;
    
    const float VelDot = FVector::DotProduct(
        Current.WorldVelocity.GetSafeNormal(),
        Previous.WorldVelocity.GetSafeNormal()
    );
    OutImportance.DirectionChangeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(VelDot, -1.f, 1.f)));
    
    if (OutImportance.DirectionChangeAngle > 30.f)
    {
        OutImportance.bIsDirectionChange = true;
        OutImportance.ImportanceScore += 2.0f;
        
        if (OutImportance.DirectionChangeAngle > 90.f)
        {
            OutImportance.bIsKeyFrame = true;
            OutImportance.ImportanceScore += 1.0f;
        }
    }
    
    OutImportance.SpeedChange = Current.GetSpeed() - Previous.GetSpeed();
    if (FMath::Abs(OutImportance.SpeedChange) > 100.f)
    {
        OutImportance.bIsTransition = true;
        OutImportance.ImportanceScore += 1.5f;
    }
    
    if (Current.bIsOnGround != Previous.bIsOnGround)
    {
        OutImportance.bIsKeyFrame = true;
        OutImportance.ImportanceScore += 3.0f;
    }
    
    const float InputChange = FVector2D::Distance(Current.RawInput, Previous.RawInput);
    if (InputChange > 0.5f)
    {
        OutImportance.ImportanceScore += InputChange;
    }
}

void FMotionData::UpdateComplexity(const FMotionSample& Current, const FMotionSample& Previous) const
{
    float NewComplexityScore = 0.f;
    
    const float SpeedDiff = FMath::Abs(Current.GetSpeed() - Previous.GetSpeed());
    NewComplexityScore += FMath::GetMappedRangeValueClamped(
        FVector2D(0.f, 300.f),
        FVector2D(0.f, 1.f),
        SpeedDiff
    );
    
    const float DirDot = FVector::DotProduct(
        Current.WorldVelocity.GetSafeNormal(),
        Previous.WorldVelocity.GetSafeNormal()
    );
    NewComplexityScore += (1.0f - DirDot) * 2.0f;
    
    const float InputDiff = FVector2D::Distance(Current.RawInput, Previous.RawInput);
    NewComplexityScore += InputDiff * 1.5f;
    
    const float AngularDiff = (Current.AngularVelocityRadians - Previous.AngularVelocityRadians).Size();
    NewComplexityScore += FMath::GetMappedRangeValueClamped(
        FVector2D(0.f, PI),
        FVector2D(0.f, 1.f),
        AngularDiff
    );
    
    const float DeltaTime = FMath::Max(0.001f, Current.TimeStamp - Previous.TimeStamp);
    ComplexityScore = FMath::FInterpTo(ComplexityScore, NewComplexityScore, DeltaTime, 2.0f);
    
    if (ComplexityScore < 1.0f)
    {
        CurrentComplexity = EMotionComplexity::Simple;
    }
    else if (ComplexityScore < 2.5f)
    {
        CurrentComplexity = EMotionComplexity::Moderate;
    }
    else if (ComplexityScore < 4.0f)
    {
        CurrentComplexity = EMotionComplexity::Complex;
    }
    else
    {
        CurrentComplexity = EMotionComplexity::Critical;
    }
}

float FMotionData::GetSampleImportanceWeight(int32 Age) const
{
    if (!MotionBufferConfig.bTrackImportance || Age < 0 || Age >= NumValidSamples)
    {
        return 1.0f;
    }
    
    const int32 BufferSize = Samples.Num();
    const int32 Index = (WriteIndex - 1 - Age + BufferSize) % BufferSize;
    
    if (ImportanceData.IsValidIndex(Index))
    {
        return ImportanceData[Index].GetTrajectoryWeight();
    }
    
    return 1.0f;
}

float FMotionData::GetMemoryUsageKB() const
{
    float TotalBytes = 0.f;
    
    TotalBytes += Samples.Num() * sizeof(FMotionSample);
    
    if (MotionBufferConfig.bTrackImportance)
    {
        TotalBytes += ImportanceData.Num() * sizeof(FSampleImportance);
    }
    
    return TotalBytes / 1024.f;
}

FString FMotionData::GetDebugStats() const
{
    return FString::Printf(
        TEXT("Buffer: %d/%d samples (%.1fKB) | Complexity: %s (%.2f) | Urgency: %.2f"),
        NumValidSamples,
        Samples.Num(),
        GetMemoryUsageKB(),
        *UEnum::GetValueAsString(CurrentComplexity),
        ComplexityScore,
        CurrentUrgency
    );
}

FMotionSample FMotionData::GetNewestSample() const
{
    FMotionSample Sample;
    GetSampleByAge(0, Sample);
    return Sample;
}

FMotionSample FMotionData::GetOldestSample() const
{
    FMotionSample Sample;
    GetSampleByAge(NumValidSamples - 1, Sample);
    return Sample;
}

void FMotionData::Clear()
{
    WriteIndex = 0;
    NumValidSamples = 0;
    CachedAnalysisFrame = 0;
    
    const int32 BufferSize = Samples.Num();
    for (int32 i = 0; i < BufferSize; i++)
    {
        Samples[i] = FMotionSample();
        if (ImportanceData.Num() > i)
        {
            ImportanceData[i] = FSampleImportance();
        }
    }
    
    ResetSmoothingState();
    ComplexityScore = 0.f;
    CurrentComplexity = EMotionComplexity::Simple;
}

FMotionAnalysis FMotionData::AnalyzeMotion(const FTrajectoryConfig& Config) const
{
    FMotionAnalysis Analysis;
    
    if (IsEmpty())
    {
        return Analysis;
    }
    
    const FMotionSample Current = GetNewestSample();
    const float CurrentSpeed = Current.GetSpeed();
    
    DetectMotionStates(Analysis, Config);
    
    if (LastStateChangeFrame != GFrameCounter && CachedAnalysis.CurrentState != Analysis.CurrentState)
    {
        const_cast<FMotionData*>(this)->CurrentStateStartTime = Current.TimeStamp;
        const_cast<FMotionData*>(this)->LastStateChangeFrame = GFrameCounter;
    }
    Analysis.TimeInState = Current.TimeStamp - CurrentStateStartTime;
    
    if (CurrentSpeed > Config.StopSpeedThreshold)
    {
        // IMPROVED: Use cached acceleration instead of estimating
        const FVector Accel = Current.CachedAcceleration;
        const float AccelDotVel = FVector::DotProduct(
            Current.WorldVelocity.GetSafeNormal(),
            Accel.GetSafeNormal()
        );
        
        if (AccelDotVel < -0.3f && Accel.Size() > 50.f)
        {
            Analysis.PredictedStopTime = CurrentSpeed / Accel.Size();
            Analysis.PredictedStopDistance = 0.5f * CurrentSpeed * Analysis.PredictedStopTime;
        }
    }
    
    const_cast<FMotionData*>(this)->CachedAnalysis = Analysis;
    const_cast<FMotionData*>(this)->CachedAnalysisFrame = GFrameCounter;
    
    return Analysis;
}

FCharacterMotionContext FMotionData::GetCharacterContext(
    const FRotator& ControlRotation, 
    const FQuat& MeshRotation,
    const FTrajectoryConfig& Config) const
{
    FCharacterMotionContext Context;
    
    if (IsEmpty())
    {
        return Context;
    }
    
    const FMotionSample Current = GetNewestSample();
    
    Context.MeshRotation = MeshRotation;
    Context.ControlRotation = ControlRotation;
    Context.Speed = Current.GetSpeed();
    Context.Speed2D = Current.GetSpeed2D();
    
    Context.ControlToMeshAngleYaw = FMath::FindDeltaAngleDegrees(
        MeshRotation.Rotator().Yaw,
        ControlRotation.Yaw
    );
    
    if (Current.WorldVelocity.Size() > 10.f)
    {
        const float VelYaw = FMath::RadiansToDegrees(
            FMath::Atan2(Current.WorldVelocity.Y, Current.WorldVelocity.X)
        );
        Context.MeshToVelocityAngleYaw = FMath::FindDeltaAngleDegrees(
            MeshRotation.Rotator().Yaw,
            VelYaw
        );
    }
    
    Context.MovementDirection = CalculateMovementDirectionRelativeToControl(Current.WorldVelocity, ControlRotation);
    
    Context.bIsMoving = Current.GetSpeed() > Config.MovementSpeedThreshold;
    
    const float AngularVel = EstimateAngularAcceleration(0.05f);
    Context.bIsTurning = (FMath::Abs(AngularVel) > Config.TurnThresholdDegrees) || 
                         (FMath::Abs(Context.ControlToMeshAngleYaw) > Config.TurnThresholdDegrees && 
                          Current.GetSpeed() < Config.MovementSpeedThreshold);
    
    // IMPROVED: Use cached acceleration
    const FVector CurrentAccel = Current.CachedAcceleration;
    const float AccelDotVel = FVector::DotProduct(
        CurrentAccel.GetSafeNormal(),
        Current.WorldVelocity.GetSafeNormal()
    );
    
    Context.bIsAccelerating = (Current.RawInput.Size() > Config.MinInputMagnitude && AccelDotVel > 0.1f) ||
                              (CurrentAccel.Size() > 100.f && AccelDotVel > 0.5f);
    
    Context.bIsDecelerating = (Current.RawInput.Size() < Config.MinInputMagnitude && Current.GetSpeed() > Config.StopSpeedThreshold) ||
                              (AccelDotVel < -0.3f && CurrentAccel.Size() > 100.f);
    
    return Context;
}
// BuildMMTrajectory - Enhanced with all fixes
// .cpp - Complete BuildMMTrajectory Function

void FMotionData::BuildMMTrajectory(
    const TArray<float>& TimeOffsets,
    FTransformTrajectory& OutTrajectory,
    const FTrajectoryConfig& Config,
    const FCharacterMotionContext& CharContext,
    const FAnimationContext& AnimContext,
    const FEnvironmentContext& EnvContext,
    UCharacterMovementComponent* MovementComp,
    float DeltaSeconds,
    const FDynamicSpeedMatchConfig* SpeedConfig,
    const FDynamicSpeedAnalysis* SpeedAnalysis) const
{
    OutTrajectory.Samples.Reset();
    if (IsEmpty() || TimeOffsets.Num() == 0) return;
    
    const FMotionSample CurrentSample = GetNewestSample();
    const FRotationConfig& RotConfig = Config.RotationConfig;
    
    float MaxWalkSpeed = 400.f;
    float MaxAcceleration = 2048.f;
    float BrakingDeceleration = 2048.f;
    float GroundFriction = 8.f;
    float AirControl = 0.05f;
    float GravityZ = -980.f;
    float AirControlBoostMultiplier = 2.f;
    float AirControlBoostVelocityThreshold = 25.f;
    float FallingLateralFriction = 0.f;
    float MaxFloorAngle = 44.f;
    float BrakingFrictionFactor = 2.f;
    
    if (MovementComp)
    {
        MaxWalkSpeed = MovementComp->MaxWalkSpeed;
        MaxAcceleration = MovementComp->GetMaxAcceleration();
        BrakingDeceleration = MovementComp->GetMaxBrakingDeceleration();
        GroundFriction = MovementComp->GroundFriction;
        AirControl = MovementComp->AirControl;
        GravityZ = MovementComp->GetGravityZ();
        AirControlBoostMultiplier = MovementComp->AirControlBoostMultiplier;
        AirControlBoostVelocityThreshold = MovementComp->AirControlBoostVelocityThreshold;
        FallingLateralFriction = MovementComp->FallingLateralFriction;
        MaxFloorAngle = MovementComp->GetWalkableFloorAngle();
        BrakingFrictionFactor = MovementComp->BrakingFrictionFactor;
    }
    
    const bool bUseSpeedMatching = SpeedConfig && 
                                   SpeedAnalysis && 
                                   SpeedAnalysis->DetectedClusters.Num() > 0;
    
    FVector RawWorldIntent = FVector::ZeroVector;
    bool bHasDiagonalInput = false;
    float InputMagnitude = 0.f;
    
    if (CurrentSample.RawInput.Size() > Config.MinInputMagnitude)
    {
        const FRotator CameraRot(0, CharContext.ControlRotation.Yaw, 0);
        const FVector CameraForward = CameraRot.RotateVector(FVector::ForwardVector);
        const FVector CameraRight = CameraRot.RotateVector(FVector::RightVector);
        
        FVector2D NormalizedInput = CurrentSample.RawInput;
        InputMagnitude = NormalizedInput.Size();
        if (InputMagnitude > 1.0f)
        {
            NormalizedInput.Normalize();
            InputMagnitude = 1.0f;
        }
        
        RawWorldIntent = (CameraForward * NormalizedInput.Y + CameraRight * NormalizedInput.X);
        RawWorldIntent.Z = 0.f;
        RawWorldIntent.Normalize();
        
        bHasDiagonalInput = (FMath::Abs(NormalizedInput.X) > 0.4f && FMath::Abs(NormalizedInput.Y) > 0.4f);
    }
    
    if (RotConfig.bUseRawInputForRotation && RotConfig.RawInputForRotation.Size() > Config.MinInputMagnitude)
    {
        RawWorldIntent = RotConfig.CalculateWorldIntentFromRawInput(RotConfig.RawInputForRotation, CharContext.ControlRotation);
    }
    
    bool bShouldResetSmoothing = false;
    bool bIsDirectionChange = false;
    
    if (bSmoothedStateInitialized && !LastSmoothedWorldIntent.IsNearlyZero() && !RawWorldIntent.IsNearlyZero())
    {
        const float IntentChange = FVector::Distance(RawWorldIntent, LastSmoothedWorldIntent);
        const float DirectionChangeAngle = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(RawWorldIntent.GetSafeNormal(), LastSmoothedWorldIntent.GetSafeNormal()), -1.f, 1.f))
        );
        
        if (DirectionChangeAngle > Config.TurnThresholdDegrees)
        {
            bIsDirectionChange = true;
            if (DirectionChangeAngle > Config.PivotThresholdDegrees || IntentChange > Config.SignificantInputChange * 2.f)
            {
                bShouldResetSmoothing = true;
                const_cast<FMotionData*>(this)->ResetSmoothingState();
            }
        }
    }
    
    FVector WorldMovementIntent = RawWorldIntent;
    if (Config.bUseAdaptiveSmoothing && bSmoothedStateInitialized && !bShouldResetSmoothing)
    {
        float SmoothingRate = GetAdaptiveSmoothingRate(Config);
        
        if (RotConfig.bIsRotationRateLimited)
        {
            SmoothingRate *= 1.3f;
        }
        
        if (bIsDirectionChange || bHasDiagonalInput)
        {
            SmoothingRate = FMath::Min(SmoothingRate * 1.5f, Config.AdaptiveSmoothingMax);
        }
        
        WorldMovementIntent = FMath::VInterpTo(LastSmoothedWorldIntent, RawWorldIntent, DeltaSeconds, SmoothingRate);
    }
    
    const_cast<FMotionData*>(this)->LastSmoothedWorldIntent = WorldMovementIntent;
    const_cast<FMotionData*>(this)->bSmoothedStateInitialized = true;
    
    FVector TargetAcceleration = FVector::ZeroVector;
    if (CurrentSample.bIsOnGround)
    {
        if (WorldMovementIntent.Size() > Config.MinInputMagnitude)
        {
            FVector TargetVelocity = WorldMovementIntent * MaxWalkSpeed * InputMagnitude;
            
            if (FMath::Abs(EnvContext.GroundSlope) > 5.f && EnvContext.GroundSlope < MaxFloorAngle)
            {
                const float SlopeEffect = FMath::Sin(FMath::DegreesToRadians(EnvContext.GroundSlope));
                TargetVelocity.Z = SlopeEffect * MaxWalkSpeed * 0.5f;
            }
            
            FVector VelocityDiff = TargetVelocity - CurrentSample.WorldVelocity;
            
            const float DotProduct = FVector::DotProduct(CurrentSample.WorldVelocity.GetSafeNormal(), WorldMovementIntent);
            
            float AccelToUse = MaxAcceleration;
            
            if (RotConfig.bIsLagging && bIsDirectionChange)
            {
                AccelToUse *= 1.2f;
            }
            
            if (DotProduct < -0.1f)
            {
                AccelToUse = BrakingDeceleration;
            }
            
            TargetAcceleration = VelocityDiff.GetClampedToMaxSize(AccelToUse);
        }
        else if (CurrentSample.GetSpeed() > Config.StopSpeedThreshold)
        {
            TargetAcceleration = -CurrentSample.WorldVelocity.GetSafeNormal() * BrakingDeceleration;
        }
    }
    else
    {
        TargetAcceleration.Z = GravityZ;
        if (WorldMovementIntent.Size() > Config.MinInputMagnitude && AirControl > 0.f)
        {
            float EffectiveAirControl = AirControl;
            if (CurrentSample.WorldVelocity.Size2D() < AirControlBoostVelocityThreshold)
            {
                EffectiveAirControl *= AirControlBoostMultiplier;
            }
            
            FVector AirAccel = WorldMovementIntent * MaxAcceleration * EffectiveAirControl;
            AirAccel.Z = 0.f;
            
            if (FallingLateralFriction > 0.f)
            {
                const FVector LateralVel = FVector(CurrentSample.WorldVelocity.X, CurrentSample.WorldVelocity.Y, 0.f);
                const FVector LateralFriction = -LateralVel.GetSafeNormal() * FallingLateralFriction;
                AirAccel += LateralFriction;
            }
            
            TargetAcceleration += AirAccel;
        }
    }
    
    FVector CurrentAcceleration = TargetAcceleration;
    if (bSmoothedStateInitialized && !bShouldResetSmoothing)
    {
        float AccelSmoothingRate = Config.AccelerationSmoothingRate;
        
        if (RotConfig.bIsRotationRateLimited && bIsDirectionChange)
        {
            AccelSmoothingRate *= 1.5f;
        }
        else if (bIsDirectionChange)
        {
            AccelSmoothingRate = FMath::Min(AccelSmoothingRate * 1.3f, 50.f);
        }
        
        CurrentAcceleration = FMath::VInterpTo(LastSmoothedAcceleration, TargetAcceleration, DeltaSeconds, AccelSmoothingRate);
    }
    const_cast<FMotionData*>(this)->LastSmoothedAcceleration = CurrentAcceleration;
    
    FVector LastPosition = CurrentSample.WorldPosition;
    FVector LastVelocity = CurrentSample.WorldVelocity;
    FVector LastAcceleration = CurrentAcceleration;
    FQuat LastRotation = CharContext.MeshRotation;
    float LastAngularVel = RotConfig.ActualAngularVelocity;
    float AccumulatedTime = 0.f;
    
    float PredictedAlpha = CurrentSample.RotationAlpha;
    float PredictedTargetYaw = RotConfig.TargetYaw;
    FVector PredictedLookAtTarget = RotConfig.LookAtTarget;
    float PredictedMovementAlpha = RotConfig.MovementAlpha;
    
    for (int32 i = 0; i < TimeOffsets.Num(); i++)
    {
        const float T = TimeOffsets[i];
        FTransformTrajectorySample& Sample = OutTrajectory.Samples.AddDefaulted_GetRef();
        Sample.TimeInSeconds = T;
        
        if (T <= 0.f)
        {
            const FMotionSample HistSample = GetSampleAtOffset(T);
            Sample.Position = HistSample.WorldPosition;
            Sample.Facing = HistSample.WorldRotation;
            
            if (i == 0 || (i > 0 && TimeOffsets[i-1] <= 0.f && T > TimeOffsets[i-1]))
            {
                LastPosition = HistSample.WorldPosition;
                LastVelocity = HistSample.WorldVelocity;
                LastRotation = HistSample.WorldRotation;
                LastAcceleration = HistSample.CachedAcceleration;
                LastAngularVel = EstimateAngularAcceleration(0.05f);
                PredictedAlpha = HistSample.RotationAlpha;
            }
        }
        else
        {
            const float DeltaToPredict = T - AccumulatedTime;
            if (DeltaToPredict <= KINDA_SMALL_NUMBER) continue;
            
            if (Config.bAccountForRootMotion && AnimContext.bHasRootMotion && AnimContext.RootMotionVelocity.Size() > 0.1f)
            {
                Sample.Position = LastPosition + AnimContext.RootMotionVelocity * DeltaToPredict;
                
                if (!AnimContext.RootMotionRotation.IsIdentity())
                {
                    Sample.Facing = AnimContext.RootMotionRotation * LastRotation;
                }
                else
                {
                    Sample.Facing = LastRotation;
                }
                
                LastPosition = Sample.Position;
                LastRotation = Sample.Facing;
                AccumulatedTime = T;
                continue;
            }
            
            const int32 NumSubSteps = FMath::Max(1, FMath::CeilToInt(DeltaToPredict / Config.PredictionSubstepTime));
            const float SubStepDelta = DeltaToPredict / NumSubSteps;
            
            FVector PredPos = LastPosition;
            FVector PredVel = LastVelocity;
            FVector PredAccel = LastAcceleration;
            FQuat PredRot = LastRotation;
            float PredAngularVel = LastAngularVel;
            
            for (int32 Step = 0; Step < NumSubSteps; Step++)
            {
                if (WorldMovementIntent.Size() > Config.MinInputMagnitude)
                {
                    FVector StepTargetVel = WorldMovementIntent * MaxWalkSpeed * InputMagnitude;
                    
                    if (Config.TrajectoryDivergence > 0.f)
                    {
                        const float NoiseScale = Config.TrajectoryDivergence;
                        const float TimeNoise = (T + Step * SubStepDelta) * PI * 2.f;
                        StepTargetVel += FVector(FMath::Sin(TimeNoise) * NoiseScale, FMath::Cos(TimeNoise) * NoiseScale, 0.f);
                    }
                    
                    FVector StepAccelDirection = (StepTargetVel - PredVel).GetSafeNormal();
                    float StepAccelMagnitude = MaxAcceleration;
                    
                    if (FVector::DotProduct(PredVel.GetSafeNormal(), WorldMovementIntent) < -0.1f)
                    {
                        StepAccelMagnitude = BrakingDeceleration;
                    }
                    
                    FVector StepTargetAccel = StepAccelDirection * StepAccelMagnitude;
                    
                    PredAccel = FMath::VInterpTo(PredAccel, StepTargetAccel, SubStepDelta, Config.PredictionSmoothingRate);
                    
                    PredVel += PredAccel * SubStepDelta;
                    
                    if (CurrentSample.bIsOnGround)
                    {
                        float EffectiveFriction = GroundFriction;
                        if (PredVel.Size() > 10.f && WorldMovementIntent.Size() < Config.MinInputMagnitude)
                        {
                            EffectiveFriction *= BrakingFrictionFactor;
                        }
                        
                        const float FrictionMultiplier = FMath::Max(0.f, 1.f - EffectiveFriction * SubStepDelta);
                        PredVel *= FrictionMultiplier;
                    }
                    
                    if (PredVel.Size() > MaxWalkSpeed)
                    {
                        PredVel = PredVel.GetClampedToMaxSize(MaxWalkSpeed);
                    }
                }
                else
                {
                    if (PredVel.Size() > Config.StopSpeedThreshold)
                    {
                        FVector StepTargetAccel = -PredVel.GetSafeNormal() * BrakingDeceleration;
                        PredAccel = FMath::VInterpTo(PredAccel, StepTargetAccel, SubStepDelta, Config.PredictionSmoothingRate * 1.5f);
                        
                        PredVel += PredAccel * SubStepDelta;
                        
                        const float StoppingFriction = GroundFriction * BrakingFrictionFactor;
                        const float FrictionMultiplier = FMath::Max(0.f, 1.f - StoppingFriction * SubStepDelta);
                        PredVel *= FrictionMultiplier;
                        
                        if (PredVel.Size() < Config.StopSpeedThreshold)
                        {
                            PredVel = FVector::ZeroVector;
                            PredAccel = FVector::ZeroVector;
                        }
                    }
                    else
                    {
                        PredVel = FVector::ZeroVector;
                        PredAccel = FVector::ZeroVector;
                    }
                }
                
                PredPos += PredVel * SubStepDelta;
                
                const float PredSpeed = PredVel.Size();
                float TargetMovementAlpha = FMath::GetRangePct(0.0f, RotConfig.MovementThreshold * 2.0f, PredSpeed);
                TargetMovementAlpha = FMath::SmoothStep(0.0f, 1.0f, TargetMovementAlpha);
                PredictedMovementAlpha = FMath::FInterpTo(PredictedMovementAlpha, TargetMovementAlpha, SubStepDelta, 5.0f);
                
                const FRotator CurrentRot = PredRot.Rotator();
                FRotator TargetRot = CurrentRot;
                float EffectiveRotationRate = RotConfig.GetPredictionRotationRate(bIsDirectionChange);
                bool bShouldRotate = false;
                
                if (RotConfig.bUseCharacterMovementRotation)
                {
                    EffectiveRotationRate = RotConfig.CMCRotationRateDegrees;
                    
                    switch (RotConfig.CMCMode)
                    {
                        case ECMCRotationMode::MovementBased:
                            if (PredVel.Size() > Config.MovementSpeedThreshold)
                            {
                                const FVector VelDir = PredVel.GetSafeNormal();
                                if (!VelDir.IsNearlyZero())
                                {
                                    TargetRot = VelDir.Rotation();
                                    bShouldRotate = true;
                                }
                            }
                            break;
                            
                        case ECMCRotationMode::ControlBased:
                            TargetRot = FRotator(0, PredictedTargetYaw, 0);
                            bShouldRotate = true;
                            PredictedTargetYaw += RotConfig.DesiredAngularVelocity * SubStepDelta * 0.3f;
                            break;
                    }
                }
                else if (PredictedMovementAlpha > 0.01f && PredVel.SizeSquared() > 1.0f)
                {
                    switch (RotConfig.ManualMode)
                    {
                        case EManualRotationMode::Orbit:
                            TargetRot = PredVel.GetSafeNormal().Rotation();
                            PredictedAlpha = 0.0f;
                            bShouldRotate = true;
                            break;
                            
                        case EManualRotationMode::Strafe:
                            TargetRot = FRotator(0, PredictedTargetYaw, 0);
                            PredictedAlpha = 1.0f;
                            bShouldRotate = true;
                            break;
                            
                        case EManualRotationMode::Dynamic:
                        {
                            const FQuat OrbitTarget = PredVel.GetSafeNormal().Rotation().Quaternion();
                            const FQuat StrafeTarget = FRotator(0, PredictedTargetYaw, 0).Quaternion();
                            
                            const float EffectiveBlendWeight = FMath::Lerp(1.0f, RotConfig.DynamicBlendWeight, PredictedMovementAlpha);
                            const FQuat BlendedTarget = FQuat::Slerp(OrbitTarget, StrafeTarget, EffectiveBlendWeight);
                            TargetRot = BlendedTarget.Rotator();
                            PredictedAlpha = EffectiveBlendWeight;
                            bShouldRotate = true;
                            break;
                        }
                            
                        case EManualRotationMode::LookAt:
                            if (RotConfig.bHasLookAtTarget)
                            {
                                const FVector ToTarget = PredictedLookAtTarget - PredPos;
                                if (ToTarget.SizeSquared() > FMath::Square(RotConfig.LookAtDeadZone))
                                {
                                    TargetRot = ToTarget.Rotation();
                                    TargetRot.Pitch = 0.f;
                                    TargetRot.Roll = 0.f;
                                    EffectiveRotationRate = RotConfig.BaseRotationRate * RotConfig.LookAtRateModifier;
                                    bShouldRotate = true;
                                }
                            }
                            else
                            {
                                TargetRot = FRotator(0, PredictedTargetYaw, 0);
                                bShouldRotate = true;
                            }
                            PredictedAlpha = 1.0f;
                            break;
                            
                        case EManualRotationMode::Cinematic:
                        {
                            const float TotalWeight = RotConfig.CinematicAccelerationInfluence + 
                                                      RotConfig.CinematicVelocityInfluence + 
                                                      RotConfig.CinematicControlInfluence;
                            
                            if (PredVel.Size() > RotConfig.CinematicMinSpeed && TotalWeight > 0.001f)
                            {
                                const float NormAccelWeight = RotConfig.CinematicAccelerationInfluence / TotalWeight;
                                const float NormVelWeight = RotConfig.CinematicVelocityInfluence / TotalWeight;
                                const float NormControlWeight = RotConfig.CinematicControlInfluence / TotalWeight;
                                
                                FQuat ResultQuat = FQuat::Identity;
                                float TotalAppliedWeight = 0.0f;
                                
                                if (NormVelWeight > 0.001f)
                                {
                                    const FQuat VelQuat = PredVel.GetSafeNormal().Rotation().Quaternion();
                                    if (TotalAppliedWeight > 0.001f)
                                    {
                                        ResultQuat = FQuat::Slerp(ResultQuat, VelQuat, NormVelWeight / (TotalAppliedWeight + NormVelWeight));
                                    }
                                    else
                                    {
                                        ResultQuat = VelQuat;
                                    }
                                    TotalAppliedWeight += NormVelWeight;
                                }
                                
                                if (NormAccelWeight > 0.001f && PredAccel.SizeSquared() > 1.0f)
                                {
                                    const FVector PredictedVel = PredVel + PredAccel * RotConfig.CinematicLookAheadTime;
                                    const FQuat AccelQuat = PredictedVel.GetSafeNormal().Rotation().Quaternion();
                                    if (TotalAppliedWeight > 0.001f)
                                    {
                                        ResultQuat = FQuat::Slerp(ResultQuat, AccelQuat, NormAccelWeight / (TotalAppliedWeight + NormAccelWeight));
                                    }
                                    else
                                    {
                                        ResultQuat = AccelQuat;
                                    }
                                    TotalAppliedWeight += NormAccelWeight;
                                }
                                
                                if (NormControlWeight > 0.001f)
                                {
                                    const FQuat ControlQuat = FRotator(0, PredictedTargetYaw, 0).Quaternion();
                                    if (TotalAppliedWeight > 0.001f)
                                    {
                                        ResultQuat = FQuat::Slerp(ResultQuat, ControlQuat, NormControlWeight / (TotalAppliedWeight + NormControlWeight));
                                    }
                                    else
                                    {
                                        ResultQuat = ControlQuat;
                                    }
                                    TotalAppliedWeight += NormControlWeight;
                                }
                                
                                ResultQuat = FQuat::Slerp(PredRot, ResultQuat, 1.0f - RotConfig.CinematicSmoothingFactor);
                                TargetRot = ResultQuat.Rotator();
                                
                                const float VelDot = FVector::DotProduct(PredVel.GetSafeNormal(), PredRot.GetForwardVector());
                                const float DirectionChangeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(VelDot, -1.f, 1.f)));
                                
                                if (DirectionChangeAngle > RotConfig.CinematicTurnBoostThreshold)
                                {
                                    EffectiveRotationRate *= RotConfig.CinematicTurnBoostMultiplier;
                                }
                                
                                const float AngleDiff = FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, TargetRot.Yaw);
                                if (FMath::Abs(AngleDiff) > RotConfig.CinematicMaxLagAngle)
                                {
                                    EffectiveRotationRate = RotConfig.RecoveryRateDegrees;
                                }
                                
                                PredictedAlpha = NormControlWeight;
                                bShouldRotate = true;
                            }
                            else
                            {
                                TargetRot = FRotator(0, PredictedTargetYaw, 0);
                                PredictedAlpha = 1.0f;
                                bShouldRotate = true;
                            }
                            break;
                        }
                    }
                }
                else
                {
                    TargetRot = FRotator(0, PredictedTargetYaw, 0);
                    PredictedAlpha = 1.0f;
                    bShouldRotate = true;
                }
                
                if (bShouldRotate)
                {
                    EffectiveRotationRate = RotConfig.GetPredictionRotationRate(bIsDirectionChange);
                    
                    if (FMath::Abs(RotConfig.AlphaErrorAccumulator) > 0.01f)
                    {
                        PredictedAlpha = FMath::Clamp(PredictedAlpha + RotConfig.AlphaErrorAccumulator * 0.3f, 0.0f, 1.0f);
                    }
                    
                    const float MaxRotationDelta = EffectiveRotationRate * SubStepDelta;
                    const FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, SubStepDelta, MaxRotationDelta);
                    
                    PredRot = NewRot.Quaternion();
                    
                    const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, NewRot.Yaw);
                    PredAngularVel = YawDelta / SubStepDelta;
                }
                else
                {
                    PredAngularVel = 0.f;
                }
            }
            
            if (bUseSpeedMatching && T > 0.f)
            {
                FVector SegmentDelta = PredPos - LastPosition;
                float SegmentTime = T - AccumulatedTime;
                float ImpliedSpeed = SegmentDelta.Size() / FMath::Max(0.001f, SegmentTime);
    
                ApplySpeedMatchingToSegment(
                    PredPos,
                    LastPosition,
                    ImpliedSpeed,
                    T,
                    *SpeedConfig,
                    SpeedAnalysis->DetectedClusters
                );
            }
            
            Sample.Position = PredPos;
            Sample.Facing = PredRot;
            
            if (Config.bUseEnvironmentAwareness)
            {
                ApplyEnvironmentalAdjustments(Sample, PredVel, T, EnvContext, BrakingDeceleration, LastPosition);
            }
            
            LastPosition = Sample.Position;
            LastVelocity = PredVel;
            LastAcceleration = PredAccel;
            LastRotation = Sample.Facing;
            LastAngularVel = PredAngularVel;
            AccumulatedTime = T;
        }
    }
    if (bUseSpeedMatching && SpeedAnalysis && SpeedAnalysis->CurrentTargetSpeed > SpeedConfig->MinSpeedThreshold) // Use SpeedConfig here!
    {
        // Find the current sample (T=0)
        int32 CurrentSampleIdx = INDEX_NONE;
        for (int32 i = 0; i < OutTrajectory.Samples.Num(); i++)
        {
            if (FMath::IsNearlyZero(OutTrajectory.Samples[i].TimeInSeconds))
            {
                CurrentSampleIdx = i;
                break;
            }
        }
    
        if (CurrentSampleIdx != INDEX_NONE && CurrentSampleIdx < OutTrajectory.Samples.Num() - 1)
        {
            // Calculate average implied speed of FUTURE trajectory samples
            float TotalImpliedSpeed = 0.f;
            int32 SampleCount = 0;
        
            for (int32 i = CurrentSampleIdx + 1; i < OutTrajectory.Samples.Num(); i++)
            {
                const FTransformTrajectorySample& Curr = OutTrajectory.Samples[i];
                const FTransformTrajectorySample& Prev = OutTrajectory.Samples[i - 1];
            
                float DeltaTime = Curr.TimeInSeconds - Prev.TimeInSeconds;
                if (DeltaTime > 0.001f)
                {
                    float ImpliedSpeed = FVector::Distance(Curr.Position, Prev.Position) / DeltaTime;
                    TotalImpliedSpeed += ImpliedSpeed;
                    SampleCount++;
                }
            }
        
            if (SampleCount > 0)
            {
                float AvgImpliedSpeed = TotalImpliedSpeed / SampleCount;
            
                // Calculate scaling factor to match target cluster speed
                float ScaleFactor = SpeedAnalysis->CurrentTargetSpeed / AvgImpliedSpeed;
                ScaleFactor = FMath::Clamp(ScaleFactor, SpeedConfig->MinSpeedRatio, SpeedConfig->MaxSpeedRatio); // Use SpeedConfig!
            
                // Apply strength - full strength recommended for cluster-based matching
                float FinalScale = FMath::Lerp(1.0f, ScaleFactor, SpeedConfig->SpeedMatchStrength); // Use SpeedConfig!
            
                // Only apply if scaling is meaningful
                if (!FMath::IsNearlyEqual(FinalScale, 1.0f, 0.01f))
                {
                    const FVector CurrentPos = OutTrajectory.Samples[CurrentSampleIdx].Position;
                
                    // Scale all FUTURE samples uniformly from current position
                    for (int32 i = CurrentSampleIdx + 1; i < OutTrajectory.Samples.Num(); i++)
                    {
                        FTransformTrajectorySample& Sample = OutTrajectory.Samples[i];
                        FVector Offset = Sample.Position - CurrentPos;
                        Sample.Position = CurrentPos + (Offset * FinalScale);
                    }
                
                    if (SpeedConfig->bDebugLog) // Use SpeedConfig!
                    {
                        static int32 PostScaleLogCounter = 0;
                        if (++PostScaleLogCounter % 300 == 0)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("POST-BUILD SCALE: AvgImplied=%.1f, Target=%.1f, Scale=%.2f"),
                                AvgImpliedSpeed, SpeedAnalysis->CurrentTargetSpeed, FinalScale);
                        }
                    }
                }
            }
        }
    }
    // ============================================================================
    // END POST-BUILD SCALING
    // ============================================================================
    
#if WITH_EDITOR
    if (bUseSpeedMatching && OutTrajectory.Samples.Num() > 1 && SpeedConfig->bDebugLog)
    {
        static int32 ValidationCounter = 0;
        if (++ValidationCounter % 300 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("=== Trajectory Speed Validation ==="));
            UE_LOG(LogTemp, Warning, TEXT("Current Cluster: %d, Target Speed: %.1f cm/s"),
                SpeedAnalysis->CurrentClusterIndex, SpeedAnalysis->CurrentTargetSpeed);
                
            for (int32 i = 1; i < OutTrajectory.Samples.Num(); i++)
            {
                const FTransformTrajectorySample& Curr = OutTrajectory.Samples[i];
                const FTransformTrajectorySample& Prev = OutTrajectory.Samples[i-1];
                
                float DeltaTime = Curr.TimeInSeconds - Prev.TimeInSeconds;
                if (DeltaTime > 0.001f)
                {
                    float ImpliedSpeed = FVector::Distance(Curr.Position, Prev.Position) / DeltaTime;
                    
                    bool bInValidRange = false;
                    int32 MatchingCluster = -1;
                    for (int32 ClusterIdx = 0; ClusterIdx < SpeedAnalysis->DetectedClusters.Num(); ClusterIdx++)
                    {
                        if (SpeedAnalysis->DetectedClusters[ClusterIdx].Contains(ImpliedSpeed))
                        {
                            bInValidRange = true;
                            MatchingCluster = ClusterIdx;
                            break;
                        }
                    }
                    
                    UE_LOG(LogTemp, Warning, TEXT("  Sample %d (T=%.2f): Speed=%.1f %s (Cluster %d)"),
                        i, Curr.TimeInSeconds, ImpliedSpeed,
                        bInValidRange ? TEXT("✓") : TEXT("✗"),
                        MatchingCluster);
                }
            }
        }
    }
#endif
}



FVector FMotionData::EstimateAcceleration(float LookbackTime) const
{
    if (GetSampleCount() < 2)
    {
        return FVector::ZeroVector;
    }
    
    // IMPROVED: Try to use cached acceleration first
    const FMotionSample Now = GetNewestSample();
    if (Now.CachedAcceleration.SizeSquared() > 0.1f)
    {
        return Now.CachedAcceleration;
    }
    
    // Fallback to estimation
    const FMotionSample Then = GetSampleAtOffset(-FMath::Abs(LookbackTime));
    
    const float DeltaTime = Now.TimeStamp - Then.TimeStamp;
    if (DeltaTime < 0.001f)
    {
        return FVector::ZeroVector;
    }
    
    return (Now.WorldVelocity - Then.WorldVelocity) / DeltaTime;
}

float FMotionData::EstimateAngularAcceleration(float LookbackTime) const
{
    if (GetSampleCount() < 2)
    {
        return 0.f;
    }
    
    const FMotionSample Now = GetNewestSample();
    const FMotionSample Then = GetSampleAtOffset(-FMath::Abs(LookbackTime));
    
    const float DeltaTime = Now.TimeStamp - Then.TimeStamp;
    if (DeltaTime < 0.001f)
    {
        return 0.f;
    }
    
    const float NowYaw = Now.WorldRotation.Rotator().Yaw;
    const float ThenYaw = Then.WorldRotation.Rotator().Yaw;
    const float YawDelta = FMath::FindDeltaAngleDegrees(ThenYaw, NowYaw);
    
    return YawDelta / DeltaTime;
}

float FMotionData::EstimateConstrainedAngularVelocity(float LookbackTime, float MaxRotationRate) const
{
    if (GetSampleCount() < 2)
    {
        return 0.f;
    }
    
    const FMotionSample Now = GetNewestSample();
    const FMotionSample Then = GetSampleAtOffset(-FMath::Abs(LookbackTime));
    
    const float DeltaTime = Now.TimeStamp - Then.TimeStamp;
    if (DeltaTime < 0.001f)
    {
        return 0.f;
    }
    
    const float NowYaw = Now.WorldRotation.Rotator().Yaw;
    const float ThenYaw = Then.WorldRotation.Rotator().Yaw;
    const float YawDelta = FMath::FindDeltaAngleDegrees(ThenYaw, NowYaw);
    
    float AngularVel = YawDelta / DeltaTime;
    
    if (MaxRotationRate > 0.f)
    {
        AngularVel = FMath::Clamp(AngularVel, -MaxRotationRate, MaxRotationRate);
    }
    
    return AngularVel;
}

EMovementDirection FMotionData::CalculateMovementDirectionRelativeToControl(const FVector& Velocity, const FRotator& ControlRotation)
{
    if (Velocity.Size() < 5.f)
    {
        return EMovementDirection::Forward;
    }
    
    const FRotator ControlYawOnly(0, ControlRotation.Yaw, 0);
    const FVector ControlForward = ControlYawOnly.RotateVector(FVector::ForwardVector);
    const FVector ControlRight = ControlYawOnly.RotateVector(FVector::RightVector);
    const FVector VelNorm = Velocity.GetSafeNormal();
    
    const float ForwardDot = FVector::DotProduct(VelNorm, ControlForward);
    const float RightDot = FVector::DotProduct(VelNorm, ControlRight);
    
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
    
    if (AngleDeg > -22.5f && AngleDeg <= 22.5f) 
        return EMovementDirection::Forward;
    else if (AngleDeg > 22.5f && AngleDeg <= 67.5f) 
        return EMovementDirection::ForwardRight;
    else if (AngleDeg > 67.5f && AngleDeg <= 112.5f) 
        return EMovementDirection::Right;
    else if (AngleDeg > 112.5f && AngleDeg <= 157.5f) 
        return EMovementDirection::BackwardRight;
    else if (FMath::Abs(AngleDeg) > 157.5f) 
        return EMovementDirection::Backward;
    else if (AngleDeg > -157.5f && AngleDeg <= -112.5f) 
        return EMovementDirection::BackwardLeft;
    else if (AngleDeg > -112.5f && AngleDeg <= -67.5f) 
        return EMovementDirection::Left;
    
    return EMovementDirection::ForwardLeft;
}

EMovementDirection FMotionData::CalculateMovementDirection(const FVector& Velocity, const FQuat& MeshRotation)
{
    if (Velocity.Size() < 5.f)
    {
        return EMovementDirection::Forward;
    }
    
    const FVector MeshForward = MeshRotation.GetForwardVector();
    const FVector MeshRight = MeshRotation.GetRightVector();
    const FVector VelNorm = Velocity.GetSafeNormal();
    
    const float ForwardDot = FVector::DotProduct(VelNorm, MeshForward);
    const float RightDot = FVector::DotProduct(VelNorm, MeshRight);
    
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
    
    if (AngleDeg > -22.5f && AngleDeg <= 22.5f) 
        return EMovementDirection::Forward;
    else if (AngleDeg > 22.5f && AngleDeg <= 67.5f) 
        return EMovementDirection::ForwardRight;
    else if (AngleDeg > 67.5f && AngleDeg <= 112.5f) 
        return EMovementDirection::Right;
    else if (AngleDeg > 112.5f && AngleDeg <= 157.5f) 
        return EMovementDirection::BackwardRight;
    else if (FMath::Abs(AngleDeg) > 157.5f) 
        return EMovementDirection::Backward;
    else if (AngleDeg > -157.5f && AngleDeg <= -112.5f) 
        return EMovementDirection::BackwardLeft;
    else if (AngleDeg > -112.5f && AngleDeg <= -67.5f) 
        return EMovementDirection::Left;
    
    return EMovementDirection::ForwardLeft;
}

void FMotionData::UpdateAdaptiveSmoothing(const FMotionSample& Current, const FTrajectoryConfig& Config) const
{
    if (GetSampleCount() < 2) 
    {
        const_cast<FMotionData*>(this)->CurrentUrgency = 0.f;
        return;
    }
    
    FMotionSample Previous;
    if (!GetSampleByAge(1, Previous))
    {
        Previous = Current;
    }
    
    const float VelocityChange = FVector::Distance(Current.WorldVelocity, Previous.WorldVelocity);
    const float InputChange = FVector2D::Distance(Current.RawInput, Previous.RawInput);
    const float AngularChange = FMath::Abs(EstimateAngularAcceleration(0.05f));
    const float AccelerationMag = Current.CachedAcceleration.Size();  // Use cached acceleration
    
    bool bIsDiagonalChange = false;
    if (!Previous.RawInput.IsNearlyZero() && !Current.RawInput.IsNearlyZero())
    {
        const float InputAngleChange = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(
                FVector2D::DotProduct(Previous.RawInput.GetSafeNormal(), Current.RawInput.GetSafeNormal()),
                -1.f, 1.f
            ))
        );
        
        if (InputAngleChange > 25.f && InputAngleChange < 65.f)
        {
            bIsDiagonalChange = true;
        }
    }
    
    const float VelocityUrgency = FMath::Clamp(VelocityChange / 200.f, 0.f, 1.f);
    const float InputUrgency = FMath::Clamp(InputChange / Config.SignificantInputChange, 0.f, 1.f);
    const float AngularUrgency = FMath::Clamp(AngularChange / Config.PivotAngularVelocityDeg, 0.f, 1.f);
    const float AccelUrgency = FMath::Clamp(AccelerationMag / 1000.f, 0.f, 1.f);
    
    float RawUrgency = (VelocityUrgency * 0.15f + 
                       InputUrgency * 0.4f + 
                       AngularUrgency * 0.2f + 
                       AccelUrgency * 0.25f);
    
    if (bIsDiagonalChange)
    {
        RawUrgency = FMath::Max(RawUrgency, 0.8f);
    }
    else if (InputChange > 0.5f || AngularChange > 90.f)
    {
        RawUrgency = FMath::Max(RawUrgency, 0.7f);
    }
    
    if (RawUrgency < Config.UrgencyThreshold && !bIsDiagonalChange)
    {
        RawUrgency *= 0.5f;
    }
    
    const float InterpSpeed = bIsDiagonalChange ? 20.f : 10.f;
    const float CurrentUrg = const_cast<const FMotionData*>(this)->CurrentUrgency;
    const float NewUrgency = FMath::FInterpTo(CurrentUrg, RawUrgency, 0.016f, InterpSpeed);
    
    const_cast<FMotionData*>(this)->CurrentUrgency = FMath::Clamp(NewUrgency, 0.f, 1.f);
}

float FMotionData::GetAdaptiveSmoothingRate(const FTrajectoryConfig& Config) const
{
    if (!Config.bUseAdaptiveSmoothing)
    {
        return Config.InputSmoothingRate;
    }
    
    return FMath::Lerp(
        Config.AdaptiveSmoothingMin,
        Config.AdaptiveSmoothingMax,
        CurrentUrgency
    );
}


void FMotionData::ApplySpeedMatchingToSegment(
    FVector& InOutPosition,
    const FVector& LastPosition,
    float ImpliedSpeed,
    float TimeOffset,
    const FDynamicSpeedMatchConfig& Config,
    const TArray<FSpeedCluster>& SpeedClusters)
{
    if (SpeedClusters.Num() == 0)
    {
        return;
    }
    
    float TargetAnimSpeed = FindBestDatabaseSpeed(ImpliedSpeed, SpeedClusters);
    
    if (ImpliedSpeed > Config.MinSpeedThreshold && TargetAnimSpeed > Config.MinSpeedThreshold)
    {
        float SpeedRatio = TargetAnimSpeed / ImpliedSpeed;
        
        SpeedRatio = FMath::Clamp(SpeedRatio, Config.MinSpeedRatio, Config.MaxSpeedRatio);
        
        float FinalRatio = FMath::Lerp(1.0f, SpeedRatio, Config.SpeedMatchStrength);
        
        // Scale from LastPosition (previous trajectory sample)
        FVector Delta = InOutPosition - LastPosition;
        FVector ScaledDelta = Delta * FinalRatio;
        InOutPosition = LastPosition + ScaledDelta;
        
        if (Config.bDebugLog)
        {
            static int32 LogCounter = 0;
            if (++LogCounter % 60 == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Speed Match Segment: CharSpeed=%.1f, Target=%.1f, Ratio=%.2f, T=%.2f, Delta=%.1f->%.1f"),
                    ImpliedSpeed, TargetAnimSpeed, FinalRatio, TimeOffset, 
                    Delta.Size(), ScaledDelta.Size());
            }
        }
    }
}

float FMotionData::FindBestDatabaseSpeed(float CharacterSpeed, const TArray<FSpeedCluster>& Clusters)
{
    if (Clusters.Num() == 0)
    {
        return CharacterSpeed;
    }
    
    float BestSpeed = Clusters[0].CenterSpeed;
    float MinDiff = FLT_MAX;
    
    for (int32 i = 0; i < Clusters.Num(); i++)
    {
        const FSpeedCluster& Cluster = Clusters[i];
        float Diff = FMath::Abs(Cluster.CenterSpeed - CharacterSpeed);
        
        if (Diff < MinDiff)
        {
            MinDiff = Diff;
            BestSpeed = Cluster.CenterSpeed;
        }
    }
    
    return BestSpeed;
}

void FMotionData::DetectMotionStates(FMotionAnalysis& Analysis, const FTrajectoryConfig& Config) const
{
    if (GetSampleCount() < 3) 
    {
        Analysis.CurrentState = ELocomotionState::Idle;
        return;
    }
    
    const FMotionSample Current = GetNewestSample();
    const float CurrentSpeed2D = Current.GetSpeed2D();
    const FVector CurrentAccel = Current.CachedAcceleration;  // Use cached acceleration
    const float AccelMag = CurrentAccel.Size();
    const float AngularVel = EstimateAngularAcceleration(0.05f);
    
    const float AccelDotVel = FVector::DotProduct(
        CurrentAccel.GetSafeNormal(), 
        Current.WorldVelocity.GetSafeNormal()
    );
    const bool bIsDecelerating = (AccelDotVel < -0.3f && AccelMag > 100.f) || 
                                 (Current.RawInput.Size() < Config.MinInputMagnitude && CurrentSpeed2D > Config.StopSpeedThreshold);
    
    if (FMath::Abs(AngularVel) > Config.PivotAngularVelocityDeg && 
        CurrentSpeed2D < Config.TransitionSpeedThreshold &&
        Current.RawInput.Size() > Config.MinInputMagnitude)
    {
        Analysis.bIsPivoting = true;
        Analysis.PivotAngle = AngularVel * 0.1f;
        Analysis.CurrentState = ELocomotionState::Pivoting;
        return;
    }
    
    bool bIsLooping = false;
    float RecentAverageSpeed = CurrentSpeed2D;

    if (GetSampleCount() >= 10)
    {
        float SpeedVariance = 0.f;
        for (int32 i = 1; i < 10; i++)
        {
            FMotionSample Sample;
            if (GetSampleByAge(i, Sample))
            {
                RecentAverageSpeed += Sample.GetSpeed2D();
                SpeedVariance += FMath::Abs(Sample.GetSpeed2D() - CurrentSpeed2D);
            }
        }
        RecentAverageSpeed /= 10.f;
        SpeedVariance /= 10.f;
        
        bIsLooping = (SpeedVariance < 50.f && RecentAverageSpeed > Config.LoopingSpeedThreshold);
        Analysis.bIsLooping = bIsLooping;
    }
    
    if (CurrentSpeed2D < Config.MovementSpeedThreshold)
    {
        if (Current.RawInput.Size() > Config.MinInputMagnitude && AccelMag > 100.f)
        {
            Analysis.CurrentState = ELocomotionState::Starting;
            Analysis.bIsTransitioning = true;
            Analysis.TransitionProgress = FMath::Clamp(AccelMag / 1000.f, 0.f, 1.f);
        }
        else if (FMath::Abs(AngularVel) > 45.f)
        {
            Analysis.CurrentState = ELocomotionState::Turning;
        }
        else
        {
            Analysis.CurrentState = ELocomotionState::Idle;
        }
    }
    else if (bIsDecelerating && CurrentSpeed2D < Config.TransitionSpeedThreshold)
    {
        Analysis.CurrentState = ELocomotionState::Stopping;
        Analysis.bIsTransitioning = true;
        Analysis.TransitionProgress = 1.f - FMath::Clamp(CurrentSpeed2D / Config.TransitionSpeedThreshold, 0.f, 1.f);
        
        if (AccelMag > 50.f)
        {
            Analysis.PredictedStopTime = CurrentSpeed2D / AccelMag;
            Analysis.PredictedStopDistance = 0.5f * CurrentSpeed2D * Analysis.PredictedStopTime;
        }
    }
    else if (bIsLooping)
    {
        Analysis.CurrentState = ELocomotionState::Looping;
    }
    else
    {
        bool bIsTransitioning = false;
        
        if (GetSampleCount() >= 5)
        {
            FMotionSample OldSample;
            if (GetSampleByAge(4, OldSample))
            {
                const float DirectionChange = FMath::RadiansToDegrees(
                    FMath::Acos(FMath::Clamp(
                        FVector::DotProduct(
                            Current.WorldVelocity.GetSafeNormal(),
                            OldSample.WorldVelocity.GetSafeNormal()
                        ), -1.f, 1.f))
                );
                
                if (DirectionChange > 30.f)
                {
                    bIsTransitioning = true;
                    Analysis.TransitionProgress = FMath::Clamp(DirectionChange / 180.f, 0.f, 1.f);
                }
                
                const float SpeedChange = FMath::Abs(CurrentSpeed2D - OldSample.GetSpeed2D());
                if (SpeedChange > 100.f)
                {
                    bIsTransitioning = true;
                    Analysis.TransitionProgress = FMath::Max(Analysis.TransitionProgress, 
                                                            FMath::Clamp(SpeedChange / 300.f, 0.f, 1.f));
                }
            }
        }
        
        if (bIsTransitioning)
        {
            Analysis.CurrentState = ELocomotionState::Transitioning;
            Analysis.bIsTransitioning = true;
        }
        else
        {
            Analysis.CurrentState = ELocomotionState::Moving;
        }
    }
    
    if (!Current.bIsOnGround)
    {
        if (Current.WorldVelocity.Z > 50.f)
        {
            Analysis.CurrentState = ELocomotionState::Jumping;
        }
        else if (Current.WorldVelocity.Z < -50.f)
        {
            Analysis.CurrentState = ELocomotionState::Falling;
        }
    }
}

void FMotionData::ApplyEnvironmentalAdjustments(
    FTransformTrajectorySample& Sample,
    const FVector& PredVel,
    float Time,
    const FEnvironmentContext& EnvContext,
    float BrakingDeceleration,
    const FVector& LastPosition) const
{
    const FVector OriginalPosition = Sample.Position;
    bool bPositionAdjusted = false;
    
    if (FMath::Abs(EnvContext.GroundSlope) > 5.f)
    {
        const float SlopeEffect = FMath::Sin(FMath::DegreesToRadians(EnvContext.GroundSlope));
        Sample.Position.Z += SlopeEffect * PredVel.Size() * Time * 0.5f;
    }
    
    if (EnvContext.bIsOnStairs && EnvContext.StepHeight > 0.f)
    {
        const float StairSpeed = FMath::Min(300.f, PredVel.Size());
        const float StepsPerSecond = StairSpeed / 150.f;
        const float StepsInTime = Time * StepsPerSecond;
        const int32 CompletedSteps = FMath::FloorToInt(StepsInTime);
        const float StepProgress = FMath::Frac(StepsInTime);
        
        const FMotionSample Current = GetNewestSample();
        Sample.Position.Z = Current.WorldPosition.Z + 
                           CompletedSteps * EnvContext.StepHeight + 
                           StepProgress * EnvContext.StepHeight * 0.5f;
    }
    
    if (EnvContext.NearestWallDistance < 150.f && EnvContext.NearestWallDistance > 0.f)
    {
        const float ApproachSpeed = FVector::DotProduct(
            PredVel.GetSafeNormal(),
            -EnvContext.NearestWallNormal
        );
        
        if (ApproachSpeed > 0.1f)
        {
            const FVector TangentDir = FVector::CrossProduct(
                EnvContext.NearestWallNormal,
                FVector::UpVector
            ).GetSafeNormal();
            
            const float Dot1 = FVector::DotProduct(TangentDir, PredVel.GetSafeNormal());
            const float Dot2 = FVector::DotProduct(-TangentDir, PredVel.GetSafeNormal());
            const FVector SteerDir = (Dot1 > Dot2) ? TangentDir : -TangentDir;
            
            const float SteerStrength = FMath::GetMappedRangeValueClamped(
                FVector2D(30.f, 150.f),
                FVector2D(0.8f, 0.1f),
                EnvContext.NearestWallDistance
            );
            
            const FVector SteerOffset = (SteerDir - PredVel.GetSafeNormal()) * SteerStrength * 50.f;
            Sample.Position += SteerOffset * Time;
            Sample.Position += EnvContext.NearestWallNormal * (150.f - EnvContext.NearestWallDistance) * 0.1f;
            bPositionAdjusted = true;
        }
    }
    
    if (EnvContext.bIsNearEdge && PredVel.Size() > 100.f)
    {
        const float StopDistance = (PredVel.Size() * PredVel.Size()) / (2.f * BrakingDeceleration);
        if (StopDistance > 50.f)
        {
            const float EmergencyStopTime = PredVel.Size() / (BrakingDeceleration * 2.f);
            if (Time > EmergencyStopTime)
            {
                const FMotionSample Current = GetNewestSample();
                Sample.Position = Current.WorldPosition + PredVel.GetSafeNormal() * 50.f;
                bPositionAdjusted = true;
            }
        }
    }
    
    if (bPositionAdjusted)
    {
        const FVector AdjustedDelta = Sample.Position - OriginalPosition;
        
        const FVector HorizontalDelta(AdjustedDelta.X, AdjustedDelta.Y, 0.f);
        if (HorizontalDelta.Size() > 5.f)
        {
            const FVector AdjustedVelocity = (Sample.Position - LastPosition).GetSafeNormal();
            
            const float BlendStrength = EnvContext.NearestWallDistance < 100.f ? 0.5f : 0.3f;
            const FQuat NewDirectionQuat = AdjustedVelocity.Rotation().Quaternion();
            Sample.Facing = FQuat::Slerp(Sample.Facing, NewDirectionQuat, BlendStrength);
        }
    }
}

FMotionSample FMotionData::InterpolateSamples(const FMotionSample& A, const FMotionSample& B, float Alpha)
{
    FMotionSample Result;
    
    Result.TimeStamp = FMath::Lerp(A.TimeStamp, B.TimeStamp, Alpha);
    Result.WorldPosition = FMath::Lerp(A.WorldPosition, B.WorldPosition, Alpha);
    Result.WorldVelocity = FMath::Lerp(A.WorldVelocity, B.WorldVelocity, Alpha);
    Result.WorldAcceleration = FMath::Lerp(A.WorldAcceleration, B.WorldAcceleration, Alpha);
    Result.CachedAcceleration = FMath::Lerp(A.CachedAcceleration, B.CachedAcceleration, Alpha);  // Interpolate cached acceleration
    Result.WorldRotation = FQuat::Slerp(A.WorldRotation, B.WorldRotation, Alpha);
    Result.AngularVelocityRadians = FMath::Lerp(A.AngularVelocityRadians, B.AngularVelocityRadians, Alpha);
    Result.RawInput = FMath::Lerp(A.RawInput, B.RawInput, Alpha);
    Result.RotationAlpha = FMath::Lerp(A.RotationAlpha, B.RotationAlpha, Alpha);
    
    Result.bIsOnGround = Alpha >= 0.5f ? B.bIsOnGround : A.bIsOnGround;
    
    return Result;
}


// Helper function to detect steady-state speed
float FMotionData::DetectSteadyStateSpeed() const
{
    if (GetSampleCount() < 20)
    {
        return GetNewestSample().GetSpeed();
    }

    // Analyze last 0.3 seconds of motion
    float TotalSpeed = 0.0f;
    float MinSpeed = FLT_MAX;
    float MaxSpeed = 0.0f;
    int ValidSamples = 0;
    
    for (int32 i = 0; i < 20; i++)
    {
        FMotionSample Sample;
        if (GetSampleByAge(i, Sample))
        {
            float Speed = Sample.GetSpeed();
            TotalSpeed += Speed;
            MinSpeed = FMath::Min(MinSpeed, Speed);
            MaxSpeed = FMath::Max(MaxSpeed, Speed);
            ValidSamples++;
        }
    }
    
    if (ValidSamples == 0)
    {
        return GetNewestSample().GetSpeed();
    }

    float AvgSpeed = TotalSpeed / ValidSamples;
    float SpeedVariance = MaxSpeed - MinSpeed;
    
    // If speed is stable (low variance), we're at steady state
    if (SpeedVariance < 20.0f)
    {
        return AvgSpeed;
    }
    
    // If accelerating, project forward based on trend
    if (GetNewestSample().GetSpeed() > AvgSpeed)
    {
        // Estimate target based on acceleration rate
        float AccelRate = (GetNewestSample().GetSpeed() - MinSpeed) / 0.3f;
        return GetNewestSample().GetSpeed() + AccelRate * 0.5f; // Project 0.5s forward
    }
    
    return GetNewestSample().GetSpeed();
}

