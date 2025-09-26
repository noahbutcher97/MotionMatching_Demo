// Data/.h - ENHANCED ROTATION SYSTEM WITH 5 MODES AND FIXES
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Animation/TrajectoryTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MMPrimaryStructs.generated.h"

UENUM(BlueprintType)
enum class ELocomotionState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Starting    UMETA(DisplayName = "Starting"),
    Moving      UMETA(DisplayName = "Moving"),
    Stopping    UMETA(DisplayName = "Stopping"),
    Turning     UMETA(DisplayName = "Turning"),
    Pivoting    UMETA(DisplayName = "Pivoting"),
    Transitioning UMETA(DisplayName = "Transitioning"),
    Looping     UMETA(DisplayName = "Looping"),
    Jumping     UMETA(DisplayName = "Jumping"),
    Falling     UMETA(DisplayName = "Falling"),
    Landing     UMETA(DisplayName = "Landing")
};

UENUM(BlueprintType)
enum class EMovementDirection : uint8
{
    Forward     UMETA(DisplayName = "Forward"),
    Backward    UMETA(DisplayName = "Backward"),
    Left        UMETA(DisplayName = "Left"),
    Right       UMETA(DisplayName = "Right"),
    ForwardLeft UMETA(DisplayName = "Forward Left"),
    ForwardRight UMETA(DisplayName = "Forward Right"),
    BackwardLeft UMETA(DisplayName = "Backward Left"),
    BackwardRight UMETA(DisplayName = "Backward Right")
};

UENUM(BlueprintType)
enum class EMotionComplexity : uint8
{
    Simple      UMETA(DisplayName = "Simple"),
    Moderate    UMETA(DisplayName = "Moderate"),
    Complex     UMETA(DisplayName = "Complex"),
    Critical    UMETA(DisplayName = "Critical")
};

UENUM(BlueprintType)
enum class EEnvironmentLOD : uint8
{
    Full        UMETA(DisplayName = "Full Quality"),
    Reduced     UMETA(DisplayName = "Reduced Quality"),
    Minimal     UMETA(DisplayName = "Minimal Quality")
};

UENUM(BlueprintType)
enum class ECMCRotationMode : uint8
{
    ControlBased    UMETA(DisplayName = "Control Based (Camera Direction)"),
    MovementBased   UMETA(DisplayName = "Movement Based (Velocity Direction)")
};

UENUM(BlueprintType)
enum class EManualRotationMode : uint8
{
    Orbit           UMETA(DisplayName = "Orbit (Face Velocity)"),
    Strafe          UMETA(DisplayName = "Strafe (Face Camera)"),
    Dynamic         UMETA(DisplayName = "Dynamic (Simple Linear Blend)"),
    LookAt          UMETA(DisplayName = "Look At Target"),
    Cinematic       UMETA(DisplayName = "Cinematic (Intelligent Multi-Factor)")
};

// ============================================================================
// FMotionSample - Generic motion data point with cached acceleration
// ============================================================================
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionSample
{
    GENERATED_BODY()

    FMotionSample()
    {
        TimeStamp = 0.f;
        WorldPosition = FVector::ZeroVector;
        WorldVelocity = FVector::ZeroVector;
        WorldAcceleration = FVector::ZeroVector;
        CachedAcceleration = FVector::ZeroVector;  // NEW: Store actual CMC acceleration
        WorldRotation = FQuat::Identity;
        AngularVelocityRadians = FVector::ZeroVector;
        RawInput = FVector2D::ZeroVector;
        bIsOnGround = true;
        RotationAlpha = 0.f;
    }

    // Core motion data
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    float TimeStamp;
    
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    FVector WorldPosition;
    
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    FVector WorldVelocity;
    
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    FVector WorldAcceleration;
    
    // NEW: Cached acceleration from CMC at sample time for accurate prediction
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    FVector CachedAcceleration;
    
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    FQuat WorldRotation;
    
    UPROPERTY(BlueprintReadOnly, Category="Motion|Core")
    FVector AngularVelocityRadians;
    
    // Input
    UPROPERTY(BlueprintReadOnly, Category="Motion|Input")
    FVector2D RawInput;
    
    // State
    UPROPERTY(BlueprintReadOnly, Category="Motion|State")
    uint8 bIsOnGround : 1;
    
    // Store rotation blend alpha for perfect prediction sync
    UPROPERTY(BlueprintReadOnly, Category="Motion|Rotation")
    float RotationAlpha;
    
    // Helper functions
    float GetSpeed() const { return WorldVelocity.Size(); }
    float GetSpeed2D() const { return WorldVelocity.Size2D(); }
    FVector GetForwardVector() const { return WorldRotation.GetForwardVector(); }
    FVector GetRightVector() const { return WorldRotation.GetRightVector(); }
    float GetYaw() const { return WorldRotation.Rotator().Yaw; }
    
    bool IsValid() const
    {
        return TimeStamp >= 0.f && 
               FMath::IsFinite(TimeStamp) &&
               !WorldPosition.ContainsNaN() &&
               !WorldVelocity.ContainsNaN() &&
               !WorldAcceleration.ContainsNaN() &&
               !CachedAcceleration.ContainsNaN() &&
               WorldRotation.IsNormalized();
    }
};


// ============================================================================
// FSampleImportance - Optional importance tracking for samples
// ============================================================================
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSampleImportance
{
    GENERATED_BODY()

    FSampleImportance()
    {
        ImportanceScore = 1.0f;
        TimeAdded = 0.f;
        bIsKeyFrame = false;
        bIsTransition = false;
        bIsDirectionChange = false;
        DirectionChangeAngle = 0.f;
        SpeedChange = 0.f;
    }

    UPROPERTY()
    float ImportanceScore;
    
    UPROPERTY()
    float TimeAdded;
    
    UPROPERTY()
    uint8 bIsKeyFrame : 1;
    UPROPERTY()
    uint8 bIsTransition : 1;
    UPROPERTY()
    uint8 bIsDirectionChange : 1;
    
    UPROPERTY()
    float DirectionChangeAngle;
    
    UPROPERTY()
    float SpeedChange;
    
    float GetTrajectoryWeight() const
    {
        float Weight = 1.0f;
        if (bIsKeyFrame) Weight *= 2.0f;
        if (bIsTransition) Weight *= 1.5f;
        if (bIsDirectionChange) Weight *= 1.3f;
        if (DirectionChangeAngle > 30.f) Weight *= 1.2f;
        if (FMath::Abs(SpeedChange) > 100.f) Weight *= 1.1f;
        return Weight;
    }
};


/**
 * Represents a cluster of similar animation speeds in the database
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSpeedCluster
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Speed")
    float CenterSpeed = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed")
    float MinSpeed = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed")
    float MaxSpeed = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed")
    int32 SampleCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed")
    float Weight = 0.f;
    
    bool Contains(float Speed) const
    {
        return Speed >= MinSpeed && Speed <= MaxSpeed;
    }
    
    float DistanceTo(float Speed) const
    {
        if (Contains(Speed)) return 0.f;
        return FMath::Min(FMath::Abs(Speed - MinSpeed), FMath::Abs(Speed - MaxSpeed));
    }
};


/**
 * Configuration for dynamic speed matching system
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FDynamicSpeedMatchConfig
{
    GENERATED_BODY()

    // Clustering
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin=2, ClampMax=10))
    int32 MaxSpeedClusters = 5;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin=10.0, ClampMax=200.0))
    float ClusterMergeThreshold = 50.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin=0.0, ClampMax=1.0))
    float ClusterWeightThreshold = 0.1f;
    
    // Matching
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching", meta=(ClampMin=0.0, ClampMax=1.0))
    float SpeedMatchStrength = 0.7f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching", meta=(ClampMin=0.0, ClampMax=200.0))
    float HysteresisZone = 30.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching")
    bool bPreventMicroTransitions = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching", meta=(ClampMin=0.1, ClampMax=2.0))
    float MinTransitionTime = 0.3f;
    
    // Smoothing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Smoothing", meta=(ClampMin=0.1, ClampMax=20.0))
    float TransitionSmoothness = 5.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Smoothing")
    bool bUseContinuousBlending = false;
    
    // Speed ratio limits for trajectory adjustment
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Limits", meta=(ClampMin=0.1, ClampMax=1.0))
    float MinSpeedRatio = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Limits", meta=(ClampMin=1.0, ClampMax=5.0))
    float MaxSpeedRatio = 2.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Limits", meta=(ClampMin=0.0))
    float MinSpeedThreshold = 50.0f;
    
    // Debug
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bDebugLog = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bDebugDraw = false;
};

/**
 * Runtime speed analysis state
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FDynamicSpeedAnalysis
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    TArray<FSpeedCluster> DetectedClusters;
    
    UPROPERTY(BlueprintReadOnly, Category="State")
    float CurrentActualSpeed = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="State")
    float CurrentTargetSpeed = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="State")
    float LastTargetSpeed = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="State")
    int32 CurrentClusterIndex = -1;
    
    UPROPERTY(BlueprintReadOnly, Category="State")
    float TimeSinceLastTransition = 0.f;
    
    UPROPERTY(BlueprintReadOnly, Category="State")
    bool bIsTransitioning = false;
    
    void Reset()
    {
        DetectedClusters.Empty();
        CurrentActualSpeed = 0.f;
        CurrentTargetSpeed = 0.f;
        LastTargetSpeed = 0.f;
        CurrentClusterIndex = -1;
        TimeSinceLastTransition = 0.f;
        bIsTransitioning = false;
    }
};


// ============================================================================
// FMotionBufferConfig - Configuration for motion buffer
// ============================================================================
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionBufferConfig
{
    GENERATED_BODY()

    FMotionBufferConfig()
    {
        BufferSize = 30;
        bTrackImportance = true;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buffer", 
        meta=(ClampMin=20, ClampMax=60, 
        Tooltip="Number of historical motion samples to store. Larger buffer provides more context but uses more memory."))
    int32 BufferSize;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buffer", 
        meta=(Tooltip="Calculate importance scores for samples to improve trajectory prediction during significant motion changes."))
    bool bTrackImportance;
};

// Environment context
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FEnvironmentContext
{
    GENERATED_BODY()

    FEnvironmentContext()
    {
        GroundNormal = FVector::UpVector;
        GroundSlope = 0.f;
        StepHeight = 0.f;
        bIsNearEdge = false;
        bIsOnStairs = false;
        bIsInTightSpace = false;
        CeilingHeight = 300.f;
        NearestWallDistance = 1000.f;
        NearestWallNormal = FVector::ZeroVector;
    }

    UPROPERTY(BlueprintReadOnly, Category="Environment")
    FVector GroundNormal;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    float GroundSlope;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    float StepHeight;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    bool bIsNearEdge;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    bool bIsOnStairs;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    bool bIsInTightSpace;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    float CeilingHeight;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    float NearestWallDistance;
    
    UPROPERTY(BlueprintReadOnly, Category="Environment")
    FVector NearestWallNormal;
};

// Environmental cache configuration
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FEnvironmentCacheConfig
{
    GENERATED_BODY()

    FEnvironmentCacheConfig()
    {
        GroundCacheTTL = 0.2f;
        WallCacheTTL = 0.15f;
        EdgeCacheTTL = 0.1f;
        StairsCacheTTL = 0.25f;
        CeilingCacheTTL = 0.3f;
        
        LOD0SpeedThreshold = 300.f;
        LOD1SpeedThreshold = 100.f;
        LODAngularThreshold = 90.f;
        
        SpatialHashGridSize = 100.f;
        SpatialReuseRadius = 50.f;
        MaxCacheEntries = 100;
        
        bUseAsyncTraces = false;
        AsyncTracesPerFrame = 2;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|TTL", 
        meta=(ClampMin=0.05, ClampMax=1.0, Tooltip="How long cached ground data remains valid."))
    float GroundCacheTTL;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|TTL", 
        meta=(ClampMin=0.05, ClampMax=1.0, Tooltip="How long cached wall data remains valid."))
    float WallCacheTTL;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|TTL", 
        meta=(ClampMin=0.05, ClampMax=1.0, Tooltip="How long cached edge data remains valid."))
    float EdgeCacheTTL;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|TTL", 
        meta=(ClampMin=0.05, ClampMax=1.0, Tooltip="How long cached stair data remains valid."))
    float StairsCacheTTL;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|TTL", 
        meta=(ClampMin=0.05, ClampMax=1.0, Tooltip="How long cached ceiling data remains valid."))
    float CeilingCacheTTL;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|LOD", 
        meta=(ClampMin=100, ClampMax=1000, Tooltip="Speed threshold for full quality scanning."))
    float LOD0SpeedThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|LOD", 
        meta=(ClampMin=10, ClampMax=300, Tooltip="Speed threshold for reduced quality scanning."))
    float LOD1SpeedThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|LOD", 
        meta=(ClampMin=30, ClampMax=180, Tooltip="Angular velocity threshold for full quality scanning."))
    float LODAngularThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|Spatial", 
        meta=(ClampMin=50, ClampMax=500, Tooltip="Size of spatial hash grid cells."))
    float SpatialHashGridSize;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|Spatial", 
        meta=(ClampMin=10, ClampMax=200, Tooltip="Radius for spatial cache reuse."))
    float SpatialReuseRadius;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|Memory", 
        meta=(ClampMin=10, ClampMax=1000, Tooltip="Maximum spatial hash entries."))
    int32 MaxCacheEntries;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|Async", 
        meta=(Tooltip="Use async traces to reduce frame hitches."))
    bool bUseAsyncTraces;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cache|Async", 
        meta=(ClampMin=1, ClampMax=8, Tooltip="Max async traces per frame."))
    int32 AsyncTracesPerFrame;
};

// Cached environment data
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FCachedEnvironmentData
{
    GENERATED_BODY()

    FCachedEnvironmentData()
    {
        LastUpdateTime = -1.f;
        LastPosition = FVector::ZeroVector;
        CurrentLOD = EEnvironmentLOD::Full;
        CacheHits = 0;
        CacheMisses = 0;
        GroundUpdateTime = -1.f;
        WallsUpdateTime = -1.f;
        EdgeUpdateTime = -1.f;
        StairsUpdateTime = -1.f;
        CeilingUpdateTime = -1.f;
    }

    UPROPERTY()
    FEnvironmentContext Data;
    
    UPROPERTY()
    float LastUpdateTime;
    
    UPROPERTY()
    FVector LastPosition;
    
    UPROPERTY()
    EEnvironmentLOD CurrentLOD;
    
    UPROPERTY()
    float GroundUpdateTime;
    
    UPROPERTY()
    float WallsUpdateTime;
    
    UPROPERTY()
    float EdgeUpdateTime;
    
    UPROPERTY()
    float StairsUpdateTime;
    
    UPROPERTY()
    float CeilingUpdateTime;
    
    UPROPERTY()
    int32 CacheHits;
    
    UPROPERTY()
    int32 CacheMisses;
    
    bool IsGroundValid(float CurrentTime, float TTL) const
    {
        return (CurrentTime - GroundUpdateTime) < TTL;
    }
    
    bool IsWallsValid(float CurrentTime, float TTL) const
    {
        return (CurrentTime - WallsUpdateTime) < TTL;
    }
    
    bool IsEdgeValid(float CurrentTime, float TTL) const
    {
        return (CurrentTime - EdgeUpdateTime) < TTL;
    }
    
    bool IsStairsValid(float CurrentTime, float TTL) const
    {
        return (CurrentTime - StairsUpdateTime) < TTL;
    }
    
    bool IsCeilingValid(float CurrentTime, float TTL) const
    {
        return (CurrentTime - CeilingUpdateTime) < TTL;
    }
    
    float GetCacheHitRate() const
    {
        const int32 Total = CacheHits + CacheMisses;
        return Total > 0 ? static_cast<float>(CacheHits) / static_cast<float>(Total) : 0.f;
    }
};

// Spatial hash entry
USTRUCT()
struct MOTIONMATCHING_DEMO_API FSpatialHashEntry
{
    GENERATED_BODY()

    FSpatialHashEntry()
    {
        Position = FVector::ZeroVector;
        TimeStamp = 0.f;
    }

    UPROPERTY()
    FVector Position;
    
    UPROPERTY()
    FCachedEnvironmentData CachedData;
    
    UPROPERTY()
    float TimeStamp;
};

// Character motion context
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FCharacterMotionContext
{
    GENERATED_BODY()

    FCharacterMotionContext()
    {
        MeshRotation = FQuat::Identity;
        ControlRotation = FRotator::ZeroRotator;
        TargetRotation = FRotator::ZeroRotator;
        Speed = 0.f;
        Speed2D = 0.f;
        ControlToMeshAngleYaw = 0.f;
        MeshToVelocityAngleYaw = 0.f;
        TargetToMeshAngleYaw = 0.f;
        MovementDirection = EMovementDirection::Forward;
        bIsMoving = false;
        bIsTurning = false;
        bIsAccelerating = false;
        bIsDecelerating = false;
    }

    UPROPERTY(BlueprintReadOnly, Category="Context|Rotation")
    FQuat MeshRotation;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Rotation")
    FRotator ControlRotation;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Rotation")
    FRotator TargetRotation;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Motion")
    float Speed;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Motion")
    float Speed2D;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Angles")
    float ControlToMeshAngleYaw;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Angles")
    float MeshToVelocityAngleYaw;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|Angles")
    float TargetToMeshAngleYaw;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|State")
    EMovementDirection MovementDirection;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|State")
    uint8 bIsMoving : 1;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|State")
    uint8 bIsTurning : 1;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|State")
    uint8 bIsAccelerating : 1;
    
    UPROPERTY(BlueprintReadOnly, Category="Context|State")
    uint8 bIsDecelerating : 1;
};

// Motion analysis
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionAnalysis
{
    GENERATED_BODY()

    FMotionAnalysis()
    {
        CurrentState = ELocomotionState::Idle;
        TimeInState = 0.f;
        bIsPivoting = false;
        bIsTransitioning = false;
        bIsLooping = false;
        PivotAngle = 0.f;
        TransitionProgress = 0.f;
        PredictedStopTime = -1.f;
        PredictedStopDistance = -1.f;
    }

    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    ELocomotionState CurrentState;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    float TimeInState;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    bool bIsPivoting;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    bool bIsTransitioning;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    bool bIsLooping;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    float PivotAngle;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    float TransitionProgress;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    float PredictedStopTime;
    
    UPROPERTY(BlueprintReadOnly, Category="Analysis")
    float PredictedStopDistance;
};

// Animation context
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FAnimationContext
{
    GENERATED_BODY()

    FAnimationContext()
    {
        CurrentAnimationName = NAME_None;
        TimeInAnimation = 0.f;
        AnimationLength = 0.f;
        bHasRootMotion = false;
        bIsInterruptible = true;
        NextFootPlantTime = -1.f;
        FootPlantFoot = 0;
        RootMotionVelocity = FVector::ZeroVector;
        RootMotionRotation = FQuat::Identity;
    }

    UPROPERTY(BlueprintReadOnly, Category="Animation")
    FName CurrentAnimationName;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    float TimeInAnimation;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    float AnimationLength;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    bool bHasRootMotion;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    bool bIsInterruptible;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    float NextFootPlantTime;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    int32 FootPlantFoot;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    FVector RootMotionVelocity;
    
    UPROPERTY(BlueprintReadOnly, Category="Animation")
    FQuat RootMotionRotation;
};

/**
 * Enhanced rotation configuration with 5 modes, improved transitions, and fixes
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FRotationConfig
{
    GENERATED_BODY()

    FRotationConfig()
    {
        // Master control
        bUseCharacterMovementRotation = false;
        CMCMode = ECMCRotationMode::MovementBased;
        ManualMode = EManualRotationMode::Dynamic;
        
        // CMC tracking
        CMCRotationRateDegrees = 360.0f;
        
        // Shared base parameters
        BaseRotationRate = 360.0f;
        RecoveryRateDegrees = 540.0f;
        AdaptiveThresholdDegrees = 90.0f;
        PredictionBias = 1.2f;
        RotationSmoothingTime = 0.15f;
        
        // Mode-specific modifiers
        OrbitRateModifier = 1.0f;
        StrafeRateModifier = 1.5f;
        DynamicRateModifier = 1.2f;
        LookAtRateModifier = 1.3f;
        CinematicRateModifier = 0.8f;
        
        // Dynamic mode - NOW SIMPLIFIED LINEAR BLEND
        DynamicBlendWeight = 0.5f;  // 0=Orbit (velocity), 1=Strafe (camera)
        
        // LookAt mode
        LookAtTarget = FVector::ZeroVector;
        bHasLookAtTarget = false;
        LookAtDeadZone = 15.0f;
        
        // Cinematic mode - INTELLIGENT MULTI-FACTOR
        CinematicAccelerationInfluence = 0.3f;
        CinematicVelocityInfluence = 0.4f;
        CinematicControlInfluence = 0.3f;
        CinematicLookAheadTime = 0.5f;
        CinematicSmoothingFactor = 0.85f;
        CinematicMinSpeed = 50.0f;
        CinematicMaxLagAngle = 120.0f;
        CinematicTurnBoostThreshold = 45.0f;
        CinematicTurnBoostMultiplier = 1.5f;
        
        // Movement state tracking - IMPROVED WITH CONTINUOUS BLEND
        MovementThreshold = 10.0f;
        IdleTransitionDelay = 0.2f;
        IdleInterpolationSpeed = 3.0f;
        MovingInterpolationSpeed = 8.0f;
        MovementAlpha = 0.0f;  // NEW: Continuous blend instead of binary
        LastMovementTime = 0.f;
        bWasMovingLastFrame = false;
        
        // Input responsiveness - NEW
        bUseRawInputForRotation = false;
        RawInputForRotation = FVector2D::ZeroVector;
        RotationInputResponseTime = 0.05f;
        
        // Runtime state
        CurrentYaw = 0.f;
        TargetYaw = 0.f;
        RotationLag = 0.f;
        ActualAngularVelocity = 0.f;
        DesiredAngularVelocity = 0.f;
        bIsLagging = false;
        bIsRotationRateLimited = false;
        BlendAlpha = 0.f;
        
        // Alpha correction - IMPROVED WITH DAMPING
        AlphaErrorAccumulator = 0.f;
        AlphaCorrectionRate = 0.1f;  // NEW: Reduced from 0.5
        AlphaCorrectionMax = 0.05f;  // NEW: Per-frame limit
        AlphaCorrectionDecay = 0.9f; // NEW: Decay factor
        LastAlphaPredictionError = 0.f;
        
        // Mode transition - IMPROVED
        ModeTransitionAlpha = 1.0f;
        ModeTransitionStartRotation = FQuat::Identity;
        ModeTransitionTargetYaw = 0.f;     // NEW: Store previous mode's target
        ModeBlendAlpha = 1.0f;              // NEW: Blend between mode targets
        LastRotationMode = -1;
        
        // Internal tracking
        bCompetingSystemsDisabled = false;
    }

    // ===== MASTER CONTROL =====
    
    /** If true, use Character Movement Component rotation. If false, use manual system. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Control", 
        meta=(DisplayPriority=1, Tooltip= "Master switch between Character Movement Component based rotation and custom 'Manual' Rotation Modes"))
    bool bUseCharacterMovementRotation;
    
    /** CMC rotation mode when using Character Movement Component. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Control", 
        meta=(EditCondition="bUseCharacterMovementRotation", EditConditionHides, DisplayPriority=2,
        Tooltip="Rotation Mode when using Character Movement Component Based Rotation -> Aligns with bUseControllerDesiredRotation or bOrientRotationToMovement Flags"))
    ECMCRotationMode CMCMode;
    
    /** Manual rotation mode when not using CMC. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Control", 
        meta=(EditCondition="!bUseCharacterMovementRotation", EditConditionHides, DisplayPriority=3))
    EManualRotationMode ManualMode;
    
    // ===== SHARED BASE PARAMETERS =====
    
    /** Base rotation rate in degrees/sec. Mode-specific modifiers are applied to this. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Shared", 
        meta=(ClampMin=90.0, ClampMax=720.0, DisplayPriority=1,
        Tooltip="Base rotation rate shared by all modes. Mode-specific modifiers multiply this value."))
    float BaseRotationRate;
    
    /** Recovery rate when rotation is lagging behind desired orientation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Shared", 
        meta=(ClampMin=180.0, ClampMax=1080.0, DisplayPriority=2,
        Tooltip="Rotation rate used when catching up from lag."))
    float RecoveryRateDegrees;
    
    /** Angular velocity threshold for adaptive lag detection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Shared", 
        meta=(ClampMin=30.0, ClampMax=180.0, DisplayPriority=3,
        Tooltip="Camera angular velocity above which rotation starts lagging."))
    float AdaptiveThresholdDegrees;
    
    /** Prediction bias for trajectory generation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Shared", 
        meta=(ClampMin=0.8, ClampMax=1.5, DisplayPriority=4,
        Tooltip="Multiplier for rotation rate during trajectory prediction."))
    float PredictionBias;
    
    /** Time to smooth transitions between rotation modes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Shared", 
        meta=(ClampMin=0.0, ClampMax=0.5, DisplayPriority=5,
        Tooltip="Smoothing time when switching between rotation modes."))
    float RotationSmoothingTime;
    
    // ===== MODE-SPECIFIC RATE MODIFIERS =====
    
    /** Rate modifier for Orbit mode (face velocity direction). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Modifiers", 
        meta=(EditCondition="!bUseCharacterMovementRotation", EditConditionHides,
        ClampMin=0.1, ClampMax=3.0, DisplayPriority=1,
        Tooltip="Multiplies base rotation rate when in Orbit mode."))
    float OrbitRateModifier;
    
    /** Rate modifier for Strafe mode (face camera direction). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Modifiers", 
        meta=(EditCondition="!bUseCharacterMovementRotation", EditConditionHides,
        ClampMin=0.1, ClampMax=3.0, DisplayPriority=2,
        Tooltip="Multiplies base rotation rate when in Strafe mode."))
    float StrafeRateModifier;
    
    /** Rate modifier for Dynamic mode (blend between orbit and strafe). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Modifiers", 
        meta=(EditCondition="!bUseCharacterMovementRotation", EditConditionHides,
        ClampMin=0.1, ClampMax=3.0, DisplayPriority=3,
        Tooltip="Multiplies base rotation rate when in Dynamic mode."))
    float DynamicRateModifier;
    
    /** Rate modifier for LookAt mode (track target). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Modifiers", 
        meta=(EditCondition="!bUseCharacterMovementRotation", EditConditionHides,
        ClampMin=0.1, ClampMax=3.0, DisplayPriority=4,
        Tooltip="Multiplies base rotation rate when in LookAt mode."))
    float LookAtRateModifier;
    
    /** Rate modifier for Cinematic mode (intelligent blend). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Modifiers", 
        meta=(EditCondition="!bUseCharacterMovementRotation", EditConditionHides,
        ClampMin=0.1, ClampMax=3.0, DisplayPriority=5,
        Tooltip="Multiplies base rotation rate when in Cinematic mode."))
    float CinematicRateModifier;
    
    // ===== DYNAMIC MODE - SIMPLIFIED LINEAR BLEND =====
    
    /** Simple linear blend between Orbit and Strafe. 0.0 = Pure Orbit (face velocity), 1.0 = Pure Strafe (face camera). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Dynamic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Dynamic", 
        EditConditionHides, ClampMin=0.0, ClampMax=1.0,
        Tooltip="Simple linear interpolation: 0.0 = Face movement direction, 1.0 = Face camera direction"))
    float DynamicBlendWeight;
    
    // ===== LOOKAT MODE =====
    
    /** World location to look at. */
    UPROPERTY(BlueprintReadWrite, Category="Rotation|LookAt")
    FVector LookAtTarget;
    
    /** Whether a look at target is currently set. */
    UPROPERTY(BlueprintReadWrite, Category="Rotation|LookAt")
    bool bHasLookAtTarget;
    
    /** Distance from target within which rotation stops. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|LookAt", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::LookAt", 
        EditConditionHides, ClampMin=5.0, ClampMax=300.0,
        Tooltip="Deadzone radius around target where character stops rotating."))
    float LookAtDeadZone;
    
    // ===== CINEMATIC MODE - INTELLIGENT MULTI-FACTOR BLENDING =====
    
    /** Weight of acceleration in rotation calculation (normalized internally). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=0.0, ClampMax=1.0, DisplayPriority=1,
        Tooltip="Influence of acceleration direction. Higher = more predictive/anticipatory rotation."))
    float CinematicAccelerationInfluence;
    
    /** Weight of velocity in rotation calculation (normalized internally). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=0.0, ClampMax=1.0, DisplayPriority=2,
        Tooltip="Influence of current velocity direction. Higher = more movement-aligned rotation."))
    float CinematicVelocityInfluence;
    
    /** Weight of camera control in rotation calculation (normalized internally). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=0.0, ClampMax=1.0, DisplayPriority=3,
        Tooltip="Influence of camera direction. Higher = more camera-aligned rotation."))
    float CinematicControlInfluence;
    
    /** How far ahead to predict movement for anticipatory rotation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=0.0, ClampMax=2.0, DisplayPriority=4,
        Tooltip="Time in seconds to look ahead when using acceleration for predictive rotation."))
    float CinematicLookAheadTime;
    
    /** Smoothing applied to cinematic rotation blending. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=0.0, ClampMax=2.0, DisplayPriority=5,
        Tooltip="Damping factor. Higher = smoother but less responsive rotation."))
    float CinematicSmoothingFactor;
    
    /** Minimum speed for cinematic rotation to engage. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=0.0, ClampMax=600.0, DisplayPriority=6,
        Tooltip="Speed below which cinematic mode defaults to camera facing."))
    float CinematicMinSpeed;
    
    /** Maximum allowed lag before forced recovery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=45.0, ClampMax=180.0, DisplayPriority=7,
        Tooltip="Maximum angle difference before using recovery rate."))
    float CinematicMaxLagAngle;
    
    /** Angle threshold for turn boost in cinematic mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=15.0, ClampMax=180.0, DisplayPriority=8,
        Tooltip="Direction change angle above which rotation rate is boosted."))
    float CinematicTurnBoostThreshold;
    
    /** Rotation rate multiplier when making sharp turns in cinematic mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Cinematic", 
        meta=(EditCondition="!bUseCharacterMovementRotation && ManualMode == EManualRotationMode::Cinematic", 
        EditConditionHides, ClampMin=1.0, ClampMax=3.0, DisplayPriority=9,
        Tooltip="Multiplier applied to rotation rate during sharp direction changes."))
    float CinematicTurnBoostMultiplier;
    
    // ===== MOVEMENT STATE TRACKING =====
    
    /** Speed threshold to consider character as moving. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Movement", 
        meta=(ClampMin=1.0, ClampMax=50.0,
        Tooltip="Velocity magnitude below which character is considered idle."))
    float MovementThreshold;
    
    /** Time delay before transitioning from moving to idle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Movement", 
        meta=(ClampMin=0.0, ClampMax=1.0,
        Tooltip="Seconds after stopping before switching to idle rotation behavior."))
    float IdleTransitionDelay;
    
    /** Interpolation speed when idle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Movement", 
        meta=(ClampMin=1.0, ClampMax=20.0,
        Tooltip="Alpha interpolation speed when character is idle."))
    float IdleInterpolationSpeed;
    
    /** Interpolation speed when moving. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Movement", 
        meta=(ClampMin=1.0, ClampMax=20.0,
        Tooltip="Alpha interpolation speed when character is moving."))
    float MovingInterpolationSpeed;
    
    // ===== INPUT RESPONSIVENESS =====
    
    /** Use raw input for immediate rotation response, bypassing smoothing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Input", 
        meta=(Tooltip="If true, rotation uses raw input for immediate response while movement uses smoothed input."))
    bool bUseRawInputForRotation;
    
    /** Response time for rotation input (smaller = more responsive). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|Input", 
        meta=(ClampMin=0.0, ClampMax=0.5,
        Tooltip="How quickly rotation responds to input changes. Lower = more responsive."))
    float RotationInputResponseTime;
    
    // ===== RUNTIME STATE (READ-ONLY) =====
    
    /** Current actor yaw rotation. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float CurrentYaw;
    
    /** Target yaw rotation. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float TargetYaw;
    
    /** Current rotation lag (0-1). */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float RotationLag;
    
    /** Actual angular velocity in degrees/sec. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float ActualAngularVelocity;
    
    /** Desired angular velocity in degrees/sec. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float DesiredAngularVelocity;
    
    /** Whether rotation is currently lagging. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    bool bIsLagging;
    
    /** Whether rotation rate is being limited. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    bool bIsRotationRateLimited;
    
    /** Current blend alpha for Dynamic/Cinematic modes. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float BlendAlpha;
    
    /** Continuous movement blend (0=idle, 1=moving). */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|State")
    float MovementAlpha;
    
    // ===== ALPHA CORRECTION =====
    
    /** Accumulated alpha prediction error for correction. */
    UPROPERTY(BlueprintReadOnly, Category="Rotation|AlphaCorrection")
    float AlphaErrorAccumulator;
    
    /** Rate at which alpha correction is applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|AlphaCorrection", 
        meta=(ClampMin=0.01, ClampMax=0.5,
        Tooltip="How quickly alpha prediction errors are corrected. Lower = more stable."))
    float AlphaCorrectionRate;
    
    /** Maximum alpha correction per frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|AlphaCorrection", 
        meta=(ClampMin=0.01, ClampMax=0.2,
        Tooltip="Limits how much alpha can change per frame due to correction."))
    float AlphaCorrectionMax;
    
    /** Decay rate for alpha error accumulator. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation|AlphaCorrection", 
        meta=(ClampMin=0.5, ClampMax=0.99,
        Tooltip="How quickly accumulated alpha error decays. Higher = faster decay."))
    float AlphaCorrectionDecay;
    
    // ===== INTERNAL STATE =====
    
    UPROPERTY()
    FVector2D RawInputForRotation;
    
    UPROPERTY()
    float LastMovementTime;
    
    UPROPERTY()
    bool bWasMovingLastFrame;
    
    UPROPERTY()
    float LastAlphaPredictionError;
    
    UPROPERTY()
    bool bCompetingSystemsDisabled;
    
    UPROPERTY()
    int32 LastRotationMode;
    
    UPROPERTY()
    float ModeTransitionAlpha;
    
    UPROPERTY()
    FQuat ModeTransitionStartRotation;
    
    UPROPERTY()
    float ModeTransitionTargetYaw;
    
    UPROPERTY()
    float ModeBlendAlpha;
    
    UPROPERTY()
    float CMCRotationRateDegrees;
    
    // ===== FUNCTIONS =====
    
    void InitializeOnPlay(AActor* Owner, UCharacterMovementComponent* MoveComp);
    void ConfigureCMC(UCharacterMovementComponent* MoveComp);
    void UpdateRotationState(float ControlYaw, float CameraAngularVelocity, float DeltaTime);
    void UpdateManualRotation(AActor* Owner, const FRotator& ControlRotation, 
                            const FVector& Velocity, const FVector2D& InputVector, 
                            const FVector& Acceleration, float DeltaTime);
    float GetCurrentRotationRate() const;
    float GetModeSpecificRate(EManualRotationMode Mode) const;
    float GetPredictionRotationRate(bool bIsDirectionChange = false) const;
    int32 GetCurrentModeAsInt() const;
    FString GetCurrentModeDescription() const;
    bool HasModeChanged() const;
    void HandleModeTransition(AActor* Owner, float DeltaTime);
    void UpdateMovementState(const FVector& Velocity, float CurrentTime);
    void UpdateAlphaCorrection(float ActualAlpha, float PredictedAlpha, float DeltaTime);
    FRotator CalculateTargetForMode(int32 ModeInt, const FRotator& ControlRot, 
                                   const FVector& Velocity, const FVector& Acceleration,
                                   const FVector& Position) const;
    void ApplyAlphaCorrection(float DeltaTime);
    static FVector CalculateWorldIntentFromRawInput(const FVector2D& RawInput, const FRotator& ControlRot);
};

// Simplified trajectory config
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FTrajectoryConfig
{
    GENERATED_BODY()

    FTrajectoryConfig()
    {
        // Movement thresholds
        MovementSpeedThreshold = 10.f;
        StopSpeedThreshold = 5.f;
        LoopingSpeedThreshold = 100.f;
        TransitionSpeedThreshold = 200.f;
    
        // Input thresholds
        MinInputMagnitude = 0.05f;
        SignificantInputChange = 0.2f;
        InputDeadzone = 0.1f;
    
        // Angular thresholds
        TurnThresholdDegrees = 5.f;
        PivotThresholdDegrees = 45.f;
        PivotAngularVelocityDeg = 180.f;
    
        // Smoothing
        PositionSmoothingFactor = 0.25f;
        VelocitySmoothingFactor = 0.2f;
        RotationSmoothingFactor = 0.3f;
        InputSmoothingRate = 15.f;
        AccelerationSmoothingRate = 20.f;
    
        // Adaptive smoothing
        AdaptiveSmoothingMin = 25.f;
        AdaptiveSmoothingMax = 60.f;
        UrgencyThreshold = 0.2f;
    
        // Prediction
        MaxPredictionTime = 1.0f;
        PredictionSubstepTime = 0.033f;
        PredictionSmoothingRate = 25.f;
        TrajectoryDivergence = 1.0f;
    
        // State detection timing
        LoopingDurationThreshold = 0.3f;
        TransitionDurationThreshold = 0.15f;
        PivotDurationThreshold = 0.2f;
    
        // Features
        bUseEnvironmentAwareness = true;
        bAccountForRootMotion = true;
        bPredictStops = true;
        bUseAdaptiveSmoothing = true;
        bAnalyzeMotionStates = true;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Movement", 
        meta=(ClampMin=0.0, Tooltip="Speed above which character is considered moving."))
    float MovementSpeedThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Movement", 
        meta=(ClampMin=0.0, Tooltip="Speed below which character is considered stopped."))
    float StopSpeedThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Movement", 
        meta=(ClampMin=0.0, Tooltip="Speed for stable looping animations."))
    float LoopingSpeedThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Movement", 
        meta=(ClampMin=0.0, Tooltip="Speed threshold for transitions."))
    float TransitionSpeedThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Input", 
        meta=(ClampMin=0.0, ClampMax=1.0, Tooltip="Minimum input magnitude to register."))
    float MinInputMagnitude;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Input", 
        meta=(ClampMin=0.0, ClampMax=1.0, Tooltip="Input change considered significant."))
    float SignificantInputChange;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Input", 
        meta=(ClampMin=0.0, ClampMax=0.5, Tooltip="Input deadzone."))
    float InputDeadzone;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Angular", 
        meta=(ClampMin=0.0, ClampMax=180.0, Tooltip="Minimum angle for turn detection."))
    float TurnThresholdDegrees;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Angular", 
        meta=(ClampMin=0.0, ClampMax=180.0, Tooltip="Minimum angle for pivot detection."))
    float PivotThresholdDegrees;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Angular", 
        meta=(ClampMin=0.0, ClampMax=720.0, Tooltip="Angular velocity for pivoting."))
    float PivotAngularVelocityDeg;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Smoothing", 
        meta=(ClampMin=0.01, ClampMax=1.0, Tooltip="Position smoothing factor."))
    float PositionSmoothingFactor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Smoothing", 
        meta=(ClampMin=0.01, ClampMax=1.0, Tooltip="Velocity smoothing factor."))
    float VelocitySmoothingFactor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Smoothing", 
        meta=(ClampMin=0.01, ClampMax=1.0, Tooltip="Rotation smoothing factor."))
    float RotationSmoothingFactor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Smoothing", 
        meta=(ClampMin=1.0, ClampMax=100.0, Tooltip="Base input smoothing rate."))
    float InputSmoothingRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Smoothing", 
        meta=(ClampMin=1.0, ClampMax=100.0, Tooltip="Base acceleration smoothing rate."))
    float AccelerationSmoothingRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Adaptive", 
        meta=(ClampMin=1.0, ClampMax=100.0, Tooltip="Minimum adaptive smoothing rate."))
    float AdaptiveSmoothingMin;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Adaptive", 
        meta=(ClampMin=1.0, ClampMax=100.0, Tooltip="Maximum adaptive smoothing rate."))
    float AdaptiveSmoothingMax;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Adaptive", 
        meta=(ClampMin=0.0, ClampMax=1.0, Tooltip="Urgency threshold for adaptive smoothing."))
    float UrgencyThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Prediction", 
        meta=(Tooltip="Maximum prediction time."))
    float MaxPredictionTime;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Prediction", 
        meta=(Tooltip="Physics substep time."))
    float PredictionSubstepTime;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Prediction", 
        meta=(Tooltip="Prediction smoothing rate."))
    float PredictionSmoothingRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Prediction", 
        meta=(ClampMin=0.0, ClampMax=100.0, Tooltip="Trajectory variation amount."))
    float TrajectoryDivergence;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|States", 
        meta=(Tooltip="Duration for looping detection."))
    float LoopingDurationThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|States", 
        meta=(Tooltip="Duration for transition detection."))
    float TransitionDurationThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|States", 
        meta=(Tooltip="Duration for pivot detection."))
    float PivotDurationThreshold;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Features", 
        meta=(Tooltip="Use environment awareness."))
    bool bUseEnvironmentAwareness;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Features", 
        meta=(Tooltip="Account for root motion."))
    bool bAccountForRootMotion;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Features", 
        meta=(Tooltip="Predict stops."))
    bool bPredictStops;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Features", 
        meta=(Tooltip="Use adaptive smoothing."))
    bool bUseAdaptiveSmoothing;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Features", 
        meta=(Tooltip="Analyze motion states."))
    bool bAnalyzeMotionStates;
    
    /** Rotation configuration (nested struct) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trajectory|Rotation", meta=(ShowOnlyInnerProperties))
    FRotationConfig RotationConfig;
};

// ============================================================================
// FMotionData - Circular buffer for motion samples with analysis
// ============================================================================
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionData
{
    GENERATED_BODY()

protected:
    UPROPERTY()
    TArray<FMotionSample> Samples;
    
    UPROPERTY()
    TArray<FSampleImportance> ImportanceData;
    
    UPROPERTY()
    FMotionBufferConfig MotionBufferConfig;

private:
    int32 WriteIndex;
    int32 NumValidSamples;
    
    mutable EMotionComplexity CurrentComplexity;
    mutable float ComplexityScore;
    
    mutable uint32 CachedAnalysisFrame;
    mutable FMotionAnalysis CachedAnalysis;
    mutable FCharacterMotionContext CachedContext;
    
    mutable FVector LastSmoothedWorldIntent;
    mutable FVector LastSmoothedAcceleration;
    mutable bool bSmoothedStateInitialized;
    
    mutable float CurrentUrgency;
    mutable float LastStateChangeTime;
    mutable uint32 LastStateChangeFrame;
    mutable float CurrentStateStartTime;
    
public:
    FMotionData();
    
    void Initialize(const FMotionBufferConfig& InConfig);
    void InitializeDefault(int32 DesiredSize = 30);
    void AddSample(const FMotionSample& Sample);
    bool GetSampleByAge(int32 Age, FMotionSample& OutSample) const;
    FMotionSample GetNewestSample() const;
    FMotionSample GetOldestSample() const;
    FMotionSample GetSampleAtOffset(float OffsetSeconds) const;
    int32 GetSampleCount() const { return NumValidSamples; }
    bool IsEmpty() const { return NumValidSamples == 0; }
    void Clear();
    void ResetSmoothingState() const;
    void UpdateComplexity(const FMotionSample& Current, const FMotionSample& Previous) const;
    EMotionComplexity GetComplexity() const { return CurrentComplexity; }
    float GetMemoryUsageKB() const;
    FString GetDebugStats() const;
    
    FMotionAnalysis AnalyzeMotion(const FTrajectoryConfig& Config) const;
    FCharacterMotionContext GetCharacterContext(
        const FRotator& ControlRotation, 
        const FQuat& MeshRotation,
        const FTrajectoryConfig& Config) const;
    
    /**
     * Builds a Motion Matching trajectory with OPTIONAL speed matching
     * DECOUPLED VERSION - accepts speed config as parameter
     * 
     * @param TimeOffsets - Time offsets for trajectory samples  
     * @param OutTrajectory - Output trajectory for pose search
     * @param Config - Trajectory configuration
     * @param CharContext - Character motion context
     * @param AnimContext - Animation context
     * @param EnvContext - Environment context  
     * @param MovementComp - Character movement component for physics params
     * @param DeltaSeconds - Frame delta time
     * @param SpeedConfig - OPTIONAL speed matching configuration (pass nullptr to disable)
     * @param SpeedAnalysis - OPTIONAL speed analysis state (pass nullptr to disable)
     */
    void BuildMMTrajectory(
        const TArray<float>& TimeOffsets,
        FTransformTrajectory& OutTrajectory,
        const FTrajectoryConfig& Config,
        const FCharacterMotionContext& CharContext,
        const FAnimationContext& AnimContext,
        const FEnvironmentContext& EnvContext,
        UCharacterMovementComponent* MovementComp,
        float DeltaSeconds,
        const FDynamicSpeedMatchConfig* SpeedConfig = nullptr,
        const FDynamicSpeedAnalysis* SpeedAnalysis = nullptr) const;


    /**
     * Legacy wrapper for backward compatibility
     */
    void BuildMMTrajectorySimple(
        const TArray<float>& TimeOffsets,
        FTransformTrajectory& OutTrajectory,
        const FTrajectoryConfig& Config,
        const FCharacterMotionContext& CharContext,
        const FAnimationContext& AnimContext,
        const FEnvironmentContext& EnvContext,
        UCharacterMovementComponent* MovementComp,
        float DeltaSeconds) const
    {
        BuildMMTrajectory(TimeOffsets, OutTrajectory, Config, CharContext, 
                         AnimContext, EnvContext, MovementComp, DeltaSeconds, 
                         nullptr, nullptr);
    }
    
    FVector EstimateAcceleration(float LookbackTime = 0.1f) const;
    float EstimateAngularAcceleration(float LookbackTime = 0.1f) const;
    float EstimateConstrainedAngularVelocity(float LookbackTime, float MaxRotationRate) const;
    float GetSampleImportanceWeight(int32 Age) const;
    
    static EMovementDirection CalculateMovementDirectionRelativeToControl(
        const FVector& Velocity, const FRotator& ControlRotation);
    static EMovementDirection CalculateMovementDirection(
        const FVector& Velocity, const FQuat& MeshRotation);

    float GetCurrentUrgency() const { return CurrentUrgency; }
    float GetAdaptiveSmoothingRate(const FTrajectoryConfig& Config) const;

private:

    /**
     * Adjusts trajectory positions to imply target animation speeds
     * This is the main function that fixes speed mismatch
    */
    static void ApplySpeedMatchingToSegment(
        FVector& InOutPosition,
        const FVector& LastPosition,
        const FVector& CurrentWorldPosition,
        float ImpliedSpeed,
        float TimeOffset,
        const FDynamicSpeedMatchConfig& Config,
        const TArray<FSpeedCluster>& SpeedClusters);


    /**
        * Analyzes a pose search database to detect animation speeds
        * Call this once at startup or in editor
    */
    static float FindBestDatabaseSpeed(
        float CharacterSpeed, 
        const TArray<FSpeedCluster>& Clusters);

    
    static void CalculateImportance(
        const FMotionSample& Current, 
        const FMotionSample& Previous, 
        FSampleImportance& OutImportance);
    
    void UpdateAdaptiveSmoothing(const FMotionSample& Current, const FTrajectoryConfig& Config) const;
    void DetectMotionStates(FMotionAnalysis& Analysis, const FTrajectoryConfig& Config) const;
    
    void ApplyEnvironmentalAdjustments(
        FTransformTrajectorySample& Sample,
        const FVector& PredVel,
        float Time,
        const FEnvironmentContext& EnvContext,
        float BrakingDeceleration,
        const FVector& LastPosition) const;
    
    static FMotionSample InterpolateSamples(const FMotionSample& A, const FMotionSample& B, float Alpha);
    float DetectSteadyStateSpeed() const;
};






