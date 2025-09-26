// Public/PoseSearch/MMPoseSearchSpeedAnalyzer.h
#pragma once
#if 0
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchContext.h"
#include "Core/MMSemanticSubsystem.h"
#include "Core/MMMotionSemanticAnalyzer.h"
#include "Animation/AnimSequence.h"
#include "Curves/CurveFloat.h"
#include "MMPoseSearchSpeedAnalyzer.generated.h"

DECLARE_STATS_GROUP(TEXT("MMSpeedAnalysis"), STATGROUP_MMSpeedAnalysis, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GenerateSpeedClusters"), STAT_MMGenerateClusters, STATGROUP_MMSpeedAnalysis);
DECLARE_CYCLE_STAT(TEXT("AnalyzeAnimationSpeed"), STAT_MMAnalyzeSpeed, STATGROUP_MMSpeedAnalysis);
DECLARE_CYCLE_STAT(TEXT("DatabaseIndexing"), STAT_MMSpeedIndexing, STATGROUP_MMSpeedAnalysis);

/**
 * Semantic speed profile for an animation
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSemanticSpeedProfile
{
    GENERATED_BODY()
    
    /** Animation identification */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    FString AnimationName;
    
    /** Asset reference */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    TSoftObjectPtr<UAnimationAsset> AnimationAsset;
    
    /** Semantic analysis of this animation */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    FMotionSemanticResult SemanticAnalysis;
    
    /** Average speed in cm/s */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float AverageSpeed = 0.0f;
    
    /** Minimum speed in cm/s */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float MinSpeed = 0.0f;
    
    /** Maximum speed in cm/s */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float MaxSpeed = 0.0f;
    
    /** Speed variance */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float SpeedVariance = 0.0f;
    
    /** Speed standard deviation */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float SpeedStandardDeviation = 0.0f;
    
    /** Speed curve sampled over normalized time (0-1) */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    TArray<float> SpeedCurve;
    
    /** Acceleration characteristics */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float AverageAcceleration = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    float MaxAcceleration = 0.0f;
    
    /** Semantic speed category */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    EMotionSpeed SemanticSpeedLevel = EMotionSpeed::Idle;
    
    /** How well this animation transitions to other speed levels */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    TMap<EMotionSpeed, float> TransitionSuitability;
    
    /** Database index for fast lookup */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    int32 DatabaseIndex = INDEX_NONE;
    
    /** Pose index in the search index */
    UPROPERTY(BlueprintReadOnly, Category="Speed Profile")
    int32 PoseIndex = INDEX_NONE;
    
    /** Last update timestamp */
    float LastUpdateTime = 0.0f;
    
    FSemanticSpeedProfile()
    {
        SpeedCurve.Reserve(30); // Reserve for ~1 second at 30fps
    }
    
    bool IsValid() const { return !AnimationName.IsEmpty() && AverageSpeed > 0.0f; }
};

/**
 * Enhanced speed cluster with semantic understanding
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSemanticSpeedCluster
{
    GENERATED_BODY()
    
    /** Cluster identifier */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    FString ClusterName;
    
    /** Primary semantic intent for this cluster */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    EMotionIntent PrimaryIntent = EMotionIntent::Idle;
    
    /** Semantic speed level */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    EMotionSpeed SpeedLevel = EMotionSpeed::Idle;
    
    /** Speed statistics */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float CenterSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float MinSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float MaxSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float StandardDeviation = 0.0f;
    
    /** Transition thresholds */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float EntryThreshold = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float ExitThreshold = 0.0f;
    
    /** Semantic-aware hysteresis band */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float HysteresisWidth = 50.0f;
    
    /** Animations in this cluster */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    TArray<FSemanticSpeedProfile> AnimationProfiles;
    
    /** Number of animations */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    int32 AnimationCount = 0;
    
    /** Average semantic confidence for this cluster */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float SemanticConfidence = 0.0f;
    
    /** Preferred blend time when transitioning to this cluster */
    UPROPERTY(BlueprintReadOnly, Category="Cluster")
    float PreferredBlendTime = 0.3f;
    
    /** Priority for selection when multiple clusters match */
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
 * Speed transition curve with semantic awareness
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSpeedTransitionCurve
{
    GENERATED_BODY()
    
    /** Source cluster */
    UPROPERTY(BlueprintReadOnly, Category="Transition")
    FString FromCluster;
    
    /** Target cluster */
    UPROPERTY(BlueprintReadOnly, Category="Transition")
    FString ToCluster;
    
    /** Blend curve asset */
    UPROPERTY(BlueprintReadWrite, Category="Transition")
    UCurveFloat* BlendCurve = nullptr;
    
    /** Optimal transition duration based on semantic analysis */
    UPROPERTY(BlueprintReadOnly, Category="Transition")
    float OptimalDuration = 0.3f;
    
    /** How smooth this transition is semantically (0-1) */
    UPROPERTY(BlueprintReadOnly, Category="Transition")
    float SemanticSmoothness = 1.0f;
    
    /** Speed delta threshold for using this transition */
    UPROPERTY(BlueprintReadOnly, Category="Transition")
    float SpeedDeltaThreshold = 100.0f;
};

/**
 * Configuration for semantic speed analysis
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FSemanticSpeedConfig
{
    GENERATED_BODY()
    
    /** Minimum animations required to form a cluster */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=2, ClampMax=20))
    int32 MinAnimationsPerCluster = 3;
    
    /** Maximum number of clusters to generate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=2, ClampMax=50))
    int32 MaxClusters = 20;
    
    /** Speed variance threshold for splitting clusters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=10.0f))
    float ClusterSplitThreshold = 100.0f;
    
    /** Merge clusters if their speeds are within this range */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=10.0f))
    float ClusterMergeThreshold = 50.0f;
    
    /** Weight for semantic similarity when clustering (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=0.0f, ClampMax=1.0f))
    float SemanticWeight = 0.4f;
    
    /** Weight for speed similarity when clustering (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clustering", meta=(ClampMin=0.0f, ClampMax=1.0f))
    float SpeedWeight = 0.6f;
    
    /** Sample rate for speed curves (samples per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Analysis", meta=(ClampMin=10, ClampMax=60))
    int32 SpeedCurveSampleRate = 30;
    
    /** Enable caching of speed profiles */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
    bool bEnableCaching = true;
    
    /** Maximum cache age in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance", meta=(ClampMin=1.0f))
    float MaxCacheAge = 300.0f;
    
    /** Debug visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bDebugDraw = false;
    
    /** Log detailed clustering information */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bVerboseLogging = false;
};

/**
 * Speed analyzer that uses semantic understanding to create intelligent speed clusters
 * This replaces the non-functional cost modification approach with database analysis
 */
UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UMMPoseSearchSpeedAnalyzer : public UObject
{
    GENERATED_BODY()
    
public:
    UMMPoseSearchSpeedAnalyzer();
    
    // ========== INITIALIZATION ==========
    
    /**
     * Initialize with semantic subsystem
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void Initialize(UMMSemanticSubsystem* InSemanticSubsystem);
    
    /**
     * Shutdown and cleanup
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void Shutdown();
    
    // ========== DATABASE ANALYSIS ==========
    
    /**
     * Index a database with semantic speed analysis
     * @param Database - The PoseSearch database to analyze
     * @param Config - Configuration for analysis
     * @param bForceReindex - Force reanalysis even if cached
     * @return Number of animations analyzed
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    int32 IndexDatabaseWithSemantics(
        UPoseSearchDatabase* Database,
        const FSemanticSpeedConfig& Config,
        bool bForceReindex = false);
    
    /**
     * Generate semantic speed clusters from a database
     * @param Database - The database to analyze
     * @param Config - Clustering configuration
     * @return Array of semantic speed clusters
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    TArray<FSemanticSpeedCluster> GenerateSemanticSpeedClusters(
        UPoseSearchDatabase* Database,
        const FSemanticSpeedConfig& Config);
    
    /**
     * Analyze a single animation's speed profile
     * @param AnimationAsset - The animation to analyze
     * @return Semantic speed profile
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    FSemanticSpeedProfile AnalyzeAnimationSpeed(UAnimationAsset* AnimationAsset);
    
    // ========== RUNTIME SPEED MATCHING ==========
    
    /**
     * Get optimal speed for current semantic context
     * @param CurrentSemantics - Current motion semantics
     * @param AvailableClusters - Available speed clusters
     * @return Optimal target speed in cm/s
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    float GetOptimalSpeedForSemanticContext(
        const FMotionSemanticResult& CurrentSemantics,
        const TArray<FSemanticSpeedCluster>& AvailableClusters);
    
    /**
     * Find best matching cluster for a given speed and semantic context
     * @param Speed - Current speed in cm/s
     * @param Semantics - Current semantic context
     * @param Clusters - Available clusters
     * @return Best matching cluster index (-1 if none)
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    int32 FindBestCluster(
        float Speed,
        const FMotionSemanticResult& Semantics,
        const TArray<FSemanticSpeedCluster>& Clusters);
    
    /**
     * Generate transition curve between clusters
     * @param FromCluster - Source cluster
     * @param ToCluster - Target cluster
     * @param SemanticContext - Current semantic context
     * @return Transition curve parameters
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    FSpeedTransitionCurve GenerateSemanticSpeedTransition(
        const FSemanticSpeedCluster& FromCluster,
        const FSemanticSpeedCluster& ToCluster,
        const FMotionSemanticResult& SemanticContext);
    
    /**
     * Calculate semantic distance between speed profiles
     * @param ProfileA - First profile
     * @param ProfileB - Second profile
     * @return Distance metric (0 = identical, 1 = completely different)
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    float CalculateProfileDistance(
        const FSemanticSpeedProfile& ProfileA,
        const FSemanticSpeedProfile& ProfileB);
    
    // ========== CACHE MANAGEMENT ==========
    
    /**
     * Clear all cached data
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void ClearCache();
    
    /**
     * Get memory usage in bytes
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    int32 GetMemoryUsage() const;
    
    /**
     * Clean old cached entries
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void CleanupCache(float MaxAge = 300.0f);
    
    // ========== DEBUGGING ==========
    
    /**
     * Draw debug visualization
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis|Debug")
    void DrawDebugClusters(
        UWorld* World,
        const FVector& Location,
        const TArray<FSemanticSpeedCluster>& Clusters,
        int32 CurrentClusterIndex = -1);

    /**
     * Export cluster data to CSV
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis|Debug")
    FString ExportClustersToCSV(const TArray<FSemanticSpeedCluster>& Clusters);
    
protected:
    /** Reference to semantic subsystem */
    UPROPERTY()
    UMMSemanticSubsystem* SemanticSubsystem = nullptr;
    
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

    static float CalculateTransitionScore(EMotionSpeed FromSpeed, EMotionSpeed ToSpeed);

};
#endif