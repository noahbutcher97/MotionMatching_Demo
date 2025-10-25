// Animation/.h - REFACTORED FOR ASYNC ENVIRONMENT SCANS
#pragma once

// At the top of MMAnimInstance.h, add these includes after the existing ones:

// Animation/MMAnimInstance.h - REFACTORED FOR ASYNC ENVIRONMENT SCANS
#pragma once

#include "CoreMinimal.h"
#include "Data/MMPrimaryStructs.h"
#include "Animation/AnimInstance.h"
#include "Data/MMSemanticTypes.h"
#include "PoseSearch/MMPoseSearchSpeedAnalyzer.h"
#include "MMAnimInstance.generated.h"



// (Struct definitions FMotionMatchingConfig and FEnvironmentScanStats remain unchanged)
USTRUCT(BlueprintType)
struct FMotionMatchingConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0.0, ClampMax=10.0))
    float TrajectoryWeight = 3.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0.0, ClampMax=10.0))
    float PoseWeight = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0.0, ClampMax=1.0))
    float ContinuingPoseBias = 0.1f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0.0, ClampMax=100.0))
    float DirectionChangeThreshold = 30.0f;
};

USTRUCT(BlueprintType)
struct FEnvironmentScanStats
{
    GENERATED_BODY()

    FEnvironmentScanStats()
    {
        TotalTraces = 0;
        CachedTraces = 0;
        SkippedTraces = 0;
        AsyncTraces = 0;
        LastFrameTraces = 0;
        AverageTracesPerFrame = 0.f;
        CacheHitRate = 0.f;
        CurrentLOD = EEnvironmentLOD::Full;
    }

    UPROPERTY(BlueprintReadOnly, Category="Stats")
    int32 TotalTraces;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    int32 CachedTraces;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    int32 SkippedTraces;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    int32 AsyncTraces;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    int32 LastFrameTraces;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    float AverageTracesPerFrame;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    float CacheHitRate;
    
    UPROPERTY(BlueprintReadOnly, Category="Stats")
    EEnvironmentLOD CurrentLOD;
};
UCLASS()
class MOTIONMATCHING_DEMO_API UMMAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    // Character reference
    UPROPERTY(BlueprintReadOnly, Category="MM|References")
    class AMMCharacter* MMCharacter;
    // OPTIMIZED Motion tracking with adaptive buffer
    UPROPERTY(BlueprintReadOnly, Category="OH|Motion")
    FMotionData MotionHistory;
    
    // Motion buffer configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|Motion", meta=(ShowOnlyInnerProperties))
    FMotionBufferConfig MotionBufferConfig;
    
    // Current motion analysis
    UPROPERTY(BlueprintReadOnly, Category="MM|Motion")
    FMotionAnalysis CurrentMotionAnalysis;
    
    UPROPERTY(BlueprintReadOnly, Category="MM|Motion")
    FCharacterMotionContext CurrentMotionContext;
    
    // Animation feedback
    UPROPERTY(BlueprintReadOnly, Category="MM|Animation")
    FAnimationContext AnimContext;
    
    // CACHED Environment awareness
    UPROPERTY(BlueprintReadOnly, Category="MM|Environment")
    FEnvironmentContext EnvContext;
    
    // Environment cache configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|Environment", meta=(ShowOnlyInnerProperties))
    FEnvironmentCacheConfig EnvironmentCacheConfig;
    
    // Environment scan statistics
    UPROPERTY(BlueprintReadOnly, Category="MM|Environment")
    FEnvironmentScanStats ScanStats;
    
    // Trajectory configuration with rotation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|Trajectory", meta=(ShowOnlyInnerProperties))
    FTrajectoryConfig TrajectoryConfig;
    
    // Trajectory time offsets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|Trajectory")
    TArray<float> TrajectoryTimeOffsets = { -0.4f, -0.2f, 0.0f, 0.15f, 0.3f, 0.45f, 0.6f, 0.8f };
    
    // The trajectory for Motion Matching
    UPROPERTY(BlueprintReadOnly, Category="MM|Trajectory")
    FTransformTrajectory TrajectoryForMM;
    
    // Motion Matching database reference
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|MotionMatching", meta=(Tooltip="The Pose Search Database used for motion matching queries."))
    class UPoseSearchDatabase* MotionMatchingDatabase;
    
    // Motion matching control flags
    UPROPERTY(BlueprintReadOnly, Category="MM|MotionMatching", meta=(Tooltip="If true, forces a motion matching update this frame, ignoring animation state interruptibility."))
    bool bForceMotionMatchingUpdate = false;
    
    UPROPERTY(BlueprintReadOnly, Category="MM|MotionMatching", meta=(Tooltip="The urgency for a motion matching update, typically driven by rapid changes in direction."))
    float MotionMatchingUpdateUrgency = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="MM|MotionMatching", meta=(Tooltip="A weight from 0-1 indicating the significance of a recent direction change, used to bias motion matching."))
    float DirectionChangeWeight = 0.0f;
    
    // Debug
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bDebugDrawTrajectory = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bDebugDrawEnvironmentCache = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bShowPerformanceStats = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bDebugDrawRotation = false;

protected:
    UPROPERTY()
    UCharacterMovementComponent* CachedMovementComponent = nullptr;

    // NEW: Speed analyzer
    UPROPERTY()
    UMMPoseSearchSpeedAnalyzer* SpeedAnalyzer = nullptr;

    // NEW: Cached semantic speed clusters  
    UPROPERTY(BlueprintReadOnly, Category="MM|Speed Matching")
    TArray<FSemanticSpeedCluster> CachedSemanticClusters;

    /**
    * Initialize speed clustering system from database
    */
    void InitializeSpeedClusters();

    /**
    * Convert semantic clusters to legacy speed cluster format
    */
    TArray<FSpeedCluster> ConvertSemanticToSpeedClusters(const TArray<FSemanticSpeedCluster>& SemanticClusters);
    
    // Environmental cache
    FCachedEnvironmentData EnvironmentCache;
    TMap<FIntVector, FSpatialHashEntry> SpatialCache;
    
    // NEW: Async trace handles and state
    FTraceHandle GroundTraceHandle;
    FTraceHandle EdgeTraceHandle;
    FTraceHandle WallTraceHandle; // Single handle for new sweep trace
    FTraceHandle CeilingTraceHandle;

    bool bIsGroundTracePending;
    bool bIsEdgeTracePending;
    bool bIsWallTracePending;
    bool bIsCeilingTracePending;
    
    float LocalTimeSeconds;
    FQuat LastMeshRotation;
    float LastControlYaw;
    bool bShouldTriggerMotionMatching;
    FVector LastLocalVelocity;
    float DirectionChangeAccumulator;
    
    // Frame timing for performance
    float LastFrameTime;
    float FrameDeltaAccumulator;
    int32 FrameCounter;

    void UpdateRotationSystem(float DeltaSeconds);
    
    // REFACTORED: Environmental scanning functions now dispatch async requests
    void UpdateEnvironmentCache(float DeltaSeconds);
    void RequestAsyncScans(EEnvironmentLOD LOD, const FVector& CharPos, const FVector& CharForward);
    void ProcessAsyncTraces(); // Now processes results and updates cache
    
    EEnvironmentLOD SelectEnvironmentLOD() const;
    FIntVector GetSpatialHashKey(const FVector& Position) const;
    bool TryGetCachedEnvironment(const FVector& Position, FCachedEnvironmentData& OutData);
    void StoreCachedEnvironment(const FVector& Position, const FCachedEnvironmentData& Data);
    
    void ExtractAnimationContext(float DeltaSeconds);
    void HandleRootMotion(FMotionSample& Sample) const;
    
    FQuat GetMeshRotation() const;
    
    void UpdatePerformanceStats(float DeltaSeconds);
    
public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    
    UFUNCTION(BlueprintCallable, Category="MM|Motion")
    void GatherMotionData(float DeltaSeconds);
    
    UFUNCTION(BlueprintCallable, Category="MM|Motion")
    void BuildTrajectory(float DeltaSeconds);

    /**
 * Build trajectory with semantic speed cluster matching
 * Uses cached semantic clusters from database analysis
 */
    void BuildTrajectoryWithSpeedMatch(float DeltaSeconds);

    
    void BuildTrajectoryWithCustomSpeedModel(
        const TArray<FSpeedCluster>& CustomSpeedClusters,
        float SpeedMatchStrength);
    
    void BuildTrajectoryNoSpeedMatch(float DeltaSeconds);
    
    UFUNCTION(BlueprintCallable, Category="Debug")
    void DebugDrawTrajectory() const;
    
    UFUNCTION(BlueprintCallable, Category="Debug")
    void DebugDrawEnvironmentCache() const;
    
    UFUNCTION(BlueprintCallable, Category="Debug")
    FString GetPerformanceReport() const;
    
    // Motion state accessors
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    ELocomotionState GetCurrentLocomotionState() const { return CurrentMotionAnalysis.CurrentState; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    bool IsInContinuousLoop() const { return CurrentMotionAnalysis.bIsLooping; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    bool IsTransitioning() const { return CurrentMotionAnalysis.bIsTransitioning; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    bool IsPivoting() const { return CurrentMotionAnalysis.bIsPivoting; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    EMovementDirection GetMovementDirection() const { return CurrentMotionContext.MovementDirection; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    float GetSpeed() const { return CurrentMotionContext.Speed; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    float GetControlToMeshAngle() const { return CurrentMotionContext.ControlToMeshAngleYaw; }
    
    UFUNCTION(BlueprintPure, Category="MM|Motion")
    EMotionComplexity GetMotionComplexity() const { return MotionHistory.GetComplexity(); }
    
    // Environment cache stats
    UFUNCTION(BlueprintPure, Category="MM|Environment")
    float GetEnvironmentCacheHitRate() const { return EnvironmentCache.GetCacheHitRate(); }
    
    UFUNCTION(BlueprintPure, Category="MM|Environment")
    EEnvironmentLOD GetCurrentEnvironmentLOD() const { return EnvironmentCache.CurrentLOD; }

    UFUNCTION(BlueprintPure, Category="MM|Rotation")
    float GetRotationLag() const { return TrajectoryConfig.RotationConfig.RotationLag; }
    
    UFUNCTION(BlueprintPure, Category="MM|Rotation")
    bool IsRotationLagging() const { return TrajectoryConfig.RotationConfig.bIsLagging; }
    
    
    // Animation helpers
    UFUNCTION(BlueprintCallable, Category="MM|Animation")
    bool IsAnimationInterruptible() const { return AnimContext.bIsInterruptible; }
    
    UFUNCTION(BlueprintCallable, Category="MM|Animation")
    float GetTimeToNextFootPlant() const 
    { 
        return AnimContext.NextFootPlantTime > 0 ? 
               FMath::Max(0.f, AnimContext.NextFootPlantTime - LocalTimeSeconds) : -1.f; 
    }
    
    UFUNCTION(BlueprintCallable, Category="Debug")
    void DebugDrawRotation() const;

    /** Sets the primary rotation mode for the character. */
    UFUNCTION(BlueprintCallable, Category="MM|Rotation", meta=(Tooltip="Sets the primary rotation control system. Can be either the native Character Movement Component or the custom manual system."))
    void SetRotationMode(bool bUseCMC, ECMCRotationMode CMCMode, EManualRotationMode ManualMode);
    
    /** Sets a world-space target for the LookAt rotation mode to track. */
    UFUNCTION(BlueprintCallable, Category="MM|Rotation", meta=(Tooltip="Sets a world-space target for the 'LookAt' rotation mode. If bEnable is true, this will also switch the Manual Mode to LookAt."))
    void SetLookAtTarget(const FVector& Target, bool bEnable);
    
    UFUNCTION(BlueprintPure, Category="MM|Rotation")
    bool IsUsingCMCRotation() const { return TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation; }
    
    UFUNCTION(BlueprintPure, Category="MM|Rotation")
    FString GetCurrentRotationMode() const { return TrajectoryConfig.RotationConfig.GetCurrentModeDescription(); }
    
    UFUNCTION(BlueprintPure, Category="MM|Rotation")
    float GetCurrentRotationRate() const { return TrajectoryConfig.RotationConfig.GetCurrentRotationRate(); }

public:

    // Motion matching selection tracking
    UPROPERTY(BlueprintReadWrite, Category="Motion Matching")
    int32 LastSelectedPoseIndex = -1;
    
    UPROPERTY(BlueprintReadWrite, Category="Motion Matching")
    float LastSelectedPoseTime = 0.f;
    
    UPROPERTY(BlueprintReadWrite, Category="Motion Matching")
    FName LastSelectedAnimationName;
    
    UPROPERTY(BlueprintReadWrite, Category="Motion Matching")
    float LastSelectedAnimationSpeed = 0.f;
    
    // Speed Matching Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|Speed Matching")
    FDynamicSpeedMatchConfig SpeedMatchConfig;
    
    UPROPERTY(BlueprintReadOnly, Category="MM|Speed Matching")
    FDynamicSpeedAnalysis SpeedAnalysis;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MM|Speed Matching")
    bool bEnableSpeedMatching = true;

private:
    TArray<float> SpeedSampleHistory;
    float SpeedSampleTimer = 0.f;
};

