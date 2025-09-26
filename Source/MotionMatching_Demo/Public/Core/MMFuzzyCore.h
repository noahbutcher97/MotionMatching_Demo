// Public/Core/MMFuzzyCore.h
#pragma once
#if 0
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HAL/ThreadSafeBool.h"
#include "MMFuzzyCore.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFuzzyCore, Log, All);

// Performance stats delegate
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFuzzyMatchPerformed, const FString& /*MethodName*/, float /*TimeMS*/);

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * Developer settings for fuzzy matching configuration
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Fuzzy Matching Settings"))
class MOTIONMATCHING_DEMO_API UFuzzyMatchingSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UFuzzyMatchingSettings();
    
    /** Maximum cache size in megabytes */
    UPROPERTY(config, EditAnywhere, Category="Performance", meta=(ClampMin=1, ClampMax=1024))
    int32 MaxCacheSizeMB = 64;
    
    /** Enable multithreading for expensive operations */
    UPROPERTY(config, EditAnywhere, Category="Performance")
    bool bUseMultithreading = true;
    
    /** Thread pool size for parallel operations */
    UPROPERTY(config, EditAnywhere, Category="Performance", meta=(ClampMin=1, ClampMax=16))
    int32 ThreadPoolSize = 4;
    
    /** Default Jaro-Winkler prefix scale */
    UPROPERTY(config, EditAnywhere, Category="Algorithms", meta=(ClampMin=0, ClampMax=0.25))
    float JaroWinklerPrefixScale = 0.1f;
    
    /** Maximum string length for automatic caching */
    UPROPERTY(config, EditAnywhere, Category="Algorithms", meta=(ClampMin=10, ClampMax=1000))
    int32 AutoCacheStringLength = 100;
    
    /** Enable phonetic matching algorithms */
    UPROPERTY(config, EditAnywhere, Category="Algorithms")
    bool bEnablePhoneticMatching = true;
    
    /** Cache eviction policy */
    UPROPERTY(config, EditAnywhere, Category="Performance")
    bool bUseLRUEviction = true;
    
    /** Maximum cache age in seconds before automatic eviction */
    UPROPERTY(config, EditAnywhere, Category="Performance", meta=(ClampMin=10, ClampMax=3600))
    float MaxCacheAgeSeconds = 300.0f;
    
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FText GetSectionText() const override { return FText::FromString(TEXT("Motion Matching")); }
    
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

// ============================================================================
// CHARACTER-LEVEL STRUCTURES
// ============================================================================

/**
 * Character comparison result with detailed similarity metrics
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FCharacterMatch
{
    GENERATED_BODY()

    //UPROPERTY(BlueprintReadOnly, Category="Character")
    TCHAR CharacterA = 0;
    
    //UPROPERTY(BlueprintReadOnly, Category="Character")
    TCHAR CharacterB = 0;
    
    /** Exact match (1.0) or case-insensitive match (0.9) */
    UPROPERTY(BlueprintReadOnly, Category="Similarity", meta=(ClampMin=0, ClampMax=1))
    float ExactSimilarity = 0.0f;
    
    /** Based on keyboard layout proximity */
    UPROPERTY(BlueprintReadOnly, Category="Similarity", meta=(ClampMin=0, ClampMax=1))
    float KeyboardSimilarity = 0.0f;
    
    /** Visual/OCR similarity (like 0/O, 1/l) */
    UPROPERTY(BlueprintReadOnly, Category="Similarity", meta=(ClampMin=0, ClampMax=1))
    float VisualSimilarity = 0.0f;
    
    /** Phonetic similarity based on sound */
    UPROPERTY(BlueprintReadOnly, Category="Similarity", meta=(ClampMin=0, ClampMax=1))
    float PhoneticSimilarity = 0.0f;
    
    /** Unicode category similarity */
    UPROPERTY(BlueprintReadOnly, Category="Similarity", meta=(ClampMin=0, ClampMax=1))
    float CategorySimilarity = 0.0f;
    
    FCharacterMatch() = default;
    
    float GetWeightedSimilarity(
        float ExactWeight = 0.4f,
        float KeyboardWeight = 0.2f,
        float VisualWeight = 0.2f,
        float PhoneticWeight = 0.15f,
        float CategoryWeight = 0.05f) const;
    
    FString ToString() const;
    bool IsValid() const { return CharacterA != 0 && CharacterB != 0; }
};

// ============================================================================
// STRING-LEVEL STRUCTURES
// ============================================================================

/**
 * Comprehensive string matching result
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FStringMatch
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Input")
    FString StringA;
    
    UPROPERTY(BlueprintReadWrite, Category="Input")
    FString StringB;
    
    /** Levenshtein edit distance (0 = identical) */
    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    int32 LevenshteinDistance = MAX_int32;
    
    /** Normalized Levenshtein (0-1, 1 = identical) */
    UPROPERTY(BlueprintReadOnly, Category="Metrics", meta=(ClampMin=0, ClampMax=1))
    float NormalizedLevenshtein = 0.0f;
    
    /** Optimal string alignment distance */
    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    int32 OptimalAlignmentDistance = MAX_int32;
    
    /** Damerau-Levenshtein with transpositions */
    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    int32 DamerauLevenshteinDistance = MAX_int32;
    
    /** Jaro similarity (0-1) */
    UPROPERTY(BlueprintReadOnly, Category="Metrics", meta=(ClampMin=0, ClampMax=1))
    float JaroSimilarity = 0.0f;
    
    /** Jaro-Winkler similarity (0-1) */
    UPROPERTY(BlueprintReadOnly, Category="Metrics", meta=(ClampMin=0, ClampMax=1))
    float JaroWinklerSimilarity = 0.0f;
    
    /** Longest common subsequence length */
    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    int32 LongestCommonSubsequence = 0;
    
    /** Longest common substring length */
    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    int32 LongestCommonSubstring = 0;
    
    /** Dice coefficient (bigram) */
    UPROPERTY(BlueprintReadOnly, Category="Metrics", meta=(ClampMin=0, ClampMax=1))
    float DiceCoefficient = 0.0f;
    
    /** Cosine similarity (trigram) */
    UPROPERTY(BlueprintReadOnly, Category="Metrics", meta=(ClampMin=0, ClampMax=1))
    float CosineSimilarity = 0.0f;
    
    /** Jaccard index */
    UPROPERTY(BlueprintReadOnly, Category="Metrics", meta=(ClampMin=0, ClampMax=1))
    float JaccardIndex = 0.0f;
    
    /** Soundex match */
    UPROPERTY(BlueprintReadOnly, Category="Phonetic")
    bool bSoundexMatch = false;
    
    /** Metaphone match */
    UPROPERTY(BlueprintReadOnly, Category="Phonetic")
    bool bMetaphoneMatch = false;
    
    /** Double Metaphone match */
    UPROPERTY(BlueprintReadOnly, Category="Phonetic")
    bool bDoubleMetaphoneMatch = false;
    
    /** Time taken to compute (milliseconds) */
    UPROPERTY(BlueprintReadOnly, Category="Performance")
    float ComputationTimeMS = 0.0f;
    
    /** Was retrieved from cache */
    UPROPERTY(BlueprintReadOnly, Category="Performance")
    bool bWasCached = false;
    
    float GetBestSimilarity() const;
    float GetWeightedSimilarity(const TMap<FString, float>& Weights) const;
    FString ToString() const;
    bool IsValid() const;
};

// ============================================================================
// N-GRAM STRUCTURES
// ============================================================================

/**
 * N-gram set for substring analysis
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FNGramSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NGram", meta=(ClampMin=1, ClampMax=5))
    int32 N = 3;
    
    UPROPERTY(BlueprintReadOnly, Category="NGram")
    TMap<FString, int32> Grams;
    
    UPROPERTY(BlueprintReadOnly, Category="NGram")
    int32 TotalGrams = 0;
    
    UPROPERTY(BlueprintReadOnly, Category="NGram")
    FString SourceString;
    
    void Build(const FString& Source);
    void Clear();
    
    float CompareTo(const FNGramSet& Other) const;
    float JaccardSimilarity(const FNGramSet& Other) const;
    float CosineSimilarity(const FNGramSet& Other) const;
    float DiceSimilarity(const FNGramSet& Other) const;
    
    TSet<FString> GetUniqueGrams() const;
    FString ToString() const;
};

// ============================================================================
// CACHE STRUCTURES
// ============================================================================

/**
 * LRU cache entry for string matches
 */
struct FCacheEntry
{
    FStringMatch Match;
    double LastAccessTime;
    int32 AccessCount;
    uint32 CacheKey;
    
    FCacheEntry() : LastAccessTime(0.0), AccessCount(0), CacheKey(0) {}
};

/**
 * Thread-safe LRU cache for fuzzy matches
 */
class FThreadSafeLRUCache
{
private:
    struct FLRUNode
    {
        uint32 Key;
        FCacheEntry Entry;
        FLRUNode* Prev = nullptr;
        FLRUNode* Next = nullptr;
    };
    
    TMap<uint32, FLRUNode*> Cache;
    FLRUNode* Head = nullptr;  // Most recently used
    FLRUNode* Tail = nullptr;  // Least recently used
    mutable FCriticalSection Mutex;
    int32 MaxSize;
    mutable int32 CacheHits = 0;
    mutable int32 CacheMisses = 0;
    
public:
    FThreadSafeLRUCache(int32 InMaxSize = 10000) : MaxSize(InMaxSize) {}
    ~FThreadSafeLRUCache();
    
    bool Get(uint32 Key, FStringMatch& OutMatch);
    void Put(uint32 Key, const FStringMatch& Match);
    void Clear();
    int32 Num() const;
    void SetMaxSize(int32 NewMaxSize);
    void GetStatistics(int32& OutHits, int32& OutMisses) const;
    
private:
    void MoveToHead(FLRUNode* Node);
    void RemoveNode(FLRUNode* Node);
    void EvictLRU();
};


// ============================================================================
// FUZZY MATCHING ENGINE
// ============================================================================

/**
 * Core fuzzy matching subsystem with caching and optimization
 */
UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UFuzzyMatcher : public UEngineSubsystem
{
    GENERATED_BODY()

private:
    // Thread-safe caches
    mutable TMap<uint32, FCharacterMatch> CharacterCache;
    mutable FCriticalSection CharacterCacheMutex;
    
    TUniquePtr<FThreadSafeLRUCache> StringCache;
    
    mutable TMap<uint32, FString> SoundexCache;
    mutable TMap<uint32, TArray<FString>> MetaphoneCache;
    mutable FCriticalSection PhoneticCacheMutex;
    
    // Cache statistics
    mutable FThreadSafeCounter CacheHits;
    mutable FThreadSafeCounter CacheMisses;
    mutable FThreadSafeCounter TotalComputations;
    
    // Thread pool for parallel operations
    FQueuedThreadPool* ThreadPool = nullptr;
    
    
    // Cache management
    void TrimCache();
    static uint32 GetCacheKey(const FString& A, const FString& B);
    static uint32 GetCacheKey(TCHAR A, TCHAR B);
    
public:
    // Lifecycle
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // Performance delegate
    FOnFuzzyMatchPerformed OnMatchPerformed;
    
    // ===== CHARACTER-LEVEL OPERATIONS (C++ Only) =====
    
    FCharacterMatch CompareCharacters(TCHAR A, TCHAR B) const;
    
    static float GetKeyboardDistance(TCHAR A, TCHAR B);
    
    static float GetVisualSimilarity(TCHAR A, TCHAR B);
    
    static float GetPhoneticSimilarity(TCHAR A, TCHAR B);
    
    static bool AreCharactersSimilar(TCHAR A, TCHAR B, float Threshold = 0.5f);
    
    
    // ===== STRING-LEVEL OPERATIONS (Blueprint Exposed) =====
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|String", meta=(DisplayName="Compare Strings (Full)"))
    FStringMatch CompareStrings(const FString& A, const FString& B, bool bUseCache = true);
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|String", meta=(DisplayName="Compare Strings (Fast)"))
    float CompareStringsFast(const FString& A, const FString& B);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|String")
    static int32 CalculateLevenshteinDistance(const FString& A, const FString& B);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|String")
    static int32 CalculateDamerauLevenshteinDistance(const FString& A, const FString& B);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|String")
    static float CalculateJaroSimilarity(const FString& A, const FString& B);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|String")
    static float CalculateJaroWinklerSimilarity(const FString& A, const FString& B, float PrefixScale = 0.1f);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|String")
    static int32 CalculateLongestCommonSubsequence(const FString& A, const FString& B);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|String")
    static int32 CalculateLongestCommonSubstring(const FString& A, const FString& B);

    // Add wrapper functions for Blueprint if character comparison is needed
    UFUNCTION(BlueprintPure, Category="Fuzzy|Character", meta=(DisplayName="Compare Single Characters"))
    static float CompareSingleCharacters(const FString& CharA, const FString& CharB);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Character", meta=(DisplayName="Get Character Keyboard Distance"))
    static float GetCharacterKeyboardDistance(const FString& CharA, const FString& CharB);
    
    // ===== N-GRAM OPERATIONS =====
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|NGram")
    static FNGramSet GenerateNGrams(const FString& Source, int32 N = 3);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|NGram")
    static float CompareNGramSets(const FNGramSet& A, const FNGramSet& B);
    
    // ===== PHONETIC OPERATIONS =====
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Phonetic")
    FString GenerateSoundex(const FString& Input);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Phonetic")
    TArray<FString> GenerateMetaphone(const FString& Input, bool bDoubleMetaphone = true);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Phonetic")
    static FString GenerateNYSIIS(const FString& Input);
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Phonetic")
    bool ArePhoneticallyEqual(const FString& A, const FString& B);
    
    // ===== BATCH OPERATIONS =====
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|Batch")
    TArray<FStringMatch> CompareBatch(const TArray<FString>& Strings, bool bParallel = true);
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|Batch")
    TArray<FString> FindBestMatches(const FString& Query, const TArray<FString>& Candidates, 
                                     int32 MaxResults = 5, float MinSimilarity = 0.0f);
    
    // ===== CACHE MANAGEMENT =====
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|Cache")
    void ClearCache();
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Cache")
    void GetCacheStatistics(int32& OutHits, int32& OutMisses, int32& OutSize) const;
    
    UFUNCTION(BlueprintPure, Category="Fuzzy|Cache")
    float GetCacheHitRate() const;
    
    UFUNCTION(BlueprintCallable, Category="Fuzzy|Cache")
    void PrewarmCache(const TArray<FString>& CommonStrings);

private:
    // Don't store as pointer - store settings values instead
    struct FCachedSettings
    {
        int32 MaxCacheSizeMB = 64;
        bool bUseMultithreading = true;
        int32 ThreadPoolSize = 4;
        float JaroWinklerPrefixScale = 0.1f;
        int32 AutoCacheStringLength = 100;
        bool bEnablePhoneticMatching = true;
        bool bUseLRUEviction = true;
        float MaxCacheAgeSeconds = 300.0f;
        
        void UpdateFromSettings(const UFuzzyMatchingSettings* Settings)
        {
            if (Settings)
            {
                MaxCacheSizeMB = Settings->MaxCacheSizeMB;
                bUseMultithreading = Settings->bUseMultithreading;
                ThreadPoolSize = Settings->ThreadPoolSize;
                JaroWinklerPrefixScale = Settings->JaroWinklerPrefixScale;
                AutoCacheStringLength = Settings->AutoCacheStringLength;
                bEnablePhoneticMatching = Settings->bEnablePhoneticMatching;
                bUseLRUEviction = Settings->bUseLRUEviction;
                MaxCacheAgeSeconds = Settings->MaxCacheAgeSeconds;
            }
        }
    };
    
    FCachedSettings CachedSettings;
};
#endif