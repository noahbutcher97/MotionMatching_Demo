// Private/Core/MMFuzzyCore.cpp

#include "Core/MMFuzzyCore.h"
#include "HAL/PlatformProcess.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeLock.h"
#include "Async/Async.h"
#if 0
DEFINE_LOG_CATEGORY(LogFuzzyCore);

// ============================================================================
// SETTINGS
// ============================================================================

UFuzzyMatchingSettings::UFuzzyMatchingSettings()
{
    CategoryName = "Plugins";
    SectionName = "Motion Matching Fuzzy Core";
}

#if WITH_EDITOR
void UFuzzyMatchingSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // Validate settings
    MaxCacheSizeMB = FMath::Clamp(MaxCacheSizeMB, 1, 1024);
    ThreadPoolSize = FMath::Clamp(ThreadPoolSize, 1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
    
    UE_LOG(LogFuzzyCore, Log, TEXT("Fuzzy Matching Settings Updated"));
}
#endif

// ============================================================================
// STATIC DATA
// ============================================================================

namespace FuzzyMatchingData
{
    // QWERTY keyboard layout for distance calculation
    static const TMap<TCHAR, FIntPoint> QWERTYLayout = {
        // Row 0 (numbers)
        {TEXT('1'), FIntPoint(1, 0)}, {TEXT('2'), FIntPoint(2, 0)}, {TEXT('3'), FIntPoint(3, 0)}, 
        {TEXT('4'), FIntPoint(4, 0)}, {TEXT('5'), FIntPoint(5, 0)}, {TEXT('6'), FIntPoint(6, 0)},
        {TEXT('7'), FIntPoint(7, 0)}, {TEXT('8'), FIntPoint(8, 0)}, {TEXT('9'), FIntPoint(9, 0)}, 
        {TEXT('0'), FIntPoint(10, 0)},
        
        // Row 1
        {TEXT('q'), FIntPoint(1, 1)}, {TEXT('w'), FIntPoint(2, 1)}, {TEXT('e'), FIntPoint(3, 1)},
        {TEXT('r'), FIntPoint(4, 1)}, {TEXT('t'), FIntPoint(5, 1)}, {TEXT('y'), FIntPoint(6, 1)},
        {TEXT('u'), FIntPoint(7, 1)}, {TEXT('i'), FIntPoint(8, 1)}, {TEXT('o'), FIntPoint(9, 1)},
        {TEXT('p'), FIntPoint(10, 1)},
        
        // Row 2
        {TEXT('a'), FIntPoint(1, 2)}, {TEXT('s'), FIntPoint(2, 2)}, {TEXT('d'), FIntPoint(3, 2)},
        {TEXT('f'), FIntPoint(4, 2)}, {TEXT('g'), FIntPoint(5, 2)}, {TEXT('h'), FIntPoint(6, 2)},
        {TEXT('j'), FIntPoint(7, 2)}, {TEXT('k'), FIntPoint(8, 2)}, {TEXT('l'), FIntPoint(9, 2)},
        
        // Row 3
        {TEXT('z'), FIntPoint(1, 3)}, {TEXT('x'), FIntPoint(2, 3)}, {TEXT('c'), FIntPoint(3, 3)},
        {TEXT('v'), FIntPoint(4, 3)}, {TEXT('b'), FIntPoint(5, 3)}, {TEXT('n'), FIntPoint(6, 3)},
        {TEXT('m'), FIntPoint(7, 3)}
    };
    
    // Visual similarity pairs (OCR confusion matrix)
    static const TMap<TPair<TCHAR, TCHAR>, float> VisualSimilarityMap = {
        {{TEXT('0'), TEXT('O')}, 0.9f}, {{TEXT('0'), TEXT('o')}, 0.85f},
        {{TEXT('1'), TEXT('l')}, 0.9f}, {{TEXT('1'), TEXT('I')}, 0.9f},
        {{TEXT('5'), TEXT('S')}, 0.7f}, {{TEXT('5'), TEXT('s')}, 0.65f},
        {{TEXT('6'), TEXT('b')}, 0.5f}, {{TEXT('8'), TEXT('B')}, 0.7f},
        {{TEXT('9'), TEXT('g')}, 0.6f}, {{TEXT('q'), TEXT('g')}, 0.6f},
        {{TEXT('b'), TEXT('h')}, 0.5f}, {{TEXT('n'), TEXT('h')}, 0.5f},
        {{TEXT('m'), TEXT('n')}, 0.6f}, {{TEXT('u'), TEXT('v')}, 0.5f},
        {{TEXT('i'), TEXT('j')}, 0.5f}, {{TEXT('c'), TEXT('(')}, 0.4f},
        {{TEXT('D'), TEXT('0')}, 0.5f}, {{TEXT('Z'), TEXT('2')}, 0.6f}
    };
    
    // Soundex encoding groups
    static const TMap<TCHAR, TCHAR> SoundexMap = {
        {TEXT('b'), TEXT('1')}, {TEXT('f'), TEXT('1')}, {TEXT('p'), TEXT('1')}, {TEXT('v'), TEXT('1')},
        {TEXT('c'), TEXT('2')}, {TEXT('g'), TEXT('2')}, {TEXT('j'), TEXT('2')}, {TEXT('k'), TEXT('2')}, 
        {TEXT('q'), TEXT('2')}, {TEXT('s'), TEXT('2')}, {TEXT('x'), TEXT('2')}, {TEXT('z'), TEXT('2')},
        {TEXT('d'), TEXT('3')}, {TEXT('t'), TEXT('3')},
        {TEXT('l'), TEXT('4')},
        {TEXT('m'), TEXT('5')}, {TEXT('n'), TEXT('5')},
        {TEXT('r'), TEXT('6')}
    };
    
    // Phonetic similarity groups
    static const TMap<TCHAR, int32> PhoneticGroups = {
        // Vowels
        {TEXT('a'), 1}, {TEXT('e'), 1}, {TEXT('i'), 1}, {TEXT('o'), 1}, {TEXT('u'), 1}, {TEXT('y'), 1},
        // Plosives
        {TEXT('b'), 2}, {TEXT('p'), 2}, {TEXT('d'), 3}, {TEXT('t'), 3}, {TEXT('g'), 4}, {TEXT('k'), 4},
        // Fricatives
        {TEXT('f'), 5}, {TEXT('v'), 5}, {TEXT('s'), 6}, {TEXT('z'), 6}, {TEXT('h'), 7},
        // Nasals
        {TEXT('m'), 8}, {TEXT('n'), 8},
        // Liquids
        {TEXT('l'), 9}, {TEXT('r'), 9},
        // Semivowels
        {TEXT('w'), 10}, {TEXT('j'), 10}
    };
}

// ============================================================================
// LRU CACHE IMPLEMENTATION
// ============================================================================
FThreadSafeLRUCache::~FThreadSafeLRUCache()
{
    Clear();
}

bool FThreadSafeLRUCache::Get(uint32 Key, FStringMatch& OutMatch)
{
    FScopeLock Lock(&Mutex);
    
    if (FLRUNode** NodePtr = Cache.Find(Key))
    {
        FLRUNode* Node = *NodePtr;
        Node->Entry.LastAccessTime = FPlatformTime::Seconds();
        Node->Entry.AccessCount++;
        
        MoveToHead(Node);
        
        OutMatch = Node->Entry.Match;
        CacheHits++;
        return true;
    }
    
    CacheMisses++;
    return false;
}

void FThreadSafeLRUCache::Put(uint32 Key, const FStringMatch& Match)
{
    FScopeLock Lock(&Mutex);
    
    if (FLRUNode** ExistingNodePtr = Cache.Find(Key))
    {
        FLRUNode* Node = *ExistingNodePtr;
        Node->Entry.Match = Match;
        Node->Entry.LastAccessTime = FPlatformTime::Seconds();
        Node->Entry.AccessCount++;
        MoveToHead(Node);
        return;
    }
    
    if (Cache.Num() >= MaxSize)
    {
        EvictLRU();
    }
    
    FLRUNode* NewNode = new FLRUNode();
    NewNode->Key = Key;
    NewNode->Entry.Match = Match;
    NewNode->Entry.LastAccessTime = FPlatformTime::Seconds();
    NewNode->Entry.AccessCount = 1;
    NewNode->Entry.CacheKey = Key;
    
    if (!Head)
    {
        Head = Tail = NewNode;
    }
    else
    {
        NewNode->Next = Head;
        Head->Prev = NewNode;
        Head = NewNode;
    }
    
    Cache.Add(Key, NewNode);
}


void FThreadSafeLRUCache::Clear()
{
    FScopeLock Lock(&Mutex);
    
    FLRUNode* Current = Head;
    while (Current)
    {
        FLRUNode* Next = Current->Next;
        delete Current;
        Current = Next;
    }
    
    Head = Tail = nullptr;
    Cache.Empty();
    CacheHits = 0;
    CacheMisses = 0;
}

int32 FThreadSafeLRUCache::Num() const
{
    FScopeLock Lock(&Mutex);
    return Cache.Num();
}

void FThreadSafeLRUCache::SetMaxSize(int32 NewMaxSize)
{
    FScopeLock Lock(&Mutex);
    MaxSize = FMath::Max(1, NewMaxSize);
    
    // Evict if necessary
    while (Cache.Num() > MaxSize)
    {
        EvictLRU();
    }
}

void FThreadSafeLRUCache::GetStatistics(int32& OutHits, int32& OutMisses) const
{
    FScopeLock Lock(&Mutex);
    OutHits = CacheHits;
    OutMisses = CacheMisses;
}

void FThreadSafeLRUCache::MoveToHead(FLRUNode* Node)
{
    if (Node == Head)
        return;
    
    RemoveNode(Node);
    
    Node->Next = Head;
    Node->Prev = nullptr;
    if (Head)
        Head->Prev = Node;
    Head = Node;
    
    if (!Tail)
        Tail = Node;
}


void FThreadSafeLRUCache::RemoveNode(FLRUNode* Node)
{
    if (Node->Prev)
        Node->Prev->Next = Node->Next;
    else
        Head = Node->Next;
    
    if (Node->Next)
        Node->Next->Prev = Node->Prev;
    else
        Tail = Node->Prev;
}


void FThreadSafeLRUCache::EvictLRU()
{
    if (!Tail)
        return;
    
    FLRUNode* NodeToEvict = Tail;
    uint32 KeyToEvict = NodeToEvict->Key;
    
    RemoveNode(NodeToEvict);
    Cache.Remove(KeyToEvict);
    delete NodeToEvict;
}




// ============================================================================
// CHARACTER MATCH IMPLEMENTATION
// ============================================================================

float FCharacterMatch::GetWeightedSimilarity(float ExactWeight, float KeyboardWeight, 
                                             float VisualWeight, float PhoneticWeight, 
                                             float CategoryWeight) const
{
    const float TotalWeight = ExactWeight + KeyboardWeight + VisualWeight + 
                             PhoneticWeight + CategoryWeight;
    
    if (TotalWeight <= 0.0f)
        return 0.0f;
    
    return (ExactSimilarity * ExactWeight +
            KeyboardSimilarity * KeyboardWeight +
            VisualSimilarity * VisualWeight +
            PhoneticSimilarity * PhoneticWeight +
            CategorySimilarity * CategoryWeight) / TotalWeight;
}

FString FCharacterMatch::ToString() const
{
    return FString::Printf(TEXT("'%c' vs '%c': Exact=%.2f, Keyboard=%.2f, Visual=%.2f, Phonetic=%.2f"),
        CharacterA, CharacterB, ExactSimilarity, KeyboardSimilarity, VisualSimilarity, PhoneticSimilarity);
}

// ============================================================================
// STRING MATCH IMPLEMENTATION
// ============================================================================

float FStringMatch::GetBestSimilarity() const
{
    TArray<float> Values;
    Values.Add(NormalizedLevenshtein);
    Values.Add(JaroWinklerSimilarity);
    Values.Add(DiceCoefficient);
    Values.Add(CosineSimilarity);
    Values.Add(JaccardIndex);
    
    return FMath::Max(Values);
}

float FStringMatch::GetWeightedSimilarity(const TMap<FString, float>& Weights) const
{
    float Total = 0.0f;
    float WeightSum = 0.0f;
    
    auto AddWeight = [&](const FString& Key, float Value) {
        if (const float* Weight = Weights.Find(Key))
        {
            Total += Value * (*Weight);
            WeightSum += *Weight;
        }
    };
    
    AddWeight(TEXT("Levenshtein"), NormalizedLevenshtein);
    AddWeight(TEXT("Jaro"), JaroSimilarity);
    AddWeight(TEXT("JaroWinkler"), JaroWinklerSimilarity);
    AddWeight(TEXT("Dice"), DiceCoefficient);
    AddWeight(TEXT("Cosine"), CosineSimilarity);
    AddWeight(TEXT("Jacquard"), JaccardIndex);
    
    return WeightSum > 0.0f ? Total / WeightSum : GetBestSimilarity();
}

bool FStringMatch::IsValid() const
{
    return !StringA.IsEmpty() && !StringB.IsEmpty() && LevenshteinDistance != MAX_int32;
}

FString FStringMatch::ToString() const
{
    return FString::Printf(TEXT("'%s' vs '%s': Lev=%d, Jaro=%.2f, JW=%.2f, Dice=%.2f"),
        *StringA, *StringB, LevenshteinDistance, JaroSimilarity, JaroWinklerSimilarity, DiceCoefficient);
}

// ============================================================================
// N-GRAM SET IMPLEMENTATION
// ============================================================================

void FNGramSet::Build(const FString& Source)
{
    Clear();
    SourceString = Source;
    
    if (Source.Len() < N)
        return;
    
    for (int32 i = 0; i <= Source.Len() - N; i++)
    {
        FString Gram = Source.Mid(i, N).ToLower();
        int32& Count = Grams.FindOrAdd(Gram);
        Count++;
        TotalGrams++;
    }
}

void FNGramSet::Clear()
{
    Grams.Empty();
    TotalGrams = 0;
    SourceString.Empty();
}

float FNGramSet::CompareTo(const FNGramSet& Other) const
{
    // This should be for ordering, not similarity
    // Compare by total gram count first, then by unique gram count
    if (TotalGrams != Other.TotalGrams)
    {
        return static_cast<float>(TotalGrams - Other.TotalGrams);
    }
    
    return static_cast<float>(Grams.Num() - Other.Grams.Num());
}


float FNGramSet::JaccardSimilarity(const FNGramSet& Other) const
{
    if (Grams.Num() == 0 || Other.Grams.Num() == 0)
        return 0.0f;
    
    TSet<FString> Union, Intersection;
    
    for (const auto& Pair : Grams)
        Union.Add(Pair.Key);
    
    for (const auto& Pair : Other.Grams)
    {
        if (Grams.Contains(Pair.Key))
            Intersection.Add(Pair.Key);
        Union.Add(Pair.Key);
    }
    
    return Union.Num() > 0 ? static_cast<float>(Intersection.Num()) / Union.Num() : 0.0f;
}

float FNGramSet::CosineSimilarity(const FNGramSet& Other) const
{
    if (Grams.Num() == 0 || Other.Grams.Num() == 0)
        return 0.0f;
    
    float DotProduct = 0.0f;
    float MagnitudeA = 0.0f;
    float MagnitudeB = 0.0f;
    
    // Calculate dot product and magnitude for A
    for (const auto& Pair : Grams)
    {
        if (const int32* CountB = Other.Grams.Find(Pair.Key))
        {
            DotProduct += Pair.Value * (*CountB);
        }
        MagnitudeA += Pair.Value * Pair.Value;
    }
    
    // Calculate magnitude for B
    for (const auto& Pair : Other.Grams)
    {
        MagnitudeB += Pair.Value * Pair.Value;
    }
    
    MagnitudeA = FMath::Sqrt(MagnitudeA);
    MagnitudeB = FMath::Sqrt(MagnitudeB);
    
    return (MagnitudeA > 0.0f && MagnitudeB > 0.0f) ? DotProduct / (MagnitudeA * MagnitudeB) : 0.0f;
}

float FNGramSet::DiceSimilarity(const FNGramSet& Other) const
{
    if (Grams.Num() == 0 || Other.Grams.Num() == 0)
        return 0.0f;
    
    int32 CommonGrams = 0;
    for (const auto& Pair : Grams)
    {
        if (Other.Grams.Contains(Pair.Key))
        {
            CommonGrams++;
        }
    }
    
    return (2.0f * CommonGrams) / (Grams.Num() + Other.Grams.Num());
}

TSet<FString> FNGramSet::GetUniqueGrams() const
{
    TSet<FString> UniqueGrams;
    for (const auto& Pair : Grams)
    {
        UniqueGrams.Add(Pair.Key);
    }
    return UniqueGrams;
}

FString FNGramSet::ToString() const
{
    FString Result = FString::Printf(TEXT("NGramSet(N=%d, Total=%d): "), N, TotalGrams);
    
    if (Grams.Num() == 0)
    {
        Result += TEXT("[empty]");
        return Result;
    }
    
    // Sort grams by frequency for display
    TArray<TPair<FString, int32>> SortedGrams;
    for (const auto& Pair : Grams)
    {
        SortedGrams.Add(Pair);
    }
    
    SortedGrams.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
    {
        return A.Value > B.Value; // Sort by count descending
    });
    
    // Display top 5 most frequent n-grams
    const int32 MaxDisplay = FMath::Min(5, SortedGrams.Num());
    for (int32 i = 0; i < MaxDisplay; i++)
    {
        if (i > 0)
            Result += TEXT(", ");
        Result += FString::Printf(TEXT("'%s':%d"), *SortedGrams[i].Key, SortedGrams[i].Value);
    }
    
    if (SortedGrams.Num() > MaxDisplay)
    {
        Result += FString::Printf(TEXT(" ... +%d more"), SortedGrams.Num() - MaxDisplay);
    }
    
    return Result;
}

// ============================================================================
// FUZZY MATCHER IMPLEMENTATION
// ============================================================================

void UFuzzyMatcher::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Load settings once and cache values
    const UFuzzyMatchingSettings* Settings = GetDefault<UFuzzyMatchingSettings>();
    CachedSettings.UpdateFromSettings(Settings);
    
    // Initialize string cache with proper size
    const int32 CacheEntries = (CachedSettings.MaxCacheSizeMB * 1024 * 1024) / 
                              (sizeof(FCacheEntry) + 100);
    StringCache = MakeUnique<FThreadSafeLRUCache>(CacheEntries);
    
    if (CachedSettings.bUseMultithreading)
    {
        ThreadPool = FQueuedThreadPool::Allocate();
        ThreadPool->Create(CachedSettings.ThreadPoolSize, 128 * 1024);
    }
    
    UE_LOG(LogFuzzyCore, Log, TEXT("Fuzzy Matcher initialized with %d thread pool, %d cache entries"), 
           CachedSettings.ThreadPoolSize, CacheEntries);
}

void UFuzzyMatcher::Deinitialize()
{
    ClearCache();
    
    if (ThreadPool)
    {
        ThreadPool->Destroy();
        delete ThreadPool;
        ThreadPool = nullptr;
    }
    
    StringCache.Reset();
    
    Super::Deinitialize();
}

FCharacterMatch UFuzzyMatcher::CompareCharacters(TCHAR A, TCHAR B) const
{
    const uint32 Key = GetCacheKey(A, B);
    
    {
        FScopeLock Lock(&CharacterCacheMutex);
        #pragma warning(suppress: 4189) // RAII lock
        if (const FCharacterMatch* Cached = CharacterCache.Find(Key))
        {
            CacheHits.Increment();
            return *Cached;
        }
    }
    CacheMisses.Increment();
    
    FCharacterMatch Match;
    Match.CharacterA = A;
    Match.CharacterB = B;
    
    // Exact match
    if (A == B)
    {
        Match.ExactSimilarity = 1.0f;
        Match.KeyboardSimilarity = 1.0f;
        Match.VisualSimilarity = 1.0f;
        Match.PhoneticSimilarity = 1.0f;
        Match.CategorySimilarity = 1.0f;
        
        FScopeLock Lock(&CharacterCacheMutex);
        const_cast<UFuzzyMatcher*>(this)->CharacterCache.Add(Key, Match);
        return Match;
    }
    
    // Case-insensitive match
    if (FChar::ToLower(A) == FChar::ToLower(B))
    {
        Match.ExactSimilarity = 0.9f;
        Match.PhoneticSimilarity = 0.95f;
    }
    
    // Keyboard similarity
    Match.KeyboardSimilarity = GetKeyboardDistance(A, B);
    
    // Visual similarity
    Match.VisualSimilarity = GetVisualSimilarity(A, B);
    
    // Phonetic similarity
    Match.PhoneticSimilarity = FMath::Max(Match.PhoneticSimilarity, GetPhoneticSimilarity(A, B));
    
    // Category similarity - Fixed for UE5
    if ((FChar::IsAlpha(A) && FChar::IsAlpha(B)) ||
        (FChar::IsDigit(A) && FChar::IsDigit(B)) ||
        (!FChar::IsAlnum(A) && !FChar::IsWhitespace(A) && 
         !FChar::IsAlnum(B) && !FChar::IsWhitespace(B)))
    {
        Match.CategorySimilarity = 0.5f;
    }
    
    FScopeLock Lock(&CharacterCacheMutex);
    const_cast<UFuzzyMatcher*>(this)->CharacterCache.Add(Key, Match);
    return Match;
}

float UFuzzyMatcher::GetKeyboardDistance(TCHAR A, TCHAR B)
{
    A = FChar::ToLower(A);
    B = FChar::ToLower(B);
    
    const FIntPoint* PosA = FuzzyMatchingData::QWERTYLayout.Find(A);
    const FIntPoint* PosB = FuzzyMatchingData::QWERTYLayout.Find(B);
    
    if (!PosA || !PosB)
        return 0.0f;
    
    const float Distance = FMath::Sqrt(
        static_cast<float>(FMath::Square(PosA->X - PosB->X) + 
                          FMath::Square(PosA->Y - PosB->Y))
    );
    
    // Normalize (max keyboard distance is approximately 10)
    return FMath::Exp(-Distance * 0.3f);
}

float UFuzzyMatcher::GetVisualSimilarity(TCHAR A, TCHAR B)
{
    // Check exact match first
    if (A == B)
        return 1.0f;
    
    // Check both orderings
    TPair<TCHAR, TCHAR> Key1(FMath::Min(A, B), FMath::Max(A, B));
    if (const float* Similarity = FuzzyMatchingData::VisualSimilarityMap.Find(Key1))
    {
        return *Similarity;
    }
    
    // Check case variations
    TPair<TCHAR, TCHAR> Key2(FMath::Min(FChar::ToLower(A), FChar::ToLower(B)), 
                             FMath::Max(FChar::ToLower(A), FChar::ToLower(B)));
    if (const float* Similarity = FuzzyMatchingData::VisualSimilarityMap.Find(Key2))
    {
        return *Similarity * 0.9f; // Slightly lower for case mismatch
    }
    
    return 0.0f;
}

float UFuzzyMatcher::GetPhoneticSimilarity(TCHAR A, TCHAR B)
{
    A = FChar::ToLower(A);
    B = FChar::ToLower(B);
    
    if (A == B)
        return 1.0f;
    
    const int32* GroupA = FuzzyMatchingData::PhoneticGroups.Find(A);
    const int32* GroupB = FuzzyMatchingData::PhoneticGroups.Find(B);
    
    if (!GroupA || !GroupB)
        return 0.0f;
    
    // Same phonetic group
    if (*GroupA == *GroupB)
        return 0.8f;
    
    // Similar groups (adjacent numbers)
    if (FMath::Abs(*GroupA - *GroupB) == 1)
        return 0.4f;
    
    return 0.0f;
}

bool UFuzzyMatcher::AreCharactersSimilar(TCHAR A, TCHAR B, float Threshold)
{
    if (A == B)
        return true;
    
    FCharacterMatch Match = GEngine->GetEngineSubsystem<UFuzzyMatcher>()->CompareCharacters(A, B);
    return Match.GetWeightedSimilarity() >= Threshold;
}

// In UFuzzyMatcher::CompareStrings
FStringMatch UFuzzyMatcher::CompareStrings(const FString& A, const FString& B, bool bUseCache)
{
    const double StartTime = FPlatformTime::Seconds();
    
    // Check cache
    const uint32 Key = GetCacheKey(A, B);
    if (bUseCache)
    {
        FStringMatch CachedMatch;
        if (StringCache->Get(Key, CachedMatch))
        {
            CachedMatch.bWasCached = true;
            return CachedMatch;
        }
    }
    
    FStringMatch Match;
    Match.StringA = A;
    Match.StringB = B;
    
    // Special case: identical strings
    if (A == B)
    {
        Match.LevenshteinDistance = 0;
        Match.NormalizedLevenshtein = 1.0f;
        Match.OptimalAlignmentDistance = 0;
        Match.DamerauLevenshteinDistance = 0;
        Match.JaroSimilarity = 1.0f;
        Match.JaroWinklerSimilarity = 1.0f;
        Match.LongestCommonSubsequence = A.Len();
        Match.LongestCommonSubstring = A.Len();
        Match.DiceCoefficient = 1.0f;
        Match.CosineSimilarity = 1.0f;
        Match.JaccardIndex = 1.0f;
        Match.bSoundexMatch = true;
        Match.bMetaphoneMatch = true;
        Match.bDoubleMetaphoneMatch = true;
        Match.ComputationTimeMS = 0.0f;
        
        if (bUseCache && A.Len() <= CachedSettings.AutoCacheStringLength) // Changed from Settings->
            StringCache->Put(Key, Match);
        
        return Match;
    }
    
    // Compute all metrics
    Match.LevenshteinDistance = CalculateLevenshteinDistance(A, B);
    Match.NormalizedLevenshtein = 1.0f - (static_cast<float>(Match.LevenshteinDistance) / 
                                          FMath::Max(A.Len(), B.Len()));
    
    Match.DamerauLevenshteinDistance = CalculateDamerauLevenshteinDistance(A, B);
    Match.OptimalAlignmentDistance = Match.DamerauLevenshteinDistance;
    
    Match.JaroSimilarity = CalculateJaroSimilarity(A, B);
    Match.JaroWinklerSimilarity = CalculateJaroWinklerSimilarity(A, B, CachedSettings.JaroWinklerPrefixScale); // Changed
    
    Match.LongestCommonSubsequence = CalculateLongestCommonSubsequence(A, B);
    Match.LongestCommonSubstring = CalculateLongestCommonSubstring(A, B);
    
    // N-gram based metrics
    FNGramSet NGramA = GenerateNGrams(A, 2);
    FNGramSet NGramB = GenerateNGrams(B, 2);
    Match.DiceCoefficient = NGramA.DiceSimilarity(NGramB);
    
    FNGramSet TrigramA = GenerateNGrams(A, 3);
    FNGramSet TrigramB = GenerateNGrams(B, 3);
    Match.CosineSimilarity = TrigramA.CosineSimilarity(TrigramB);
    Match.JaccardIndex = TrigramA.JaccardSimilarity(TrigramB);
    
    // Phonetic matching
    if (CachedSettings.bEnablePhoneticMatching) // Changed from Settings->
    {
        Match.bSoundexMatch = (GenerateSoundex(A) == GenerateSoundex(B));
        
        TArray<FString> MetaA = GenerateMetaphone(A, true);
        TArray<FString> MetaB = GenerateMetaphone(B, true);
        
        for (const FString& MA : MetaA)
        {
            if (MetaB.Contains(MA))
            {
                Match.bMetaphoneMatch = true;
                Match.bDoubleMetaphoneMatch = true;
                break;
            }
        }
    }
    
    Match.ComputationTimeMS = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    
    // Cache if appropriate
    if (bUseCache && A.Len() <= CachedSettings.AutoCacheStringLength) // Changed from Settings->
    {
        StringCache->Put(Key, Match);
        TrimCache();
    }
    
    // Fire performance delegate
    OnMatchPerformed.Broadcast(TEXT("CompareStrings"), Match.ComputationTimeMS);
    
    TotalComputations.Increment();
    
    return Match;
}

float UFuzzyMatcher::CompareStringsFast(const FString& A, const FString& B)
{
    if (A == B)
        return 1.0f;
    
    // Just use Jaro-Winkler for speed
    return CalculateJaroWinklerSimilarity(A, B);
}

int32 UFuzzyMatcher::CalculateLevenshteinDistance(const FString& A, const FString& B)
{
    const int32 LenA = A.Len();
    const int32 LenB = B.Len();
    
    if (LenA == 0) return LenB;
    if (LenB == 0) return LenA;
    
    // Use single array optimization for space efficiency
    TArray<int32> Current;
    Current.SetNum(LenB + 1);
    
    for (int32 j = 0; j <= LenB; j++)
        Current[j] = j;
    
    for (int32 i = 1; i <= LenA; i++)
    {
        int32 Previous = Current[0];
        Current[0] = i;
        
        for (int32 j = 1; j <= LenB; j++)
        {
            int32 Temp = Current[j];
            
            if (A[i-1] == B[j-1])
            {
                Current[j] = Previous;
            }
            else
            {
                Current[j] = FMath::Min3(
                    Current[j] + 1,      // Deletion
                    Current[j-1] + 1,    // Insertion
                    Previous + 1         // Substitution
                );
            }
            
            Previous = Temp;
        }
    }
    
    return Current[LenB];
}

int32 UFuzzyMatcher::CalculateDamerauLevenshteinDistance(const FString& A, const FString& B)
{
    const int32 LenA = A.Len();
    const int32 LenB = B.Len();
    
    if (LenA == 0) return LenB;
    if (LenB == 0) return LenA;
    
    // Create distance matrix
    TArray<TArray<int32>> D;
    D.SetNum(LenA + 1);
    for (int32 i = 0; i <= LenA; i++)
    {
        D[i].SetNum(LenB + 1);
        D[i][0] = i;
    }
    for (int32 j = 0; j <= LenB; j++)
    {
        D[0][j] = j;
    }
    
    // Calculate distance
    for (int32 i = 1; i <= LenA; i++)
    {
        for (int32 j = 1; j <= LenB; j++)
        {
            int32 Cost = (A[i-1] == B[j-1]) ? 0 : 1;
            
            D[i][j] = FMath::Min3(
                D[i-1][j] + 1,      // Deletion
                D[i][j-1] + 1,      // Insertion
                D[i-1][j-1] + Cost  // Substitution
            );
            
            // Transposition
            if (i > 1 && j > 1 && A[i-1] == B[j-2] && A[i-2] == B[j-1])
            {
                D[i][j] = FMath::Min(D[i][j], D[i-2][j-2] + Cost);
            }
        }
    }
    
    return D[LenA][LenB];
}

float UFuzzyMatcher::CalculateJaroSimilarity(const FString& A, const FString& B)
{
    if (A.IsEmpty() || B.IsEmpty())
        return 0.0f;
    
    const int32 LenA = A.Len();
    const int32 LenB = B.Len();
    
    const int32 MatchWindow = FMath::Max(LenA, LenB) / 2 - 1;
    if (MatchWindow < 0)
        return A[0] == B[0] ? 1.0f : 0.0f;
    
    TBitArray<> MatchesA(false, LenA);
    TBitArray<> MatchesB(false, LenB);
    
    int32 Matches = 0;
    
    // Find matches
    for (int32 i = 0; i < LenA; i++)
    {
        const int32 Start = FMath::Max(0, i - MatchWindow);
        const int32 End = FMath::Min(i + MatchWindow + 1, LenB);
        
        for (int32 j = Start; j < End; j++)
        {
            if (!MatchesB[j] && A[i] == B[j])
            {
                MatchesA[i] = true;
                MatchesB[j] = true;
                Matches++;
                break;
            }
        }
    }
    
    if (Matches == 0)
        return 0.0f;
    
    // Count transpositions
    int32 Transpositions = 0;
    int32 k = 0;
    
    for (int32 i = 0; i < LenA; i++)
    {
        if (!MatchesA[i])
            continue;
        
        while (!MatchesB[k])
            k++;
        
        if (A[i] != B[k])
            Transpositions++;
        
        k++;
    }
    
    return (static_cast<float>(Matches) / LenA +
            static_cast<float>(Matches) / LenB +
            static_cast<float>(Matches - Transpositions/2) / Matches) / 3.0f;
}

float UFuzzyMatcher::CalculateJaroWinklerSimilarity(const FString& A, const FString& B, float PrefixScale)
{

    // If PrefixScale is default (0.0f), use cached setting
    if (FMath::IsNearlyZero(PrefixScale))
    {
        if (GEngine)
        {
            if (UFuzzyMatcher* Matcher = GEngine->GetEngineSubsystem<UFuzzyMatcher>())
            {
                PrefixScale = Matcher->CachedSettings.JaroWinklerPrefixScale;
            }
        }
        else
        {
            PrefixScale = 0.1f; // Default fallback
        }
    }

    
    float JaroSim = CalculateJaroSimilarity(A, B);
    
    if (JaroSim < 0.6f)
        return JaroSim;
    
    // Find common prefix (up to 4 chars)
    int32 PrefixLen = 0;
    for (int32 i = 0; i < FMath::Min(FMath::Min(A.Len(), B.Len()), 4); i++)
    {
        if (A[i] == B[i])
            PrefixLen++;
        else
            break;
    }
    
    return JaroSim + PrefixLen * PrefixScale * (1.0f - JaroSim);
}

int32 UFuzzyMatcher::CalculateLongestCommonSubsequence(const FString& A, const FString& B)
{
    const int32 LenA = A.Len();
    const int32 LenB = B.Len();
    
    if (LenA == 0 || LenB == 0)
        return 0;
    
    // Dynamic programming table
    TArray<TArray<int32>> LCS;
    LCS.SetNum(LenA + 1);
    for (int32 i = 0; i <= LenA; i++)
    {
        LCS[i].SetNum(LenB + 1);
        LCS[i][0] = 0;
    }
    for (int32 j = 0; j <= LenB; j++)
    {
        LCS[0][j] = 0;
    }
    
    // Fill table
    for (int32 i = 1; i <= LenA; i++)
    {
        for (int32 j = 1; j <= LenB; j++)
        {
            if (A[i-1] == B[j-1])
            {
                LCS[i][j] = LCS[i-1][j-1] + 1;
            }
            else
            {
                LCS[i][j] = FMath::Max(LCS[i-1][j], LCS[i][j-1]);
            }
        }
    }
    
    return LCS[LenA][LenB];
}

int32 UFuzzyMatcher::CalculateLongestCommonSubstring(const FString& A, const FString& B)
{
    const int32 LenA = A.Len();
    const int32 LenB = B.Len();
    
    if (LenA == 0 || LenB == 0)
        return 0;
    
    int32 MaxLength = 0;
    TArray<TArray<int32>> LCSuff;
    LCSuff.SetNum(LenA + 1);
    
    for (int32 i = 0; i <= LenA; i++)
    {
        LCSuff[i].SetNum(LenB + 1);
        for (int32 j = 0; j <= LenB; j++)
        {
            if (i == 0 || j == 0)
            {
                LCSuff[i][j] = 0;
            }
            else if (A[i-1] == B[j-1])
            {
                LCSuff[i][j] = LCSuff[i-1][j-1] + 1;
                MaxLength = FMath::Max(MaxLength, LCSuff[i][j]);
            }
            else
            {
                LCSuff[i][j] = 0;
            }
        }
    }
    
    return MaxLength;
}

float UFuzzyMatcher::CompareSingleCharacters(const FString& CharA, const FString& CharB)
{
    if (CharA.Len() != 1 || CharB.Len() != 1)
    {
        return 0.0f;
    }
    
    TCHAR A = CharA[0];
    TCHAR B = CharB[0];
    
    FCharacterMatch Match = GEngine->GetEngineSubsystem<UFuzzyMatcher>()->CompareCharacters(A, B);
    
    // Return the exact similarity as the primary metric
    return Match.ExactSimilarity;
    
    // OR use weighted similarity with custom weights
    // return Match.GetWeightedSimilarity(0.5f, 0.2f, 0.1f, 0.1f, 0.1f);
}

float UFuzzyMatcher::GetCharacterKeyboardDistance(const FString& CharA, const FString& CharB)
{
    if (CharA.Len() != 1 || CharB.Len() != 1)
    {
        return 0.0f;
    }
    
    return GetKeyboardDistance(CharA[0], CharB[0]);
}

FNGramSet UFuzzyMatcher::GenerateNGrams(const FString& Source, int32 N)
{
    FNGramSet Set;
    Set.N = N;
    Set.Build(Source);
    return Set;
}

float UFuzzyMatcher::CompareNGramSets(const FNGramSet& A, const FNGramSet& B)
{
    // Use Jaccard as default
    return A.JaccardSimilarity(B);
}

FString UFuzzyMatcher::GenerateSoundex(const FString& Input)
{
    if (Input.IsEmpty())
        return TEXT("");
    
    // Check cache
    const uint32 Key = GetTypeHash(Input);
    
    {
        FScopeLock Lock(&PhoneticCacheMutex);
        if (const FString* Cached = SoundexCache.Find(Key))
        {
            return *Cached;
        }
    }
    
    FString Clean = Input.ToLower();
    FString Result;
    
    // Keep first character
    Result += Clean[0];
    
    // Process remaining characters
    TCHAR LastCode = TEXT('0');
    for (int32 i = 1; i < Clean.Len() && Result.Len() < 4; i++)
    {
        const TCHAR* Code = FuzzyMatchingData::SoundexMap.Find(Clean[i]);
        if (Code && *Code != LastCode)
        {
            Result += *Code;
            LastCode = *Code;
        }
    }
    
    // Pad with zeros
    while (Result.Len() < 4)
    {
        Result += TEXT('0');
    }
    
    {
        FScopeLock Lock(&PhoneticCacheMutex);
        const_cast<UFuzzyMatcher*>(this)->SoundexCache.Add(Key, Result);
    }
    
    return Result;
}

TArray<FString> UFuzzyMatcher::GenerateMetaphone(const FString& Input, bool bDoubleMetaphone)
{
    TArray<FString> Results;
    
    if (Input.IsEmpty())
        return Results;
    
    // Check cache
    const uint32 Key = GetTypeHash(Input);
    
    {
        FScopeLock Lock(&PhoneticCacheMutex);
        if (const TArray<FString>* Cached = MetaphoneCache.Find(Key))
        {
            return *Cached;
        }
    }
    
    // Simplified Metaphone implementation
    FString Primary, Secondary;
    FString Clean = Input.ToUpper();
    
    int32 Current = 0;
    const int32 Length = Clean.Len();
    
    // Handle initial letters
    if (Length > 1)
    {
        if (Clean[0] == TEXT('X'))
        {
            Primary += TEXT("S");
            Secondary += TEXT("S");
            Current = 1;
        }
        else if (Clean.Mid(0, 2) == TEXT("WH"))
        {
            Primary += TEXT("W");
            Secondary += TEXT("W");
            Current = 2;
        }
    }
    
    // Process remaining letters (simplified)
    while (Current < Length && Primary.Len() < 4)
    {
        TCHAR C = Clean[Current];
        
        switch (C)
        {
            case TEXT('A'):
            case TEXT('E'):
            case TEXT('I'):
            case TEXT('O'):
            case TEXT('U'):
            case TEXT('Y'):
                if (Current == 0)
                    Primary += TEXT("A");
                break;
                
            case TEXT('B'):
                Primary += TEXT("B");
                break;
                
            case TEXT('C'):
                if (Current + 1 < Length && Clean[Current + 1] == TEXT('H'))
                {
                    Primary += TEXT("X");
                    Current++;
                }
                else
                {
                    Primary += TEXT("K");
                }
                break;
                
            case TEXT('D'):
                Primary += TEXT("T");
                break;
                
            case TEXT('F'):
                Primary += TEXT("F");
                break;
                
            case TEXT('G'):
                if (Current + 1 < Length && Clean[Current + 1] == TEXT('H'))
                {
                    Current++;
                }
                else
                {
                    Primary += TEXT("K");
                }
                break;
                
            case TEXT('H'):
                if (Current > 0 && 
                    (Clean[Current-1] == TEXT('C') || 
                     Clean[Current-1] == TEXT('S') ||
                     Clean[Current-1] == TEXT('P') ||
                     Clean[Current-1] == TEXT('T')))
                {
                    // Silent H
                }
                else
                {
                    Primary += TEXT("H");
                }
                break;
                
            case TEXT('J'):
                Primary += TEXT("J");
                break;
                
            case TEXT('K'):
                if (Current == 0 || Clean[Current-1] != TEXT('C'))
                    Primary += TEXT("K");
                break;
                
            case TEXT('L'):
                Primary += TEXT("L");
                break;
                
            case TEXT('M'):
                Primary += TEXT("M");
                break;
                
            case TEXT('N'):
                Primary += TEXT("N");
                break;
                
            case TEXT('P'):
                if (Current + 1 < Length && Clean[Current + 1] == TEXT('H'))
                {
                    Primary += TEXT("F");
                    Current++;
                }
                else
                {
                    Primary += TEXT("P");
                }
                break;
                
            case TEXT('Q'):
                Primary += TEXT("K");
                break;
                
            case TEXT('R'):
                Primary += TEXT("R");
                break;
                
            case TEXT('S'):
                Primary += TEXT("S");
                break;
                
            case TEXT('T'):
                Primary += TEXT("T");
                break;
                
            case TEXT('V'):
                Primary += TEXT("F");
                break;
                
            case TEXT('W'):
                if (Current > 0)
                    Primary += TEXT("W");
                break;
                
            case TEXT('X'):
                Primary += TEXT("KS");
                break;
                
            case TEXT('Z'):
                Primary += TEXT("S");
                break;
        }
        
        Current++;
    }
    
    Results.Add(Primary);
    if (bDoubleMetaphone && !Secondary.IsEmpty() && Secondary != Primary)
    {
        Results.Add(Secondary);
    }
    
    {
        FScopeLock Lock(&PhoneticCacheMutex);
        const_cast<UFuzzyMatcher*>(this)->MetaphoneCache.Add(Key, Results);
    }
    
    return Results;
}

FString UFuzzyMatcher::GenerateNYSIIS(const FString& Input)
{
    if (Input.IsEmpty())
        return TEXT("");
    
    FString Result = Input.ToUpper();
    
    // Initial transformations
    if (Result.StartsWith(TEXT("MAC")))
        Result = TEXT("MCC") + Result.Mid(3);
    if (Result.StartsWith(TEXT("KN")))
        Result = TEXT("NN") + Result.Mid(2);
    if (Result.StartsWith(TEXT("K")))
        Result = TEXT("C") + Result.Mid(1);
    if (Result.StartsWith(TEXT("PH")) || Result.StartsWith(TEXT("PF")))
        Result = TEXT("FF") + Result.Mid(2);
    if (Result.StartsWith(TEXT("SCH")))
        Result = TEXT("SSS") + Result.Mid(3);
    
    // Ending transformations
    if (Result.EndsWith(TEXT("EE")) || Result.EndsWith(TEXT("IE")))
        Result = Result.Left(Result.Len() - 2) + TEXT("Y");
    if (Result.EndsWith(TEXT("DT")) || Result.EndsWith(TEXT("RT")) || 
        Result.EndsWith(TEXT("RD")) || Result.EndsWith(TEXT("NT")) || 
        Result.EndsWith(TEXT("ND")))
        Result = Result.Left(Result.Len() - 2) + TEXT("D");
    
    return Result;
}

bool UFuzzyMatcher::ArePhoneticallyEqual(const FString& A, const FString& B)
{
    if (A.IsEmpty() || B.IsEmpty())
        return false;
    
    // Check Soundex
    if (GenerateSoundex(A) == GenerateSoundex(B))
        return true;
    
    // Check Metaphone
    TArray<FString> MetaA = GenerateMetaphone(A, true);
    TArray<FString> MetaB = GenerateMetaphone(B, true);
    
    for (const FString& MA : MetaA)
    {
        if (MetaB.Contains(MA))
            return true;
    }
    
    return false;
}

TArray<FStringMatch> UFuzzyMatcher::CompareBatch(const TArray<FString>& Strings, bool bParallel)
{
    TArray<FStringMatch> Results;
    const int32 Count = Strings.Num();
    
    if (Count < 2)
        return Results;
    
    const int32 NumComparisons = (Count * (Count - 1)) / 2;
    Results.Reserve(NumComparisons);
    
    if (bParallel && CachedSettings.bUseMultithreading && ThreadPool && NumComparisons > 10) // Changed from Settings->
    {
        TArray<TFuture<FStringMatch>> Futures;
        
        for (int32 i = 0; i < Count - 1; i++)
        {
            for (int32 j = i + 1; j < Count; j++)
            {
                Futures.Add(Async(EAsyncExecution::ThreadPool, [this, &Strings, i, j]()
                {
                    return CompareStrings(Strings[i], Strings[j], true);
                }));
            }
        }
        
        // Wait and collect results
        for (TFuture<FStringMatch>& Future : Futures)
        {
            Results.Add(Future.Get());
        }
    }
    else
    {
        // Serial processing
        for (int32 i = 0; i < Count - 1; i++)
        {
            for (int32 j = i + 1; j < Count; j++)
            {
                Results.Add(CompareStrings(Strings[i], Strings[j], true));
            }
        }
    }
    
    return Results;
}

TArray<FString> UFuzzyMatcher::FindBestMatches(const FString& Query, const TArray<FString>& Candidates, 
                                               int32 MaxResults, float MinSimilarity)
{
    struct FMatchResult
    {
        FString Candidate;
        float Score;
        
        bool operator<(const FMatchResult& Other) const
        {
            return Score > Other.Score;
        }
    };
    
    TArray<FMatchResult> Matches;
    
    for (const FString& Candidate : Candidates)
    {
        FStringMatch Match = CompareStrings(Query, Candidate, true);
        float Score = Match.GetBestSimilarity();
        
        if (Score >= MinSimilarity)
        {
            FMatchResult Result;
            Result.Candidate = Candidate;
            Result.Score = Score;
            Matches.Add(Result);
        }
    }
    
    Matches.Sort();
    
    TArray<FString> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, Matches.Num()); i++)
    {
        Results.Add(Matches[i].Candidate);
    }
    
    return Results;
}

void UFuzzyMatcher::ClearCache()
{
    {
        FScopeLock Lock(&CharacterCacheMutex);
        CharacterCache.Empty();
    }
    
    if (StringCache)
    {
        StringCache->Clear();
    }
    
    {
        FScopeLock Lock(&PhoneticCacheMutex);
        SoundexCache.Empty();
        MetaphoneCache.Empty();
    }
    
    CacheHits.Reset();
    CacheMisses.Reset();
    TotalComputations.Reset();
    
    UE_LOG(LogFuzzyCore, Log, TEXT("All caches cleared"));
}

void UFuzzyMatcher::GetCacheStatistics(int32& OutHits, int32& OutMisses, int32& OutSize) const
{
    OutHits = CacheHits.GetValue();
    OutMisses = CacheMisses.GetValue();
    
    OutSize = CharacterCache.Num();
    if (StringCache)
    {
        OutSize += StringCache->Num();
    }
    OutSize += SoundexCache.Num() + MetaphoneCache.Num();
}

float UFuzzyMatcher::GetCacheHitRate() const
{
    const int32 Hits = CacheHits.GetValue();
    const int32 Misses = CacheMisses.GetValue();
    const int32 Total = Hits + Misses;
    
    return Total > 0 ? static_cast<float>(Hits) / Total : 0.0f;
}

void UFuzzyMatcher::PrewarmCache(const TArray<FString>& CommonStrings)
{
    const int32 Count = CommonStrings.Num();
    
    for (int32 i = 0; i < Count - 1; i++)
    {
        for (int32 j = i + 1; j < Count; j++)
        {
            CompareStrings(CommonStrings[i], CommonStrings[j], true);
        }
    }
    
    UE_LOG(LogFuzzyCore, Log, TEXT("Prewarmed cache with %d string comparisons"), 
           (Count * (Count - 1)) / 2);
}

void UFuzzyMatcher::TrimCache()
{
    // String cache handles its own trimming via LRU
    // Trim character cache if too large
    const int32 MaxCharCacheSize = 10000;
    
    if (CharacterCache.Num() > MaxCharCacheSize)
    {
        FScopeLock Lock(&CharacterCacheMutex);
        
        // Simple trimming - remove half
        const int32 ToRemove = CharacterCache.Num() / 2;
        int32 Removed = 0;
        
        for (auto It = CharacterCache.CreateIterator(); It && Removed < ToRemove; ++It)
        {
            It.RemoveCurrent();
            Removed++;
        }
    }
}

uint32 UFuzzyMatcher::GetCacheKey(const FString& A, const FString& B)
{
    // Order-independent hash
    const uint32 HashA = GetTypeHash(A);
    const uint32 HashB = GetTypeHash(B);
    return HashA < HashB ? HashCombine(HashA, HashB) : HashCombine(HashB, HashA);
}

uint32 UFuzzyMatcher::GetCacheKey(TCHAR A, TCHAR B)
{
    return A < B ? (static_cast<uint32>(A) << 16) | B : (static_cast<uint32>(B) << 16) | A;
}
#endif