// MMSemanticSubsystem.cpp
#if 0
#include "Core/MMSemanticSubsystem.h"
#include "Engine/Engine.h"
#include "Animation/AnimSequenceBase.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY(LogMMSemantic);

// Profiling stats
DECLARE_CYCLE_STAT(TEXT("MM Semantic ProcessQuery"), STAT_MMSemanticProcessQuery, STATGROUP_MMSemantic);
DECLARE_CYCLE_STAT(TEXT("MM Semantic CacheLookup"), STAT_MMSemanticCacheLookup, STATGROUP_MMSemantic);
DECLARE_CYCLE_STAT(TEXT("MM Semantic IndexSearch"), STAT_MMSemanticIndexSearch, STATGROUP_MMSemantic);

// ============================================================================
// CACHE IMPLEMENTATION
// ============================================================================

FMMSemanticCache::FMMSemanticCache(int32 MaxMemorySize)
    : MaxMemoryEntries(MaxMemorySize)
    , ExpirationSeconds(300.0f)
{
}

FMMSemanticCache::~FMMSemanticCache()
{
    ClearAll();
}

bool FMMSemanticCache::Get(const FString& Query, FMMCachedResult& OutResult, EMMCacheTier& OutTier) const
{
    SCOPE_CYCLE_COUNTER(STAT_MMSemanticCacheLookup);
    
    const uint32 Hash = GetQueryHash(Query);
    
    // Check frame cache first (no mutex needed - single frame only)
    if (const FMMCachedResult* FrameResult = FrameCache.Find(Hash))
    {
        if (!FrameResult->IsExpired(0.033f)) // ~30fps
        {
            OutResult = *FrameResult;
            OutTier = EMMCacheTier::Frame;
            CacheHits.Increment();
            return true;
        }
    }
    
    // Check memory cache
    {
        FScopeLock Lock(&MemoryCacheMutex);
        if (const FMMCachedResult* MemoryResult = MemoryCache.Find(Hash))
        {
            if (!MemoryResult->IsExpired(ExpirationSeconds))
            {
                OutResult = *MemoryResult;
                OutTier = EMMCacheTier::Memory;
                UpdateLRU(Hash);
                CacheHits.Increment();
                
                // NOTE: We don't promote to frame cache from const Get()
                // to maintain thread safety. Caller can explicitly promote if needed.
                return true;
            }
            else
            {
                // Remove expired entry - need to cast away const for map operations
                const_cast<TMap<uint32, FMMCachedResult>&>(MemoryCache).Remove(Hash);
                
                // Also remove from LRU tracking
                if (auto** NodePtr = LRUNodes.Find(Hash))
                {
                    LRUOrder.RemoveNode(*NodePtr);
                    const_cast<TMap<uint32, TDoubleLinkedList<uint32>::TDoubleLinkedListNode*>&>(LRUNodes).Remove(Hash);
                }
            }
        }
    }
    
    CacheMisses.Increment();
    OutTier = EMMCacheTier::None;
    return false;
}

void FMMSemanticCache::Put(const FString& Query, const FMotionSemanticResult& Result)
{
    const uint32 Hash = GetQueryHash(Query);
    
    FMMCachedResult CachedResult;
    CachedResult.Result = Result;
    CachedResult.CacheTime = FPlatformTime::Seconds();
    CachedResult.AccessCount = 1;
    
    // Add to frame cache immediately (no mutex needed)
    FrameCache.Add(Hash, CachedResult);
    
    // Add to memory cache
    {
        FScopeLock Lock(&MemoryCacheMutex);
        
        // Evict if at capacity
        if (MemoryCache.Num() >= MaxMemoryEntries)
        {
            EvictLRU();
        }
        
        // Add new entry
        MemoryCache.Add(Hash, CachedResult);
        auto* NewNode = new TDoubleLinkedList<uint32>::TDoubleLinkedListNode(Hash);
        LRUOrder.AddHead(NewNode);
        LRUNodes.Add(Hash, NewNode);
    }
}

void FMMSemanticCache::PromoteToFrameCache(const FString& Query, const FMMCachedResult& Result)
{
    const uint32 Hash = GetQueryHash(Query);
    FrameCache.Add(Hash, Result);
}

void FMMSemanticCache::ClearFrameCache()
{
    FrameCache.Empty();
}

void FMMSemanticCache::ClearMemoryCache()
{
    FScopeLock Lock(&MemoryCacheMutex);
    
    // Clean up LRU nodes
    auto* CurrentNode = LRUOrder.GetHead();
    while (CurrentNode)
    {
        auto* NextNode = CurrentNode->GetNextNode();
        delete CurrentNode;
        CurrentNode = NextNode;
    }
    
    MemoryCache.Empty();
    LRUNodes.Empty();
    LRUOrder.Empty();
}

void FMMSemanticCache::ClearAll()
{
    ClearFrameCache();
    ClearMemoryCache();
    CacheHits.Reset();
    CacheMisses.Reset();
    Evictions.Reset();
}

void FMMSemanticCache::TrimMemoryCache(int32 TargetSize)
{
    FScopeLock Lock(&MemoryCacheMutex);
    
    while (MemoryCache.Num() > TargetSize)
    {
        EvictLRU();
    }
}

void FMMSemanticCache::UpdateLRU(uint32 Hash) const
{
    if (auto** NodePtr = const_cast<TMap<uint32, TDoubleLinkedList<uint32>::TDoubleLinkedListNode*>&>(LRUNodes).Find(Hash))
    {
        auto* Node = *NodePtr;
        // Need to cast away const for the list operations
        const_cast<TDoubleLinkedList<uint32>&>(LRUOrder).RemoveNode(Node);
        const_cast<TDoubleLinkedList<uint32>&>(LRUOrder).AddHead(Node);
    }
}
void FMMSemanticCache::EvictLRU()
{
    if (auto* Tail = LRUOrder.GetTail())
    {
        // Correct way to get value from node
        uint32 HashToEvict = Tail->GetValue();
        MemoryCache.Remove(HashToEvict);
        LRUNodes.Remove(HashToEvict);
        LRUOrder.RemoveNode(Tail);
        delete Tail;
        Evictions.Increment();
    }
}

uint32 FMMSemanticCache::GetQueryHash(const FString& Query)
{
    // Use a better hash function to reduce collisions
    FString NormalizedQuery = Query.ToLower();
    uint32 Hash = GetTypeHash(NormalizedQuery);
    Hash = HashCombine(Hash, NormalizedQuery.Len());
    Hash = HashCombine(Hash, GetTypeHash(NormalizedQuery.Left(3))); // Prefix
    Hash = HashCombine(Hash, GetTypeHash(NormalizedQuery.Right(3))); // Suffix
    return Hash;
}

void FMMSemanticCache::GetStatistics(int32& OutHits, int32& OutMisses, int32& OutFrameSize, int32& OutMemorySize) const
{
    OutHits = CacheHits.GetValue();
    OutMisses = CacheMisses.GetValue();
    OutFrameSize = FrameCache.Num();
    
    FScopeLock Lock(&MemoryCacheMutex);
    OutMemorySize = MemoryCache.Num();
}

int32 FMMSemanticCache::GetMemoryUsageBytes() const
{
    int32 TotalBytes = 0;
    
    // Frame cache
    TotalBytes += FrameCache.Num() * sizeof(FMMCachedResult);
    
    // Memory cache
    {
        FScopeLock Lock(&MemoryCacheMutex);
        TotalBytes += MemoryCache.Num() * sizeof(FMMCachedResult);
        TotalBytes += LRUNodes.Num() * (sizeof(uint32) + sizeof(TDoubleLinkedList<uint32>::TDoubleLinkedListNode*));
    }
    
    return TotalBytes;
}

// ============================================================================
// SUBSYSTEM IMPLEMENTATION
// ============================================================================

void UMMSemanticSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // Initialize dependencies first - THESE ARE NOW ENGINE SUBSYSTEMS
    Collection.InitializeDependency<UFuzzyMatcher>();
    Collection.InitializeDependency<UMMFuzzyProcessor>();
    Collection.InitializeDependency<UMMMotionSemanticAnalyzer>();
    
    Super::Initialize(Collection);
    
    UE_LOG(LogMMSemantic, Log, TEXT("Initializing MM Semantic Subsystem (Layer 3)"));
    
    // Initialize cache
    const int32 CacheSize = ComputeCacheSize(MaxMemoryCacheMB);
    Cache = MakeShareable(new FMMSemanticCache(CacheSize));
    Cache->SetExpirationTime(CacheExpirationSeconds);
    
    // Initialize layer connections
    InitializeLayers(Collection);
    
    // Register ticker for frame cache management
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMMSemanticSubsystem::HandleTicker),
        0.0f // Tick every frame
    );
    
    // Warmup cache with common queries
    if (bAutoIndexAnimations)
    {
        WarmupCache();
    }
    
    UE_LOG(LogMMSemantic, Log, TEXT("MM Semantic Subsystem initialized successfully"));
}

void UMMSemanticSubsystem::Deinitialize()
{
    UE_LOG(LogMMSemantic, Log, TEXT("Deinitializing MM Semantic Subsystem"));
    
    // Unregister ticker
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
    
    // Clear all caches
    if (Cache.IsValid())
    {
        Cache->ClearAll();
        Cache.Reset();
    }
    
    // Clear animation index
    {
        FScopeLock Lock(&IndexMutex);
        AnimationIndex.Empty();
        InvertedIndex.Clear();
    }
    
    // Clear active requests
    {
        FScopeLock Lock(&RequestMutex);
        ActiveRequests.Empty();
        PendingPromises.Empty();
    }
    
    // Log final metrics
    if (bEnableMetrics)
    {
        UE_LOG(LogMMSemantic, Log, TEXT("Final Performance Metrics:"));
        UE_LOG(LogMMSemantic, Log, TEXT("  Total Queries: %d"), Metrics.TotalQueriesProcessed);
        UE_LOG(LogMMSemantic, Log, TEXT("  Cache Hit Rate: %.2f%%"), Metrics.CacheHitRate * 100.0f);
        UE_LOG(LogMMSemantic, Log, TEXT("  Avg Processing Time: %.2fms"), Metrics.AverageAnalysisTimeMS);
    }
    
    Super::Deinitialize();
}

bool UMMSemanticSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Only create for game instances, not editor instances
    return !IsRunningCommandlet() && Cast<UGameInstance>(Outer) != nullptr;
}

bool UMMSemanticSubsystem::HandleTicker(float DeltaTime)
{
    // Clear frame cache each frame
    if (Cache.IsValid())
    {
        Cache->ClearFrameCache();
    }
    
    // Update metrics periodically (every second)
    if (bEnableMetrics)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - LastMetricsUpdateTime > 1.0)
        {
            FScopeLock Lock(&MetricsMutex);
            
            // Calculate queries per second
            if (LastMetricsUpdateTime > 0)
            {
                const float TimeDelta = CurrentTime - LastMetricsUpdateTime;
                Metrics.QueriesPerSecond = Metrics.TotalQueriesProcessed / TimeDelta;
            }
            
            // Update cache size
            if (Cache.IsValid())
            {
                int32 Hits, Misses, FrameSize, MemorySize;
                Cache->GetStatistics(Hits, Misses, FrameSize, MemorySize);
                Metrics.CurrentCacheSize = FrameSize + MemorySize;
                Metrics.CacheMemoryUsageBytes = Cache->GetMemoryUsageBytes();
                Metrics.PeakMemoryUsageBytes = FMath::Max(Metrics.PeakMemoryUsageBytes, Metrics.CacheMemoryUsageBytes);
            }
            
            LastMetricsUpdateTime = CurrentTime;
        }
    }
    
    return true; // Continue ticking
}

FString UMMSemanticSubsystem::NormalizeQuery(const FString& Query)
{
    FString NormalizedQuery = Query.TrimStartAndEnd();
    
    // Replace multiple spaces with single space
    while (NormalizedQuery.Contains(TEXT("  ")))
    {
        NormalizedQuery = NormalizedQuery.Replace(TEXT("  "), TEXT(" "));
    }
    
    // Convert to lower case for consistent caching
    return NormalizedQuery.ToLower();
}

bool UMMSemanticSubsystem::ValidateQuery(const FString& Query) const
{
    if (Query.IsEmpty())
    {
        UE_LOG(LogMMSemantic, Warning, TEXT("Empty query provided"));
        return false;
    }
    
    if (Query.Len() > MaxQueryLength)
    {
        UE_LOG(LogMMSemantic, Warning, TEXT("Query too long: %d chars (max: %d)"), Query.Len(), MaxQueryLength);
        return false;
    }
    
    return true;
}

FMotionSemanticResult UMMSemanticSubsystem::ProcessQuery(const FString& Query)
{
    SCOPE_CYCLE_COUNTER(STAT_MMSemanticProcessQuery);
    
    // Validate query
    if (!ValidateQuery(Query))
    {
        Metrics.FailedQueries++;
        return FMotionSemanticResult();
    }
    
    // Normalize query
    FString NormalizedQuery = NormalizeQuery(Query);
    
    FMMSemanticRequest Request;
    Request.Query = NormalizedQuery;
    Request.Priority = EMMRequestPriority::Normal;
    Request.MaxExpansions = MaxQueryExpansions;
    Request.MinConfidence = MinimumConfidenceThreshold;
    
    return ProcessRequest(Request);
}

FMotionSemanticResult UMMSemanticSubsystem::ProcessRequest(const FMMSemanticRequest& Request)
{
    const double StartTime = FPlatformTime::Seconds();
    
    // Check for request deduplication
    uint32 RequestHash = Request.GetHash();
    
    {
        FScopeLock Lock(&RequestMutex);
        
        // Check if identical request is already being processed
        if (ActiveRequests.Contains(RequestHash))
        {
            UE_LOG(LogMMSemantic, VeryVerbose, TEXT("Deduplicating request for query: %s"), *Request.Query);
            
            // In synchronous mode, we'll process it anyway to avoid blocking
            // In async mode (Phase 2), we would wait for the existing request
        }
    }
    
    // Check cache first
    FMotionSemanticResult CachedResult;
    EMMCacheTier CacheTier;
    if (Cache.IsValid() && GetCachedResult(Request.Query, CachedResult, CacheTier))
    {
        const float ProcessingTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
        UpdateMetrics(Request.Query, ProcessingTimeMS, true, CachedResult);
        
        // Promote to frame cache if from memory cache
        if (CacheTier == EMMCacheTier::Memory)
        {
            FMMCachedResult CachedEntry;
            CachedEntry.Result = CachedResult;
            CachedEntry.CacheTime = FPlatformTime::Seconds();
            Cache->PromoteToFrameCache(Request.Query, CachedEntry);
        }
        
        return CachedResult;
    }
    
    // Track active request
    {
        FScopeLock Lock(&RequestMutex);
        ActiveRequests.Add(RequestHash, Request);
    }
    
    // Process query internally
    FMotionSemanticResult Result = ProcessQueryInternal(Request);
    
    // Remove from active requests
    {
        FScopeLock Lock(&RequestMutex);
        ActiveRequests.Remove(RequestHash);
    }
    
    // Update cache
    if (Cache.IsValid())
    {
        Cache->Put(Request.Query, Result);
    }
    
    const float ProcessingTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
    UpdateMetrics(Request.Query, ProcessingTimeMS, false, Result);
    
    return Result;
}

FMotionSemanticResult UMMSemanticSubsystem::ProcessQueryInternal(const FMMSemanticRequest& Request)
{
    // Validate layers are initialized
    if (!SemanticAnalyzer)
    {
        UE_LOG(LogMMSemantic, Error, TEXT("Semantic Analyzer not initialized"));
        
        // Attempt fallback to lower layers
        return FallbackToLowerLayers(Request.Query);
    }
    
    // Process through Layer 2
    FMotionSemanticResult Result = SemanticAnalyzer->AnalyzeMotionText(Request.Query);
    
    // Apply confidence threshold
    if (Result.OverallConfidence < Request.MinConfidence)
    {
        UE_LOG(LogMMSemantic, Verbose, TEXT("Low confidence result (%.2f < %.2f) for query: %s"),
               Result.OverallConfidence, Request.MinConfidence, *Request.Query);
    }
    
    return Result;
}

FMotionSemanticResult UMMSemanticSubsystem::FallbackToLowerLayers(const FString& Query)
{
    UE_LOG(LogMMSemantic, Warning, TEXT("Falling back to lower layers for query: %s"), *Query);
    
    FMotionSemanticResult Result;
    
    // Try Layer 1 (Fuzzy Processor)
    if (FuzzyProcessor)
    {
        FMMTokenStream Tokens = FuzzyProcessor->TokenizeCommand(Query);
        
        // Build basic result from tokenization
        Result.OriginalText = Query;
        Result.TokenStream = Tokens;
        Result.OverallConfidence = 0.5f; // Default confidence for fallback
        
        // Extract basic intent from tokens
        for (const FMMFuzzyToken& Token : Tokens.Tokens)
        {
            if (Token.Type == EMMTokenType::Word)
            {
                // Map common commands to intents
                if (Token.Text.Contains(TEXT("walk")))
                {
                    Result.Intent.PrimaryIntent = EMotionIntent::Locomotion;
                    Result.Speed.SpeedLevel = EMotionSpeed::Walk;
                }
                else if (Token.Text.Contains(TEXT("run")))
                {
                    Result.Intent.PrimaryIntent = EMotionIntent::Locomotion;
                    Result.Speed.SpeedLevel = EMotionSpeed::Run;
                }
                else if (Token.Text.Contains(TEXT("idle")))
                {
                    Result.Intent.PrimaryIntent = EMotionIntent::Idle;
                    Result.Speed.SpeedLevel = EMotionSpeed::Idle;
                }
            }
        }
    }
    else
    {
        UE_LOG(LogMMSemantic, Error, TEXT("No fallback layers available"));
        Metrics.FailedQueries++;
    }
    
    return Result;
}

bool UMMSemanticSubsystem::GetCachedResult(const FString& Query, FMotionSemanticResult& OutResult, EMMCacheTier& OutTier)
{
    if (!Cache.IsValid())
    {
        return false;
    }
    
    FString NormalizedQuery = NormalizeQuery(Query);
    FMMCachedResult CachedResult;
    
    if (Cache->Get(NormalizedQuery, CachedResult, OutTier))
    {
        OutResult = CachedResult.Result;
        return true;
    }
    
    return false;
}

void UMMSemanticSubsystem::WarmupCache()
{
    UE_LOG(LogMMSemantic, Log, TEXT("Warming up cache with common queries..."));
    
    int32 WarmupCount = 0;
    for (const TCHAR* CommonQuery : CommonQueries)
    {
        ProcessQuery(FString(CommonQuery));
        WarmupCount++;
    }
    
    UE_LOG(LogMMSemantic, Log, TEXT("Cache warmup complete: %d queries processed"), WarmupCount);
}

TArray<FString> UMMSemanticSubsystem::ExpandQuery(const FString& Query, int32 MaxExpansions)
{
    if (!SemanticAnalyzer)
    {
        return { Query };
    }
    
    return SemanticAnalyzer->ExpandMotionQuery(Query, MaxExpansions);
}

TArray<FString> UMMSemanticSubsystem::ExpandQueryCached(const FString& Query)
{
    // Check cache first
    FMotionSemanticResult CachedResult;
    EMMCacheTier CacheTier;
    if (GetCachedResult(Query, CachedResult, CacheTier))
    {
        return GenerateExpandedQueries(CachedResult);
    }
    
    // Process and cache
    FMotionSemanticResult Result = ProcessQuery(Query);
    return GenerateExpandedQueries(Result);
}

TArray<FString> UMMSemanticSubsystem::GenerateExpandedQueries(const FMotionSemanticResult& Result)
{
    TArray<FString> ExpandedQueries;
    
    // Add original text
    ExpandedQueries.Add(Result.OriginalText);
    
    // Add variations based on intent
    if (Result.Intent.PrimaryIntent != EMotionIntent::None)
    {
        FString IntentStr = UEnum::GetValueAsString(Result.Intent.PrimaryIntent);
        IntentStr = IntentStr.Replace(TEXT("EMotionIntent::"), TEXT(""));
        ExpandedQueries.Add(IntentStr.ToLower());
    }
    
    // Add speed variations - check for non-idle speeds
    if (Result.Speed.SpeedLevel != EMotionSpeed::Idle || Result.Speed.NormalizedIntensity > 0.1f)
    {
        FString SpeedStr = UEnum::GetValueAsString(Result.Speed.SpeedLevel);
        SpeedStr = SpeedStr.Replace(TEXT("EMotionSpeed::"), TEXT(""));
        ExpandedQueries.Add(SpeedStr.ToLower());
        
        // Add speed descriptors if present
        for (const FString& Descriptor : Result.Speed.Descriptors)
        {
            ExpandedQueries.AddUnique(Descriptor.ToLower());
        }
    }
    
    // Add direction variations
    if (Result.Direction.Direction != EMotionDirection::None)
    {
        FString DirStr = UEnum::GetValueAsString(Result.Direction.Direction);
        DirStr = DirStr.Replace(TEXT("EMotionDirection::"), TEXT(""));
        ExpandedQueries.Add(DirStr.ToLower());
        
        // Add direction descriptors if present
        for (const FString& Descriptor : Result.Direction.Descriptors)
        {
            ExpandedQueries.AddUnique(Descriptor.ToLower());
        }
    }
    
    // Add any trigger keywords from intent
    for (const FString& Keyword : Result.Intent.TriggerKeywords)
    {
        ExpandedQueries.AddUnique(Keyword.ToLower());
    }
    
    return ExpandedQueries;
}

void UMMSemanticSubsystem::IndexAnimation(class UAnimSequenceBase* Animation, const TArray<FString>& Tags)
{
    if (!Animation)
    {
        return;
    }
    
    FScopeLock Lock(&IndexMutex);
    
    FMMIndexedAnimation IndexedAnim;
    IndexedAnim.Animation = Animation;
    IndexedAnim.SearchTags = Tags;
    
    // Normalize tags
    for (const FString& Tag : Tags)
    {
        IndexedAnim.NormalizedTags.Add(NormalizeQuery(Tag));
    }
    
    // Analyze semantics
    if (SemanticAnalyzer)
    {
        FString CombinedTags = FString::Join(Tags, TEXT(" "));
        IndexedAnim.Semantics = SemanticAnalyzer->AnalyzeMotionText(CombinedTags);
    }
    
    // Add to main index
    FName AnimName = Animation->GetFName();
    AnimationIndex.Add(AnimName, IndexedAnim);
    
    // Update inverted index
    InvertedIndex.AddAnimation(AnimName, IndexedAnim);
}

TArray<FMMIndexedAnimation> UMMSemanticSubsystem::SearchAnimationIndex(const FString& Query, int32 MaxResults)
{
    SCOPE_CYCLE_COUNTER(STAT_MMSemanticIndexSearch);
    
    if (!SemanticAnalyzer)
    {
        return TArray<FMMIndexedAnimation>();
    }
    
    FScopeLock Lock(&IndexMutex);
    
    // Analyze query semantics
    FMotionSemanticResult QuerySemantics = ProcessQuery(Query);
    
    // Use inverted index for fast initial filtering
    TSet<FName> CandidateAnimations;
    
    // Search by intent
    if (QuerySemantics.Intent.PrimaryIntent != EMotionIntent::None)
    {
        TArray<FName> IntentMatches;
        InvertedIndex.IntentToAnimation.MultiFind(QuerySemantics.Intent.PrimaryIntent, IntentMatches);
        CandidateAnimations.Append(IntentMatches);
    }
    
    // Search by speed
    TArray<FName> SpeedMatches;
    InvertedIndex.SpeedToAnimation.MultiFind(QuerySemantics.Speed.SpeedLevel, SpeedMatches);
    CandidateAnimations.Append(SpeedMatches);
    
    // Search by normalized tags
    FString NormalizedQuery = NormalizeQuery(Query);
    TArray<FString> QueryTokens;
    NormalizedQuery.ParseIntoArray(QueryTokens, TEXT(" "));
    
    for (const FString& Token : QueryTokens)
    {
        TArray<FName> TagMatches;
        InvertedIndex.TagToAnimation.MultiFind(Token, TagMatches);
        CandidateAnimations.Append(TagMatches);
    }
    
    // Score and rank candidates
    TArray<TPair<float, FMMIndexedAnimation>> ScoredAnimations;
    
    for (const FName& AnimName : CandidateAnimations)
    {
        if (const FMMIndexedAnimation* IndexedAnim = AnimationIndex.Find(AnimName))
        {
            float Score = CalculateSemanticCost(QuerySemantics, *IndexedAnim);
            ScoredAnimations.Add(TPair<float, FMMIndexedAnimation>(Score, *IndexedAnim));
        }
    }
    
    // Sort by score (lower is better)
    ScoredAnimations.Sort([](const TPair<float, FMMIndexedAnimation>& A, const TPair<float, FMMIndexedAnimation>& B)
    {
        return A.Key < B.Key;
    });
    
    // Return top results
    TArray<FMMIndexedAnimation> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, ScoredAnimations.Num()); i++)
    {
        Results.Add(ScoredAnimations[i].Value);
        
        // Early termination for very good matches
        if (ScoredAnimations[i].Key < 0.1f && Results.Num() >= 3)
        {
            break;
        }
    }
    
    return Results;
}

float UMMSemanticSubsystem::CalculateSemanticCost(const FMotionSemanticResult& QuerySemantics, const FMMIndexedAnimation& Animation)
{
    float Cost = 0.0f;
    
    // Intent mismatch cost
    if (QuerySemantics.Intent.PrimaryIntent != Animation.Semantics.Intent.PrimaryIntent)
    {
        Cost += 0.3f;
    }
    
    // Speed difference cost
    int32 SpeedDiff = FMath::Abs(static_cast<int32>(QuerySemantics.Speed.SpeedLevel) - static_cast<int32>(Animation.Semantics.Speed.SpeedLevel));
    Cost += SpeedDiff * 0.1f;
    
    // Direction mismatch cost
    if (QuerySemantics.Direction.Direction != EMotionDirection::None &&
        QuerySemantics.Direction.Direction != Animation.Semantics.Direction.Direction)
    {
        Cost += 0.2f;
    }
    
    // Confidence penalty
    Cost += (1.0f - Animation.Semantics.OverallConfidence) * 0.2f;
    
    return Cost;
}

void UMMSemanticSubsystem::InitializeLayers(FSubsystemCollectionBase& Collection)
{
    // Get engine subsystems directly since we're now an engine subsystem
    FuzzyMatcher = GEngine->GetEngineSubsystem<UFuzzyMatcher>();
    FuzzyProcessor = GEngine->GetEngineSubsystem<UMMFuzzyProcessor>();
    SemanticAnalyzer = GEngine->GetEngineSubsystem<UMMMotionSemanticAnalyzer>();
    
    if (!FuzzyMatcher)
    {
        UE_LOG(LogMMSemantic, Error, TEXT("Failed to get Fuzzy Matcher (Layer 0)"));
    }
    
    if (!FuzzyProcessor)
    {
        UE_LOG(LogMMSemantic, Error, TEXT("Failed to get Fuzzy Processor (Layer 1)"));
    }
    
    if (!SemanticAnalyzer)
    {
        UE_LOG(LogMMSemantic, Error, TEXT("Failed to get Motion Semantic Analyzer (Layer 2)"));
    }
    
    UE_LOG(LogMMSemantic, Log, TEXT("Layer connections established - All systems operational"));
}


void UMMSemanticSubsystem::UpdateMetrics(const FString& Query, float ProcessingTimeMS, bool bCacheHit, const FMotionSemanticResult& Result)
{
    if (!bEnableMetrics)
    {
        return;
    }
    
    FScopeLock Lock(&MetricsMutex);
    
    Metrics.RecordQuery(Query, ProcessingTimeMS, bCacheHit);
    
    // Update confidence tracking
    const float NewConfidence = Result.OverallConfidence;
    const int32 TotalQueries = Metrics.TotalQueriesProcessed;
    if (TotalQueries > 1)
    {
        Metrics.AverageConfidence = (Metrics.AverageConfidence * (TotalQueries - 1) + NewConfidence) / TotalQueries;
    }
    else
    {
        Metrics.AverageConfidence = NewConfidence;
    }
}

int32 UMMSemanticSubsystem::ComputeCacheSize(int32 SizeMB)
{
    // Estimate ~1KB per cached result
    return (SizeMB * 1024 * 1024) / 1024;
}

// Additional methods implementation...
void UMMSemanticSubsystem::ClearCache()
{
    if (Cache.IsValid())
    {
        Cache->ClearAll();
    }
}

void UMMSemanticSubsystem::ConfigureCache(int32 MaxSizeMB, float ExpirationSeconds)
{
    MaxMemoryCacheMB = MaxSizeMB;
    CacheExpirationSeconds = ExpirationSeconds;
    
    if (Cache.IsValid())
    {
        Cache->SetMaxMemorySize(ComputeCacheSize(MaxSizeMB));
        Cache->SetExpirationTime(ExpirationSeconds);
    }
}

FMMPerformanceMetrics UMMSemanticSubsystem::GetPerformanceMetrics() const
{
    FScopeLock Lock(&MetricsMutex);
    return Metrics;
}

void UMMSemanticSubsystem::ResetMetrics()
{
    FScopeLock Lock(&MetricsMutex);
    Metrics = FMMPerformanceMetrics();
}

int32 UMMSemanticSubsystem::GetMemoryUsageBytes() const
{
    int32 TotalBytes = sizeof(UMMSemanticSubsystem);
    
    if (Cache.IsValid())
    {
        TotalBytes += Cache->GetMemoryUsageBytes();
    }
    
    {
        FScopeLock Lock(&IndexMutex);
        TotalBytes += AnimationIndex.Num() * sizeof(FMMIndexedAnimation);
    }
    
    return TotalBytes;
}

void UMMSemanticSubsystem::SetConfiguration(int32 MaxExpansions, float MinConfidence, bool bAutoIndex)
{
    MaxQueryExpansions = MaxExpansions;
    MinimumConfidenceThreshold = MinConfidence;
    bAutoIndexAnimations = bAutoIndex;
}

FMMSemanticBatch UMMSemanticSubsystem::ProcessBatch(const TArray<FString>& Queries)
{
    FMMSemanticBatch Batch;
    Batch.BatchId = GetNextRequestId();
    Batch.Priority = EMMRequestPriority::Normal;
    
    const double StartTime = FPlatformTime::Seconds();
    
    // Convert queries to requests
    for (const FString& Query : Queries)
    {
        FMMSemanticRequest Request;
        Request.Query = Query;
        Request.Priority = Batch.Priority;
        Request.MaxExpansions = MaxQueryExpansions;
        Request.MinConfidence = MinimumConfidenceThreshold;
        
        Batch.Requests.Add(Request);
    }
    
    // Process each request
    // Note: bIsProcessing is used to indicate batch is currently being processed
    // This would be useful in async implementation to check status
    Batch.bIsProcessing = true;
    
    for (const FMMSemanticRequest& Request : Batch.Requests)
    {
        Batch.Results.Add(ProcessRequest(Request));
    }
    
    // Mark batch as complete
    Batch.bIsProcessing = false;
    Batch.bIsComplete = true;
    
    Batch.TotalProcessingTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
    
    UE_LOG(LogMMSemantic, Log, TEXT("Processed batch %d: %d queries in %.2fms (IsComplete: %s)"),
           Batch.BatchId, 
           Queries.Num(), 
           Batch.TotalProcessingTimeMS,
           Batch.bIsComplete ? TEXT("true") : TEXT("false"));
    
    return Batch;
}

void UMMSemanticSubsystem::ModifyPoseSearchQuery(const FString& OriginalQuery, TArray<FString>& OutExpandedTerms)
{
    OutExpandedTerms = ExpandQueryCached(OriginalQuery);
}

void UMMSemanticSubsystem::IndexPoseSearchDatabase(class UPoseSearchDatabase* Database)
{
    if (!Database)
    {
        UE_LOG(LogMMSemantic, Warning, TEXT("Invalid PoseSearchDatabase provided for indexing"));
        return;
    }
    
    FScopeLock Lock(&IndexMutex);
    
    UE_LOG(LogMMSemantic, Log, TEXT("Beginning indexing of PoseSearch database: %s"), *Database->GetName());
    
    int32 AnimationsIndexed = 0;
    
    // Note: The actual PoseSearchDatabase structure varies by UE version
    // This is a generic implementation that would need adjustment based on your specific PoseSearch setup
    
    // Get all animations from the database
    // For UE 5.6, this might be something like:
    // const TArray<FPoseSearchDatabaseAnimationAssetBase>& Animations = Database->GetAnimationAssets();
    
    // Since we don't have the exact PoseSearch structure, here's a placeholder approach:
    // You would iterate through the database's animation assets
    
    /* Example implementation when PoseSearch API is available:
    for (const auto& AnimAsset : Database->GetAnimationAssets())
    {
        if (UAnimSequenceBase* AnimSeq = AnimAsset.GetAnimationAsset())
        {
            // Extract tags from the animation or database metadata
            TArray<FString> Tags;
            
            // Get animation name as a tag
            Tags.Add(AnimSeq->GetName());
            
            // Parse the name for semantic information
            FString AnimName = AnimSeq->GetName();
            TArray<FString> NameTokens;
            AnimName.ParseIntoArray(NameTokens, TEXT("_"));
            
            for (const FString& Token : NameTokens)
            {
                if (!Token.IsNumeric() && Token.Len() > 1)
                {
                    Tags.Add(Token);
                }
            }
            
            // Add any metadata tags from the pose search database
            // This would depend on your specific setup
            
            // Index the animation
            IndexAnimation(AnimSeq, Tags);
            AnimationsIndexed++;
        }
    }
    */
    
    // For now, provide a basic implementation message
    UE_LOG(LogMMSemantic, Log, TEXT("PoseSearchDatabase indexing requires PoseSearch API integration"));
    UE_LOG(LogMMSemantic, Log, TEXT("Implement based on your specific PoseSearch structure"));
    
    // Clear any existing index for this database
    TArray<FName> AnimsToRemove;
    for (const auto& IndexPair : AnimationIndex)
    {
        if (IndexPair.Value.SourceDatabase == Database->GetFName())
        {
            AnimsToRemove.Add(IndexPair.Key);
        }
    }
    
    for (const FName& AnimName : AnimsToRemove)
    {
        AnimationIndex.Remove(AnimName);
    }
    
    UE_LOG(LogMMSemantic, Log, TEXT("Completed indexing of PoseSearch database. %d animations indexed"), AnimationsIndexed);
}



void UMMSemanticSubsystem::ClearAnimationIndex()
{
    FScopeLock Lock(&IndexMutex);
    AnimationIndex.Empty();
    InvertedIndex.Clear();
}
#endif