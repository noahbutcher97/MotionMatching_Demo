// MMPoseSearchSpeedAnalyzer.h - CLEANED VERSION
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Data/MMSemanticTypes.h"
#include "MMPoseSearchSpeedAnalyzer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMMSpeedAnalysis, Log, All);

/**
 * Semantic speed profile for an animation
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSemanticSpeedProfile
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    FString AnimationName;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    TSoftObjectPtr<UAnimationAsset> AnimationAsset;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    FMotionSemanticResult SemanticAnalysis;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float AverageSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float MinSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float MaxSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float SpeedVariance = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float SpeedStandardDeviation = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    EMotionSpeed SemanticSpeedLevel = EMotionSpeed::Idle;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    TMap<EMotionSpeed, float> TransitionSuitability;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    int32 DatabaseIndex = INDEX_NONE;
    
    float LastUpdateTime = 0.0f;
    
    bool IsValid() const { return !AnimationName.IsEmpty() && AverageSpeed >= 0.0f; }
};

/**
 * Enhanced speed cluster with semantic understanding
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSemanticSpeedCluster
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    FString ClusterName;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    EMotionIntent PrimaryIntent = EMotionIntent::Idle;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    EMotionSpeed SpeedLevel = EMotionSpeed::Idle;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float CenterSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float MinSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float MaxSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float StandardDeviation = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float EntryThreshold = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float ExitThreshold = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float HysteresisWidth = 50.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    TArray<FSemanticSpeedProfile> AnimationProfiles;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    int32 AnimationCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float SemanticConfidence = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    int32 Priority = 0;
    
    bool ContainsSpeed(float Speed) const
    {
        return Speed >= EntryThreshold && Speed <= ExitThreshold;
    }
    
    float GetDistanceToSpeed(float Speed) const
    {
        if (ContainsSpeed(Speed)) return 0.0f;
        return FMath::Min(
            FMath::Abs(Speed - EntryThreshold),
            FMath::Abs(Speed - ExitThreshold)
        );
    }
};

/**
 * Configuration for semantic speed analysis
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSemanticSpeedConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=2, ClampMax=20))
    int32 MinAnimationsPerCluster = 2;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=2, ClampMax=50))
    int32 MaxClusters = 10;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=10.0f))
    float ClusterMergeThreshold = 75.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=0.0f, ClampMax=1.0f))
    float SemanticWeight = 0.4f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Analysis", meta=(ClampMin=10, ClampMax=60))
    int32 SpeedCurveSampleRate = 30;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
    bool bEnableCaching = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bVerboseLogging = false;
};

/**
 * Simple speed analyzer using only animation name parsing
 * NO external plugin dependencies
 */
UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UMMPoseSearchSpeedAnalyzer : public UObject
{
    GENERATED_BODY()
    
public:
    UMMPoseSearchSpeedAnalyzer();
    
    // ========== INITIALIZATION ==========
    
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void Initialize();
    
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void Shutdown();
    
    // ========== DATABASE ANALYSIS ==========
    
    /**
     * Index a database with semantic speed analysis
     * Analyzes animation NAMES to determine speed clusters
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    int32 IndexDatabaseWithSemantics(
        UPoseSearchDatabase* Database,
        const FSemanticSpeedConfig& Config,
        bool bForceReindex = false);
    
    /**
     * Generate semantic speed clusters from a database
     * Groups animations by detected speed from their names
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    TArray<FSemanticSpeedCluster> GenerateSemanticSpeedClusters(
        UPoseSearchDatabase* Database,
        const FSemanticSpeedConfig& Config);
    
    /**
     * Analyze a single animation's speed profile
     * Combines name analysis with actual root motion speed
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    FSemanticSpeedProfile AnalyzeAnimationSpeed(UAnimationAsset* AnimationAsset);
    
    // ========== RUNTIME SPEED MATCHING ==========
    
    /**
     * Find best matching cluster for current speed
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    int32 FindBestCluster(
        float CurrentSpeed,
        const TArray<FSemanticSpeedCluster>& Clusters);
    
    // ========== CACHE MANAGEMENT ==========
    
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void ClearCache();
    
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    int32 GetMemoryUsage() const;
    
protected:
    /** Cached speed profiles per database */
    TMap<FObjectKey, TMap<int32, FSemanticSpeedProfile>> DatabaseProfiles;
    
    /** Cached clusters per database */
    TMap<FObjectKey, TArray<FSemanticSpeedCluster>> DatabaseClusters;
    
    /** Thread safety */
    mutable FCriticalSection CacheMutex;
    
private:
    /** Extract speed data from animation sequence */
    static FSemanticSpeedProfile ExtractSpeedFromSequence(UAnimSequence* Sequence);
    
    /** Calculate speed statistics */
    static void CalculateSpeedStatistics(TArray<float>& Speeds, FSemanticSpeedProfile& OutProfile);
    
    /** Perform hierarchical clustering */
    static TArray<FSemanticSpeedCluster> PerformHierarchicalClustering(
        const TArray<FSemanticSpeedProfile>& Profiles,
        const FSemanticSpeedConfig& Config);
    
    /** Merge similar clusters */
    static void MergeSimilarClusters(
        TArray<FSemanticSpeedCluster>& Clusters,
        const FSemanticSpeedConfig& Config);
    
    /** Calculate cluster statistics */
    static void CalculateClusterStatistics(FSemanticSpeedCluster& Cluster);
    
    /** Determine semantic thresholds */
    static void DetermineSemanticThresholds(
        FSemanticSpeedCluster& Cluster,
        const FSemanticSpeedConfig& Config);
};