

#if 0
// Forward declarations
class UFuzzyMatcher;
class UMMFuzzyProcessor;

// ============================================================================
// MOTION SEMANTIC ENUMS
// ============================================================================

/**
 * Motion intent categories for classification
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EMotionIntent : uint8
{
    None         = 0     UMETA(DisplayName = "None"),
    Locomotion   = 1 << 0 UMETA(DisplayName = "Locomotion"),
    Combat       = 1 << 1 UMETA(DisplayName = "Combat"),
    Interaction  = 1 << 2 UMETA(DisplayName = "Interaction"),
    Idle         = 1 << 3 UMETA(DisplayName = "Idle"),
    Transition   = 1 << 4 UMETA(DisplayName = "Transition"),
    Gesture      = 1 << 5 UMETA(DisplayName = "Gesture"),
    Special      = 1 << 6 UMETA(DisplayName = "Special")
};
ENUM_CLASS_FLAGS(EMotionIntent);

/**
 * Motion speed levels with semantic hierarchy
 */
UENUM(BlueprintType)
enum class EMotionSpeed : uint8
{
    Idle       = 0 UMETA(DisplayName = "Idle (0.0)"),
    Slow       = 1 UMETA(DisplayName = "Slow (0.2)"),
    Walk       = 2 UMETA(DisplayName = "Walk (0.4)"),
    Jog        = 3 UMETA(DisplayName = "Jog (0.6)"),
    Run        = 4 UMETA(DisplayName = "Run (0.8)"),
    Sprint     = 5 UMETA(DisplayName = "Sprint (1.0)"),
    Custom     = 6 UMETA(DisplayName = "Custom")
};

/**
 * Direction types for motion
 */
UENUM(BlueprintType)
enum class EMotionDirection : uint8
{
    None           = 0  UMETA(DisplayName = "None"),
    Forward        = 1  UMETA(DisplayName = "Forward"),
    Backward       = 2  UMETA(DisplayName = "Backward"),
    Left           = 3  UMETA(DisplayName = "Left"),
    Right          = 4  UMETA(DisplayName = "Right"),
    ForwardLeft    = 5  UMETA(DisplayName = "Forward-Left"),
    ForwardRight   = 6  UMETA(DisplayName = "Forward-Right"),
    BackwardLeft   = 7  UMETA(DisplayName = "Backward-Left"),
    BackwardRight  = 8  UMETA(DisplayName = "Backward-Right"),
    Up             = 9  UMETA(DisplayName = "Up"),
    Down           = 10 UMETA(DisplayName = "Down"),
    Custom         = 11 UMETA(DisplayName = "Custom")
};

/**
 * Direction reference frame
 */
UENUM(BlueprintType)
enum class EDirectionType : uint8
{
    Absolute    = 0 UMETA(DisplayName = "Absolute/World"),
    Relative    = 1 UMETA(DisplayName = "Relative to Character"),
    Target      = 2 UMETA(DisplayName = "Relative to Target"),
    Camera      = 3 UMETA(DisplayName = "Relative to Camera")
};

/**
 * Motion state for transitions
 */
UENUM(BlueprintType)
enum class EMotionState : uint8
{
    Unknown     = 0 UMETA(DisplayName = "Unknown"),
    Idle        = 1 UMETA(DisplayName = "Idle"),
    Moving      = 2 UMETA(DisplayName = "Moving"),
    Jumping     = 3 UMETA(DisplayName = "Jumping"),
    Falling     = 4 UMETA(DisplayName = "Falling"),
    Landing     = 5 UMETA(DisplayName = "Landing"),
    Crouching   = 6 UMETA(DisplayName = "Crouching"),
    Attacking   = 7 UMETA(DisplayName = "Attacking"),
    Defending   = 8 UMETA(DisplayName = "Defending"),
    Interacting = 9 UMETA(DisplayName = "Interacting")
};

// ============================================================================
// MOTION SEMANTIC STRUCTURES
// ============================================================================

/**
 * Motion intent classification result
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionIntentResult
{
    GENERATED_BODY()

    /** Primary intent classification */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Intent")
    EMotionIntent PrimaryIntent = EMotionIntent::None;
    
    /** Secondary intents (multi-label support) */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Intent")
    TArray<EMotionIntent> SecondaryIntents;
    
    /** Confidence scores for each intent */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Intent")
    TMap<EMotionIntent, float> IntentConfidences;
    
    /** Keywords that triggered this classification */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Intent")
    TArray<FString> TriggerKeywords;
    
    float GetPrimaryConfidence() const;
    bool HasIntent(EMotionIntent Intent) const;
    FString ToString() const;
};

/**
 * Motion speed analysis result
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionSpeedResult
{
    GENERATED_BODY()

    /** Classified speed level */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Speed")
    EMotionSpeed SpeedLevel = EMotionSpeed::Idle;
    
    /** Normalized intensity (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Speed", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float NormalizedIntensity = 0.0f;
    
    /** Custom speed value if SpeedLevel is Custom */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Speed")
    float CustomSpeed = 0.0f;
    
    /** Speed modifiers detected (e.g., "very", "slightly") */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Speed")
    TArray<FString> SpeedModifiers;
    
    /** Confidence in speed classification */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Speed", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float Confidence = 0.0f;
    
    /** Semantic descriptors used */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Speed")
    TArray<FString> Descriptors;
    
    static float SpeedLevelToIntensity(EMotionSpeed Speed);
    static EMotionSpeed IntensityToSpeedLevel(float Intensity);
    FString ToString() const;
};

/**
 * Motion direction analysis result
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionDirectionResult
{
    GENERATED_BODY()

    /** Primary direction */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction")
    EMotionDirection Direction = EMotionDirection::None;
    
    /** Direction reference frame */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction")
    EDirectionType DirectionType = EDirectionType::Relative;
    
    /** Custom direction vector if Direction is Custom */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction")
    FVector CustomDirection = FVector::ZeroVector;
    
    /** Alternative directions with confidence */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction")
    TMap<EMotionDirection, float> AlternativeDirections;
    
    /** Ambiguity level (0 = clear, 1 = highly ambiguous) */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float AmbiguityLevel = 0.0f;
    
    /** Confidence in direction classification */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float Confidence = 0.0f;
    
    /** Direction descriptors used */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Direction")
    TArray<FString> Descriptors;
    
    FVector GetDirectionVector() const;
    bool IsAmbiguous() const { return AmbiguityLevel > 0.3f; }
    FString ToString() const;
};

/**
 * Motion state transition
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionTransition
{
    GENERATED_BODY()

    /** Source state */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Transition")
    EMotionState FromState = EMotionState::Unknown;
    
    /** Target state */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Transition")
    EMotionState ToState = EMotionState::Unknown;
    
    /** Is this a valid transition? */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Transition")
    bool bIsValid = false;
    
    /** Transition duration hint in seconds */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Transition")
    float DurationHint = 0.0f;
    
    /** Transition priority (higher = more likely) */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Transition")
    int32 Priority = 0;
    
    /** Confidence in transition detection */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Transition", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float Confidence = 0.0f;
    
    bool IsValid() const { return bIsValid && FromState != EMotionState::Unknown && ToState != EMotionState::Unknown; }
    FString ToString() const;
};

/**
 * Complete motion semantic analysis result
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionSemanticResult
{
    GENERATED_BODY()

    /** Original input text */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    FString OriginalText;
    
    /** Tokenized input from Layer 1 */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    FMMTokenStream TokenStream;
    
    /** Motion intent classification */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    FMotionIntentResult Intent;
    
    /** Speed analysis */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    FMotionSpeedResult Speed;
    
    /** Direction analysis */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    FMotionDirectionResult Direction;
    
    /** Detected state transition */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    FMotionTransition Transition;
    
    /** Overall confidence score */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float OverallConfidence = 0.0f;
    
    /** Analysis time in milliseconds */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    float AnalysisTimeMS = 0.0f;
    
    /** Was retrieved from cache */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    bool bWasCached = false;
    
    /** Additional metadata */
    UPROPERTY(BlueprintReadOnly, Category = "MM|Semantic|Result")
    TMap<FString, FString> Metadata;
    
    bool IsValid() const { return !OriginalText.IsEmpty() && OverallConfidence > 0.0f; }
    FString GetSummary() const;
    FString ToString() const;
};

/**
 * Configuration for motion semantic analysis
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionSemanticConfig
{
    GENERATED_BODY()

    /** Enable multi-label intent classification */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config")
    bool bEnableMultiLabel = true;
    
    /** Minimum confidence for secondary intents */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float SecondaryIntentThreshold = 0.3f;
    
    /** Enable speed modifier parsing (very, slightly, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config")
    bool bParseSpeedModifiers = true;
    
    /** Enable relative direction resolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config")
    bool bResolveRelativeDirections = true;
    
    /** Maximum ambiguity level before requesting clarification */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float MaxAmbiguityLevel = 0.5f;
    
    /** Enable state transition validation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config")
    bool bValidateTransitions = true;
    
    /** Cache analysis results */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config")
    bool bCacheResults = true;
    
    /** Maximum cache size in MB */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config", meta = (ClampMin = 1, ClampMax = 256))
    int32 MaxCacheSizeMB = 32;
    
    /** Use threaded analysis for performance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Config")
    bool bUseThreadedAnalysis = true;
    
    FMotionSemanticConfig();
};


// Create a wrapper struct for the synonym array
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMMSynonymGroup
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TArray<FString> Synonyms;
    
    FMMSynonymGroup() {}
    
    FMMSynonymGroup(std::initializer_list<FString> InitList)
        : Synonyms(InitList)
    {
    }
    
    // Helper methods
    void AddSynonym(const FString& Word) { Synonyms.AddUnique(Word); }
    bool Contains(const FString& Word) const { return Synonyms.Contains(Word); }
    int32 Num() const { return Synonyms.Num(); }
};

/**
 * Motion vocabulary for domain-specific understanding
 */

USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionVocabulary
{
    GENERATED_BODY()

    /** Intent classification keywords */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TMap<FString, EMotionIntent> IntentKeywords;
    
    /** Speed descriptors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TMap<FString, float> SpeedDescriptors;
    
    /** Direction terms */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TMap<FString, EMotionDirection> DirectionTerms;
    
    /** State keywords */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TMap<FString, EMotionState> StateKeywords;

    /** Speed modifier multipliers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TMap<FString, float> SpeedModifiers;
    
    /** Synonym groups for motion terms - FIXED */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Semantic|Vocabulary")
    TMap<FString, FMMSynonymGroup> SynonymGroups;
    
    // Update the methods to work with the new structure
    TArray<FString> GetSynonyms(const FString& Term) const;
    void LoadDefaults();
    bool HasIntentKeyword(const FString& Keyword) const;
    bool HasSpeedDescriptor(const FString& Descriptor) const;
    bool HasDirectionTerm(const FString& Term) const;
};

// ============================================================================
// MAIN SEMANTIC ANALYZER CLASS
// ============================================================================

/**
 * Motion Semantic Analyzer - Layer 2 of the fuzzy-semantic architecture
 * Provides motion-specific semantic understanding on top of tokenization
 */
UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UMMMotionSemanticAnalyzer : public UEngineSubsystem
{
    GENERATED_BODY()

private:
    // Layer dependencies
    UPROPERTY()
    UFuzzyMatcher* FuzzyMatcher = nullptr;
    
    UPROPERTY()
    UMMFuzzyProcessor* FuzzyProcessor = nullptr;
    
    // Configuration
    FMotionSemanticConfig CurrentConfig;
    FMotionVocabulary MotionVocabulary;
    
    // Caching
    mutable TMap<uint32, FMotionSemanticResult> ResultCache;
    mutable FCriticalSection CacheMutex;
    
    // Statistics
    mutable FThreadSafeCounter CacheHits;
    mutable FThreadSafeCounter CacheMisses;
    mutable FThreadSafeCounter TotalAnalyses;
    
    // Thread pool for parallel analysis
    FQueuedThreadPool* ThreadPool = nullptr;
    
    // Internal analysis methods
    FMotionIntentResult ClassifyIntent(const FMMTokenStream& Tokens) const;
    FMotionSpeedResult AnalyzeSpeed(const FMMTokenStream& Tokens) const;
    FMotionDirectionResult AnalyzeDirection(const FMMTokenStream& Tokens) const;
    FMotionTransition DetectTransition(const FMMTokenStream& Tokens) const;
    
    // Helper methods
    float CalculateTokenRelevance(const FMMFuzzyToken& Token, const FString& Category) const;
    TArray<FString> ExtractKeywordsForCategory(const FMMTokenStream& Tokens, const FString& Category) const;
    static bool IsValidTransition(EMotionState From, EMotionState To);
    static float GetTransitionPriority(EMotionState From, EMotionState To);
    
    // Cache management
    void TrimCache();
    static uint32 GetCacheKey(const FString& Text);
    
public:
    // ========== LIFECYCLE ==========
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // ========== CONFIGURATION ==========
    
    /**
     * Initialize with custom configuration
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Config")
    void InitializeAnalyzer(const FMotionSemanticConfig& Config);
    
    /**
     * Set custom motion vocabulary
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Config")
    void SetMotionVocabulary(const FMotionVocabulary& Vocabulary);
    
    /**
     * Load default motion vocabulary
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Config")
    void LoadDefaultVocabulary();
    
    // ========== MAIN ANALYSIS ==========
    
    /**
     * Analyze motion text with full semantic understanding
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Analysis")
    FMotionSemanticResult AnalyzeMotionText(const FString& Text);
    
    /**
     * Analyze pre-tokenized stream
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Analysis")
    FMotionSemanticResult AnalyzeTokenStream(const FMMTokenStream& TokenStream);
    
    /**
     * Analyze animation asset name
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Analysis")
    FMotionSemanticResult AnalyzeAnimationName(const FString& AnimationName);
    
    // ========== SPECIFIC ANALYSIS ==========
    
    /**
     * Classify motion intent only
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Intent")
    FMotionIntentResult ClassifyMotionIntent(const FString& Text);
    
    /**
     * Analyze speed/intensity only
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Speed")
    FMotionSpeedResult AnalyzeMotionSpeed(const FString& Text);
    
    /**
     * Analyze direction only
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Direction")
    FMotionDirectionResult AnalyzeMotionDirection(const FString& Text);
    
    /**
     * Detect state transition
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Transition")
    FMotionTransition DetectMotionTransition(const FString& Text);
    
    // ========== SIMILARITY & COMPARISON ==========
    
    /**
     * Compare semantic similarity between two motion descriptions
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Similarity")
    float CompareMotionSemantics(const FString& MotionA, const FString& MotionB);
    
    /**
     * Calculate similarity between semantic results
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Similarity")
    static float CompareSemanticResults(const FMotionSemanticResult& ResultA, const FMotionSemanticResult& ResultB);
    
    /**
     * Check if two motions are semantically equivalent
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Similarity")
    bool AreMotionsSemanticallyEquivalent(const FString& MotionA, const FString& MotionB, float Threshold = 0.8f);
    
    // ========== QUERY EXPANSION ==========
    
    /**
     * Expand motion query for PoseSearch
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Query")
    TArray<FString> ExpandMotionQuery(const FString& Query, int32 MaxExpansions = 10);
    
    /**
     * Generate motion variants with semantic understanding
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Query")
    TArray<FString> GenerateMotionVariants(const FString& Motion);
    
    /**
     * Get semantically similar motions
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Query")
    TArray<FString> GetSimilarMotions(const FString& Motion, float MinSimilarity = 0.7f);
    
    // ========== UTILITIES ==========
    
    /**
     * Clear analysis cache
     */
    UFUNCTION(BlueprintCallable, Category = "MM|Semantic|Cache")
    void ClearCache();
    
    /**
     * Get cache statistics
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Cache")
    void GetCacheStatistics(int32& OutHits, int32& OutMisses, int32& OutSize) const;
    
    /**
     * Get cache hit rate
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Cache")
    float GetCacheHitRate() const;
    
    /**
     * Validate state transition
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Validation")
    static bool ValidateStateTransition(EMotionState From, EMotionState To);
    
    /**
     * Get valid transitions from state
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Validation")
    static TArray<EMotionState> GetValidTransitionsFrom(EMotionState State);
    
    /**
     * Convert speed to normalized intensity
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Conversion")
    static float SpeedToIntensity(EMotionSpeed Speed);
    
    /**
     * Convert intensity to speed level
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Conversion")
    static EMotionSpeed IntensityToSpeed(float Intensity);
    
    /**
     * Convert direction to vector
     */
    UFUNCTION(BlueprintPure, Category = "MM|Semantic|Conversion")
    static FVector DirectionToVector(EMotionDirection Direction);
};

#endif
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Semantic/Data/RETokenTypes.h"
#include "Semantic/Data/RESemanticTypes.h"
#include "MMMotionSemanticAnalyzer.generated.h"

// Forward declarations
class UMMSemanticEngineCore;
class UMMTokenizer;
class UMMFuzzyMatcher;
class UMMPatternEngine;

/**
 * Motion type enumeration
 */
UENUM(BlueprintType)
enum class EMotionType : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Walk        UMETA(DisplayName = "Walk"),
    Run         UMETA(DisplayName = "Run"),
    Sprint      UMETA(DisplayName = "Sprint"),
    Jump        UMETA(DisplayName = "Jump"),
    Turn        UMETA(DisplayName = "Turn"),
    Strafe      UMETA(DisplayName = "Strafe"),
    Crouch      UMETA(DisplayName = "Crouch"),
    Crawl       UMETA(DisplayName = "Crawl"),
    Climb       UMETA(DisplayName = "Climb"),
    Unknown     UMETA(DisplayName = "Unknown")
};

/**
 * Motion semantic analysis result
 */
USTRUCT(BlueprintType)
struct FMotionSemanticResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    EMotionType MotionType = EMotionType::Unknown;
    
    UPROPERTY(BlueprintReadOnly)
    float Speed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    FVector Direction = FVector::ZeroVector;
    
    UPROPERTY(BlueprintReadOnly)
    float Confidence = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    FRETokenStream TokenStream;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> DetectedKeywords;
    
    UPROPERTY(BlueprintReadOnly)
    FString AnalysisDetails;
};

/**
 * Motion intent classification result
 */
USTRUCT(BlueprintType)
struct FMotionIntentResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FString PrimaryIntent;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> SecondaryIntents;
    
    UPROPERTY(BlueprintReadOnly)
    float Confidence = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, float> IntentScores;
};

/**
 * Motion speed analysis result
 */
USTRUCT(BlueprintType)
struct FMotionSpeedResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    float EstimatedSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    FString SpeedCategory;  // "Slow", "Medium", "Fast"
    
    UPROPERTY(BlueprintReadOnly)
    float Confidence = 0.0f;
};

/**
 * Motion direction analysis result
 */
USTRUCT(BlueprintType)
struct FMotionDirectionResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FVector Direction = FVector::ZeroVector;
    
    UPROPERTY(BlueprintReadOnly)
    FString DirectionName;  // "Forward", "Backward", "Left", "Right"
    
    UPROPERTY(BlueprintReadOnly)
    float Angle = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    float Confidence = 0.0f;
};

/**
 * Motion transition detection
 */
USTRUCT(BlueprintType)
struct FMotionTransition
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    EMotionType FromMotion = EMotionType::Unknown;
    
    UPROPERTY(BlueprintReadOnly)
    EMotionType ToMotion = EMotionType::Unknown;
    
    UPROPERTY(BlueprintReadOnly)
    float TransitionDuration = 0.0f;
    
    UPROPERTY(BlueprintReadOnly)
    FString TransitionType;  // "Start", "Stop", "Pivot", "Loop"
};

/**
 * Motion vocabulary for domain-specific terms
 */
USTRUCT()
struct FMotionVocabulary
{
    GENERATED_BODY()
    
    TSet<FString> IntentKeywords = {
        TEXT("walk"), TEXT("run"), TEXT("sprint"), TEXT("jump"),
        TEXT("turn"), TEXT("strafe"), TEXT("crouch"), TEXT("crawl"),
        TEXT("climb"), TEXT("idle"), TEXT("stand"), TEXT("sit")
    };
    
    TSet<FString> SpeedDescriptors = {
        TEXT("slow"), TEXT("fast"), TEXT("quick"), TEXT("rapid"),
        TEXT("moderate"), TEXT("normal"), TEXT("accelerate"), TEXT("decelerate")
    };
    
    TSet<FString> DirectionTerms = {
        TEXT("forward"), TEXT("backward"), TEXT("left"), TEXT("right"),
        TEXT("up"), TEXT("down"), TEXT("diagonal"), TEXT("strafe")
    };
    
    bool HasIntentKeyword(const FString& Word) const
    {
        return IntentKeywords.Contains(Word.ToLower());
    }
    
    bool HasSpeedDescriptor(const FString& Word) const
    {
        return SpeedDescriptors.Contains(Word.ToLower());
    }
    
    bool HasDirectionTerm(const FString& Word) const
    {
        return DirectionTerms.Contains(Word.ToLower());
    }
};

/**
 * Motion Semantic Analyzer using Plugin Systems
 * Analyzes animation names, commands, and motion descriptions
 */
UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UMMMotionSemanticAnalyzer : public UObject
{
    GENERATED_BODY()
    
private:
    // Plugin components
    UPROPERTY()
    URECore* SemanticEngine;
    
    UPROPERTY()
    URETokenizer* Tokenizer;
    
    UPROPERTY()
    REFuzzy* FuzzyMatcher;
    
    UPROPERTY()
    UREPatterns* PatternEngine;
    
    // Domain vocabulary
    FMotionVocabulary MotionVocabulary;
    
    // Pattern IDs for motion patterns
    static constexpr const TCHAR* PATTERN_ANIM_NAME = TEXT("AnimationName");
    static constexpr const TCHAR* PATTERN_MOTION_COMMAND = TEXT("MotionCommand");
    static constexpr const TCHAR* PATTERN_SPEED_DESC = TEXT("SpeedDescription");
    
public:
    UMMMotionSemanticAnalyzer();
    
    // ========== INITIALIZATION ==========
    
    /**
     * Initialize analyzer with plugin components
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Semantic")
    void Initialize();
    
    /**
     * Check if analyzer is ready
     */
    UFUNCTION(BlueprintPure, Category="MM|Motion|Semantic")
    bool IsInitialized() const;
    
    // ========== HIGH-LEVEL ANALYSIS ==========
    
    /**
     * Analyze any motion-related text
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Semantic")
    FMotionSemanticResult AnalyzeText(const FString& Text, const FString& Context = TEXT(""));
    
    /**
     * Analyze animation name specifically
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Semantic")
    FMotionSemanticResult AnalyzeAnimationName(const FString& AnimationName);
    
    /**
     * Analyze motion command
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Semantic")
    FMotionIntentResult AnalyzeMotionCommand(const FString& Command);
    
    // ========== SPECIFIC ANALYSIS ==========
    
    /**
     * Extract motion intent from text
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Intent")
    FMotionIntentResult ExtractMotionIntent(const FString& Text);
    
    /**
     * Analyze speed information
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Speed")
    FMotionSpeedResult ExtractSpeed(const FString& Text);
    
    /**
     * Analyze direction information
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Direction")
    FMotionDirectionResult ExtractDirection(const FString& Text);
    
    /**
     * Detect motion transitions
     */
    UFUNCTION(BlueprintCallable, Category="MM|Motion|Transition")
    FMotionTransition DetectTransition(const FString& Text);
    
    // ========== TOKEN ANALYSIS ==========
    
    /**
     * Analyze pre-tokenized stream
     */
    FMotionSemanticResult AnalyzeTokenStream(const FTokenStream& TokenStream);
    
    /**
     * Classify intent from tokens
     */
    FMotionIntentResult ClassifyIntent(const FTokenStream& Tokens) const;
    
    /**
     * Analyze speed from tokens
     */
    FMotionSpeedResult AnalyzeSpeed(const FTokenStream& Tokens) const;
    
    /**
     * Analyze direction from tokens
     */
    FMotionDirectionResult AnalyzeDirection(const FTokenStream& Tokens) const;
    
    /**
     * Detect transition from tokens
     */
    FMotionTransition DetectTransition(const FTokenStream& Tokens) const;
    
    // ========== UTILITIES ==========
    
    /**
     * Calculate token relevance for category
     */
    float CalculateTokenRelevance(const FREFuzzyToken& Token, const FString& Category) const;
    
    /**
     * Check if tokens contain motion keywords
     */
    bool ContainsMotionKeywords(const FTokenStream& Tokens, const FString& Category) const;
    
    /**
     * Get motion type from keyword
     */
    EMotionType GetMotionTypeFromKeyword(const FString& Keyword) const;
    
private:
    /**
     * Register default patterns with pattern engine
     */
    void RegisterDefaultPatterns();
};