// MMSemanticTypes.h
// Simplified semantic analysis for speed clustering from animation names
#pragma once

#include "CoreMinimal.h"
#include "MMSemanticTypes.generated.h"

// ============================================================================
// CORE ENUMS - Single source of truth
// ============================================================================

UENUM(BlueprintType)
enum class EMotionIntent : uint8
{
    None         UMETA(DisplayName = "None"),
    Locomotion   UMETA(DisplayName = "Locomotion"),
    Combat       UMETA(DisplayName = "Combat"),
    Interaction  UMETA(DisplayName = "Interaction"),
    Idle         UMETA(DisplayName = "Idle"),
    Transition   UMETA(DisplayName = "Transition")
};

UENUM(BlueprintType)
enum class EMotionSpeed : uint8
{
    Idle       = 0 UMETA(DisplayName = "Idle"),
    Walk       = 1 UMETA(DisplayName = "Walk"),
    Jog        = 2 UMETA(DisplayName = "Jog"),
    Run        = 3 UMETA(DisplayName = "Run"),
    Sprint     = 4 UMETA(DisplayName = "Sprint")
};

UENUM(BlueprintType)
enum class EMotionDirection : uint8
{
    None           UMETA(DisplayName = "None"),
    Forward        UMETA(DisplayName = "Forward"),
    Backward       UMETA(DisplayName = "Backward"),
    Left           UMETA(DisplayName = "Left"),
    Right          UMETA(DisplayName = "Right"),
    ForwardLeft    UMETA(DisplayName = "Forward-Left"),
    ForwardRight   UMETA(DisplayName = "Forward-Right"),
    BackwardLeft   UMETA(DisplayName = "Backward-Left"),
    BackwardRight  UMETA(DisplayName = "Backward-Right")
};

// ============================================================================
// SIMPLE RESULT STRUCTS
// ============================================================================

USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionIntentResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    EMotionIntent PrimaryIntent = EMotionIntent::None;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    TArray<EMotionIntent> SecondaryIntents;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    TMap<EMotionIntent, float> IntentConfidences;
};

USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionSpeedResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    EMotionSpeed SpeedLevel = EMotionSpeed::Idle;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    float EstimatedSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    FString SpeedCategory;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    float NormalizedIntensity = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    float Confidence = 0.0f;
};

USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionDirectionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    EMotionDirection Direction = EMotionDirection::None;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    FString DirectionName;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    float Confidence = 0.0f;
};

// ============================================================================
// COMBINED SEMANTIC RESULT
// ============================================================================

USTRUCT(BlueprintType)
struct MOTIONMATCHING_DEMO_API FMotionSemanticResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    FString OriginalText;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    FMotionIntentResult Intent;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    FMotionSpeedResult Speed;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    FMotionDirectionResult Direction;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    float OverallConfidence = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Semantic")
    TArray<FString> DetectedKeywords;
    
    bool IsValid() const { return !OriginalText.IsEmpty() && OverallConfidence > 0.0f; }
};

// ============================================================================
// SIMPLE NAME ANALYZER - No plugin dependencies
// ============================================================================

UCLASS(BlueprintType)
class MOTIONMATCHING_DEMO_API UMMSemanticNameAnalyzer : public UObject
{
    GENERATED_BODY()
    
public:
    /**
     * Analyze animation name using simple keyword matching
     * NO plugin dependencies - pure string parsing
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic")
    static FMotionSemanticResult AnalyzeAnimationName(const FString& AnimationName);
    
    /**
     * Extract speed from animation name
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic")
    static FMotionSpeedResult ExtractSpeed(const FString& AnimationName);
    
    /**
     * Extract intent from animation name
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic")
    static FMotionIntentResult ExtractIntent(const FString& AnimationName);
    
    /**
     * Extract direction from animation name
     */
    UFUNCTION(BlueprintCallable, Category="MM|Semantic")
    static FMotionDirectionResult ExtractDirection(const FString& AnimationName);

private:
    // Simple keyword dictionaries
    static TMap<FString, EMotionSpeed> SpeedKeywords;
    static TMap<FString, EMotionIntent> IntentKeywords;
    static TMap<FString, EMotionDirection> DirectionKeywords;
    
    static void InitializeKeywords();
    static TArray<FString> TokenizeAnimationName(const FString& Name);
    static float CalculateConfidence(int32 MatchedKeywords, int32 TotalTokens);
};