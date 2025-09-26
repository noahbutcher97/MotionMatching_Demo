// Public/Core/MMFuzzyProcessor.h
#pragma once
#if 0
#include "CoreMinimal.h"
#include "Core/MMFuzzyCore.h"
//#include "Engine/EngineSubsystem.h"
#include "MMFuzzyProcessor.generated.h"

// Forward declarations
class UFuzzyMatcher;

// ============================================================================
// TOKEN ENUMS
// ============================================================================

/**
 * Token types for categorization
 */
UENUM(BlueprintType)
enum class EMTokenType : uint8
{
    Unknown         UMETA(DisplayName = "Unknown"),
    Word           UMETA(DisplayName = "Word"),
    Number         UMETA(DisplayName = "Number"),
    Separator      UMETA(DisplayName = "Separator"),
    Punctuation    UMETA(DisplayName = "Punctuation"),
    CamelCase      UMETA(DisplayName = "CamelCase"),
    SnakeCase      UMETA(DisplayName = "SnakeCase"),
    Abbreviation   UMETA(DisplayName = "Abbreviation"),
    Compound       UMETA(DisplayName = "Compound"),
    Mixed          UMETA(DisplayName = "Mixed")
};

/**
 * Token confidence levels
 */
UENUM(BlueprintType)
enum class EMMTokenConfidence : uint8
{
    VeryLow    = 0  UMETA(DisplayName = "Very Low (0-20%)"),
    Low        = 1  UMETA(DisplayName = "Low (20-40%)"),
    Medium     = 2  UMETA(DisplayName = "Medium (40-60%)"),
    High       = 3  UMETA(DisplayName = "High (60-80%)"),
    VeryHigh   = 4  UMETA(DisplayName = "Very High (80-100%)")
};

// ============================================================================
// TOKEN STRUCTURES
// ============================================================================

/**
 * Single token with metadata
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMFuzzyToken
{
    GENERATED_BODY()

    /** Original text of the token */
    UPROPERTY(BlueprintReadOnly, Category="Token")
    FString Text;
    
    /** Normalized/canonical form */
    UPROPERTY(BlueprintReadOnly, Category="Token")
    FString Canonical;
    
    /** Token type */
    UPROPERTY(BlueprintReadOnly, Category="Token")
    EMTokenType Type = EMTokenType::Unknown;
    
    /** Position in original string */
    UPROPERTY(BlueprintReadOnly, Category="Token")
    int32 StartIndex = -1;
    
    UPROPERTY(BlueprintReadOnly, Category="Token")
    int32 EndIndex = -1;
    
    /** Confidence in token boundaries */
    UPROPERTY(BlueprintReadOnly, Category="Token", meta=(ClampMin=0, ClampMax=1))
    float BoundaryConfidence = 1.0f;
    
    /** Is this a fuzzy/uncertain token? */
    UPROPERTY(BlueprintReadOnly, Category="Token")
    bool bIsFuzzy = false;
    
    /** Possible variations of this token */
    UPROPERTY(BlueprintReadOnly, Category="Variants")
    TArray<FString> Variants;
    
    /** Weights for each variant (parallel array) */
    UPROPERTY(BlueprintReadOnly, Category="Variants")
    TArray<float> VariantWeights;
    
    /** Context hints for disambiguation */
    UPROPERTY(BlueprintReadOnly, Category="Context")
    TMap<FString, FString> ContextHints;
    
    FMFuzzyToken() = default;
    FMFuzzyToken(const FString& InText, int32 Start, int32 End);
    
    void AddVariant(const FString& Variant, float Weight = 1.0f);
    bool HasVariant(const FString& Variant) const;
    float GetVariantWeight(const FString& Variant) const;
    FString GetBestVariant() const;
    
    bool IsAlpha() const;
    bool IsNumeric() const;
    bool IsAlphaNumeric() const;
    bool IsSeparator() const;
    
    FString ToString() const;
    bool IsValid() const { return !Text.IsEmpty() && StartIndex >= 0; }
};

/**
 * Token similarity group for semantic matching
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMTokenGroup
{
    GENERATED_BODY()

    /** Group identifier */
    UPROPERTY(BlueprintReadOnly, Category="Group")
    FString GroupId;
    
    /** Canonical/primary form */
    UPROPERTY(BlueprintReadOnly, Category="Group")
    FString CanonicalForm;
    
    /** All members of this group */
    UPROPERTY(BlueprintReadOnly, Category="Group")
    TSet<FString> Members;
    
    /** Semantic category (e.g., "motion", "direction", "speed") */
    UPROPERTY(BlueprintReadOnly, Category="Group")
    FString Category;
    
    /** Priority for disambiguation */
    UPROPERTY(BlueprintReadOnly, Category="Group")
    int32 Priority = 0;
    
    void AddMember(const FString& Member);
    bool ContainsMember(const FString& Member) const;
    float GetSimilarityToMember(const FString& Text, UFuzzyMatcher* Matcher) const;
};

/**
 * Stream of tokens with navigation
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMTokenStream
{
    GENERATED_BODY()

    /** Original input string */
    UPROPERTY(BlueprintReadOnly, Category="Stream")
    FString OriginalText;
    
    /** All tokens in order */
    UPROPERTY(BlueprintReadOnly, Category="Stream")
    TArray<FMFuzzyToken> Tokens;
    
    /** Current position in stream */
    UPROPERTY(BlueprintReadOnly, Category="Stream")
    int32 CurrentIndex = 0;
    
    /** Overlapping/ambiguous interpretations */
    UPROPERTY(BlueprintReadOnly, Category="Stream")
    TArray<FMFuzzyToken> AlternativeTokens;
    
    /** Performance metrics */
    UPROPERTY(BlueprintReadOnly, Category="Performance")
    float TokenizationTimeMS = 0.0f;
    
    void Reset();
    void AddToken(const FMFuzzyToken& Token);
    void AddAlternativeToken(const FMFuzzyToken& Token);
    
    // Navigation
    bool HasNext() const;
    FMFuzzyToken GetNext();
    FMFuzzyToken Peek(int32 Ahead = 1) const;
    void Advance(int32 Count = 1);
    void Rewind(int32 Count = 1);
    
    // Analysis
    int32 GetTokenCount() const { return Tokens.Num(); }
    TArray<FMFuzzyToken> GetTokensOfType(EMTokenType Type) const;
    TArray<FString> GetAllText() const;
    TArray<FString> GetAllCanonical() const;
    
    FString ToString() const;
    FString ToCanonicalString() const;
    bool IsValid() const { return !OriginalText.IsEmpty(); }
};

// ============================================================================
// TOKENIZATION CONFIGURATIONS
// ============================================================================

/**
 * Configuration for fuzzy tokenization
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMTokenizerConfig
{
    GENERATED_BODY()

    /** Minimum confidence to accept a token boundary */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boundaries", meta=(ClampMin=0, ClampMax=1))
    float MinBoundaryConfidence = 0.5f;
    
    /** Generate typo variants automatically */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variants")
    bool bGenerateTypoVariants = true;
    
    /** Maximum edit distance for typo variants */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variants", meta=(ClampMin=1, ClampMax=3))
    int32 MaxTypoDistance = 2;
    
    /** Generate abbreviation variants */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variants")
    bool bGenerateAbbreviations = true;
    
    /** Recognize compound words */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Compounds")
    bool bRecognizeCompounds = true;
    
    /** Split camelCase into tokens */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Splitting")
    bool bSplitCamelCase = true;
    
    /** Split snake_case into tokens */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Splitting")
    bool bSplitSnakeCase = true;
    
    /** Split numbers from text */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Splitting")
    bool bSplitNumbers = true;
    
    /** Preserve original tokens alongside splits */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Splitting")
    bool bPreserveOriginal = true;
    
    /** Custom separators beyond standard whitespace */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Separators")
    FString CustomSeparators = "_-/.";
    
    /** Known abbreviations to recognize */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abbreviations")
    TMap<FString, FString> KnownAbbreviations;
    
    /** Known compound words */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Compounds")
    TSet<FString> KnownCompounds;
    
    FMTokenizerConfig();
    void LoadDefaults();
};

/**
 * Context for disambiguation
 */
USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMMTokenContext
{
    GENERATED_BODY()

    /** Domain (e.g., "animation", "combat", "ui") */
    UPROPERTY(BlueprintReadWrite, Category="Context")
    FString Domain;
    
    /** Previous tokens for context */
    UPROPERTY(BlueprintReadWrite, Category="Context")
    TArray<FMFuzzyToken> PreviousTokens;
    
    /** Expected token types */
    UPROPERTY(BlueprintReadWrite, Category="Context")
    TArray<EMTokenType> ExpectedTypes;
    
    /** Known vocabulary for this context */
    UPROPERTY(BlueprintReadWrite, Category="Context")
    TSet<FString> Vocabulary;
    
    /** Priority hints for ambiguous cases */
    UPROPERTY(BlueprintReadWrite, Category="Context")
    TMap<FString, int32> PriorityHints;
    
    bool HasVocabulary() const { return Vocabulary.Num() > 0; }
    bool IsInVocabulary(const FString& Word) const;
    int32 GetPriority(const FString& Word) const;
};

// ============================================================================
// MAIN PROCESSOR CLASS
// ============================================================================

/**
 * Fuzzy text processor for tokenization and variant generation
 * Layer 1 of the fuzzy-semantic architecture
 * Extends the MM fuzzy matching system with tokenization capabilities
 */
UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UMMFuzzyProcessor : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    // ========== LIFECYCLE ==========
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // ========== INITIALIZATION ==========
    
    /**
     * Initialize with configuration
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Processor")
    void InitializeProcessor(const FMTokenizerConfig& Config);
    
    // ========== TOKENIZATION ==========
    
    /**
     * Tokenize with fuzzy tolerance
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Tokenization")
    FMTokenStream Tokenize(const FString& Input, const FMMTokenContext& Context);
    
    /**
     * Tokenize animation asset name (handles naming conventions)
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Tokenization")
    FMTokenStream TokenizeAnimationName(const FString& AnimationName);
    
    /**
     * Tokenize natural language command
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Tokenization")
    FMTokenStream TokenizeCommand(const FString& Command);
    
    // ========== VARIANT GENERATION ==========
    
    /**
     * Generate variants for a token
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Variants")
    TArray<FString> GenerateVariants(const FString& Token, int32 MaxVariants = 10);
    
    /**
     * Generate typo variants
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Variants")
    TArray<FString> GenerateTypoVariants(const FString& Word, int32 MaxDistance = 2);
    
    /**
     * Generate abbreviation variants
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Variants")
    TArray<FString> GenerateAbbreviationVariants(const FString& Word);
    
    // ========== SIMILARITY GROUPS ==========
    
    /**
     * Register a token similarity group
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Groups")
    void RegisterTokenGroup(const FMTokenGroup& Group);
    
    /**
     * Find groups containing a token
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Groups")
    TArray<FMTokenGroup> FindTokenGroups(const FString& Token);
    
    /**
     * Get canonical form from any variant
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Groups")
    FString GetCanonicalForm(const FString& Token);
    
    // ========== COMPOUND WORDS ==========
    
    /**
     * Check if tokens could form a compound
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Compounds")
    bool CouldBeCompound(const TArray<FMFuzzyToken>& Tokens);
    
    /**
     * Merge tokens into compound
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Compounds")
    FMFuzzyToken MergeToCompound(const TArray<FMFuzzyToken>& Tokens);
    
    /**
     * Split compound into components
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Compounds")
    TArray<FMFuzzyToken> SplitCompound(const FMFuzzyToken& Compound);
    
    // ========== CONTEXT RESOLUTION ==========
    
    /**
     * Disambiguate based on context
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Context")
    FMFuzzyToken DisambiguateToken(const FMFuzzyToken& Token, const FMMTokenContext& Context);
    
    /**
     * Score token in context
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Context")
    float ScoreTokenInContext(const FMFuzzyToken& Token, const FMMTokenContext& Context);
    
    // ========== UTILITIES ==========
    
    /**
     * Normalize text for processing
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Utilities")
    static FString NormalizeText(const FString& Text);
    
    /**
     * Detect naming convention
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Utilities")
    static EMTokenType DetectNamingConvention(const FString& Text);
    
    /**
     * Is likely abbreviation?
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Utilities")
    static bool IsLikelyAbbreviation(const FString& Text);
    
    /**
     * Get token confidence level
     */
    UFUNCTION(BlueprintCallable, Category="MM|Fuzzy|Utilities")
    static EMMTokenConfidence GetConfidenceLevel(float Confidence);

protected:
    // ========== INTERNAL METHODS ==========
    
    /** Split on separators */
    TArray<FMFuzzyToken> SplitBySeparators(const FString& Input);
    
    /** Split camelCase text */
    static TArray<FMFuzzyToken> SplitCamelCase(const FString& Text, int32 StartIndex);
    
    /** Split snake_case text */
    static TArray<FMFuzzyToken> SplitSnakeCase(const FString& Text, int32 StartIndex);
    
    /** Split numbers from text */
    static TArray<FMFuzzyToken> SplitNumbers(const FString& Text, int32 StartIndex);
    
    /** Find fuzzy boundaries */
    TArray<int32> FindFuzzyBoundaries(const FString& Text, float& OutConfidence);
    
    /** Generate single typo */
    static FString GenerateSingleTypo(const FString& Word, int32 TypeIndex);
    
    /** Calculate boundary confidence */
    float CalculateBoundaryConfidence(const FString& Text, int32 BoundaryIndex);
    
    /** Load default abbreviations */
    void LoadDefaultAbbreviations();
    
    /** Load default compounds */
    void LoadDefaultCompounds();
    
    /** Load animation vocabulary */
    void LoadAnimationVocabulary();

private:
    UPROPERTY()
    FMTokenizerConfig CurrentConfig;
    
    /** Reference to Layer 0 fuzzy matcher */
    UPROPERTY()
    UFuzzyMatcher* FuzzyMatcher;
    
    /** Registered token groups */
    TMap<FString, FMTokenGroup> TokenGroups;
    
    /** Canonical form mappings */
    TMap<FString, FString> CanonicalMappings;
    
    /** Animation-specific vocabulary */
    TSet<FString> AnimationVocabulary;
    
    /** Motion vocabulary */
    TSet<FString> MotionVocabulary;
    
    /** Direction vocabulary */
    TSet<FString> DirectionVocabulary;
    
    /** Common English words for filtering */
    TSet<FString> CommonWords;
    
    /** Cache for variant generation */
    TMap<FString, TArray<FString>> VariantCache;
    
    /** Performance tracking */
    double LastTokenizationTime;
    int32 TotalTokenizationCount;
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline void FMFuzzyToken::AddVariant(const FString& Variant, float Weight)
{
    if (!HasVariant(Variant))
    {
        Variants.Add(Variant);
        VariantWeights.Add(Weight);
    }
}

inline bool FMFuzzyToken::HasVariant(const FString& Variant) const
{
    return Variants.Contains(Variant);
}

inline bool FMTokenStream::HasNext() const
{
    return CurrentIndex < Tokens.Num();
}

inline void FMTokenStream::Advance(int32 Count)
{
    CurrentIndex = FMath::Min(CurrentIndex + Count, Tokens.Num());
}

inline void FMTokenStream::Rewind(int32 Count)
{
    CurrentIndex = FMath::Max(CurrentIndex - Count, 0);
}

inline EMMTokenConfidence UMMFuzzyProcessor::GetConfidenceLevel(float Confidence)
{
    if (Confidence >= 0.8f) return EMMTokenConfidence::VeryHigh;
    if (Confidence >= 0.6f) return EMMTokenConfidence::High;
    if (Confidence >= 0.4f) return EMMTokenConfidence::Medium;
    if (Confidence >= 0.2f) return EMMTokenConfidence::Low;
    return EMMTokenConfidence::VeryLow;
}
#endif
