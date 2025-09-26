// Private/Core/MMFuzzyProcessor.cpp
#if 0
#include "Core/MMFuzzyProcessor.h"
#include "Core/MMFuzzyCore.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"

// ============================================================================
// FMFuzzyToken Implementation
// ============================================================================

FMFuzzyToken::FMFuzzyToken(const FString& InText, int32 Start, int32 End)
    : Text(InText)
    , Canonical(InText.ToLower())
    , Type(EMTokenType::Unknown)
    , StartIndex(Start)
    , EndIndex(End)
    , BoundaryConfidence(1.0f)
    , bIsFuzzy(false)
{
}

float FMFuzzyToken::GetVariantWeight(const FString& Variant) const
{
    int32 Index = Variants.Find(Variant);
    return Index != INDEX_NONE ? VariantWeights[Index] : 0.0f;
}

FString FMFuzzyToken::GetBestVariant() const
{
    if (Variants.Num() == 0)
        return Canonical;
    
    int32 BestIndex = 0;
    float BestWeight = 0.0f;
    
    for (int32 i = 0; i < VariantWeights.Num(); i++)
    {
        if (VariantWeights[i] > BestWeight)
        {
            BestWeight = VariantWeights[i];
            BestIndex = i;
        }
    }
    
    return Variants[BestIndex];
}

bool FMFuzzyToken::IsAlpha() const
{
    for (TCHAR C : Text)
    {
        if (!FChar::IsAlpha(C))
            return false;
    }
    return !Text.IsEmpty();
}

bool FMFuzzyToken::IsNumeric() const
{
    for (TCHAR C : Text)
    {
        if (!FChar::IsDigit(C))
            return false;
    }
    return !Text.IsEmpty();
}

bool FMFuzzyToken::IsAlphaNumeric() const
{
    for (TCHAR C : Text)
    {
        if (!FChar::IsAlnum(C))
            return false;
    }
    return !Text.IsEmpty();
}

bool FMFuzzyToken::IsSeparator() const
{
    return Type == EMTokenType::Separator || Type == EMTokenType::Punctuation;
}

FString FMFuzzyToken::ToString() const
{
    return FString::Printf(TEXT("[%s|%s|%s|%d-%d|%.2f%s]"),
        *Text,
        *Canonical,
        *UEnum::GetValueAsString(Type),
        StartIndex,
        EndIndex,
        BoundaryConfidence,
        bIsFuzzy ? TEXT("|FUZZY") : TEXT("")
    );
}

// ============================================================================
// FMTokenGroup Implementation
// ============================================================================

void FMTokenGroup::AddMember(const FString& Member)
{
    Members.Add(Member.ToLower());
}

bool FMTokenGroup::ContainsMember(const FString& Member) const
{
    return Members.Contains(Member.ToLower());
}

float FMTokenGroup::GetSimilarityToMember(const FString& Text, UFuzzyMatcher* Matcher) const
{
    if (!Matcher)
        return 0.0f;
    
    float MaxSimilarity = 0.0f;
    FString NormalizedText = Text.ToLower();
    
    // Check exact match first
    if (Members.Contains(NormalizedText))
        return 1.0f;
    
    // Check fuzzy matches using Layer 0
    for (const FString& Member : Members)
    {
        FStringMatch Match = Matcher->CompareStrings(NormalizedText, Member, true);
        float Similarity = Match.GetBestSimilarity();
        MaxSimilarity = FMath::Max(MaxSimilarity, Similarity);
    }
    
    return MaxSimilarity;
}

// ============================================================================
// FMTokenStream Implementation
// ============================================================================

void FMTokenStream::Reset()
{
    CurrentIndex = 0;
}

void FMTokenStream::AddToken(const FMFuzzyToken& Token)
{
    Tokens.Add(Token);
}

void FMTokenStream::AddAlternativeToken(const FMFuzzyToken& Token)
{
    AlternativeTokens.Add(Token);
}

FMFuzzyToken FMTokenStream::GetNext()
{
    if (CurrentIndex < Tokens.Num())
    {
        return Tokens[CurrentIndex++];
    }
    return FMFuzzyToken();
}

FMFuzzyToken FMTokenStream::Peek(int32 Ahead) const
{
    int32 Index = CurrentIndex + Ahead - 1;
    if (Index >= 0 && Index < Tokens.Num())
    {
        return Tokens[Index];
    }
    return FMFuzzyToken();
}

TArray<FMFuzzyToken> FMTokenStream::GetTokensOfType(EMTokenType Type) const
{
    TArray<FMFuzzyToken> Result;
    for (const FMFuzzyToken& Token : Tokens)
    {
        if (Token.Type == Type)
        {
            Result.Add(Token);
        }
    }
    return Result;
}

TArray<FString> FMTokenStream::GetAllText() const
{
    TArray<FString> Result;
    for (const FMFuzzyToken& Token : Tokens)
    {
        Result.Add(Token.Text);
    }
    return Result;
}

TArray<FString> FMTokenStream::GetAllCanonical() const
{
    TArray<FString> Result;
    for (const FMFuzzyToken& Token : Tokens)
    {
        Result.Add(Token.Canonical);
    }
    return Result;
}

FString FMTokenStream::ToString() const
{
    TArray<FString> TokenStrings;
    for (const FMFuzzyToken& Token : Tokens)
    {
        TokenStrings.Add(Token.ToString());
    }
    return FString::Join(TokenStrings, TEXT(" "));
}

FString FMTokenStream::ToCanonicalString() const
{
    TArray<FString> CanonicalStrings;
    for (const FMFuzzyToken& Token : Tokens)
    {
        if (Token.Type != EMTokenType::Separator && Token.Type != EMTokenType::Punctuation)
        {
            CanonicalStrings.Add(Token.Canonical);
        }
    }
    return FString::Join(CanonicalStrings, TEXT(" "));
}

// ============================================================================
// FMTokenizerConfig Implementation
// ============================================================================

FMTokenizerConfig::FMTokenizerConfig()
{
    LoadDefaults();
}

void FMTokenizerConfig::LoadDefaults()
{
    // Common animation abbreviations
    KnownAbbreviations.Add(TEXT("anim"), TEXT("animation"));
    KnownAbbreviations.Add(TEXT("seq"), TEXT("sequence"));
    KnownAbbreviations.Add(TEXT("mtg"), TEXT("montage"));
    KnownAbbreviations.Add(TEXT("bs"), TEXT("blendspace"));
    KnownAbbreviations.Add(TEXT("fwd"), TEXT("forward"));
    KnownAbbreviations.Add(TEXT("bwd"), TEXT("backward"));
    KnownAbbreviations.Add(TEXT("lt"), TEXT("left"));
    KnownAbbreviations.Add(TEXT("rt"), TEXT("right"));
    KnownAbbreviations.Add(TEXT("diag"), TEXT("diagonal"));
    KnownAbbreviations.Add(TEXT("trans"), TEXT("transition"));
    KnownAbbreviations.Add(TEXT("loc"), TEXT("locomotion"));
    KnownAbbreviations.Add(TEXT("mm"), TEXT("motionmatching"));
    
    // Common compound words
    KnownCompounds.Add(TEXT("setup"));
    KnownCompounds.Add(TEXT("walkforward"));
    KnownCompounds.Add(TEXT("runforward"));
    KnownCompounds.Add(TEXT("turnaround"));
    KnownCompounds.Add(TEXT("stepback"));
    KnownCompounds.Add(TEXT("sidestep"));
    KnownCompounds.Add(TEXT("quick-turn"));
}

// ============================================================================
// FMMTokenContext Implementation
// ============================================================================

bool FMMTokenContext::IsInVocabulary(const FString& Word) const
{
    return Vocabulary.Contains(Word.ToLower());
}

int32 FMMTokenContext::GetPriority(const FString& Word) const
{
    const int32* Priority = PriorityHints.Find(Word.ToLower());
    return Priority ? *Priority : 0;
}

// ============================================================================
// UMMFuzzyProcessor Implementation
// ============================================================================

void UMMFuzzyProcessor::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Get reference to Layer 0 (UFuzzyMatcher)
    FuzzyMatcher = GEngine->GetEngineSubsystem<UFuzzyMatcher>();
    
    if (!FuzzyMatcher)
    {
        UE_LOG(LogFuzzyCore, Error, TEXT("UMMFuzzyProcessor: Failed to get UFuzzyMatcher subsystem"));
    }
    
    CurrentConfig = FMTokenizerConfig();
    LastTokenizationTime = 0.0;
    TotalTokenizationCount = 0;
    
    LoadAnimationVocabulary();
    LoadDefaultAbbreviations();
    LoadDefaultCompounds();
    
    UE_LOG(LogFuzzyCore, Log, TEXT("UMMFuzzyProcessor initialized"));
}

void UMMFuzzyProcessor::Deinitialize()
{
    VariantCache.Empty();
    TokenGroups.Empty();
    CanonicalMappings.Empty();
    
    Super::Deinitialize();
}

void UMMFuzzyProcessor::InitializeProcessor(const FMTokenizerConfig& Config)
{
    CurrentConfig = Config;
    
    // Merge with defaults if needed
    if (CurrentConfig.KnownAbbreviations.Num() == 0)
    {
        CurrentConfig.LoadDefaults();
    }
}

FMTokenStream UMMFuzzyProcessor::Tokenize(const FString& Input, const FMMTokenContext& Context)
{
    double StartTime = FPlatformTime::Seconds();
    
    FMTokenStream Stream;
    Stream.OriginalText = Input;
    
    if (Input.IsEmpty())
    {
        Stream.TokenizationTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        return Stream;
    }
    
    // Normalize input
    FString NormalizedInput = NormalizeText(Input);
    
    // First pass: Split by separators
    TArray<FMFuzzyToken> InitialTokens = SplitBySeparators(NormalizedInput);
    
    // Second pass: Handle special cases (camelCase, snake_case, numbers)
    TArray<FMFuzzyToken> ProcessedTokens;
    
    for (const FMFuzzyToken& Token : InitialTokens)
    {
        if (Token.Type == EMTokenType::Separator || Token.Type == EMTokenType::Punctuation)
        {
            ProcessedTokens.Add(Token);
            continue;
        }
        
        // Check naming conventions
        EMTokenType Convention = DetectNamingConvention(Token.Text);
        
        if (Convention == EMTokenType::CamelCase && CurrentConfig.bSplitCamelCase)
        {
            TArray<FMFuzzyToken> SplitTokens = SplitCamelCase(Token.Text, Token.StartIndex);
            ProcessedTokens.Append(SplitTokens);
            
            if (CurrentConfig.bPreserveOriginal)
            {
                FMFuzzyToken Original = Token;
                Original.Type = EMTokenType::CamelCase;
                Original.bIsFuzzy = true;
                Stream.AddAlternativeToken(Original);
            }
        }
        else if (Convention == EMTokenType::SnakeCase && CurrentConfig.bSplitSnakeCase)
        {
            TArray<FMFuzzyToken> SplitTokens = SplitSnakeCase(Token.Text, Token.StartIndex);
            ProcessedTokens.Append(SplitTokens);
            
            if (CurrentConfig.bPreserveOriginal)
            {
                FMFuzzyToken Original = Token;
                Original.Type = EMTokenType::SnakeCase;
                Original.bIsFuzzy = true;
                Stream.AddAlternativeToken(Original);
            }
        }
        else if (CurrentConfig.bSplitNumbers)
        {
            // Check if contains numbers
            bool HasNumbers = false;
            for (TCHAR C : Token.Text)
            {
                if (FChar::IsDigit(C))
                {
                    HasNumbers = true;
                    break;
                }
            }
            
            if (HasNumbers && !Token.IsNumeric())
            {
                TArray<FMFuzzyToken> SplitTokens = SplitNumbers(Token.Text, Token.StartIndex);
                ProcessedTokens.Append(SplitTokens);
            }
            else
            {
                FMFuzzyToken ProcessedToken = Token;
                ProcessedToken.Type = Token.IsAlpha() ? EMTokenType::Word :
                                    Token.IsNumeric() ? EMTokenType::Number :
                                    EMTokenType::Mixed;
                ProcessedTokens.Add(ProcessedToken);
            }
        }
        else
        {
            FMFuzzyToken ProcessedToken = Token;
            ProcessedToken.Type = Token.IsAlpha() ? EMTokenType::Word :
                                Token.IsNumeric() ? EMTokenType::Number :
                                EMTokenType::Mixed;
            
            // Check for abbreviation
            if (IsLikelyAbbreviation(Token.Text))
            {
                ProcessedToken.Type = EMTokenType::Abbreviation;
                
                // Expand abbreviation if known in instance config
                FString Lower = Token.Text.ToLower();
                if (const FString* Expansion = CurrentConfig.KnownAbbreviations.Find(Lower))
                {
                    ProcessedToken.AddVariant(*Expansion, 0.9f);
                }
            }
            
            // Generate variants if configured
            if (CurrentConfig.bGenerateTypoVariants)
            {
                TArray<FString> TypoVariants = GenerateTypoVariants(ProcessedToken.Text, CurrentConfig.MaxTypoDistance);
                for (const FString& Variant : TypoVariants)
                {
                    ProcessedToken.AddVariant(Variant, 0.7f);
                }
            }
            
            ProcessedTokens.Add(ProcessedToken);
        }
    }
    
    // Third pass: Check for compounds
    if (CurrentConfig.bRecognizeCompounds)
    {
        TArray<FMFuzzyToken> FinalTokens;
        
        for (int32 i = 0; i < ProcessedTokens.Num(); i++)
        {
            if (i < ProcessedTokens.Num() - 1)
            {
                TArray<FMFuzzyToken> PotentialCompound = {ProcessedTokens[i], ProcessedTokens[i + 1]};
                
                if (CouldBeCompound(PotentialCompound))
                {
                    FMFuzzyToken Compound = MergeToCompound(PotentialCompound);
                    FinalTokens.Add(Compound);
                    
                    // Keep original tokens as alternatives
                    Stream.AddAlternativeToken(ProcessedTokens[i]);
                    Stream.AddAlternativeToken(ProcessedTokens[i + 1]);
                    
                    i++; // Skip next token as it's part of compound
                    continue;
                }
            }
            
            FinalTokens.Add(ProcessedTokens[i]);
        }
        
        ProcessedTokens = FinalTokens;
    }
    
    // Apply context if available
    if (Context.HasVocabulary())
    {
        for (FMFuzzyToken& Token : ProcessedTokens)
        {
            Token = DisambiguateToken(Token, Context);
        }
    }
    
    // Add to stream
    for (const FMFuzzyToken& Token : ProcessedTokens)
    {
        Stream.AddToken(Token);
    }
    
    Stream.TokenizationTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    LastTokenizationTime = Stream.TokenizationTimeMS;
    TotalTokenizationCount++;
    
    return Stream;
}

FMTokenStream UMMFuzzyProcessor::TokenizeAnimationName(const FString& AnimationName)
{
    FMMTokenContext AnimContext;
    AnimContext.Domain = TEXT("animation");
    AnimContext.Vocabulary = AnimationVocabulary;
    
    // Animation names often use specific conventions
    FMTokenizerConfig AnimConfig = CurrentConfig;
    AnimConfig.bSplitCamelCase = true;
    AnimConfig.bSplitSnakeCase = true;
    AnimConfig.bSplitNumbers = true;
    AnimConfig.bRecognizeCompounds = false; // Animation names are usually already split
    
    FMTokenizerConfig OldConfig = CurrentConfig;
    CurrentConfig = AnimConfig;
    
    FMTokenStream Result = Tokenize(AnimationName, AnimContext);
    
    CurrentConfig = OldConfig;
    
    return Result;
}

FMTokenStream UMMFuzzyProcessor::TokenizeCommand(const FString& Command)
{
    FMMTokenContext CommandContext;
    CommandContext.Domain = TEXT("command");
    CommandContext.Vocabulary = MotionVocabulary;
    CommandContext.Vocabulary.Append(DirectionVocabulary);
    
    // Natural language commands need different handling
    FMTokenizerConfig CommandConfig = CurrentConfig;
    CommandConfig.bSplitCamelCase = false;
    CommandConfig.bSplitSnakeCase = false;
    CommandConfig.bRecognizeCompounds = true;
    CommandConfig.bGenerateTypoVariants = true;
    
    FMTokenizerConfig OldConfig = CurrentConfig;
    CurrentConfig = CommandConfig;
    
    FMTokenStream Result = Tokenize(Command, CommandContext);
    
    CurrentConfig = OldConfig;
    
    return Result;
}

TArray<FString> UMMFuzzyProcessor::GenerateVariants(const FString& Token, int32 MaxVariants)
{
    // Check cache first
    if (const TArray<FString>* CachedVariants = VariantCache.Find(Token))
    {
        TArray<FString> Result = *CachedVariants;
        if (Result.Num() > MaxVariants)
        {
            Result.SetNum(MaxVariants);
        }
        return Result;
    }
    
    TArray<FString> Variants;
    
    // Add canonical form
    Variants.Add(Token.ToLower());
    
    // Add typo variants
    if (CurrentConfig.bGenerateTypoVariants)
    {
        TArray<FString> Typos = GenerateTypoVariants(Token, CurrentConfig.MaxTypoDistance);
        Variants.Append(Typos);
    }
    
    // Add abbreviation variants
    if (CurrentConfig.bGenerateAbbreviations)
    {
        TArray<FString> Abbreviations = GenerateAbbreviationVariants(Token);
        Variants.Append(Abbreviations);
    }
    
    // Cache the full result
    VariantCache.Add(Token, Variants);
    
    // Limit to max variants
    if (Variants.Num() > MaxVariants)
    {
        Variants.SetNum(MaxVariants);
    }
    
    return Variants;
}

TArray<FString> UMMFuzzyProcessor::GenerateTypoVariants(const FString& Word, int32 MaxDistance)
{
    TArray<FString> Variants;
    
    if (Word.IsEmpty() || MaxDistance <= 0)
        return Variants;
    
    FString LowerWord = Word.ToLower();
    
    // Character substitution - use keyboard proximity from Layer 0
    if (FuzzyMatcher)
    {
        for (int32 i = 0; i < LowerWord.Len(); i++)
        {
            TCHAR Original = LowerWord[i];
            
            // Try adjacent keys on QWERTY keyboard
            static const TMap<TCHAR, FString> AdjacentKeys = {
                {TEXT('q'), TEXT("wa")},
                {TEXT('w'), TEXT("qeas")},
                {TEXT('e'), TEXT("wrds")},
                {TEXT('r'), TEXT("etfd")},
                {TEXT('t'), TEXT("rygf")},
                {TEXT('y'), TEXT("tuhg")},
                {TEXT('u'), TEXT("yijh")},
                {TEXT('i'), TEXT("uokj")},
                {TEXT('o'), TEXT("iplk")},
                {TEXT('p'), TEXT("ol")},
                {TEXT('a'), TEXT("qwsz")},
                {TEXT('s'), TEXT("awedxz")},
                {TEXT('d'), TEXT("serfcx")},
                {TEXT('f'), TEXT("drtgvc")},
                {TEXT('g'), TEXT("ftyhbv")},
                {TEXT('h'), TEXT("gyujnb")},
                {TEXT('j'), TEXT("huikmn")},
                {TEXT('k'), TEXT("jiolm")},
                {TEXT('l'), TEXT("kop")},
                {TEXT('z'), TEXT("asx")},
                {TEXT('x'), TEXT("zsdc")},
                {TEXT('c'), TEXT("xdfv")},
                {TEXT('v'), TEXT("cfgb")},
                {TEXT('b'), TEXT("vghn")},
                {TEXT('n'), TEXT("bhjm")},
                {TEXT('m'), TEXT("njk")}
            };
            
            if (const FString* Adjacent = AdjacentKeys.Find(Original))
            {
                for (TCHAR Replacement : *Adjacent)
                {
                    FString Variant = LowerWord;
                    Variant[i] = Replacement;
                    Variants.AddUnique(Variant);
                }
            }
        }
    }
    
    // Character deletion
    if (MaxDistance >= 1 && LowerWord.Len() > 2)
    {
        for (int32 i = 0; i < LowerWord.Len(); i++)
        {
            FString Variant = LowerWord.Left(i) + LowerWord.Mid(i + 1);
            Variants.AddUnique(Variant);
        }
    }
    
    // Character insertion (limited to prevent explosion)
    if (MaxDistance >= 1 && LowerWord.Len() < 20 && Variants.Num() < 30)
    {
        // Only insert common letters
        static const FString CommonLetters = TEXT("aeiourstln");
        
        for (int32 i = 0; i <= LowerWord.Len() && Variants.Num() < 50; i++)
        {
            for (TCHAR C : CommonLetters)
            {
                FString Variant = LowerWord.Left(i) + FString::Chr(C) + LowerWord.Mid(i);
                Variants.AddUnique(Variant);
                
                if (Variants.Num() > 50)
                    break;
            }
        }
    }
    
    // Transposition
    if (MaxDistance >= 1 && LowerWord.Len() > 1)
    {
        for (int32 i = 0; i < LowerWord.Len() - 1; i++)
        {
            FString Variant = LowerWord;
            Swap(Variant[i], Variant[i + 1]);
            Variants.AddUnique(Variant);
        }
    }
    
    return Variants;
}

TArray<FString> UMMFuzzyProcessor::GenerateAbbreviationVariants(const FString& Word)
{
    TArray<FString> Variants;
    
    if (Word.Len() <= 2)
        return Variants;
    
    FString LowerWord = Word.ToLower();
    
    // First letters only
    if (LowerWord.Len() >= 3)
    {
        Variants.Add(LowerWord.Left(3));
        if (LowerWord.Len() >= 4)
        {
            Variants.Add(LowerWord.Left(2));
        }
    }
    
    // Remove vowels
    FString ConsonantOnly;
    for (TCHAR C : LowerWord)
    {
        if (!FString(TEXT("aeiou")).Contains(FString::Chr(C)))
        {
            ConsonantOnly += C;
        }
    }
    if (!ConsonantOnly.IsEmpty() && ConsonantOnly != LowerWord)
    {
        Variants.Add(ConsonantOnly);
    }
    
    // First and last letters
    if (LowerWord.Len() >= 4)
    {
        FString FirstLast = FString::Chr(LowerWord[0]) + LowerWord.Right(1);
        Variants.Add(FirstLast);
    }
    
    // Check if this is an expansion of a known abbreviation
    for (const auto& Pair : CurrentConfig.KnownAbbreviations)
    {
        if (Pair.Value.Equals(LowerWord, ESearchCase::IgnoreCase))
        {
            Variants.Add(Pair.Key);
        }
    }
    
    return Variants;
}

void UMMFuzzyProcessor::RegisterTokenGroup(const FMTokenGroup& Group)
{
    TokenGroups.Add(Group.GroupId, Group);
    
    // Update canonical mappings
    for (const FString& Member : Group.Members)
    {
        CanonicalMappings.Add(Member, Group.CanonicalForm);
    }
}

TArray<FMTokenGroup> UMMFuzzyProcessor::FindTokenGroups(const FString& Token)
{
    TArray<FMTokenGroup> FoundGroups;
    FString NormalizedToken = Token.ToLower();
    
    for (const auto& Pair : TokenGroups)
    {
        if (Pair.Value.ContainsMember(NormalizedToken))
        {
            FoundGroups.Add(Pair.Value);
        }
        else if (FuzzyMatcher)
        {
            // Check fuzzy match with group members
            float Similarity = Pair.Value.GetSimilarityToMember(NormalizedToken, FuzzyMatcher);
            if (Similarity > 0.8f)
            {
                FoundGroups.Add(Pair.Value);
            }
        }
    }
    
    return FoundGroups;
}

FString UMMFuzzyProcessor::GetCanonicalForm(const FString& Token)
{
    FString NormalizedToken = Token.ToLower();
    
    if (const FString* Canonical = CanonicalMappings.Find(NormalizedToken))
    {
        return *Canonical;
    }
    
    return NormalizedToken;
}

bool UMMFuzzyProcessor::CouldBeCompound(const TArray<FMFuzzyToken>& Tokens)
{
    if (Tokens.Num() != 2)
        return false;
    
    // Check if combination is a known compound
    FString Combined = Tokens[0].Canonical + Tokens[1].Canonical;
    if (CurrentConfig.KnownCompounds.Contains(Combined))
        return true;
    
    // Check common compound patterns
    const FString& First = Tokens[0].Canonical;
    const FString& Second = Tokens[1].Canonical;
    
    // Motion + Direction
    if (MotionVocabulary.Contains(First) && DirectionVocabulary.Contains(Second))
        return true;
    
    // Direction + Motion
    if (DirectionVocabulary.Contains(First) && MotionVocabulary.Contains(Second))
        return true;
    
    return false;
}

FMFuzzyToken UMMFuzzyProcessor::MergeToCompound(const TArray<FMFuzzyToken>& Tokens)
{
    FMFuzzyToken Compound;
    
    if (Tokens.Num() == 0)
        return Compound;
    
    // Merge text
    TArray<FString> TextParts;
    for (const FMFuzzyToken& Token : Tokens)
    {
        TextParts.Add(Token.Text);
    }
    Compound.Text = FString::Join(TextParts, TEXT(""));
    
    // Merge canonical
    TArray<FString> CanonicalParts;
    for (const FMFuzzyToken& Token : Tokens)
    {
        CanonicalParts.Add(Token.Canonical);
    }
    Compound.Canonical = FString::Join(CanonicalParts, TEXT(""));
    
    // Set type
    Compound.Type = EMTokenType::Compound;
    
    // Set position
    Compound.StartIndex = Tokens[0].StartIndex;
    Compound.EndIndex = Tokens.Last().EndIndex;
    
    // Average confidence
    float TotalConfidence = 0.0f;
    for (const FMFuzzyToken& Token : Tokens)
    {
        TotalConfidence += Token.BoundaryConfidence;
    }
    Compound.BoundaryConfidence = TotalConfidence / Tokens.Num();
    
    // Mark as fuzzy
    Compound.bIsFuzzy = true;
    
    return Compound;
}

TArray<FMFuzzyToken> UMMFuzzyProcessor::SplitCompound(const FMFuzzyToken& Compound)
{
    TArray<FMFuzzyToken> Components;
    
    // Try to find split point
    FString Text = Compound.Text;
    
    // Check known compounds
    for (const FString& Known : CurrentConfig.KnownCompounds)
    {
        if (Text.Equals(Known, ESearchCase::IgnoreCase))
        {
            // Find natural split point
            // This is simplified - could be more sophisticated
            int32 SplitPoint = Text.Len() / 2;
            
            // Try to find a better split based on vocabulary
            for (int32 i = 1; i < Text.Len(); i++)
            {
                FString First = Text.Left(i);
                FString Second = Text.Mid(i);
                
                if ((MotionVocabulary.Contains(First.ToLower()) || 
                     DirectionVocabulary.Contains(First.ToLower())) &&
                    (MotionVocabulary.Contains(Second.ToLower()) || 
                     DirectionVocabulary.Contains(Second.ToLower())))
                {
                    SplitPoint = i;
                    break;
                }
            }
            
            FMFuzzyToken First(Text.Left(SplitPoint), Compound.StartIndex, Compound.StartIndex + SplitPoint);
            FMFuzzyToken Second(Text.Mid(SplitPoint), Compound.StartIndex + SplitPoint, Compound.EndIndex);
            
            Components.Add(First);
            Components.Add(Second);
            
            return Components;
        }
    }
    
    // Fallback: return original as single component
    Components.Add(Compound);
    return Components;
}

FMFuzzyToken UMMFuzzyProcessor::DisambiguateToken(const FMFuzzyToken& Token, const FMMTokenContext& Context)
{
    FMFuzzyToken Result = Token;
    
    // Check vocabulary
    if (Context.IsInVocabulary(Token.Canonical))
    {
        Result.BoundaryConfidence = FMath::Min(Result.BoundaryConfidence + 0.2f, 1.0f);
    }
    
    // Check priority
    int32 Priority = Context.GetPriority(Token.Canonical);
    if (Priority > 0)
    {
        Result.ContextHints.Add(TEXT("Priority"), FString::FromInt(Priority));
    }
    
    // Domain-specific processing
    if (Context.Domain == TEXT("animation"))
    {
        // Animation-specific disambiguation
        if (AnimationVocabulary.Contains(Token.Canonical))
        {
            Result.ContextHints.Add(TEXT("Domain"), TEXT("Animation"));
        }
    }
    else if (Context.Domain == TEXT("command"))
    {
        // Command-specific disambiguation
        if (MotionVocabulary.Contains(Token.Canonical))
        {
            Result.ContextHints.Add(TEXT("Type"), TEXT("Motion"));
        }
        else if (DirectionVocabulary.Contains(Token.Canonical))
        {
            Result.ContextHints.Add(TEXT("Type"), TEXT("Direction"));
        }
    }
    
    return Result;
}

float UMMFuzzyProcessor::ScoreTokenInContext(const FMFuzzyToken& Token, const FMMTokenContext& Context)
{
    float Score = 0.5f; // Base score
    
    // Vocabulary match
    if (Context.IsInVocabulary(Token.Canonical))
    {
        Score += 0.3f;
    }
    
    // Priority boost
    int32 Priority = Context.GetPriority(Token.Canonical);
    Score += Priority * 0.1f;
    
    // Type expectation
    if (Context.ExpectedTypes.Contains(Token.Type))
    {
        Score += 0.2f;
    }
    
    // Use Layer 0 fuzzy matching for vocabulary similarity
    if (FuzzyMatcher && Context.HasVocabulary())
    {
        float MaxSimilarity = 0.0f;
        for (const FString& VocabWord : Context.Vocabulary)
        {
            float Similarity = FuzzyMatcher->CompareStringsFast(Token.Canonical, VocabWord);
            MaxSimilarity = FMath::Max(MaxSimilarity, Similarity);
        }
        Score += MaxSimilarity * 0.2f;
    }
    
    return FMath::Clamp(Score, 0.0f, 1.0f);
}

FString UMMFuzzyProcessor::NormalizeText(const FString& Text)
{
    FString Result = Text;
    
    // Trim whitespace
    Result.TrimStartAndEndInline();
    
    // Normalize whitespace
    Result = Result.Replace(TEXT("\t"), TEXT(" "));
    Result = Result.Replace(TEXT("\r\n"), TEXT(" "));
    Result = Result.Replace(TEXT("\n"), TEXT(" "));
    
    // Collapse multiple spaces
    while (Result.Contains(TEXT("  ")))
    {
        Result = Result.Replace(TEXT("  "), TEXT(" "));
    }
    
    return Result;
}

EMTokenType UMMFuzzyProcessor::DetectNamingConvention(const FString& Text)
{
    if (Text.IsEmpty())
        return EMTokenType::Unknown;
    
    // Check for snake_case
    if (Text.Contains(TEXT("_")))
        return EMTokenType::SnakeCase;
    
    // Check for camelCase or PascalCase
    bool HasUppercase = false;
    bool HasLowercase = false;
    
    for (TCHAR C : Text)
    {
        if (FChar::IsUpper(C))
            HasUppercase = true;
        if (FChar::IsLower(C))
            HasLowercase = true;
    }
    
    if (HasUppercase && HasLowercase)
    {
        // Has mixed case - likely camelCase or PascalCase
        return EMTokenType::CamelCase;
    }
    
    // Check content
    if (Text.IsNumeric())
        return EMTokenType::Number;
    
    bool AllAlpha = true;
    for (TCHAR C : Text)
    {
        if (!FChar::IsAlpha(C))
        {
            AllAlpha = false;
            break;
        }
    }
    
    if (AllAlpha)
        return EMTokenType::Word;
    
    return EMTokenType::Mixed;
}

bool UMMFuzzyProcessor::IsLikelyAbbreviation(const FString& Text)
{
    if (Text.IsEmpty() || Text.Len() > 5)
        return false;
    
    // Check if all uppercase
    bool AllUppercase = true;
    for (TCHAR C : Text)
    {
        if (!FChar::IsUpper(C) && FChar::IsAlpha(C))
        {
            AllUppercase = false;
            break;
        }
    }
    
    if (AllUppercase && Text.Len() <= 4)
        return true;
    
    FString Lower = Text.ToLower();
    
    // Check against common abbreviations (static list)
    static const TSet<FString> CommonAbbreviations = {
        TEXT("anim"), TEXT("seq"), TEXT("mtg"), TEXT("bs"),
        TEXT("fwd"), TEXT("bwd"), TEXT("lt"), TEXT("rt"),
        TEXT("diag"), TEXT("trans"), TEXT("loc"), TEXT("mm"),
        TEXT("rm"), TEXT("ik"), TEXT("fk"), TEXT("ai"),
        TEXT("ui"), TEXT("hud"), TEXT("fps"), TEXT("rpg")
    };
    
    if (CommonAbbreviations.Contains(Lower))
        return true;
    
    // Check consonant/vowel ratio
    int32 Consonants = 0;
    int32 Vowels = 0;
    for (TCHAR C : Lower)
    {
        if (FString(TEXT("aeiou")).Contains(FString::Chr(C)))
            Vowels++;
        else if (FChar::IsAlpha(C))
            Consonants++;
    }
    
    // High consonant ratio suggests abbreviation
    if (Consonants > 0 && static_cast<float>(Vowels) / static_cast<float>(Consonants) < 0.3f)
        return true;
    
    return false;
}

TArray<FMFuzzyToken> UMMFuzzyProcessor::SplitBySeparators(const FString& Input)
{
    TArray<FMFuzzyToken> Tokens;
    
    FString CurrentToken;
    int32 TokenStart = 0;
    
    for (int32 i = 0; i < Input.Len(); i++)
    {
        TCHAR C = Input[i];
        
        bool IsSeparator = FChar::IsWhitespace(C) || CurrentConfig.CustomSeparators.Contains(FString::Chr(C));
        
        if (IsSeparator)
        {
            // Save current token if not empty
            if (!CurrentToken.IsEmpty())
            {
                FMFuzzyToken Token(CurrentToken, TokenStart, i - 1);
                Tokens.Add(Token);
                CurrentToken.Empty();
            }
            
            // Add separator token (skip whitespace)
            if (!FChar::IsWhitespace(C))
            {
                FMFuzzyToken SepToken(FString::Chr(C), i, i);
                SepToken.Type = EMTokenType::Punctuation;
                Tokens.Add(SepToken);
            }
            
            TokenStart = i + 1;
        }
        else
        {
            CurrentToken += C;
        }
    }
    
    // Add final token
    if (!CurrentToken.IsEmpty())
    {
        FMFuzzyToken Token(CurrentToken, TokenStart, Input.Len() - 1);
        Tokens.Add(Token);
    }
    
    return Tokens;
}

TArray<FMFuzzyToken> UMMFuzzyProcessor::SplitCamelCase(const FString& Text, int32 StartIndex)
{
    TArray<FMFuzzyToken> Tokens;
    
    FString CurrentWord;
    int32 WordStart = StartIndex;
    
    for (int32 i = 0; i < Text.Len(); i++)
    {
        TCHAR C = Text[i];
        
        if (i > 0 && FChar::IsUpper(C) && !CurrentWord.IsEmpty())
        {
            // Check if next character is lowercase (start of new word)
            bool IsNewWord = (i + 1 < Text.Len() && FChar::IsLower(Text[i + 1])) ||
                            (i > 0 && FChar::IsLower(Text[i - 1]));
            
            if (IsNewWord)
            {
                // Save current word
                FMFuzzyToken Token(CurrentWord, WordStart, StartIndex + i - 1);
                Token.Type = EMTokenType::Word;
                Tokens.Add(Token);
                
                CurrentWord = FString::Chr(C);
                WordStart = StartIndex + i;
            }
            else
            {
                CurrentWord += C;
            }
        }
        else
        {
            CurrentWord += C;
        }
    }
    
    // Add final word
    if (!CurrentWord.IsEmpty())
    {
        FMFuzzyToken Token(CurrentWord, WordStart, StartIndex + Text.Len() - 1);
        Token.Type = EMTokenType::Word;
        Tokens.Add(Token);
    }
    
    return Tokens;
}

TArray<FMFuzzyToken> UMMFuzzyProcessor::SplitSnakeCase(const FString& Text, int32 StartIndex)
{
    TArray<FMFuzzyToken> Tokens;
    
    TArray<FString> Parts;
    Text.ParseIntoArray(Parts, TEXT("_"));
    
    int32 CurrentPos = StartIndex;
    
    for (int32 i = 0; i < Parts.Num(); i++)
    {
        const FString& Part = Parts[i];
        
        if (!Part.IsEmpty())
        {
            FMFuzzyToken Token(Part, CurrentPos, CurrentPos + Part.Len() - 1);
            Token.Type = Part.IsNumeric() ? EMTokenType::Number : EMTokenType::Word;
            Tokens.Add(Token);
        }
        
        CurrentPos += Part.Len();
        
        // Account for underscore
        if (i < Parts.Num() - 1)
        {
            CurrentPos += 1;
        }
    }
    
    return Tokens;
}

TArray<FMFuzzyToken> UMMFuzzyProcessor::SplitNumbers(const FString& Text, int32 StartIndex)
{
    TArray<FMFuzzyToken> Tokens;
    
    FString CurrentPart;
    int32 PartStart = StartIndex;
    bool InNumber = false;
    
    for (int32 i = 0; i < Text.Len(); i++)
    {
        TCHAR C = Text[i];
        bool IsDigit = FChar::IsDigit(C);
        
        if (IsDigit != InNumber && !CurrentPart.IsEmpty())
        {
            // Transition between number and text
            FMFuzzyToken Token(CurrentPart, PartStart, StartIndex + i - 1);
            Token.Type = InNumber ? EMTokenType::Number : EMTokenType::Word;
            Tokens.Add(Token);
            
            CurrentPart.Empty();
            PartStart = StartIndex + i;
        }
        
        CurrentPart += C;
        InNumber = IsDigit;
    }
    
    // Add final part
    if (!CurrentPart.IsEmpty())
    {
        FMFuzzyToken Token(CurrentPart, PartStart, StartIndex + Text.Len() - 1);
        Token.Type = InNumber ? EMTokenType::Number : EMTokenType::Word;
        Tokens.Add(Token);
    }
    
    return Tokens;
}

void UMMFuzzyProcessor::LoadAnimationVocabulary()
{
    // Motion types
    MotionVocabulary = {
        TEXT("idle"), TEXT("walk"), TEXT("run"), TEXT("sprint"), TEXT("jog"),
        TEXT("crouch"), TEXT("crawl"), TEXT("sneak"), TEXT("tiptoe"),
        TEXT("jump"), TEXT("leap"), TEXT("hop"), TEXT("bounce"),
        TEXT("turn"), TEXT("pivot"), TEXT("rotate"), TEXT("spin"),
        TEXT("dodge"), TEXT("evade"), TEXT("roll"), TEXT("slide"),
        TEXT("climb"), TEXT("vault"), TEXT("mantle"), TEXT("scale"),
        TEXT("swim"), TEXT("dive"), TEXT("float"), TEXT("tread"),
        TEXT("fly"), TEXT("glide"), TEXT("hover"), TEXT("soar"),
        TEXT("attack"), TEXT("strike"), TEXT("punch"), TEXT("kick"),
        TEXT("block"), TEXT("parry"), TEXT("defend"), TEXT("guard"),
        TEXT("reload"), TEXT("aim"), TEXT("shoot"), TEXT("fire"),
        TEXT("pickup"), TEXT("grab"), TEXT("take"), TEXT("hold"),
        TEXT("throw"), TEXT("toss"), TEXT("launch"), TEXT("hurl"),
        TEXT("start"), TEXT("stop"), TEXT("begin"), TEXT("end"),
        TEXT("enter"), TEXT("exit"), TEXT("mount"), TEXT("dismount")
    };
    
    // Directions
    DirectionVocabulary = {
        TEXT("forward"), TEXT("backward"), TEXT("back"), TEXT("reverse"),
        TEXT("left"), TEXT("right"), TEXT("sideways"), TEXT("lateral"),
        TEXT("up"), TEXT("down"), TEXT("vertical"), TEXT("horizontal"),
        TEXT("diagonal"), TEXT("northeast"), TEXT("northwest"), TEXT("southeast"), TEXT("southwest"),
        TEXT("clockwise"), TEXT("counterclockwise"), TEXT("cw"), TEXT("ccw"),
        TEXT("north"), TEXT("south"), TEXT("east"), TEXT("west"),
        TEXT("ahead"), TEXT("behind"), TEXT("beside"), TEXT("around"),
        TEXT("towards"), TEXT("away"), TEXT("into"), TEXT("outof")
    };
    
    // Animation-specific terms
    AnimationVocabulary = {
        TEXT("animation"), TEXT("anim"), TEXT("sequence"), TEXT("seq"),
        TEXT("montage"), TEXT("mtg"), TEXT("blendspace"), TEXT("bs"),
        TEXT("state"), TEXT("transition"), TEXT("trans"), TEXT("blend"),
        TEXT("loop"), TEXT("cycle"), TEXT("oneshot"), TEXT("additive"),
        TEXT("overlay"), TEXT("layer"), TEXT("slot"), TEXT("group"),
        TEXT("notify"), TEXT("event"), TEXT("marker"), TEXT("cue"),
        TEXT("frame"), TEXT("pose"), TEXT("bone"), TEXT("joint"),
        TEXT("root"), TEXT("motion"), TEXT("rootmotion"), TEXT("rm"),
        TEXT("ik"), TEXT("fk"), TEXT("inverse"), TEXT("forward"),
        TEXT("start"), TEXT("end"), TEXT("mid"), TEXT("apex"),
        TEXT("contact"), TEXT("passing"), TEXT("up"), TEXT("down"),
        TEXT("entry"), TEXT("exit"), TEXT("loop"), TEXT("recovery")
    };
    
    // Common words (for filtering)
    CommonWords = {
        TEXT("the"), TEXT("a"), TEXT("an"), TEXT("and"), TEXT("or"), TEXT("but"),
        TEXT("in"), TEXT("on"), TEXT("at"), TEXT("to"), TEXT("for"), TEXT("of"),
        TEXT("with"), TEXT("by"), TEXT("from"), TEXT("as"), TEXT("is"), TEXT("was"),
        TEXT("are"), TEXT("were"), TEXT("be"), TEXT("been"), TEXT("being"),
        TEXT("have"), TEXT("has"), TEXT("had"), TEXT("do"), TEXT("does"), TEXT("did"),
        TEXT("will"), TEXT("would"), TEXT("should"), TEXT("could"), TEXT("may"), TEXT("might"),
        TEXT("can"), TEXT("must"), TEXT("shall"), TEXT("ought"), TEXT("need")
    };
}

void UMMFuzzyProcessor::LoadDefaultAbbreviations()
{
    // Motion abbreviations
    FMTokenGroup WalkGroup;
    WalkGroup.GroupId = TEXT("motion_walk");
    WalkGroup.CanonicalForm = TEXT("walk");
    WalkGroup.Members = {TEXT("walk"), TEXT("wlk"), TEXT("w")};
    WalkGroup.Category = TEXT("motion");
    WalkGroup.Priority = 100;
    RegisterTokenGroup(WalkGroup);
    
    FMTokenGroup RunGroup;
    RunGroup.GroupId = TEXT("motion_run");
    RunGroup.CanonicalForm = TEXT("run");
    RunGroup.Members = {TEXT("run"), TEXT("rn"), TEXT("r")};
    RunGroup.Category = TEXT("motion");
    RunGroup.Priority = 100;
    RegisterTokenGroup(RunGroup);
    
    FMTokenGroup JumpGroup;
    JumpGroup.GroupId = TEXT("motion_jump");
    JumpGroup.CanonicalForm = TEXT("jump");
    JumpGroup.Members = {TEXT("jump"), TEXT("jmp"), TEXT("j")};
    JumpGroup.Category = TEXT("motion");
    JumpGroup.Priority = 100;
    RegisterTokenGroup(JumpGroup);
    
    // Direction abbreviations
    FMTokenGroup ForwardGroup;
    ForwardGroup.GroupId = TEXT("dir_forward");
    ForwardGroup.CanonicalForm = TEXT("forward");
    ForwardGroup.Members = {TEXT("forward"), TEXT("fwd"), TEXT("f"), TEXT("ahead")};
    ForwardGroup.Category = TEXT("direction");
    ForwardGroup.Priority = 100;
    RegisterTokenGroup(ForwardGroup);
    
    FMTokenGroup BackwardGroup;
    BackwardGroup.GroupId = TEXT("dir_backward");
    BackwardGroup.CanonicalForm = TEXT("backward");
    BackwardGroup.Members = {TEXT("backward"), TEXT("back"), TEXT("bwd"), TEXT("b"), TEXT("reverse")};
    BackwardGroup.Category = TEXT("direction");
    BackwardGroup.Priority = 100;
    RegisterTokenGroup(BackwardGroup);
    
    FMTokenGroup LeftGroup;
    LeftGroup.GroupId = TEXT("dir_left");
    LeftGroup.CanonicalForm = TEXT("left");
    LeftGroup.Members = {TEXT("left"), TEXT("lt"), TEXT("l")};
    LeftGroup.Category = TEXT("direction");
    LeftGroup.Priority = 100;
    RegisterTokenGroup(LeftGroup);
    
    FMTokenGroup RightGroup;
    RightGroup.GroupId = TEXT("dir_right");
    RightGroup.CanonicalForm = TEXT("right");
    RightGroup.Members = {TEXT("right"), TEXT("rt"), TEXT("r")};
    RightGroup.Category = TEXT("direction");
    RightGroup.Priority = 100;
    RegisterTokenGroup(RightGroup);
}

void UMMFuzzyProcessor::LoadDefaultCompounds()
{
    // Motion compounds
    CurrentConfig.KnownCompounds.Add(TEXT("walkforward"));
    CurrentConfig.KnownCompounds.Add(TEXT("walkbackward"));
    CurrentConfig.KnownCompounds.Add(TEXT("walkleft"));
    CurrentConfig.KnownCompounds.Add(TEXT("walkright"));
    
    CurrentConfig.KnownCompounds.Add(TEXT("runforward"));
    CurrentConfig.KnownCompounds.Add(TEXT("runbackward"));
    CurrentConfig.KnownCompounds.Add(TEXT("runleft"));
    CurrentConfig.KnownCompounds.Add(TEXT("runright"));
    
    CurrentConfig.KnownCompounds.Add(TEXT("jumpleft"));
    CurrentConfig.KnownCompounds.Add(TEXT("jumpright"));
    CurrentConfig.KnownCompounds.Add(TEXT("jumpforward"));
    CurrentConfig.KnownCompounds.Add(TEXT("jumpbackward"));
    
    // Other common compounds
    CurrentConfig.KnownCompounds.Add(TEXT("setup"));
    CurrentConfig.KnownCompounds.Add(TEXT("startup"));
    CurrentConfig.KnownCompounds.Add(TEXT("shutdown"));
    CurrentConfig.KnownCompounds.Add(TEXT("turnaround"));
    CurrentConfig.KnownCompounds.Add(TEXT("stepback"));
    CurrentConfig.KnownCompounds.Add(TEXT("sidestep"));
    CurrentConfig.KnownCompounds.Add(TEXT("quickturn"));
    CurrentConfig.KnownCompounds.Add(TEXT("hardstop"));
}

TArray<int32> UMMFuzzyProcessor::FindFuzzyBoundaries(const FString& Text, float& OutConfidence)
{
    TArray<int32> Boundaries;
    OutConfidence = 1.0f;
    
    if (Text.IsEmpty())
        return Boundaries;
    
    // Always include start
    Boundaries.Add(0);
    
    float TotalConfidence = 0.0f;
    int32 BoundaryCount = 0;
    
    for (int32 i = 1; i < Text.Len(); i++)
    {
        TCHAR PrevChar = Text[i - 1];
        TCHAR CurrChar = Text[i];
        
        bool IsBoundary = false;
        float LocalConfidence = 0.0f;
        
        // Check for obvious boundaries
        if (FChar::IsWhitespace(CurrChar) || FChar::IsWhitespace(PrevChar))
        {
            IsBoundary = true;
            LocalConfidence = 1.0f;
        }
        // Check for separator characters
        else if (CurrentConfig.CustomSeparators.Contains(FString::Chr(CurrChar)) ||
                 CurrentConfig.CustomSeparators.Contains(FString::Chr(PrevChar)))
        {
            IsBoundary = true;
            LocalConfidence = 1.0f;
        }
        // Check for case changes (camelCase)
        else if (FChar::IsLower(PrevChar) && FChar::IsUpper(CurrChar))
        {
            IsBoundary = true;
            LocalConfidence = 0.9f;
        }
        // Check for letter to number transition
        else if (FChar::IsAlpha(PrevChar) && FChar::IsDigit(CurrChar))
        {
            IsBoundary = true;
            LocalConfidence = 0.95f;
        }
        // Check for number to letter transition
        else if (FChar::IsDigit(PrevChar) && FChar::IsAlpha(CurrChar))
        {
            IsBoundary = true;
            LocalConfidence = 0.95f;
        }
        // Check for multiple uppercase letters followed by lowercase (XMLParser -> XML|Parser)
        else if (i > 1 && FChar::IsUpper(Text[i - 2]) && FChar::IsUpper(PrevChar) && FChar::IsLower(CurrChar))
        {
            // Boundary is actually before the last uppercase
            if (!Boundaries.Contains(i - 1))
            {
                Boundaries.Add(i - 1);
                TotalConfidence += 0.85f;
                BoundaryCount++;
            }
            LocalConfidence = 0.85f;
        }
        // Fuzzy boundary: potential compound word split point
        else if (i > 3 && i < Text.Len() - 3)
        {
            // Check if this position could be a compound word boundary
            FString LeftPart = Text.Left(i).ToLower();
            FString RightPart = Text.Mid(i).ToLower();
            
            // Check against vocabularies
            bool LeftInVocab = MotionVocabulary.Contains(LeftPart) || 
                              DirectionVocabulary.Contains(LeftPart) || 
                              AnimationVocabulary.Contains(LeftPart);
            
            bool RightInVocab = MotionVocabulary.Contains(RightPart) || 
                               DirectionVocabulary.Contains(RightPart) || 
                               AnimationVocabulary.Contains(RightPart);
            
            if (LeftInVocab && RightInVocab)
            {
                IsBoundary = true;
                LocalConfidence = 0.7f; // Lower confidence for fuzzy boundaries
            }
        }
        
        if (IsBoundary)
        {
            Boundaries.Add(i);
            TotalConfidence += LocalConfidence;
            BoundaryCount++;
        }
    }
    
    // Always include end
    Boundaries.Add(Text.Len());
    
    // Calculate overall confidence
    if (BoundaryCount > 0)
    {
        OutConfidence = TotalConfidence / BoundaryCount;
    }
    
    return Boundaries;
}

FString UMMFuzzyProcessor::GenerateSingleTypo(const FString& Word, int32 TypeIndex)
{
    if (Word.IsEmpty())
        return Word;
    
    FString Result = Word.ToLower();
    
    // Different typo types based on TypeIndex
    switch (TypeIndex % 5)
    {
        case 0: // Character substitution
        {
            if (Result.Len() > 0)
            {
                int32 Pos = FMath::RandRange(0, Result.Len() - 1);
                
                // Substitute with keyboard-adjacent character
                static const TMap<TCHAR, FString> AdjacentKeys = {
                    {TEXT('a'), TEXT("qwsz")},
                    {TEXT('s'), TEXT("awedxz")},
                    {TEXT('d'), TEXT("serfcx")},
                    // ... (abbreviated for space)
                };
                
                TCHAR Original = Result[Pos];
                if (const FString* Adjacent = AdjacentKeys.Find(Original))
                {
                    if (Adjacent->Len() > 0)
                    {
                        Result[Pos] = (*Adjacent)[FMath::RandRange(0, Adjacent->Len() - 1)];
                    }
                }
            }
            break;
        }
        
        case 1: // Character deletion
        {
            if (Result.Len() > 2)
            {
                int32 Pos = FMath::RandRange(0, Result.Len() - 1);
                Result = Result.Left(Pos) + Result.Mid(Pos + 1);
            }
            break;
        }
        
        case 2: // Character insertion
        {
            if (Result.Len() < 20)
            {
                int32 Pos = FMath::RandRange(0, Result.Len());
                TCHAR NewChar = TEXT("aeiourstln")[FMath::RandRange(0, 9)];
                Result = Result.Left(Pos) + FString::Chr(NewChar) + Result.Mid(Pos);
            }
            break;
        }
        
        case 3: // Transposition
        {
            if (Result.Len() > 1)
            {
                int32 Pos = FMath::RandRange(0, Result.Len() - 2);
                Swap(Result[Pos], Result[Pos + 1]);
            }
            break;
        }
        
        case 4: // Double character
        {
            if (Result.Len() > 0 && Result.Len() < 20)
            {
                int32 Pos = FMath::RandRange(0, Result.Len() - 1);
                Result = Result.Left(Pos + 1) + FString::Chr(Result[Pos]) + Result.Mid(Pos + 1);
            }
            break;
        }
    }
    
    return Result;
}

float UMMFuzzyProcessor::CalculateBoundaryConfidence(const FString& Text, int32 BoundaryIndex)
{
    if (BoundaryIndex <= 0 || BoundaryIndex >= Text.Len())
        return 1.0f; // Start and end boundaries are always certain
    
    float Confidence = 0.5f; // Base confidence
    
    TCHAR PrevChar = Text[BoundaryIndex - 1];
    TCHAR CurrChar = Text[BoundaryIndex];
    
    // High confidence for obvious boundaries
    if (FChar::IsWhitespace(CurrChar) || FChar::IsWhitespace(PrevChar))
    {
        Confidence = 1.0f;
    }
    // Separator characters
    else if (CurrentConfig.CustomSeparators.Contains(FString::Chr(CurrChar)) ||
             CurrentConfig.CustomSeparators.Contains(FString::Chr(PrevChar)))
    {
        Confidence = 0.95f;
    }
    // Case change boundary
    else if (FChar::IsLower(PrevChar) && FChar::IsUpper(CurrChar))
    {
        Confidence = 0.9f;
    }
    // Alpha-numeric boundary
    else if ((FChar::IsAlpha(PrevChar) && FChar::IsDigit(CurrChar)) ||
             (FChar::IsDigit(PrevChar) && FChar::IsAlpha(CurrChar)))
    {
        Confidence = 0.85f;
    }
    // Check context for fuzzy boundaries
    else
    {
        // Look at surrounding context
        FString LeftContext = Text.Left(BoundaryIndex).ToLower();
        FString RightContext = Text.Mid(BoundaryIndex).ToLower();
        
        // Check if left part ends with a known word
        for (const FString& Word : MotionVocabulary)
        {
            if (LeftContext.EndsWith(Word))
            {
                Confidence += 0.2f;
                break;
            }
        }
        
        // Check if right part starts with a known word
        for (const FString& Word : DirectionVocabulary)
        {
            if (RightContext.StartsWith(Word))
            {
                Confidence += 0.2f;
                break;
            }
        }
        
        // Use fuzzy matcher to check similarity if available
        if (FuzzyMatcher && BoundaryIndex > 2 && BoundaryIndex < Text.Len() - 2)
        {
            // Check if splitting here creates recognizable words
            FString LeftWord = Text.Mid(FMath::Max(0, BoundaryIndex - 4), 4);
            FString RightWord = Text.Mid(BoundaryIndex, FMath::Min(4, Text.Len() - BoundaryIndex));
            
            // Check against known patterns
            float LeftScore = 0.0f;
            float RightScore = 0.0f;
            
            for (const FString& Known : AnimationVocabulary)
            {
                if (Known.Len() >= 3)
                {
                    LeftScore = FMath::Max(LeftScore, FuzzyMatcher->CompareStringsFast(LeftWord, Known.Left(LeftWord.Len())));
                    RightScore = FMath::Max(RightScore, FuzzyMatcher->CompareStringsFast(RightWord, Known.Left(RightWord.Len())));
                }
            }
            
            Confidence += (LeftScore + RightScore) * 0.2f;
        }
    }
    
    return FMath::Clamp(Confidence, 0.0f, 1.0f);
}
#endif
