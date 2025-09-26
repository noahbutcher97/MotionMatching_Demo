#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimationAsset.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Data/MMPrimaryStructs.h"
#include "Animation/TrajectoryTypes.h"
#include "MMPoseSearchUtils.generated.h"

class UMMAnimInstance;

// ============================================================================
// Pose Search Utilities
// ============================================================================
UCLASS()
class MOTIONMATCHING_DEMO_API UMMPoseSearchUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========== Core Speed Matching Functions ==========
    
    /**
     * Gets the target animation speed based on current actual speed and detected clusters
     * @param ActualSpeed - Current character velocity magnitude
     * @param InOutAnalysis - Speed analysis data (will be updated)
     * @param Config - Speed matching configuration
     * @param DeltaTime - Frame delta time for transitions
     * @return Target speed for trajectory generation
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Speed", meta=(DisplayName="Get Target Animation Speed"))
    static float GetTargetAnimationSpeed(
        float ActualSpeed,
        UPARAM(ref) FDynamicSpeedAnalysis& InOutAnalysis,
        const FDynamicSpeedMatchConfig& Config,
        float DeltaTime);
    
    /**
     * Adjusts trajectory positions to imply target animation speeds
     * This is the main function that fixes speed mismatch
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Trajectory", meta=(DisplayName="Apply Speed Matching To Trajectory"))
    static void ApplySpeedMatchingToTrajectory(
        UPARAM(ref) FTransformTrajectory& Trajectory,
        const FDynamicSpeedAnalysis& Analysis,
        const FDynamicSpeedMatchConfig& Config);    
    
    /**
     * Detects speed clusters from a set of speed samples
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Analysis", meta=(DisplayName="Detect Speed Clusters"))
    static void DetectSpeedClusters(
        const TArray<float>& SpeedSamples,
        UPARAM(ref) FDynamicSpeedAnalysis& InOutAnalysis,
        const FDynamicSpeedMatchConfig& Config);
    
    /**
     * Samples the current animation's root motion speed
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Sampling", meta=(DisplayName="Sample Animation Speed"))
    static float SampleAnimationSpeed(
        const UAnimInstance* AnimInstance);
    static float GetMotionMatchingAnimationSpeed(const UMMAnimInstance* AnimInstance);

    /**
     * Analyzes a pose search database to detect animation speeds
     * Call this once at startup or in editor
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Analysis", meta=(DisplayName="Analyze Database Speeds", CallInEditor="true"))
    static TArray<float> AnalyzeDatabaseSpeeds(
        const UPoseSearchDatabase* Database);

    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Debug")
    static float GetSpeedMatchRatio(const UMMAnimInstance* AnimInstance);

    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Analysis")
    static void InitializeSpeedMatchingFromDatabase(
        UPARAM(ref) FDynamicSpeedAnalysis& InOutAnalysis,
        const UPoseSearchDatabase* Database);

    UFUNCTION(BlueprintPure, Category="MM|PoseSearch|Debug")
    static bool IsSpeedMatchingValid(
        const FDynamicSpeedAnalysis& Analysis,
        float CharacterSpeed);

    UFUNCTION(BlueprintPure, Category="MM|PoseSearch|Debug")
    static float GetPoseSearchQuerySpeed(const FTransformTrajectory& Trajectory);
    // ========== Debug Functions ==========
    
    /**
     * Draws debug visualization for speed matching
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Debug", meta=(WorldContext="WorldContextObject", DisplayName="Draw Speed Match Debug"))
    static void DrawSpeedMatchDebug(
        const UObject* WorldContextObject,
        const FVector& Location,
        const FDynamicSpeedAnalysis& Analysis,
        const FDynamicSpeedMatchConfig& Config);
    
    /**
     * Calculates what velocities will be derived from trajectory positions
     */
    UFUNCTION(BlueprintPure, Category="MM|PoseSearch|Debug", meta=(DisplayName="Calculate Derived Velocities"))
    static TArray<FVector> CalculateDerivedVelocities(
        const FTransformTrajectory& Trajectory);
    
    /**
     * Measures foot sliding in current animation
     */
    UFUNCTION(BlueprintCallable, Category="MM|PoseSearch|Debug", meta=(DisplayName="Measure Foot Sliding"))
    static float MeasureFootSliding(
        const USkeletalMeshComponent* Mesh,
        const FName& FootBoneName,
        float DeltaTime);
    
    /**
     * Gets a quality score for current speed matching (0-1)
     */
    UFUNCTION(BlueprintPure, Category="MM|PoseSearch|Debug", meta=(DisplayName="Get Speed Match Quality"))
    static float GetSpeedMatchQuality(
        float ActualSpeed,
        float TargetSpeed,
        float AnimationSpeed);

private:
    // Internal clustering algorithm
    static void PerformKMeansClustering(
        const TArray<float>& Speeds,
        int32 K,
        TArray<FSpeedCluster>& OutClusters,
        int32 MaxIterations = 100);
    
    static int32 FindNearestCluster(
        float Speed,
        const TArray<FSpeedCluster>& Clusters);
    
    static void MergeSimilarClusters(
        TArray<FSpeedCluster>& Clusters,
        float MergeThreshold);
};

