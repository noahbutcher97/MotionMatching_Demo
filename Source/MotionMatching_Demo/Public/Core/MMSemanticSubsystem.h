// MMSemanticSubsystem.h
#pragma once
#if 0
#include "CoreMinimal.h"
//#include "Engine/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Core/MMMotionSemanticAnalyzer.h"
#include "Core/MMFuzzyProcessor.h"
#include "Core/MMFuzzyCore.h"
#include "MMSemanticSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMMSemantic, Log, All);

// Declare stats group for profiling
DECLARE_STATS_GROUP(TEXT("MMSemantic"), STATGROUP_MMSemantic, STATCAT_Advanced);

// ============================================================================
// ENUMS
// ============================================================================

UENUM(BlueprintType)
enum class EMMCacheTier : uint8
{
    Frame    UMETA(DisplayName="Frame Cache (L1)"),
    Memory   UMETA(DisplayName="Memory Cache (L2)"),
    Disk     UMETA(DisplayName="Disk Cache (L3) - Future"),
    None     UMETA(DisplayName="No Cache")
};

UENUM(BlueprintType)
enum class EMMRequestPriority : uint8
{
    Low      UMETA(DisplayName="Low Priority"),
    Normal   UMETA(DisplayName="Normal Priority"),
    High     UMETA(DisplayName="High Priority"),
    Critical UMETA(DisplayName="Critical - Process Immediately")
};

// ============================================================================
// STRUCTS
// ============================================================================

USTRUCT(BlueprintType)
struct FMMCachedResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FMotionSemanticResult Result;
    
    UPROPERTY(BlueprintReadOnly)
    double CacheTime = 0.0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 AccessCount = 0;
    
    bool IsExpired(float ExpirationSeconds) const
    {
        return (FPlatformTime::Seconds() - CacheTime) > ExpirationSeconds;
    }
};

USTRUCT(BlueprintType)
struct FMMSemanticRequest
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite)
    FString Query;
    
    UPROPERTY(BlueprintReadWrite)
    EMMRequestPriority Priority = EMMRequestPriority::Normal;
    
    UPROPERTY(BlueprintReadWrite)
    int32 MaxExpansions = 10;
    
    UPROPERTY(BlueprintReadWrite)
    float MinConfidence = 0.5f;
    
    UPROPERTY(BlueprintReadOnly)
    int32 RequestId = -1;
    
    UPROPERTY(BlueprintReadOnly)
    double RequestTime = 0.0;
    
    uint32 GetHash() const
    {
        uint32 Hash = GetTypeHash(Query.ToLower());
        Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Priority)));
        Hash = HashCombine(Hash, GetTypeHash(MaxExpansions));
        Hash = HashCombine(Hash, GetTypeHash(MinConfidence));
        return Hash;
    }
};

USTRUCT(BlueprintType)
struct FMMIndexedAnimation
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    TSoftObjectPtr<class UAnimSequenceBase> Animation;
    
    UPROPERTY(BlueprintReadOnly)
    FMotionSemanticResult Semantics;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> SearchTags;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> NormalizedTags;
    
    UPROPERTY(BlueprintReadOnly)
    int32 UsageCount = 0;
    
    UPROPERTY(BlueprintReadOnly)
    float LastUsedTime = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    FName SourceDatabase;
};

// Improved inverted index structure for efficient searching
USTRUCT()
struct FMMSemanticIndex
{
    GENERATED_BODY()
    
    // Inverted indices for fast lookup
    TMultiMap<FString, FName> TagToAnimation;
    TMultiMap<EMotionIntent, FName> IntentToAnimation;
    TMultiMap<EMotionSpeed, FName> SpeedToAnimation;
    TMultiMap<EMotionDirection, FName> DirectionToAnimation;
    
    // Spatial index for semantic vectors (future use)
    TArray<TPair<FVector, FName>> SemanticVectors;
    
    void Clear()
    {
        TagToAnimation.Empty();
        IntentToAnimation.Empty();
        SpeedToAnimation.Empty();
        DirectionToAnimation.Empty();
        SemanticVectors.Empty();
    }
    
    void AddAnimation(FName AnimName, const FMMIndexedAnimation& IndexedAnim)
    {
        // Add to tag index
        for (const FString& Tag : IndexedAnim.NormalizedTags)
        {
            TagToAnimation.Add(Tag, AnimName);
        }
        
        // Add to intent index
        if (IndexedAnim.Semantics.Intent.PrimaryIntent != EMotionIntent::None)
        {
            IntentToAnimation.Add(IndexedAnim.Semantics.Intent.PrimaryIntent, AnimName);
        }
        
        // Add to speed index  
        SpeedToAnimation.Add(IndexedAnim.Semantics.Speed.SpeedLevel, AnimName);
        
        // Add to direction index
        if (IndexedAnim.Semantics.Direction.Direction != EMotionDirection::None)
        {
            DirectionToAnimation.Add(IndexedAnim.Semantics.Direction.Direction, AnimName);
        }
    }
};

USTRUCT(BlueprintType)
struct FMMSemanticBatch
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    int32 BatchId = -1;
    
    UPROPERTY(BlueprintReadWrite)
    TArray<FMMSemanticRequest> Requests;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FMotionSemanticResult> Results;
    
    UPROPERTY(BlueprintReadWrite)
    EMMRequestPriority Priority = EMMRequestPriority::Normal;
    
    UPROPERTY(BlueprintReadOnly)
    bool bIsProcessing = false;
    
    UPROPERTY(BlueprintReadOnly)
    bool bIsComplete = false;
    
    UPROPERTY(BlueprintReadOnly)
    float TotalProcessingTimeMS = 0.0f;
};

USTRUCT(BlueprintType)
struct FMMPerformanceMetrics
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    int32 TotalQueriesProcessed = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 CacheHits = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 CacheMisses = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 CacheEvictions = 0;
    
    UPROPERTY(BlueprintReadOnly)
    float CacheHitRate = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    float AverageAnalysisTimeMS = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    float PeakAnalysisTimeMS = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentCacheSize = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 PeakMemoryUsageBytes = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 CacheMemoryUsageBytes = 0;
    
    UPROPERTY(BlueprintReadOnly)
    float QueriesPerSecond = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    float AverageConfidence = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    int32 FailedQueries = 0;
    
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> CommonQueries;
    
    void RecordQuery(const FString& Query, float ProcessingTimeMS, bool bCacheHit)
    {
        TotalQueriesProcessed++;
        
        if (bCacheHit)
        {
            CacheHits++;
        }
        else
        {
            CacheMisses++;
        }
        
        CacheHitRate = TotalQueriesProcessed > 0 ? 
            static_cast<float>(CacheHits) / static_cast<float>(TotalQueriesProcessed) : 0.0f;
        
        AverageAnalysisTimeMS = (AverageAnalysisTimeMS * (TotalQueriesProcessed - 1) + ProcessingTimeMS) 
            / TotalQueriesProcessed;
        
        PeakAnalysisTimeMS = FMath::Max(PeakAnalysisTimeMS, ProcessingTimeMS);
        
        // Track common queries
        if (CommonQueries.Contains(Query))
        {
            CommonQueries[Query]++;
        }
        else if (CommonQueries.Num() < 100) // Track top 100
        {
            CommonQueries.Add(Query, 1);
        }
    }
};

// ============================================================================
// CACHE SYSTEM
// ============================================================================

class FMMSemanticCache
{
public:
    FMMSemanticCache(int32 MaxMemorySize = 1000);
    ~FMMSemanticCache();
    
    // Thread-safe cache operations
    bool Get(const FString& Query, FMMCachedResult& OutResult, EMMCacheTier& OutTier) const;
    void Put(const FString& Query, const FMotionSemanticResult& Result);
    void PromoteToFrameCache(const FString& Query, const FMMCachedResult& Result);
    
    // Cache management
    void ClearFrameCache();
    void ClearMemoryCache();
    void ClearAll();
    void TrimMemoryCache(int32 TargetSize);
    
    // Configuration
    void SetMaxMemorySize(int32 NewSize) { MaxMemoryEntries = FMath::Max(1, NewSize); }
    void SetExpirationTime(float Seconds) { ExpirationSeconds = FMath::Max(0.0f, Seconds); }
    
    // Statistics
    void GetStatistics(int32& OutHits, int32& OutMisses, int32& OutFrameSize, int32& OutMemorySize) const;
    int32 GetMemoryUsageBytes() const;
    
private:
    // Frame cache (no mutex needed - single frame only, but make it mutable for const methods)
    mutable TMap<uint32, FMMCachedResult> FrameCache;
    
    // Memory cache with LRU eviction
    mutable FCriticalSection MemoryCacheMutex;
    mutable TMap<uint32, FMMCachedResult> MemoryCache;
    mutable TDoubleLinkedList<uint32> LRUOrder;
    mutable TMap<uint32, TDoubleLinkedList<uint32>::TDoubleLinkedListNode*> LRUNodes;
    
    // Configuration
    int32 MaxMemoryEntries;
    float ExpirationSeconds;
    
    // Statistics
    mutable FThreadSafeCounter CacheHits;
    mutable FThreadSafeCounter CacheMisses;
    mutable FThreadSafeCounter Evictions;
    
    // Helper methods
    static uint32 GetQueryHash(const FString& Query);
    void UpdateLRU(uint32 Hash) const;
    void EvictLRU();
};

// ============================================================================
// MAIN SUBSYSTEM
// ============================================================================

UCLASS(DisplayName="MM Semantic Subsystem")
class MOTIONMATCHING_DEMO_API UMMSemanticSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()
    
private:
    // Layer connections
    UPROPERTY()
    class UFuzzyMatcher* FuzzyMatcher = nullptr;
    
    UPROPERTY()
    class UMMFuzzyProcessor* FuzzyProcessor = nullptr;
    
    UPROPERTY()
    class UMMMotionSemanticAnalyzer* SemanticAnalyzer = nullptr;
    
    // Cache system
    TSharedPtr<FMMSemanticCache> Cache;
    
    // Animation index with inverted indices
    TMap<FName, FMMIndexedAnimation> AnimationIndex;
    FMMSemanticIndex InvertedIndex;
    mutable FCriticalSection IndexMutex;
    
    // Request tracking for deduplication
    TMap<uint32, FMMSemanticRequest> ActiveRequests;
    TMap<uint32, TArray<TPromise<FMotionSemanticResult>>> PendingPromises;
    mutable FCriticalSection RequestMutex;
    
    // Tick handle
    FTSTicker::FDelegateHandle TickHandle;
    
    // Performance tracking
    FMMPerformanceMetrics Metrics;
    mutable FCriticalSection MetricsMutex;
    double LastMetricsUpdateTime = 0.0;
    
    // Configuration
    float CacheExpirationSeconds = 300.0f;
    int32 MaxMemoryCacheMB = 64;
    int32 MaxQueryExpansions = 20;
    float MinimumConfidenceThreshold = 0.5f;
    bool bAutoIndexAnimations = true;
    bool bEnableMetrics = true;
    int32 MaxQueryLength = 1000;
    
    // Request ID counter
    FThreadSafeCounter NextRequestId;
    
    // Common queries for cache warmup
    static constexpr const TCHAR* CommonQueries[] = {
        TEXT("idle"),
        TEXT("walk"),
        TEXT("run"),
        TEXT("jump"),
        TEXT("crouch"),
        TEXT("attack"),
        TEXT("walk forward"),
        TEXT("run forward"),
        TEXT("strafe left"),
        TEXT("strafe right"),
        TEXT("turn left"),
        TEXT("turn right"),
        TEXT("sprint"),
        TEXT("dodge"),
        TEXT("roll")
    };
    
public:
    // ========== LIFECYCLE ==========
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    
    /**
     * Tick handler for frame cache management
     */
    bool HandleTicker(float DeltaTime);
    
    // ========== CORE API (SYNCHRONOUS) ==========
    
    /**
     * Process a semantic query synchronously with validation
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Core", meta=(DisplayName="Process Query"))
    FMotionSemanticResult ProcessQuery(const FString& Query);
    
    /**
     * Process query with full request options
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Core", meta=(DisplayName="Process Request"))
    FMotionSemanticResult ProcessRequest(const FMMSemanticRequest& Request);
    
    /**
     * Get cached result if available
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Cache", meta=(DisplayName="Get Cached Result"))
    bool GetCachedResult(const FString& Query, FMotionSemanticResult& OutResult, EMMCacheTier& OutTier);
    
    /**
     * Expand query for PoseSearch
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Search", meta=(DisplayName="Expand Query"))
    TArray<FString> ExpandQuery(const FString& Query, int32 MaxExpansions = 10);
    
    /**
     * Expand query using cache first
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Search", meta=(DisplayName="Expand Query Cached"))
    TArray<FString> ExpandQueryCached(const FString& Query);
    
    // ========== ANIMATION INDEXING ==========
    
    /**
     * Index an entire PoseSearch database
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Index", meta=(DisplayName="Index Database"))
    void IndexPoseSearchDatabase(class UPoseSearchDatabase* Database);
    
    /**
     * Index a single animation
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Index", meta=(DisplayName="Index Animation"))
    void IndexAnimation(class UAnimSequenceBase* Animation, const TArray<FString>& Tags);
    
    /**
     * Search indexed animations
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Index", meta=(DisplayName="Search Index"))
    TArray<FMMIndexedAnimation> SearchAnimationIndex(const FString& Query, int32 MaxResults = 10);
    
    /**
     * Clear animation index
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Index", meta=(DisplayName="Clear Index"))
    void ClearAnimationIndex();
    
    // ========== POSE SEARCH INTEGRATION ==========
    
    /**
     * Modify a PoseSearch query with semantic understanding
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|PoseSearch", meta=(DisplayName="Modify Query"))
    void ModifyPoseSearchQuery(const FString& OriginalQuery, TArray<FString>& OutExpandedTerms);
    
    // ========== CACHE MANAGEMENT ==========
    
    /**
     * Clear all caches
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Cache", meta=(DisplayName="Clear Cache"))
    void ClearCache();
    
    /**
     * Warmup cache with common queries
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Cache", meta=(DisplayName="Warmup Cache"))
    void WarmupCache();
    
    /**
     * Configure cache settings
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Cache", meta=(DisplayName="Configure Cache"))
    void ConfigureCache(int32 MaxSizeMB, float ExpirationSeconds);
    
    // ========== PERFORMANCE & METRICS ==========
    
    /**
     * Get current performance metrics
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Performance", meta=(DisplayName="Get Metrics"))
    FMMPerformanceMetrics GetPerformanceMetrics() const;
    
    /**
     * Reset performance metrics
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Performance", meta=(DisplayName="Reset Metrics"))
    void ResetMetrics();
    
    /**
     * Get memory usage
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Performance", meta=(DisplayName="Get Memory Usage"))
    int32 GetMemoryUsageBytes() const;
    
    // ========== CONFIGURATION ==========
    
    /**
     * Set processing configuration
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Config", meta=(DisplayName="Set Configuration"))
    void SetConfiguration(int32 MaxExpansions, float MinConfidence, bool bAutoIndex);
    
    /**
     * Enable/disable metrics collection
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Config", meta=(DisplayName="Set Metrics Enabled"))
    void SetMetricsEnabled(bool bEnabled) { bEnableMetrics = bEnabled; }
    
    /**
     * Set maximum query length
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Config", meta=(DisplayName="Set Max Query Length"))
    void SetMaxQueryLength(int32 MaxLength) { MaxQueryLength = FMath::Clamp(MaxLength, 10, 10000); }
    
    // ========== BATCH OPERATIONS ==========
    
    /**
     * Process multiple queries in a batch
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic|Batch", meta=(DisplayName="Process Batch"))
    FMMSemanticBatch ProcessBatch(const TArray<FString>& Queries);
    
    // ========== INTERNAL HELPERS ==========
    
private:
    /**
     * Internal query processing with deduplication
     */
    FMotionSemanticResult ProcessQueryInternal(const FMMSemanticRequest& Request);
    
    /**
     * Normalize query string
     */
    static FString NormalizeQuery(const FString& Query);
    
    /**
     * Validate query string
     */
    bool ValidateQuery(const FString& Query) const;
    
    /**
     * Update metrics after processing
     */
    void UpdateMetrics(const FString& Query, float ProcessingTimeMS, bool bCacheHit, const FMotionSemanticResult& Result);
    
    /**
     * Get next unique request ID
     */
    int32 GetNextRequestId() { return NextRequestId.Increment(); }
    
    /**
     * Initialize layer connections with dependencies
     */
    void InitializeLayers(FSubsystemCollectionBase& Collection);
    
    /**
     * Compute memory size from cache size in MB
     */
    static int32 ComputeCacheSize(int32 SizeMB);
    
    /**
     * Calculate semantic cost between query and animation
     */
    static float CalculateSemanticCost(const FMotionSemanticResult& QuerySemantics, const FMMIndexedAnimation& Animation);
    
    /**
     * Fallback to lower layers on error
     */
    FMotionSemanticResult FallbackToLowerLayers(const FString& Query);
    
    /**
     * Create expanded queries from semantic result
     */
    static TArray<FString> GenerateExpandedQueries(const FMotionSemanticResult& Result);
};
#endif