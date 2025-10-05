// MMSemanticTypes.cpp
#include "Data/MMSemanticTypes.h"

// Initialize static keyword maps
TMap<FString, EMotionSpeed> UMMSemanticNameAnalyzer::SpeedKeywords;
TMap<FString, EMotionIntent> UMMSemanticNameAnalyzer::IntentKeywords;
TMap<FString, EMotionDirection> UMMSemanticNameAnalyzer::DirectionKeywords;

void UMMSemanticNameAnalyzer::InitializeKeywords()
{
    static bool bInitialized = false;
    if (bInitialized) return;
    
    // Speed keywords
    SpeedKeywords.Add(TEXT("idle"), EMotionSpeed::Idle);
    SpeedKeywords.Add(TEXT("stand"), EMotionSpeed::Idle);
    SpeedKeywords.Add(TEXT("stop"), EMotionSpeed::Idle);
    
    SpeedKeywords.Add(TEXT("walk"), EMotionSpeed::Walk);
    SpeedKeywords.Add(TEXT("walking"), EMotionSpeed::Walk);
    
    SpeedKeywords.Add(TEXT("jog"), EMotionSpeed::Jog);
    SpeedKeywords.Add(TEXT("jogging"), EMotionSpeed::Jog);
    
    SpeedKeywords.Add(TEXT("run"), EMotionSpeed::Run);
    SpeedKeywords.Add(TEXT("running"), EMotionSpeed::Run);
    
    SpeedKeywords.Add(TEXT("sprint"), EMotionSpeed::Sprint);
    SpeedKeywords.Add(TEXT("sprinting"), EMotionSpeed::Sprint);
    SpeedKeywords.Add(TEXT("fast"), EMotionSpeed::Sprint);
    
    // Intent keywords
    IntentKeywords.Add(TEXT("idle"), EMotionIntent::Idle);
    IntentKeywords.Add(TEXT("stand"), EMotionIntent::Idle);
    
    IntentKeywords.Add(TEXT("walk"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("run"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("jog"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("sprint"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("move"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("strafe"), EMotionIntent::Locomotion);
    
    IntentKeywords.Add(TEXT("attack"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("hit"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("punch"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("kick"), EMotionIntent::Combat);
    
    IntentKeywords.Add(TEXT("start"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("stop"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("turn"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("pivot"), EMotionIntent::Transition);
    
    // Direction keywords
    DirectionKeywords.Add(TEXT("forward"), EMotionDirection::Forward);
    DirectionKeywords.Add(TEXT("fwd"), EMotionDirection::Forward);
    DirectionKeywords.Add(TEXT("front"), EMotionDirection::Forward);
    
    DirectionKeywords.Add(TEXT("backward"), EMotionDirection::Backward);
    DirectionKeywords.Add(TEXT("back"), EMotionDirection::Backward);
    DirectionKeywords.Add(TEXT("bwd"), EMotionDirection::Backward);
    
    DirectionKeywords.Add(TEXT("left"), EMotionDirection::Left);
    DirectionKeywords.Add(TEXT("l"), EMotionDirection::Left);
    
    DirectionKeywords.Add(TEXT("right"), EMotionDirection::Right);
    DirectionKeywords.Add(TEXT("r"), EMotionDirection::Right);
    
    bInitialized = true;
}

TArray<FString> UMMSemanticNameAnalyzer::TokenizeAnimationName(const FString& Name)
{
    TArray<FString> Tokens;
    FString LowerName = Name.ToLower();
    
    // Split by common delimiters: underscore, dash, space, camelCase
    FString CurrentToken;
    bool bLastWasLower = false;
    
    for (int32 i = 0; i < LowerName.Len(); i++)
    {
        TCHAR Char = LowerName[i];
        
        // Delimiters
        if (Char == '_' || Char == '-' || Char == ' ' || Char == '.')
        {
            if (!CurrentToken.IsEmpty())
            {
                Tokens.Add(CurrentToken);
                CurrentToken.Empty();
            }
            bLastWasLower = false;
            continue;
        }
        
        // CamelCase detection
        if (FChar::IsUpper(Name[i]) && bLastWasLower && !CurrentToken.IsEmpty())
        {
            Tokens.Add(CurrentToken);
            CurrentToken.Empty();
        }
        
        CurrentToken.AppendChar(FChar::ToLower(Char));
        bLastWasLower = FChar::IsLower(Char);
    }
    
    if (!CurrentToken.IsEmpty())
    {
        Tokens.Add(CurrentToken);
    }
    
    return Tokens;
}

float UMMSemanticNameAnalyzer::CalculateConfidence(int32 MatchedKeywords, int32 TotalTokens)
{
    if (TotalTokens == 0) return 0.0f;
    
    float BaseConfidence = static_cast<float>(MatchedKeywords) / static_cast<float>(TotalTokens);
    
    // Boost confidence if we found any matches
    if (MatchedKeywords > 0)
    {
        BaseConfidence = FMath::Min(1.0f, BaseConfidence + 0.2f);
    }
    
    return BaseConfidence;
}

FMotionSemanticResult UMMSemanticNameAnalyzer::AnalyzeAnimationName(const FString& AnimationName)
{
    InitializeKeywords();
    
    FMotionSemanticResult Result;
    Result.OriginalText = AnimationName;
    
    TArray<FString> Tokens = TokenizeAnimationName(AnimationName);
    
    // Extract components
    Result.Speed = ExtractSpeed(AnimationName);
    Result.Intent = ExtractIntent(AnimationName);
    Result.Direction = ExtractDirection(AnimationName);
    
    // Overall confidence is average of component confidences
    int32 ComponentCount = 0;
    float TotalConfidence = 0.0f;
    
    if (Result.Speed.Confidence > 0.0f)
    {
        TotalConfidence += Result.Speed.Confidence;
        ComponentCount++;
    }
    
    if (Result.Intent.IntentConfidences.Num() > 0)
    {
        float MaxIntentConf = 0.0f;
        for (const auto& Pair : Result.Intent.IntentConfidences)
        {
            MaxIntentConf = FMath::Max(MaxIntentConf, Pair.Value);
        }
        TotalConfidence += MaxIntentConf;
        ComponentCount++;
    }
    
    if (Result.Direction.Confidence > 0.0f)
    {
        TotalConfidence += Result.Direction.Confidence;
        ComponentCount++;
    }
    
    Result.OverallConfidence = ComponentCount > 0 ? TotalConfidence / ComponentCount : 0.0f;
    
    return Result;
}

FMotionSpeedResult UMMSemanticNameAnalyzer::ExtractSpeed(const FString& AnimationName)
{
    InitializeKeywords();
    
    FMotionSpeedResult Result;
    TArray<FString> Tokens = TokenizeAnimationName(AnimationName);
    
    int32 MatchCount = 0;
    EMotionSpeed DetectedSpeed = EMotionSpeed::Idle;
    
    for (const FString& Token : Tokens)
    {
        if (SpeedKeywords.Contains(Token))
        {
            DetectedSpeed = SpeedKeywords[Token];
            MatchCount++;
        }
    }
    
    Result.SpeedLevel = DetectedSpeed;
    Result.Confidence = CalculateConfidence(MatchCount, Tokens.Num());
    
    // Map speed level to estimated cm/s and normalized intensity
    switch (DetectedSpeed)
    {
        case EMotionSpeed::Idle:
            Result.EstimatedSpeed = 0.0f;
            Result.NormalizedIntensity = 0.0f;
            Result.SpeedCategory = TEXT("Idle");
            break;
        case EMotionSpeed::Walk:
            Result.EstimatedSpeed = 150.0f;
            Result.NormalizedIntensity = 0.3f;
            Result.SpeedCategory = TEXT("Walk");
            break;
        case EMotionSpeed::Jog:
            Result.EstimatedSpeed = 300.0f;
            Result.NormalizedIntensity = 0.6f;
            Result.SpeedCategory = TEXT("Jog");
            break;
        case EMotionSpeed::Run:
            Result.EstimatedSpeed = 450.0f;
            Result.NormalizedIntensity = 0.8f;
            Result.SpeedCategory = TEXT("Run");
            break;
        case EMotionSpeed::Sprint:
            Result.EstimatedSpeed = 600.0f;
            Result.NormalizedIntensity = 1.0f;
            Result.SpeedCategory = TEXT("Sprint");
            break;
    }
    
    return Result;
}

FMotionIntentResult UMMSemanticNameAnalyzer::ExtractIntent(const FString& AnimationName)
{
    InitializeKeywords();
    
    FMotionIntentResult Result;
    TArray<FString> Tokens = TokenizeAnimationName(AnimationName);
    
    TMap<EMotionIntent, int32> IntentCounts;
    int32 TotalMatches = 0;
    
    for (const FString& Token : Tokens)
    {
        if (IntentKeywords.Contains(Token))
        {
            EMotionIntent Intent = IntentKeywords[Token];
            
            if (!IntentCounts.Contains(Intent))
            {
                IntentCounts.Add(Intent, 0);
            }
            IntentCounts[Intent]++;
            TotalMatches++;
        }
    }
    
    // Find primary intent (most common)
    EMotionIntent PrimaryIntent = EMotionIntent::None;
    int32 MaxCount = 0;
    
    for (const auto& Pair : IntentCounts)
    {
        if (Pair.Value > MaxCount)
        {
            MaxCount = Pair.Value;
            PrimaryIntent = Pair.Key;
        }
    }
    
    Result.PrimaryIntent = PrimaryIntent;
    
    // Calculate confidences
    for (const auto& Pair : IntentCounts)
    {
        float Confidence = TotalMatches > 0 ? 
            static_cast<float>(Pair.Value) / static_cast<float>(TotalMatches) : 0.0f;
        
        Result.IntentConfidences.Add(Pair.Key, Confidence);
        
        // Add to secondary if not primary
        if (Pair.Key != PrimaryIntent)
        {
            Result.SecondaryIntents.Add(Pair.Key);
        }
    }
    
    return Result;
}

FMotionDirectionResult UMMSemanticNameAnalyzer::ExtractDirection(const FString& AnimationName)
{
    InitializeKeywords();
    
    FMotionDirectionResult Result;
    TArray<FString> Tokens = TokenizeAnimationName(AnimationName);
    
    int32 MatchCount = 0;
    EMotionDirection DetectedDirection = EMotionDirection::None;
    
    for (const FString& Token : Tokens)
    {
        if (DirectionKeywords.Contains(Token))
        {
            DetectedDirection = DirectionKeywords[Token];
            Result.DirectionName = Token;
            MatchCount++;
        }
    }
    
    Result.Direction = DetectedDirection;
    Result.Confidence = CalculateConfidence(MatchCount, Tokens.Num());
    
    return Result;
}