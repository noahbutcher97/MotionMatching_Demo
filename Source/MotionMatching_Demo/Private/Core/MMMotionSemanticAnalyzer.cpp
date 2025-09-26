// Private/Core/MMMotionSemanticAnalyzer.cpp
#if 0
#include "Core/MMMotionSemanticAnalyzer.h"
#include "Core/MMFuzzyCore.h"
#include "Core/MMFuzzyProcessor.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"
#include "Engine/Engine.h"

// ============================================================================
// MOTION INTENT RESULT IMPLEMENTATION
// ============================================================================

float FMotionIntentResult::GetPrimaryConfidence() const
{
    if (const float* Confidence = IntentConfidences.Find(PrimaryIntent))
    {
        return *Confidence;
    }
    return 0.0f;
}

bool FMotionIntentResult::HasIntent(EMotionIntent Intent) const
{
    if (PrimaryIntent == Intent)
        return true;
    
    return SecondaryIntents.Contains(Intent);
}

FString FMotionIntentResult::ToString() const
{
    FString Result = FString::Printf(TEXT("Intent: %s (%.2f)"), 
        *UEnum::GetValueAsString(PrimaryIntent), GetPrimaryConfidence());
    
    if (SecondaryIntents.Num() > 0)
    {
        Result += TEXT(" Secondary: ");
        for (EMotionIntent Secondary : SecondaryIntents)
        {
            float Conf = IntentConfidences.Contains(Secondary) ? IntentConfidences[Secondary] : 0.0f;
            Result += FString::Printf(TEXT("%s(%.2f) "), *UEnum::GetValueAsString(Secondary), Conf);
        }
    }
    
    return Result;
}

// ============================================================================
// MOTION SPEED RESULT IMPLEMENTATION
// ============================================================================

float FMotionSpeedResult::SpeedLevelToIntensity(EMotionSpeed Speed)
{
    switch (Speed)
    {
        case EMotionSpeed::Idle:   return 0.0f;
        case EMotionSpeed::Slow:   return 0.2f;
        case EMotionSpeed::Walk:   return 0.4f;
        case EMotionSpeed::Jog:    return 0.6f;
        case EMotionSpeed::Run:    return 0.8f;
        case EMotionSpeed::Sprint: return 1.0f;
        default: return 0.0f;
    }
}

EMotionSpeed FMotionSpeedResult::IntensityToSpeedLevel(float Intensity)
{
    if (Intensity <= 0.1f) return EMotionSpeed::Idle;
    if (Intensity <= 0.3f) return EMotionSpeed::Slow;
    if (Intensity <= 0.5f) return EMotionSpeed::Walk;
    if (Intensity <= 0.7f) return EMotionSpeed::Jog;
    if (Intensity <= 0.9f) return EMotionSpeed::Run;
    return EMotionSpeed::Sprint;
}

FString FMotionSpeedResult::ToString() const
{
    FString Result = FString::Printf(TEXT("Speed: %s (%.2f intensity, %.2f confidence)"),
        *UEnum::GetValueAsString(SpeedLevel), NormalizedIntensity, Confidence);
    
    if (SpeedModifiers.Num() > 0)
    {
        Result += TEXT(" Modifiers: ") + FString::Join(SpeedModifiers, TEXT(", "));
    }
    
    return Result;
}

// ============================================================================
// MOTION DIRECTION RESULT IMPLEMENTATION
// ============================================================================

FVector FMotionDirectionResult::GetDirectionVector() const
{
    if (Direction == EMotionDirection::Custom)
        return CustomDirection;
    
    switch (Direction)
    {
        case EMotionDirection::Forward:        return FVector::ForwardVector;
        case EMotionDirection::Backward:       return -FVector::ForwardVector;
        case EMotionDirection::Left:            return -FVector::RightVector;
        case EMotionDirection::Right:           return FVector::RightVector;
        case EMotionDirection::ForwardLeft:    return (FVector::ForwardVector - FVector::RightVector).GetSafeNormal();
        case EMotionDirection::ForwardRight:   return (FVector::ForwardVector + FVector::RightVector).GetSafeNormal();
        case EMotionDirection::BackwardLeft:    return (-FVector::ForwardVector - FVector::RightVector).GetSafeNormal();
        case EMotionDirection::BackwardRight:  return (-FVector::ForwardVector + FVector::RightVector).GetSafeNormal();
        case EMotionDirection::Up:              return FVector::UpVector;
        case EMotionDirection::Down:            return -FVector::UpVector;
        default: return FVector::ZeroVector;
    }
}

FString FMotionDirectionResult::ToString() const
{
    FString Result = FString::Printf(TEXT("Direction: %s (%s, %.2f confidence, %.2f ambiguity)"),
        *UEnum::GetValueAsString(Direction),
        *UEnum::GetValueAsString(DirectionType),
        Confidence, AmbiguityLevel);
    
    if (AlternativeDirections.Num() > 0)
    {
        Result += TEXT(" Alternatives: ");
        for (const auto& Alt : AlternativeDirections)
        {
            Result += FString::Printf(TEXT("%s(%.2f) "), 
                *UEnum::GetValueAsString(Alt.Key), Alt.Value);
        }
    }
    
    return Result;
}

// ============================================================================
// MOTION TRANSITION IMPLEMENTATION
// ============================================================================

FString FMotionTransition::ToString() const
{
    return FString::Printf(TEXT("Transition: %s -> %s (Valid:%s, Duration:%.2fs, Priority:%d, Confidence:%.2f)"),
        *UEnum::GetValueAsString(FromState),
        *UEnum::GetValueAsString(ToState),
        bIsValid ? TEXT("Yes") : TEXT("No"),
        DurationHint,
        Priority,
        Confidence);
}

// ============================================================================
// MOTION SEMANTIC RESULT IMPLEMENTATION
// ============================================================================

FString FMotionSemanticResult::GetSummary() const
{
    return FString::Printf(TEXT("Motion: Intent=%s, Speed=%.2f, Direction=%s, Confidence=%.2f"),
        *UEnum::GetValueAsString(Intent.PrimaryIntent),
        Speed.NormalizedIntensity,
        *UEnum::GetValueAsString(Direction.Direction),
        OverallConfidence);
}

FString FMotionSemanticResult::ToString() const
{
    FString Result = FString::Printf(TEXT("=== Motion Semantic Analysis ===\n"));
    Result += FString::Printf(TEXT("Input: %s\n"), *OriginalText);
    Result += FString::Printf(TEXT("%s\n"), *Intent.ToString());
    Result += FString::Printf(TEXT("%s\n"), *Speed.ToString());
    Result += FString::Printf(TEXT("%s\n"), *Direction.ToString());
    
    if (Transition.IsValid())
    {
        Result += FString::Printf(TEXT("%s\n"), *Transition.ToString());
    }
    
    Result += FString::Printf(TEXT("Overall Confidence: %.2f\n"), OverallConfidence);
    Result += FString::Printf(TEXT("Analysis Time: %.2fms %s\n"), 
        AnalysisTimeMS, bWasCached ? TEXT("(cached)") : TEXT(""));
    
    return Result;
}

// ============================================================================
// MOTION SEMANTIC CONFIG IMPLEMENTATION
// ============================================================================

FMotionSemanticConfig::FMotionSemanticConfig()
{
    bEnableMultiLabel = true;
    SecondaryIntentThreshold = 0.3f;
    bParseSpeedModifiers = true;
    bResolveRelativeDirections = true;
    MaxAmbiguityLevel = 0.5f;
    bValidateTransitions = true;
    bCacheResults = true;
    MaxCacheSizeMB = 32;
    bUseThreadedAnalysis = true;
}

// ============================================================================
// MOTION VOCABULARY IMPLEMENTATION
// ============================================================================

void FMotionVocabulary::LoadDefaults()
{
    // Intent keywords
    IntentKeywords.Empty();
    IntentKeywords.Add(TEXT("idle"), EMotionIntent::Idle);
    IntentKeywords.Add(TEXT("stand"), EMotionIntent::Idle);
    IntentKeywords.Add(TEXT("wait"), EMotionIntent::Idle);
    IntentKeywords.Add(TEXT("rest"), EMotionIntent::Idle);
    IntentKeywords.Add(TEXT("walk"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("run"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("sprint"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("jog"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("move"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("strafe"), EMotionIntent::Locomotion);
    IntentKeywords.Add(TEXT("attack"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("hit"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("strike"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("punch"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("kick"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("defend"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("block"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("parry"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("dodge"), EMotionIntent::Combat);
    IntentKeywords.Add(TEXT("interact"), EMotionIntent::Interaction);
    IntentKeywords.Add(TEXT("use"), EMotionIntent::Interaction);
    IntentKeywords.Add(TEXT("pickup"), EMotionIntent::Interaction);
    IntentKeywords.Add(TEXT("grab"), EMotionIntent::Interaction);
    IntentKeywords.Add(TEXT("push"), EMotionIntent::Interaction);
    IntentKeywords.Add(TEXT("pull"), EMotionIntent::Interaction);
    IntentKeywords.Add(TEXT("jump"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("land"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("fall"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("crouch"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("prone"), EMotionIntent::Transition);
    IntentKeywords.Add(TEXT("wave"), EMotionIntent::Gesture);
    IntentKeywords.Add(TEXT("point"), EMotionIntent::Gesture);
    IntentKeywords.Add(TEXT("nod"), EMotionIntent::Gesture);
    IntentKeywords.Add(TEXT("shake"), EMotionIntent::Gesture);
    
    // Speed descriptors (mapped to 0.0-1.0)
    SpeedDescriptors.Empty();
    SpeedDescriptors.Add(TEXT("idle"), 0.0f);
    SpeedDescriptors.Add(TEXT("still"), 0.0f);
    SpeedDescriptors.Add(TEXT("stationary"), 0.0f);
    SpeedDescriptors.Add(TEXT("slow"), 0.2f);
    SpeedDescriptors.Add(TEXT("careful"), 0.2f);
    SpeedDescriptors.Add(TEXT("cautious"), 0.2f);
    SpeedDescriptors.Add(TEXT("walk"), 0.4f);
    SpeedDescriptors.Add(TEXT("normal"), 0.4f);
    SpeedDescriptors.Add(TEXT("casual"), 0.4f);
    SpeedDescriptors.Add(TEXT("jog"), 0.6f);
    SpeedDescriptors.Add(TEXT("quick"), 0.6f);
    SpeedDescriptors.Add(TEXT("brisk"), 0.6f);
    SpeedDescriptors.Add(TEXT("run"), 0.8f);
    SpeedDescriptors.Add(TEXT("fast"), 0.8f);
    SpeedDescriptors.Add(TEXT("rapid"), 0.8f);
    SpeedDescriptors.Add(TEXT("sprint"), 1.0f);
    SpeedDescriptors.Add(TEXT("dash"), 1.0f);
    SpeedDescriptors.Add(TEXT("rush"), 1.0f);
    SpeedDescriptors.Add(TEXT("max"), 1.0f);
    
    // Speed modifiers (multipliers)
    SpeedModifiers.Empty();
    SpeedModifiers.Add(TEXT("very"), 1.3f);
    SpeedModifiers.Add(TEXT("extremely"), 1.5f);
    SpeedModifiers.Add(TEXT("super"), 1.4f);
    SpeedModifiers.Add(TEXT("slightly"), 0.8f);
    SpeedModifiers.Add(TEXT("somewhat"), 0.9f);
    SpeedModifiers.Add(TEXT("barely"), 0.6f);
    SpeedModifiers.Add(TEXT("almost"), 0.95f);
    
    // Direction terms
    DirectionTerms.Empty();
    DirectionTerms.Add(TEXT("forward"), EMotionDirection::Forward);
    DirectionTerms.Add(TEXT("ahead"), EMotionDirection::Forward);
    DirectionTerms.Add(TEXT("front"), EMotionDirection::Forward);
    DirectionTerms.Add(TEXT("backward"), EMotionDirection::Backward);
    DirectionTerms.Add(TEXT("back"), EMotionDirection::Backward);
    DirectionTerms.Add(TEXT("reverse"), EMotionDirection::Backward);
    DirectionTerms.Add(TEXT("behind"), EMotionDirection::Backward);
    DirectionTerms.Add(TEXT("left"), EMotionDirection::Left);
    DirectionTerms.Add(TEXT("leftward"), EMotionDirection::Left);
    DirectionTerms.Add(TEXT("port"), EMotionDirection::Left);
    DirectionTerms.Add(TEXT("right"), EMotionDirection::Right);
    DirectionTerms.Add(TEXT("rightward"), EMotionDirection::Right);
    DirectionTerms.Add(TEXT("starboard"), EMotionDirection::Right);
    DirectionTerms.Add(TEXT("up"), EMotionDirection::Up);
    DirectionTerms.Add(TEXT("upward"), EMotionDirection::Up);
    DirectionTerms.Add(TEXT("above"), EMotionDirection::Up);
    DirectionTerms.Add(TEXT("down"), EMotionDirection::Down);
    DirectionTerms.Add(TEXT("downward"), EMotionDirection::Down);
    DirectionTerms.Add(TEXT("below"), EMotionDirection::Down);
    DirectionTerms.Add(TEXT("diagonal"), EMotionDirection::ForwardLeft); // Default diagonal
    DirectionTerms.Add(TEXT("sideways"), EMotionDirection::Left); // Default sideways
    
    // State keywords
    StateKeywords.Empty();
    StateKeywords.Add(TEXT("idle"), EMotionState::Idle);
    StateKeywords.Add(TEXT("standing"), EMotionState::Idle);
    StateKeywords.Add(TEXT("moving"), EMotionState::Moving);
    StateKeywords.Add(TEXT("walking"), EMotionState::Moving);
    StateKeywords.Add(TEXT("running"), EMotionState::Moving);
    StateKeywords.Add(TEXT("jumping"), EMotionState::Jumping);
    StateKeywords.Add(TEXT("airborne"), EMotionState::Jumping);
    StateKeywords.Add(TEXT("falling"), EMotionState::Falling);
    StateKeywords.Add(TEXT("landing"), EMotionState::Landing);
    StateKeywords.Add(TEXT("crouching"), EMotionState::Crouching);
    StateKeywords.Add(TEXT("attacking"), EMotionState::Attacking);
    StateKeywords.Add(TEXT("defending"), EMotionState::Defending);
    StateKeywords.Add(TEXT("interacting"), EMotionState::Interacting);
    
    // Synonym groups
    // Initialize synonym groups with the new structure
    SynonymGroups.Add(TEXT("walk"), FMMSynonymGroup({TEXT("stride"), TEXT("step"), TEXT("pace"), TEXT("amble")}));
    SynonymGroups.Add(TEXT("run"), FMMSynonymGroup({TEXT("jog"), TEXT("sprint"), TEXT("dash"), TEXT("rush")}));
    SynonymGroups.Add(TEXT("jump"), FMMSynonymGroup({TEXT("leap"), TEXT("hop"), TEXT("bound"), TEXT("vault")}));
    SynonymGroups.Add(TEXT("turn"), FMMSynonymGroup({TEXT("rotate"), TEXT("pivot"), TEXT("spin"), TEXT("swivel")}));
    SynonymGroups.Add(TEXT("attack"), FMMSynonymGroup({TEXT("strike"), TEXT("hit"), TEXT("assault"), TEXT("offensive")}));
    SynonymGroups.Add(TEXT("defend"), FMMSynonymGroup({TEXT("block"), TEXT("parry"), TEXT("guard"), TEXT("protect")}));
}

bool FMotionVocabulary::HasIntentKeyword(const FString& Keyword) const
{
    return IntentKeywords.Contains(Keyword.ToLower());
}

bool FMotionVocabulary::HasSpeedDescriptor(const FString& Descriptor) const
{
    return SpeedDescriptors.Contains(Descriptor.ToLower());
}

bool FMotionVocabulary::HasDirectionTerm(const FString& Term) const
{
    return DirectionTerms.Contains(Term.ToLower());
}

TArray<FString> FMotionVocabulary::GetSynonyms(const FString& Term) const
{
    FString Lower = Term.ToLower();
    if (const FMMSynonymGroup* Group = SynonymGroups.Find(Lower))
    {
        return Group->Synonyms;
    }
    
    // Check if the term is in any synonym group
    for (const auto& GroupPair : SynonymGroups)
    {
        if (GroupPair.Value.Contains(Lower))
        {
            TArray<FString> Result = GroupPair.Value.Synonyms;
            Result.Add(GroupPair.Key);
            Result.Remove(Lower);
            return Result;
        }
    }
    
    return TArray<FString>();
}

// ============================================================================
// MAIN ANALYZER CLASS IMPLEMENTATION
// ============================================================================

void UMMMotionSemanticAnalyzer::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Get references to Layer 0 and Layer 1
    FuzzyMatcher = GEngine->GetEngineSubsystem<UFuzzyMatcher>();
    FuzzyProcessor = GEngine->GetEngineSubsystem<UMMFuzzyProcessor>();
    
    if (!FuzzyMatcher)
    {
        UE_LOG(LogTemp, Error, TEXT("MMMotionSemanticAnalyzer: Failed to get UREFuzzy subsystem"));
    }
    
    if (!FuzzyProcessor)
    {
        UE_LOG(LogTemp, Error, TEXT("MMMotionSemanticAnalyzer: Failed to get UMMFuzzyProcessor subsystem"));
    }
    
    // Initialize with default config
    CurrentConfig = FMotionSemanticConfig();
    
    // Load default vocabulary
    LoadDefaultVocabulary();
    
    // Initialize thread pool if configured
    if (CurrentConfig.bUseThreadedAnalysis)
    {
        ThreadPool = FQueuedThreadPool::Allocate();
        ThreadPool->Create(4, 128 * 1024); // 4 threads, 128KB stack
    }
    
    UE_LOG(LogTemp, Log, TEXT("MMMotionSemanticAnalyzer initialized"));
}

void UMMMotionSemanticAnalyzer::Deinitialize()
{
    ClearCache();
    
    if (ThreadPool)
    {
        ThreadPool->Destroy();
        delete ThreadPool;
        ThreadPool = nullptr;
    }
    
    Super::Deinitialize();
}

void UMMMotionSemanticAnalyzer::InitializeAnalyzer(const FMotionSemanticConfig& Config)
{
    CurrentConfig = Config;
    
    // Resize cache if needed
    const int32 MaxCacheEntries = (Config.MaxCacheSizeMB * 1024 * 1024) / sizeof(FMotionSemanticResult);
    
    FScopeLock Lock(&CacheMutex);
    #pragma warning(suppress: 4189) // local variable is initialized but not referenced

    if (ResultCache.Num() > MaxCacheEntries)
    {
        TrimCache();
    }
}

void UMMMotionSemanticAnalyzer::SetMotionVocabulary(const FMotionVocabulary& Vocabulary)
{
    MotionVocabulary = Vocabulary;
    ClearCache(); // Clear cache when vocabulary changes
}

void UMMMotionSemanticAnalyzer::LoadDefaultVocabulary()
{
    MotionVocabulary.LoadDefaults();
}

FMotionSemanticResult UMMMotionSemanticAnalyzer::AnalyzeMotionText(const FString& Text)
{
    double StartTime = FPlatformTime::Seconds();
    
    if (Text.IsEmpty())
    {
        return FMotionSemanticResult();
    }
    
    // Check cache
    uint32 CacheKey = GetCacheKey(Text);
    
    if (CurrentConfig.bCacheResults)
    {
        FScopeLock Lock(&CacheMutex);
        #pragma warning(suppress: 4189) // local variable is initialized but not referenced

        if (const FMotionSemanticResult* Cached = ResultCache.Find(CacheKey))
        {
            CacheHits.Increment();
            FMotionSemanticResult CachedResult = *Cached;
            CachedResult.bWasCached = true;
            return CachedResult;
        }
    }
    CacheMisses.Increment();
    
    // Tokenize using Layer 1
    FMMTokenContext Context;
    Context.Domain = TEXT("motion");
    FMMTokenStream TokenStream = FuzzyProcessor->Tokenize(Text, Context);
    
    // Analyze token stream
    FMotionSemanticResult Result = AnalyzeTokenStream(TokenStream);
    Result.OriginalText = Text;
    Result.AnalysisTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    
    // Cache result
    if (CurrentConfig.bCacheResults)
    {
        FScopeLock Lock(&CacheMutex);
        #pragma warning(suppress: 4189) // local variable is initialized but not referenced

        ResultCache.Add(CacheKey, Result);
        TrimCache();
    }
    
    TotalAnalyses.Increment();
    
    return Result;
}

FMotionSemanticResult UMMMotionSemanticAnalyzer::AnalyzeTokenStream(const FMMTokenStream& TokenStream)
{
    FMotionSemanticResult Result;
    Result.TokenStream = TokenStream;
    
    // Parallel analysis if configured
    if (CurrentConfig.bUseThreadedAnalysis && ThreadPool)
    {
        TArray<TFuture<void>> Futures;
        
        Futures.Add(Async(EAsyncExecution::ThreadPool, [this, &Result, &TokenStream]()
        {
            Result.Intent = ClassifyIntent(TokenStream);
        }));
        
        Futures.Add(Async(EAsyncExecution::ThreadPool, [this, &Result, &TokenStream]()
        {
            Result.Speed = AnalyzeSpeed(TokenStream);
        }));
        
        Futures.Add(Async(EAsyncExecution::ThreadPool, [this, &Result, &TokenStream]()
        {
            Result.Direction = AnalyzeDirection(TokenStream);
        }));
        
        Futures.Add(Async(EAsyncExecution::ThreadPool, [this, &Result, &TokenStream]()
        {
            Result.Transition = DetectTransition(TokenStream);
        }));
        
        // Wait for all analyses to complete
        for (auto& Future : Futures)
        {
            Future.Wait();
        }
    }
    else
    {
        // Sequential analysis
        Result.Intent = ClassifyIntent(TokenStream);
        Result.Speed = AnalyzeSpeed(TokenStream);
        Result.Direction = AnalyzeDirection(TokenStream);
        Result.Transition = DetectTransition(TokenStream);
    }
    
    // Calculate overall confidence
    float TotalConfidence = 0.0f;
    int32 ConfidenceCount = 0;
    
    if (Result.Intent.GetPrimaryConfidence() > 0.0f)
    {
        TotalConfidence += Result.Intent.GetPrimaryConfidence();
        ConfidenceCount++;
    }
    
    if (Result.Speed.Confidence > 0.0f)
    {
        TotalConfidence += Result.Speed.Confidence;
        ConfidenceCount++;
    }
    
    if (Result.Direction.Confidence > 0.0f)
    {
        TotalConfidence += Result.Direction.Confidence;
        ConfidenceCount++;
    }
    
    if (Result.Transition.Confidence > 0.0f)
    {
        TotalConfidence += Result.Transition.Confidence;
        ConfidenceCount++;
    }
    
    Result.OverallConfidence = ConfidenceCount > 0 ? TotalConfidence / ConfidenceCount : 0.0f;
    
    return Result;
}

FMotionSemanticResult UMMMotionSemanticAnalyzer::AnalyzeAnimationName(const FString& AnimationName)
{
    // Use Layer 1's specialized animation tokenizer
    FMMTokenStream TokenStream = FuzzyProcessor->TokenizeAnimationName(AnimationName);
    
    FMotionSemanticResult Result = AnalyzeTokenStream(TokenStream);
    Result.OriginalText = AnimationName;
    Result.Metadata.Add(TEXT("Type"), TEXT("AnimationAsset"));
    
    return Result;
}

FMotionIntentResult UMMMotionSemanticAnalyzer::ClassifyMotionIntent(const FString& Text)
{
    FMMTokenStream TokenStream = FuzzyProcessor->TokenizeCommand(Text);
    return ClassifyIntent(TokenStream);
}

FMotionSpeedResult UMMMotionSemanticAnalyzer::AnalyzeMotionSpeed(const FString& Text)
{
    FMMTokenStream TokenStream = FuzzyProcessor->TokenizeCommand(Text);
    return AnalyzeSpeed(TokenStream);
}

FMotionDirectionResult UMMMotionSemanticAnalyzer::AnalyzeMotionDirection(const FString& Text)
{
    FMMTokenStream TokenStream = FuzzyProcessor->TokenizeCommand(Text);
    return AnalyzeDirection(TokenStream);
}

FMotionTransition UMMMotionSemanticAnalyzer::DetectMotionTransition(const FString& Text)
{
    FMMTokenStream TokenStream = FuzzyProcessor->TokenizeCommand(Text);
    return DetectTransition(TokenStream);
}

float UMMMotionSemanticAnalyzer::CompareMotionSemantics(const FString& MotionA, const FString& MotionB)
{
    FMotionSemanticResult ResultA = AnalyzeMotionText(MotionA);
    FMotionSemanticResult ResultB = AnalyzeMotionText(MotionB);
    
    return CompareSemanticResults(ResultA, ResultB);
}

float UMMMotionSemanticAnalyzer::CompareSemanticResults(const FMotionSemanticResult& ResultA, const FMotionSemanticResult& ResultB)
{
    float Similarity = 0.0f;
    float WeightSum = 0.0f;
    
    // Compare intents (weight: 0.4)
    if (ResultA.Intent.PrimaryIntent == ResultB.Intent.PrimaryIntent)
    {
        Similarity += 0.4f;
    }
    else
    {
        // Check if they share any intents
        float IntentSimilarity = 0.0f;
        if (ResultB.Intent.HasIntent(ResultA.Intent.PrimaryIntent))
            IntentSimilarity = 0.5f;
        if (ResultA.Intent.HasIntent(ResultB.Intent.PrimaryIntent))
            IntentSimilarity = FMath::Max(IntentSimilarity, 0.5f);
        
        Similarity += IntentSimilarity * 0.4f;
    }
    WeightSum += 0.4f;
    
    // Compare speeds (weight: 0.3)
    float SpeedDiff = FMath::Abs(ResultA.Speed.NormalizedIntensity - ResultB.Speed.NormalizedIntensity);
    float SpeedSimilarity = 1.0f - SpeedDiff;
    Similarity += SpeedSimilarity * 0.3f;
    WeightSum += 0.3f;
    
    // Compare directions (weight: 0.2)
    if (ResultA.Direction.Direction != EMotionDirection::None && ResultB.Direction.Direction != EMotionDirection::None)
    {
        if (ResultA.Direction.Direction == ResultB.Direction.Direction)
        {
            Similarity += 0.2f;
        }
        else
        {
            // Calculate angular difference
            FVector DirA = ResultA.Direction.GetDirectionVector();
            FVector DirB = ResultB.Direction.GetDirectionVector();
            float DotProduct = FVector::DotProduct(DirA, DirB);
            float DirectionSimilarity = (DotProduct + 1.0f) / 2.0f; // Normalize to 0-1
            Similarity += DirectionSimilarity * 0.2f;
        }
        WeightSum += 0.2f;
    }
    
    // Compare states if transitions present (weight: 0.1)
    if (ResultA.Transition.IsValid() && ResultB.Transition.IsValid())
    {
        float StateSimilarity = 0.0f;
        if (ResultA.Transition.FromState == ResultB.Transition.FromState)
            StateSimilarity += 0.5f;
        if (ResultA.Transition.ToState == ResultB.Transition.ToState)
            StateSimilarity += 0.5f;
        
        Similarity += StateSimilarity * 0.1f;
        WeightSum += 0.1f;
    }
    
    // Normalize by actual weight sum
    return WeightSum > 0.0f ? Similarity / WeightSum : 0.0f;
}

bool UMMMotionSemanticAnalyzer::AreMotionsSemanticallyEquivalent(const FString& MotionA, const FString& MotionB, float Threshold)
{
    float Similarity = CompareMotionSemantics(MotionA, MotionB);
    return Similarity >= Threshold;
}

TArray<FString> UMMMotionSemanticAnalyzer::ExpandMotionQuery(const FString& Query, int32 MaxExpansions)
{
    TArray<FString> Expansions;
    Expansions.Add(Query);
    
    // Analyze the query
    FMotionSemanticResult Analysis = AnalyzeMotionText(Query);
    
    // Generate variants using Layer 1
    TArray<FString> Variants = FuzzyProcessor->GenerateVariants(Query, MaxExpansions / 2);
    Expansions.Append(Variants);
    
    // Add synonyms for key terms
    for (const FMMFuzzyToken& Token : Analysis.TokenStream.Tokens)
    {
        TArray<FString> Synonyms = MotionVocabulary.GetSynonyms(Token.Text);
        for (const FString& Synonym : Synonyms)
        {
            FString ExpandedQuery = Query;
            ExpandedQuery = ExpandedQuery.Replace(*Token.Text, *Synonym);
            Expansions.AddUnique(ExpandedQuery);
            
            if (Expansions.Num() >= MaxExpansions)
                break;
        }
        
        if (Expansions.Num() >= MaxExpansions)
            break;
    }
    
    // Add speed variations if applicable
    if (Analysis.Speed.SpeedLevel != EMotionSpeed::Idle)
    {
        TArray<EMotionSpeed> NearbyLevels = {
            EMotionSpeed::Slow, EMotionSpeed::Walk, EMotionSpeed::Jog, 
            EMotionSpeed::Run, EMotionSpeed::Sprint
        };
        
        for (EMotionSpeed Level : NearbyLevels)
        {
            if (Level != Analysis.Speed.SpeedLevel)
            {
                FString SpeedName = UEnum::GetValueAsString(Level);
                SpeedName = SpeedName.Replace(TEXT("EMotionSpeed::"), TEXT(""));
                
                FString ExpandedQuery = Query;
                for (const auto& SpeedDesc : MotionVocabulary.SpeedDescriptors)
                {
                    if (FMath::IsNearlyEqual(SpeedDesc.Value, 
                        FMotionSpeedResult::SpeedLevelToIntensity(Analysis.Speed.SpeedLevel), 0.1f))
                    {
                        ExpandedQuery = ExpandedQuery.Replace(*SpeedDesc.Key, *SpeedName.ToLower());
                        break;
                    }
                }
                
                if (ExpandedQuery != Query)
                {
                    Expansions.AddUnique(ExpandedQuery);
                }
            }
            
            if (Expansions.Num() >= MaxExpansions)
                break;
        }
    }
    
    // Limit to MaxExpansions
    if (Expansions.Num() > MaxExpansions)
    {
        Expansions.SetNum(MaxExpansions);
    }
    
    return Expansions;
}

TArray<FString> UMMMotionSemanticAnalyzer::GenerateMotionVariants(const FString& Motion)
{
    return ExpandMotionQuery(Motion, 20);
}

TArray<FString> UMMMotionSemanticAnalyzer::GetSimilarMotions(const FString& Motion, float MinSimilarity)
{
    TArray<FString> SimilarMotions;
    
    // This would typically query a database of known motions
    // For now, generate variants and filter by similarity
    TArray<FString> Variants = GenerateMotionVariants(Motion);
    
    for (const FString& Variant : Variants)
    {
        if (Variant != Motion)
        {
            float Similarity = CompareMotionSemantics(Motion, Variant);
            if (Similarity >= MinSimilarity)
            {
                SimilarMotions.Add(Variant);
            }
        }
    }
    
    return SimilarMotions;
}

void UMMMotionSemanticAnalyzer::ClearCache()
{
    FScopeLock Lock(&CacheMutex);
    #pragma warning(suppress: 4189) // local variable is initialized but not referenced

    ResultCache.Empty();
    
    CacheHits.Reset();
    CacheMisses.Reset();
    TotalAnalyses.Reset();
    
    UE_LOG(LogTemp, Log, TEXT("MMMotionSemanticAnalyzer cache cleared"));
}

void UMMMotionSemanticAnalyzer::GetCacheStatistics(int32& OutHits, int32& OutMisses, int32& OutSize) const
{
    OutHits = CacheHits.GetValue();
    OutMisses = CacheMisses.GetValue();
    
    FScopeLock Lock(&CacheMutex);
    #pragma warning(suppress: 4189) // local variable is initialized but not referenced

    OutSize = ResultCache.Num();
}

float UMMMotionSemanticAnalyzer::GetCacheHitRate() const
{
    const int32 Hits = CacheHits.GetValue();
    const int32 Misses = CacheMisses.GetValue();
    const int32 Total = Hits + Misses;
    
    return Total > 0 ? static_cast<float>(Hits) / Total : 0.0f;
}

bool UMMMotionSemanticAnalyzer::ValidateStateTransition(EMotionState From, EMotionState To)
{
    return IsValidTransition(From, To);
}

TArray<EMotionState> UMMMotionSemanticAnalyzer::GetValidTransitionsFrom(EMotionState State)
{
    TArray<EMotionState> ValidTransitions;
    
    switch (State)
    {
        case EMotionState::Idle:
            ValidTransitions = {EMotionState::Moving, EMotionState::Jumping, 
                              EMotionState::Crouching, EMotionState::Attacking, 
                              EMotionState::Interacting};
            break;
            
        case EMotionState::Moving:
            ValidTransitions = {EMotionState::Idle, EMotionState::Jumping, 
                              EMotionState::Crouching, EMotionState::Attacking,
                              EMotionState::Falling};
            break;
            
        case EMotionState::Jumping:
            ValidTransitions = {EMotionState::Falling, EMotionState::Landing};
            break;
            
        case EMotionState::Falling:
            ValidTransitions = {EMotionState::Landing};
            break;
            
        case EMotionState::Landing:
            ValidTransitions = {EMotionState::Idle, EMotionState::Moving, 
                              EMotionState::Crouching};
            break;
            
        case EMotionState::Crouching:
            ValidTransitions = {EMotionState::Idle, EMotionState::Moving};
            break;
            
        case EMotionState::Attacking:
            ValidTransitions = {EMotionState::Idle, EMotionState::Moving, 
                              EMotionState::Defending};
            break;
            
        case EMotionState::Defending:
            ValidTransitions = {EMotionState::Idle, EMotionState::Moving, 
                              EMotionState::Attacking};
            break;
            
        case EMotionState::Interacting:
            ValidTransitions = {EMotionState::Idle, EMotionState::Moving};
            break;
            
        default:
            break;
    }
    
    return ValidTransitions;
}

float UMMMotionSemanticAnalyzer::SpeedToIntensity(EMotionSpeed Speed)
{
    return FMotionSpeedResult::SpeedLevelToIntensity(Speed);
}

EMotionSpeed UMMMotionSemanticAnalyzer::IntensityToSpeed(float Intensity)
{
    return FMotionSpeedResult::IntensityToSpeedLevel(Intensity);
}

FVector UMMMotionSemanticAnalyzer::DirectionToVector(EMotionDirection Direction)
{
    FMotionDirectionResult TempResult;
    TempResult.Direction = Direction;
    return TempResult.GetDirectionVector();
}

// ============================================================================
// PRIVATE ANALYSIS METHODS
// ============================================================================

FMotionIntentResult UMMMotionSemanticAnalyzer::ClassifyIntent(const FMMTokenStream& Tokens) const
{
    FMotionIntentResult Result;
    TMap<EMotionIntent, float> IntentScores;
    
    // Analyze each token
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (Token.Type == EMMTokenType::Separator || Token.Type == EMMTokenType::Punctuation)
            continue;
        
        FString Canonical = Token.Canonical;
        
        // Check direct intent keywords
        if (const EMotionIntent* Intent = MotionVocabulary.IntentKeywords.Find(Canonical))
        {
            float Score = IntentScores.Contains(*Intent) ? IntentScores[*Intent] : 0.0f;
            IntentScores.Add(*Intent, Score + 1.0f);
            Result.TriggerKeywords.AddUnique(Token.Text);
        }
        
        // Check variants and fuzzy matches
        for (const FString& Variant : Token.Variants)
        {
            if (const EMotionIntent* Intent = MotionVocabulary.IntentKeywords.Find(Variant.ToLower()))
            {
                float Score = IntentScores.Contains(*Intent) ? IntentScores[*Intent] : 0.0f;
                float VariantWeight = Token.GetVariantWeight(Variant);
                IntentScores.Add(*Intent, Score + VariantWeight * 0.5f);
                Result.TriggerKeywords.AddUnique(Variant);
            }
        }
        
        // Fuzzy match against known intent keywords using Layer 0
        if (FuzzyMatcher)
        {
            for (const auto& IntentPair : MotionVocabulary.IntentKeywords)
            {
                FStringMatch Match = FuzzyMatcher->CompareStrings(Canonical, IntentPair.Key, false);
                float Similarity = Match.GetBestSimilarity();
                
                if (Similarity > 0.7f)
                {
                    float Score = IntentScores.Contains(IntentPair.Value) ? 
                                IntentScores[IntentPair.Value] : 0.0f;
                    IntentScores.Add(IntentPair.Value, Score + Similarity * 0.3f);
                }
            }
        }
    }
    
    // Normalize scores and determine primary intent
    float MaxScore = 0.0f;
    EMotionIntent PrimaryIntent = EMotionIntent::None;
    
    for (const auto& Score : IntentScores)
    {
        if (Score.Value > MaxScore)
        {
            MaxScore = Score.Value;
            PrimaryIntent = Score.Key;
        }
    }
    
    // Calculate confidence scores
    float TotalScore = 0.0f;
    for (const auto& Score : IntentScores)
    {
        TotalScore += Score.Value;
    }
    
    if (TotalScore > 0.0f)
    {
        Result.PrimaryIntent = PrimaryIntent;
        
        for (const auto& Score : IntentScores)
        {
            float Confidence = Score.Value / TotalScore;
            Result.IntentConfidences.Add(Score.Key, Confidence);
            
            // Add as secondary intent if above threshold and not primary
            if (Score.Key != PrimaryIntent && 
                Confidence >= CurrentConfig.SecondaryIntentThreshold && 
                CurrentConfig.bEnableMultiLabel)
            {
                Result.SecondaryIntents.Add(Score.Key);
            }
        }
    }
    
    return Result;
}

FMotionSpeedResult UMMMotionSemanticAnalyzer::AnalyzeSpeed(const FMMTokenStream& Tokens) const
{
    FMotionSpeedResult Result;
    
    float TotalIntensity = 0.0f;
    float TotalWeight = 0.0f;
    float ModifierMultiplier = 1.0f;
    
    // Analyze each token
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (Token.Type == EMMTokenType::Separator || Token.Type == EMMTokenType::Punctuation)
            continue;
        
        FString Canonical = Token.Canonical;
        
        // Check speed descriptors
        if (const float* Intensity = MotionVocabulary.SpeedDescriptors.Find(Canonical))
        {
            TotalIntensity += *Intensity;
            TotalWeight += 1.0f;
            Result.Descriptors.AddUnique(Token.Text);
        }
        
        // Check speed modifiers
        if (CurrentConfig.bParseSpeedModifiers)
        {
            if (const float* Modifier = MotionVocabulary.SpeedModifiers.Find(Canonical))
            {
                ModifierMultiplier *= *Modifier;
                Result.SpeedModifiers.AddUnique(Token.Text);
            }
        }
        
        // Fuzzy match against speed descriptors using Layer 0
        if (FuzzyMatcher)
        {
            for (const auto& SpeedPair : MotionVocabulary.SpeedDescriptors)
            {
                FStringMatch Match = FuzzyMatcher->CompareStrings(Canonical, SpeedPair.Key, false);
                float Similarity = Match.GetBestSimilarity();
                
                if (Similarity > 0.7f && Similarity < 1.0f) // Avoid double-counting exact matches
                {
                    TotalIntensity += SpeedPair.Value * Similarity;
                    TotalWeight += Similarity;
                }
            }
        }
    }
    
    // Calculate final intensity
    if (TotalWeight > 0.0f)
    {
        Result.NormalizedIntensity = (TotalIntensity / TotalWeight) * ModifierMultiplier;
        Result.NormalizedIntensity = FMath::Clamp(Result.NormalizedIntensity, 0.0f, 1.0f);
        Result.SpeedLevel = FMotionSpeedResult::IntensityToSpeedLevel(Result.NormalizedIntensity);
        Result.Confidence = FMath::Min(TotalWeight / Tokens.Tokens.Num(), 1.0f);
    }
    else
    {
        // Default to idle if no speed information found
        Result.SpeedLevel = EMotionSpeed::Idle;
        Result.NormalizedIntensity = 0.0f;
        Result.Confidence = 0.0f;
    }
    
    // Handle custom speed if needed
    if (Result.SpeedLevel == EMotionSpeed::Custom)
    {
        Result.CustomSpeed = Result.NormalizedIntensity;
    }
    
    return Result;
}

FMotionDirectionResult UMMMotionSemanticAnalyzer::AnalyzeDirection(const FMMTokenStream& Tokens) const
{
    FMotionDirectionResult Result;
    TMap<EMotionDirection, float> DirectionScores;
    
    // Analyze each token
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (Token.Type == EMMTokenType::Separator || Token.Type == EMMTokenType::Punctuation)
            continue;
        
        FString Canonical = Token.Canonical;
        
        // Check direction terms
        if (const EMotionDirection* Direction = MotionVocabulary.DirectionTerms.Find(Canonical))
        {
            float Score = DirectionScores.Contains(*Direction) ? DirectionScores[*Direction] : 0.0f;
            DirectionScores.Add(*Direction, Score + 1.0f);
            Result.Descriptors.AddUnique(Token.Text);
        }
        
        // Check for compound directions
        if (Canonical.Contains(TEXT("forward")) || Canonical.Contains(TEXT("ahead")))
        {
            if (Canonical.Contains(TEXT("left")))
            {
                DirectionScores.Add(EMotionDirection::ForwardLeft, 
                    DirectionScores.Contains(EMotionDirection::ForwardLeft) ? 
                    DirectionScores[EMotionDirection::ForwardLeft] + 1.0f : 1.0f);
            }
            else if (Canonical.Contains(TEXT("right")))
            {
                DirectionScores.Add(EMotionDirection::ForwardRight,
                    DirectionScores.Contains(EMotionDirection::ForwardRight) ? 
                    DirectionScores[EMotionDirection::ForwardRight] + 1.0f : 1.0f);
            }
        }
        else if (Canonical.Contains(TEXT("back")) || Canonical.Contains(TEXT("behind")))
        {
            if (Canonical.Contains(TEXT("left")))
            {
                DirectionScores.Add(EMotionDirection::BackwardLeft,
                    DirectionScores.Contains(EMotionDirection::BackwardLeft) ? 
                    DirectionScores[EMotionDirection::BackwardLeft] + 1.0f : 1.0f);
            }
            else if (Canonical.Contains(TEXT("right")))
            {
                DirectionScores.Add(EMotionDirection::BackwardRight,
                    DirectionScores.Contains(EMotionDirection::BackwardRight) ? 
                    DirectionScores[EMotionDirection::BackwardRight] + 1.0f : 1.0f);
            }
        }
        
        // Check for relative direction indicators
        if (CurrentConfig.bResolveRelativeDirections)
        {
            if (Canonical.Contains(TEXT("towards")) || Canonical.Contains(TEXT("at")))
            {
                Result.DirectionType = EDirectionType::Target;
            }
            else if (Canonical.Contains(TEXT("away")) || Canonical.Contains(TEXT("from")))
            {
                Result.DirectionType = EDirectionType::Target;
                // Invert direction if "away from"
            }
            else if (Canonical.Contains(TEXT("camera")))
            {
                Result.DirectionType = EDirectionType::Camera;
            }
        }
    }
    
    // Determine primary direction
    float MaxScore = 0.0f;
    EMotionDirection PrimaryDirection = EMotionDirection::None;
    
    for (const auto& Score : DirectionScores)
    {
        if (Score.Value > MaxScore)
        {
            MaxScore = Score.Value;
            PrimaryDirection = Score.Key;
        }
    }
    
    Result.Direction = PrimaryDirection;
    
    // Calculate ambiguity and alternatives
    if (DirectionScores.Num() > 1)
    {
        float TotalScore = 0.0f;
        for (const auto& Score : DirectionScores)
        {
            TotalScore += Score.Value;
        }
        
        for (const auto& Score : DirectionScores)
        {
            float NormalizedScore = Score.Value / TotalScore;
            if (Score.Key != PrimaryDirection)
            {
                Result.AlternativeDirections.Add(Score.Key, NormalizedScore);
            }
        }
        
        // Calculate ambiguity (entropy-based)
        float Entropy = 0.0f;
        for (const auto& Score : DirectionScores)
        {
            float P = Score.Value / TotalScore;
            if (P > 0.0f)
            {
                Entropy -= P * FMath::Loge(P);
            }
        }
        
        Result.AmbiguityLevel = FMath::Min(Entropy / FMath::Loge(static_cast<float>(DirectionScores.Num())), 1.0f);
    }
    
    // Calculate confidence
    if (MaxScore > 0.0f)
    {
        Result.Confidence = FMath::Min(MaxScore / Tokens.Tokens.Num(), 1.0f);
    }
    
    return Result;
}

FMotionTransition UMMMotionSemanticAnalyzer::DetectTransition(const FMMTokenStream& Tokens) const
{
    FMotionTransition Result;
    
    EMotionState DetectedStates[2] = {EMotionState::Unknown, EMotionState::Unknown};
    int32 StateIndex = 0;
    bool bFoundTransitionKeyword = false;
    
    // Look for state keywords and transition indicators
    for (int32 i = 0; i < Tokens.Tokens.Num(); i++)
    {
        const FMMFuzzyToken& Token = Tokens.Tokens[i];
        
        if (Token.Type == EMMTokenType::Separator || Token.Type == EMMTokenType::Punctuation)
            continue;
        
        FString Canonical = Token.Canonical;
        
        // Check for transition keywords
        if (Canonical == TEXT("to") || Canonical == TEXT("into") || 
            Canonical == TEXT("from") || Canonical == TEXT("then"))
        {
            bFoundTransitionKeyword = true;
            if (StateIndex == 0 && DetectedStates[0] != EMotionState::Unknown)
            {
                StateIndex = 1;
            }
            continue;
        }
        
        // Check state keywords
        if (const EMotionState* State = MotionVocabulary.StateKeywords.Find(Canonical))
        {
            if (StateIndex < 2)
            {
                DetectedStates[StateIndex] = *State;
                if (!bFoundTransitionKeyword && StateIndex == 0)
                {
                    // Keep looking for second state
                    StateIndex = 1;
                }
            }
        }
    }
    
    // Set transition states
    Result.FromState = DetectedStates[0];
    Result.ToState = DetectedStates[1];
    
    // If only one state detected, try to infer transition
    if (Result.FromState != EMotionState::Unknown && Result.ToState == EMotionState::Unknown)
    {
        // Try to infer destination state based on common patterns
        switch (Result.FromState)
        {
            case EMotionState::Idle:
                Result.ToState = EMotionState::Moving;
                break;
            case EMotionState::Moving:
                Result.ToState = EMotionState::Idle;
                break;
            case EMotionState::Jumping:
                Result.ToState = EMotionState::Falling;
                break;
            case EMotionState::Falling:
                Result.ToState = EMotionState::Landing;
                break;
            case EMotionState::Landing:
                Result.ToState = EMotionState::Idle;
                break;
            default:
                break;
        }
    }
    
    // Validate transition
    if (CurrentConfig.bValidateTransitions)
    {
        Result.bIsValid = IsValidTransition(Result.FromState, Result.ToState);
    }
    else
    {
        Result.bIsValid = (Result.FromState != EMotionState::Unknown && 
                          Result.ToState != EMotionState::Unknown);
    }
    
    // Calculate priority and duration
    if (Result.bIsValid)
    {
        Result.Priority = GetTransitionPriority(Result.FromState, Result.ToState);
        
        // Estimate duration based on transition type
        if (Result.FromState == EMotionState::Idle && Result.ToState == EMotionState::Moving)
        {
            Result.DurationHint = 0.3f;
        }
        else if (Result.FromState == EMotionState::Moving && Result.ToState == EMotionState::Idle)
        {
            Result.DurationHint = 0.5f;
        }
        else if (Result.FromState == EMotionState::Jumping || Result.ToState == EMotionState::Landing)
        {
            Result.DurationHint = 0.8f;
        }
        else
        {
            Result.DurationHint = 0.4f; // Default
        }
        
        Result.Confidence = bFoundTransitionKeyword ? 0.9f : 0.6f;
    }
    else
    {
        Result.Confidence = 0.0f;
    }
    
    return Result;
}

float UMMMotionSemanticAnalyzer::CalculateTokenRelevance(const FMMFuzzyToken& Token, const FString& Category) const
{
    // This would calculate how relevant a token is to a specific category
    // For now, return a simple binary relevance
    
    if (Category == TEXT("intent"))
    {
        return MotionVocabulary.HasIntentKeyword(Token.Canonical) ? 1.0f : 0.0f;
    }
    else if (Category == TEXT("speed"))
    {
        return MotionVocabulary.HasSpeedDescriptor(Token.Canonical) ? 1.0f : 0.0f;
    }
    else if (Category == TEXT("direction"))
    {
        return MotionVocabulary.HasDirectionTerm(Token.Canonical) ? 1.0f : 0.0f;
    }
    
    return 0.0f;
}

TArray<FString> UMMMotionSemanticAnalyzer::ExtractKeywordsForCategory(const FMMTokenStream& Tokens, const FString& Category) const
{
    TArray<FString> Keywords;
    
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (CalculateTokenRelevance(Token, Category) > 0.5f)
        {
            Keywords.AddUnique(Token.Text);
        }
    }
    
    return Keywords;
}

bool UMMMotionSemanticAnalyzer::IsValidTransition(EMotionState From, EMotionState To)
{
    TArray<EMotionState> ValidTransitions = GetValidTransitionsFrom(From);
    return ValidTransitions.Contains(To);
}

float UMMMotionSemanticAnalyzer::GetTransitionPriority(EMotionState From, EMotionState To)
{
    // Common transitions get higher priority
    if ((From == EMotionState::Idle && To == EMotionState::Moving) ||
        (From == EMotionState::Moving && To == EMotionState::Idle))
    {
        return 10.0f;
    }
    
    if ((From == EMotionState::Jumping && To == EMotionState::Falling) ||
        (From == EMotionState::Falling && To == EMotionState::Landing))
    {
        return 9.0f;
    }
    
    if (From == EMotionState::Landing && (To == EMotionState::Idle || To == EMotionState::Moving))
    {
        return 8.0f;
    }
    
    // Default priority
    return 5.0f;
}

void UMMMotionSemanticAnalyzer::TrimCache()
{
    const int32 MaxCacheEntries = (CurrentConfig.MaxCacheSizeMB * 1024 * 1024) / sizeof(FMotionSemanticResult);
    
    if (ResultCache.Num() > MaxCacheEntries)
    {
        // Simple trimming - remove half
        const int32 ToRemove = ResultCache.Num() / 2;
        int32 Removed = 0;
        
        for (auto It = ResultCache.CreateIterator(); It && Removed < ToRemove; ++It)
        {
            It.RemoveCurrent();
            Removed++;
        }
    }
}

uint32 UMMMotionSemanticAnalyzer::GetCacheKey(const FString& Text)
{
    return GetTypeHash(Text.ToLower());
}

#endif

#include "Core/MMMotionSemanticAnalyzer.h"
#include "Core/MMSemanticEngineCore.h"
#include "Core/MMTokenizer.h"
#include "Core/MMFuzzyMatcher.h"
#include "Core/MMPatternEngine.h"
#include "Data/MMPatternTypes.h"

UMMMotionSemanticAnalyzer::UMMMotionSemanticAnalyzer()
{
    // Will be initialized when needed
}

void UMMMotionSemanticAnalyzer::Initialize()
{
    // Get plugin engine
    SemanticEngine = UMMSemanticEngineCore::Get();
    if (!SemanticEngine)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Semantic Engine Core"));
        return;
    }
    
    // Get components
    Tokenizer = SemanticEngine->GetTokenizer();
    FuzzyMatcher = SemanticEngine->GetFuzzyMatcher();
    PatternEngine = SemanticEngine->GetPatternEngine();
    
    if (!Tokenizer || !FuzzyMatcher)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get required components from Semantic Engine"));
        return;
    }
    
    // Register motion-specific patterns if pattern engine available
    if (PatternEngine)
    {
        RegisterDefaultPatterns();
    }
    
    UE_LOG(LogTemp, Log, TEXT("Motion Semantic Analyzer initialized successfully"));
}

bool UMMMotionSemanticAnalyzer::IsInitialized() const
{
    return SemanticEngine && Tokenizer && FuzzyMatcher;
}

void UMMMotionSemanticAnalyzer::RegisterDefaultPatterns()
{
    if (!PatternEngine)
        return;
    
    // Register animation name pattern (e.g., "MM_Walk_Forward_01")
    FMMPatternTemplate AnimNamePattern;
    AnimNamePattern.PatternID = FName(PATTERN_ANIM_NAME);
    AnimNamePattern.PatternString = TEXT("{Prefix}_{Motion}_{Direction}_{Variant}");
    AnimNamePattern.ExpectedTokenTypes = {
        EMMTokenType::Word,      // Prefix (MM, ALS, etc.)
        EMMTokenType::Delimiter,  // _
        EMMTokenType::Word,      // Motion type
        EMMTokenType::Delimiter,  // _
        EMMTokenType::Word,      // Direction
        EMMTokenType::Delimiter,  // _
        EMMTokenType::Number     // Variant number
    };
    AnimNamePattern.bAllowPartialMatch = true;
    AnimNamePattern.MinConfidence = 0.6f;
    PatternEngine->RegisterPattern(AnimNamePattern.PatternID, AnimNamePattern);
    
    // Register motion command pattern (e.g., "walk forward slowly")
    FMMPatternTemplate CommandPattern;
    CommandPattern.PatternID = FName(PATTERN_MOTION_COMMAND);
    CommandPattern.PatternString = TEXT("{Motion} {Direction} {Speed}");
    CommandPattern.ExpectedTokenTypes = {
        EMMTokenType::Word,  // Motion verb
        EMMTokenType::Word,  // Direction
        EMMTokenType::Word   // Speed modifier
    };
    CommandPattern.bAllowPartialMatch = true;
    CommandPattern.MinConfidence = 0.5f;
    PatternEngine->RegisterPattern(CommandPattern.PatternID, CommandPattern);
}

// ========== HIGH-LEVEL ANALYSIS ==========

FMotionSemanticResult UMMMotionSemanticAnalyzer::AnalyzeText(const FString& Text, const FString& Context)
{
    if (!IsInitialized())
    {
        Initialize();
        if (!IsInitialized())
        {
            return FMotionSemanticResult();
        }
    }
    
    // Tokenize the text
    FMMTokenStream TokenStream = Tokenizer->Tokenize(Text);
    
    // Analyze the token stream
    FMotionSemanticResult Result = AnalyzeTokenStream(TokenStream);
    Result.TokenStream = TokenStream;
    
    // Add context if provided
    if (!Context.IsEmpty())
    {
        Result.AnalysisDetails = FString::Printf(TEXT("Context: %s\n%s"), 
            *Context, *Result.AnalysisDetails);
    }
    
    return Result;
}

FMotionSemanticResult UMMMotionSemanticAnalyzer::AnalyzeAnimationName(const FString& AnimationName)
{
    if (!IsInitialized())
    {
        Initialize();
        if (!IsInitialized())
        {
            return FMotionSemanticResult();
        }
    }
    
    // Use specialized animation name tokenization
    FMMTokenStream TokenStream = Tokenizer->TokenizeAnimationName(AnimationName);
    
    FMotionSemanticResult Result = AnalyzeTokenStream(TokenStream);
    Result.TokenStream = TokenStream;
    
    // Try pattern matching if available
    if (PatternEngine)
    {
        FMMPatternMatch Match = PatternEngine->MatchPattern(AnimationName, 
            FName(PATTERN_ANIM_NAME), EMMPatternMatchMode::Fuzzy);
        
        if (Match.bMatched)
        {
            Result.Confidence = FMath::Max(Result.Confidence, Match.Confidence);
            Result.AnalysisDetails += FString::Printf(
                TEXT("\nPattern Match: %s (%.2f confidence)"),
                *Match.PatternID.ToString(), Match.Confidence);
        }
    }
    
    return Result;
}

FMotionIntentResult UMMMotionSemanticAnalyzer::AnalyzeMotionCommand(const FString& Command)
{
    return ExtractMotionIntent(Command);
}

// ========== SPECIFIC ANALYSIS ==========

FMotionIntentResult UMMMotionSemanticAnalyzer::ExtractMotionIntent(const FString& Text)
{
    if (!IsInitialized())
    {
        Initialize();
        if (!IsInitialized())
        {
            return FMotionIntentResult();
        }
    }
    
    FMMTokenStream TokenStream = Tokenizer->Tokenize(Text);
    return ClassifyIntent(TokenStream);
}

FMotionSpeedResult UMMMotionSemanticAnalyzer::ExtractSpeed(const FString& Text)
{
    if (!IsInitialized())
    {
        Initialize();
        if (!IsInitialized())
        {
            return FMotionSpeedResult();
        }
    }
    
    FMMTokenStream TokenStream = Tokenizer->Tokenize(Text);
    return AnalyzeSpeed(TokenStream);
}

FMotionDirectionResult UMMMotionSemanticAnalyzer::ExtractDirection(const FString& Text)
{
    if (!IsInitialized())
    {
        Initialize();
        if (!IsInitialized())
        {
            return FMotionDirectionResult();
        }
    }
    
    FMMTokenStream TokenStream = Tokenizer->Tokenize(Text);
    return AnalyzeDirection(TokenStream);
}

FMotionTransition UMMMotionSemanticAnalyzer::DetectTransition(const FString& Text)
{
    if (!IsInitialized())
    {
        Initialize();
        if (!IsInitialized())
        {
            return FMotionTransition();
        }
    }
    
    FMMTokenStream TokenStream = Tokenizer->Tokenize(Text);
    return DetectTransition(TokenStream);
}

// ========== TOKEN ANALYSIS ==========

FMotionSemanticResult UMMMotionSemanticAnalyzer::AnalyzeTokenStream(const FMMTokenStream& TokenStream)
{
    FMotionSemanticResult Result;
    
    // Extract various components
    FMotionIntentResult Intent = ClassifyIntent(TokenStream);
    FMotionSpeedResult Speed = AnalyzeSpeed(TokenStream);
    FMotionDirectionResult Direction = AnalyzeDirection(TokenStream);
    
    // Map intent to motion type
    Result.MotionType = GetMotionTypeFromKeyword(Intent.PrimaryIntent);
    Result.Speed = Speed.EstimatedSpeed;
    Result.Direction = Direction.Direction;
    Result.Confidence = (Intent.Confidence + Speed.Confidence + Direction.Confidence) / 3.0f;
    
    // Build detected keywords
    Result.DetectedKeywords.Empty();
    for (const FMMFuzzyToken& Token : TokenStream.Tokens)
    {
        if (Token.Type == EMMTokenType::Word)
        {
            FString Lower = Token.Text.ToLower();
            if (MotionVocabulary.HasIntentKeyword(Lower) ||
                MotionVocabulary.HasSpeedDescriptor(Lower) ||
                MotionVocabulary.HasDirectionTerm(Lower))
            {
                Result.DetectedKeywords.Add(Token.Text);
            }
        }
    }
    
    // Build analysis details
    Result.AnalysisDetails = FString::Printf(
        TEXT("Intent: %s (%.2f)\nSpeed: %s - %.1f cm/s (%.2f)\nDirection: %s (%.2f)"),
        *Intent.PrimaryIntent, Intent.Confidence,
        *Speed.SpeedCategory, Speed.EstimatedSpeed, Speed.Confidence,
        *Direction.DirectionName, Direction.Confidence
    );
    
    return Result;
}

FMotionIntentResult UMMMotionSemanticAnalyzer::ClassifyIntent(const FMMTokenStream& Tokens) const
{
    FMotionIntentResult Result;
    Result.IntentScores.Empty();
    
    float TotalWeight = 0.0f;
    
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (Token.Type == EMMTokenType::Delimiter || Token.Type == EMMTokenType::Symbol)
            continue;
        
        FString Lower = Token.Text.ToLower();
        
        // Check against motion vocabulary
        if (MotionVocabulary.HasIntentKeyword(Lower))
        {
            float& Score = Result.IntentScores.FindOrAdd(Lower);
            Score += Token.Weight * Token.Confidence;
            TotalWeight += Token.Weight * Token.Confidence;
        }
        
        // Also check variants if tokenizer generated them
        for (const FString& Variant : Token.Variants)
        {
            FString VarLower = Variant.ToLower();
            if (MotionVocabulary.HasIntentKeyword(VarLower))
            {
                float& Score = Result.IntentScores.FindOrAdd(VarLower);
                Score += Token.Weight * 0.5f;  // Lower weight for variants
                TotalWeight += Token.Weight * 0.5f;
            }
        }
    }
    
    // Find primary intent
    float MaxScore = 0.0f;
    for (const auto& Pair : Result.IntentScores)
    {
        if (Pair.Value > MaxScore)
        {
            MaxScore = Pair.Value;
            Result.PrimaryIntent = Pair.Key;
        }
    }
    
    // Calculate confidence
    if (Tokens.Tokens.Num() > 0 && TotalWeight > 0)
    {
        Result.Confidence = FMath::Min(TotalWeight / Tokens.Tokens.Num(), 1.0f);
    }
    
    return Result;
}

FMotionSpeedResult UMMMotionSemanticAnalyzer::AnalyzeSpeed(const FMMTokenStream& Tokens) const
{
    FMotionSpeedResult Result;
    
    float SpeedScore = 0.0f;
    int32 SpeedTokens = 0;
    
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (Token.Type == EMMTokenType::Delimiter || Token.Type == EMMTokenType::Symbol)
            continue;
        
        FString Lower = Token.Text.ToLower();
        
        // Check speed descriptors
        if (MotionVocabulary.HasSpeedDescriptor(Lower))
        {
            SpeedTokens++;
            
            // Map descriptors to speed values
            if (Lower == TEXT("slow") || Lower == TEXT("walk"))
            {
                SpeedScore += 150.0f;  // Walking speed
                Result.SpeedCategory = TEXT("Slow");
            }
            else if (Lower == TEXT("run") || Lower == TEXT("moderate"))
            {
                SpeedScore += 400.0f;  // Running speed
                Result.SpeedCategory = TEXT("Medium");
            }
            else if (Lower == TEXT("sprint") || Lower == TEXT("fast") || Lower == TEXT("rapid"))
            {
                SpeedScore += 600.0f;  // Sprinting speed
                Result.SpeedCategory = TEXT("Fast");
            }
        }
        
        // Check for motion types that imply speed
        if (Lower == TEXT("idle") || Lower == TEXT("stand"))
        {
            SpeedScore += 0.0f;
            Result.SpeedCategory = TEXT("Stationary");
            SpeedTokens++;
        }
        else if (Lower == TEXT("crawl"))
        {
            SpeedScore += 50.0f;
            Result.SpeedCategory = TEXT("Very Slow");
            SpeedTokens++;
        }
    }
    
    // Calculate average speed
    if (SpeedTokens > 0)
    {
        Result.EstimatedSpeed = SpeedScore / SpeedTokens;
        Result.Confidence = FMath::Min(SpeedTokens / FMath::Max(1.0f, Tokens.Tokens.Num() * 0.5f), 1.0f);
    }
    else
    {
        // Default to medium speed if no speed info found
        Result.EstimatedSpeed = 250.0f;
        Result.SpeedCategory = TEXT("Unknown");
        Result.Confidence = 0.1f;
    }
    
    return Result;
}

FMotionDirectionResult UMMMotionSemanticAnalyzer::AnalyzeDirection(const FMMTokenStream& Tokens) const
{
    FMotionDirectionResult Result;
    
    FVector DirectionSum = FVector::ZeroVector;
    int32 DirectionTokens = 0;
    
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (Token.Type == EMMTokenType::Delimiter || Token.Type == EMMTokenType::Symbol)
            continue;
        
        FString Lower = Token.Text.ToLower();
        
        if (MotionVocabulary.HasDirectionTerm(Lower))
        {
            DirectionTokens++;
            
            // Map direction terms to vectors
            if (Lower == TEXT("forward") || Lower == TEXT("ahead"))
            {
                DirectionSum += FVector::ForwardVector;
                Result.DirectionName = TEXT("Forward");
            }
            else if (Lower == TEXT("backward") || Lower == TEXT("back"))
            {
                DirectionSum += FVector::BackwardVector;
                Result.DirectionName = TEXT("Backward");
            }
            else if (Lower == TEXT("left"))
            {
                DirectionSum += FVector::LeftVector;
                Result.DirectionName = TEXT("Left");
            }
            else if (Lower == TEXT("right"))
            {
                DirectionSum += FVector::RightVector;
                Result.DirectionName = TEXT("Right");
            }
            else if (Lower == TEXT("up"))
            {
                DirectionSum += FVector::UpVector;
                Result.DirectionName = TEXT("Up");
            }
            else if (Lower == TEXT("down"))
            {
                DirectionSum += FVector::DownVector;
                Result.DirectionName = TEXT("Down");
            }
        }
    }
    
    if (DirectionTokens > 0)
    {
        // Normalize the direction
        Result.Direction = DirectionSum.GetSafeNormal();
        
        // Calculate angle from forward
        Result.Angle = FMath::RadiansToDegrees(
            FMath::Acos(FVector::DotProduct(Result.Direction, FVector::ForwardVector))
        );
        
        Result.Confidence = FMath::Min(DirectionTokens / FMath::Max(1.0f, Tokens.Tokens.Num() * 0.3f), 1.0f);
        
        // Update direction name for diagonal directions
        if (DirectionTokens > 1 && Result.DirectionName.IsEmpty())
        {
            Result.DirectionName = TEXT("Diagonal");
        }
    }
    else
    {
        Result.Direction = FVector::ForwardVector;  // Default forward
        Result.DirectionName = TEXT("Unknown");
        Result.Angle = 0.0f;
        Result.Confidence = 0.1f;
    }
    
    return Result;
}

FMotionTransition UMMMotionSemanticAnalyzer::DetectTransition(const FMMTokenStream& Tokens) const
{
    FMotionTransition Result;
    
    // Look for transition keywords
    bool bFoundFrom = false;
    bool bFoundTo = false;
    
    for (int32 i = 0; i < Tokens.Tokens.Num(); i++)
    {
        const FMMFuzzyToken& Token = Tokens.Tokens[i];
        
        if (Token.Type == EMMTokenType::Delimiter || Token.Type == EMMTokenType::Symbol)
            continue;
        
        FString Lower = Token.Text.ToLower();
        
        // Look for transition indicators
        if (Lower == TEXT("from") && i + 1 < Tokens.Tokens.Num())
        {
            FString NextWord = Tokens.Tokens[i + 1].Text.ToLower();
            Result.FromMotion = GetMotionTypeFromKeyword(NextWord);
            bFoundFrom = true;
        }
        else if (Lower == TEXT("to") && i + 1 < Tokens.Tokens.Num())
        {
            FString NextWord = Tokens.Tokens[i + 1].Text.ToLower();
            Result.ToMotion = GetMotionTypeFromKeyword(NextWord);
            bFoundTo = true;
        }
        else if (Lower == TEXT("start"))
        {
            Result.TransitionType = TEXT("Start");
        }
        else if (Lower == TEXT("stop"))
        {
            Result.TransitionType = TEXT("Stop");
        }
        else if (Lower == TEXT("pivot") || Lower == TEXT("turn"))
        {
            Result.TransitionType = TEXT("Pivot");
        }
        else if (Lower == TEXT("loop") || Lower == TEXT("cycle"))
        {
            Result.TransitionType = TEXT("Loop");
        }
    }
    
    // Default transition type if not specified
    if (Result.TransitionType.IsEmpty())
    {
        if (bFoundFrom && bFoundTo)
        {
            Result.TransitionType = TEXT("Blend");
        }
    }
    
    // Estimate transition duration based on motion types
    if (Result.FromMotion != EMotionType::Unknown && Result.ToMotion != EMotionType::Unknown)
    {
        // Simple heuristic: larger changes take longer
        int32 FromSpeed = (int32)Result.FromMotion;
        int32 ToSpeed = (int32)Result.ToMotion;
        Result.TransitionDuration = FMath::Abs(FromSpeed - ToSpeed) * 0.15f + 0.2f;
    }
    
    return Result;
}

// ========== UTILITIES ==========

float UMMMotionSemanticAnalyzer::CalculateTokenRelevance(const FMMFuzzyToken& Token, const FString& Category) const
{
    FString Lower = Token.Text.ToLower();
    
    if (Category == TEXT("intent"))
    {
        return MotionVocabulary.HasIntentKeyword(Lower) ? 1.0f : 0.0f;
    }
    else if (Category == TEXT("speed"))
    {
        return MotionVocabulary.HasSpeedDescriptor(Lower) ? 1.0f : 0.0f;
    }
    else if (Category == TEXT("direction"))
    {
        return MotionVocabulary.HasDirectionTerm(Lower) ? 1.0f : 0.0f;
    }
    
    return 0.0f;
}

bool UMMMotionSemanticAnalyzer::ContainsMotionKeywords(const FMMTokenStream& Tokens, const FString& Category) const
{
    for (const FMMFuzzyToken& Token : Tokens.Tokens)
    {
        if (CalculateTokenRelevance(Token, Category) > 0.5f)
        {
            return true;
        }
    }
    return false;
}

EMotionType UMMMotionSemanticAnalyzer::GetMotionTypeFromKeyword(const FString& Keyword) const
{
    FString Lower = Keyword.ToLower();
    
    if (Lower == TEXT("idle") || Lower == TEXT("stand"))
        return EMotionType::Idle;
    else if (Lower == TEXT("walk"))
        return EMotionType::Walk;
    else if (Lower == TEXT("run") || Lower == TEXT("jog"))
        return EMotionType::Run;
    else if (Lower == TEXT("sprint"))
        return EMotionType::Sprint;
    else if (Lower == TEXT("jump") || Lower == TEXT("leap"))
        return EMotionType::Jump;
    else if (Lower == TEXT("turn") || Lower == TEXT("rotate"))
        return EMotionType::Turn;
    else if (Lower == TEXT("strafe"))
        return EMotionType::Strafe;
    else if (Lower == TEXT("crouch"))
        return EMotionType::Crouch;
    else if (Lower == TEXT("crawl"))
        return EMotionType::Crawl;
    else if (Lower == TEXT("climb"))
        return EMotionType::Climb;
    
    return EMotionType::Unknown;
}