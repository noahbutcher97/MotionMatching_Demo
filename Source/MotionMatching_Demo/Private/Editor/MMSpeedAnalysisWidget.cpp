#if 0
#include "Editor/MMSpeedAnalysisWidget.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Core/MMSemanticSubsystem.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Editor.h"

void UMMSpeedAnalysisWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeDefaults();
}

void UMMSpeedAnalysisWidget::InitializeDefaults()
{
    // Set default config values
    AnalysisConfig.MinAnimationsPerCluster = 3;
    AnalysisConfig.MaxClusters = 20;
    AnalysisConfig.ClusterSplitThreshold = 100.0f;
    AnalysisConfig.ClusterMergeThreshold = 50.0f;
    AnalysisConfig.SemanticWeight = 0.4f;
    AnalysisConfig.SpeedWeight = 0.6f;
    AnalysisConfig.SpeedCurveSampleRate = 30;
    AnalysisConfig.bEnableCaching = true;
    AnalysisConfig.MaxCacheAge = 300.0f;
    AnalysisConfig.bDebugDraw = false;
    AnalysisConfig.bVerboseLogging = true;
}

void UMMSpeedAnalysisWidget::RunAnalysis()
{
    if (!DatabaseToAnalyze)
    {
        LastAnalysisStatus = TEXT("ERROR: No database selected!");
        UE_LOG(LogTemp, Error, TEXT("MMSpeedAnalysis: No database selected"));
        return;
    }
    
    LastAnalysisStatus = TEXT("Running analysis...");
    
    // Initialize analyzer if needed
    if (!SpeedAnalyzer)
    {
        SpeedAnalyzer = NewObject<UMMPoseSearchSpeedAnalyzer>(this);
    }
    
    // Get semantic subsystem directly from engine
    SemanticSubsystem = GEngine->GetEngineSubsystem<UMMSemanticSubsystem>();
    
    if (!SemanticSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("Semantic subsystem not available - will use speed-only clustering"));
        LastAnalysisStatus = TEXT("WARNING: Running without semantic analysis (speed-based only)");
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Found semantic subsystem - full semantic analysis enabled"));
    }
    
    SpeedAnalyzer->Initialize(SemanticSubsystem);
    
    // Log analysis start
    UE_LOG(LogTemp, Display, TEXT("=== SPEED CLUSTER ANALYSIS ==="));
    UE_LOG(LogTemp, Display, TEXT("Database: %s"), *DatabaseToAnalyze->GetName());
    UE_LOG(LogTemp, Display, TEXT("Animations: %d"), DatabaseToAnalyze->GetNumAnimationAssets());
    
    // Generate clusters
    LastAnalysisResults = SpeedAnalyzer->GenerateSemanticSpeedClusters(DatabaseToAnalyze, AnalysisConfig);
    NumClustersGenerated = LastAnalysisResults.Num();
    
    UE_LOG(LogTemp, Display, TEXT("Generated %d clusters"), NumClustersGenerated);
    
    if (NumClustersGenerated == 0)
    {
        LastAnalysisStatus = TEXT("ERROR: No clusters generated! Check Output Log");
        UE_LOG(LogTemp, Error, TEXT("No clusters were generated. Check configuration and database."));
        return;
    }
    
    // Log each cluster
    for (int32 i = 0; i < LastAnalysisResults.Num(); ++i)
    {
        UE_LOG(LogTemp, Display, TEXT("\nCluster %d: %s"), i, *LastAnalysisResults[i].ClusterName);
        LogClusterSummary(LastAnalysisResults[i]);
    }
    
    // Generate reports
    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    
    // Save CSV
    FString CSVContent = GenerateCSVContent(LastAnalysisResults);
    FString CSVFilename = FString::Printf(TEXT("%s/SpeedClusters_%s_%s.csv"), 
        *OutputDirectory, *DatabaseToAnalyze->GetName(), *Timestamp);
    LastCSVPath = FPaths::ProjectDir() + CSVFilename;
    
    // Ensure directory exists
    FString FullOutputDir = FPaths::ProjectDir() + OutputDirectory;
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*FullOutputDir))
    {
        PlatformFile.CreateDirectoryTree(*FullOutputDir);
    }
    
    if (FFileHelper::SaveStringToFile(CSVContent, *LastCSVPath))
    {
        UE_LOG(LogTemp, Display, TEXT("CSV saved to: %s"), *LastCSVPath);
        
        if (bAutoOpenResults)
        {
            FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastCSVPath);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save CSV to: %s"), *LastCSVPath);
    }
    
    // Save HTML
    FString HTMLContent = GenerateHTMLContent(LastAnalysisResults);
    FString HTMLFilename = FString::Printf(TEXT("%s/SpeedAnalysis_%s_%s.html"), 
        *OutputDirectory, *DatabaseToAnalyze->GetName(), *Timestamp);
    LastHTMLPath = FPaths::ProjectDir() + HTMLFilename;
    
    if (FFileHelper::SaveStringToFile(HTMLContent, *LastHTMLPath))
    {
        UE_LOG(LogTemp, Display, TEXT("HTML saved to: %s"), *LastHTMLPath);
        
        if (bAutoOpenResults)
        {
            FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastHTMLPath);
        }
    }
    
    LastAnalysisStatus = FString::Printf(TEXT("Success! Generated %d clusters"), NumClustersGenerated);
}
void UMMSpeedAnalysisWidget::ClearResults()
{
    LastAnalysisResults.Empty();
    NumClustersGenerated = 0;
    LastAnalysisStatus = TEXT("Results cleared");
    LastCSVPath.Empty();
    LastHTMLPath.Empty();
    
    if (SpeedAnalyzer)
    {
        SpeedAnalyzer->ClearCache();
    }
}

void UMMSpeedAnalysisWidget::OpenOutputFolder()
{
    FString FullPath = FPaths::ProjectDir() + OutputDirectory;
    FPlatformProcess::ExploreFolder(*FullPath);
}

void UMMSpeedAnalysisWidget::OpenLastCSV()
{
    if (!LastCSVPath.IsEmpty() && FPaths::FileExists(LastCSVPath))
    {
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastCSVPath);
    }
}

void UMMSpeedAnalysisWidget::OpenLastHTML()
{
    if (!LastHTMLPath.IsEmpty() && FPaths::FileExists(LastHTMLPath))
    {
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastHTMLPath);
    }
}

void UMMSpeedAnalysisWidget::LogClusterSummary(const FSemanticSpeedCluster& Cluster)
{
    UE_LOG(LogTemp, Display, TEXT("  Intent: %s, Speed: %s"), 
        *UEnum::GetValueAsString(Cluster.PrimaryIntent),
        *UEnum::GetValueAsString(Cluster.SpeedLevel));
    UE_LOG(LogTemp, Display, TEXT("  Animations: %d"), Cluster.AnimationCount);
    UE_LOG(LogTemp, Display, TEXT("  Speed: %.1f ± %.1f cm/s"), 
        Cluster.CenterSpeed, Cluster.StandardDeviation);
    UE_LOG(LogTemp, Display, TEXT("  Range: [%.1f - %.1f]"), 
        Cluster.EntryThreshold, Cluster.ExitThreshold);
}

FString UMMSpeedAnalysisWidget::GenerateCSVContent(const TArray<FSemanticSpeedCluster>& Clusters)
{
    FString CSV = TEXT("Index,Name,Intent,SpeedLevel,AnimCount,MinSpeed,MaxSpeed,CenterSpeed,StdDev,Entry,Exit,Animations\n");
    
    for (int32 i = 0; i < Clusters.Num(); ++i)
    {
        const FSemanticSpeedCluster& Cluster = Clusters[i];
        
        FString AnimList;
        for (const FSemanticSpeedProfile& Profile : Cluster.AnimationProfiles)
        {
            if (!AnimList.IsEmpty()) AnimList += TEXT(";");
            AnimList += Profile.AnimationName;
        }
        
        CSV += FString::Printf(TEXT("%d,%s,%s,%s,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,\"%s\"\n"),
            i,
            *Cluster.ClusterName,
            *UEnum::GetValueAsString(Cluster.PrimaryIntent),
            *UEnum::GetValueAsString(Cluster.SpeedLevel),
            Cluster.AnimationCount,
            Cluster.MinSpeed,
            Cluster.MaxSpeed,
            Cluster.CenterSpeed,
            Cluster.StandardDeviation,
            Cluster.EntryThreshold,
            Cluster.ExitThreshold,
            *AnimList);
    }
    
    return CSV;
}

FString UMMSpeedAnalysisWidget::GenerateHTMLContent(const TArray<FSemanticSpeedCluster>& Clusters)
{
    // Simple HTML report
    FString HTML = TEXT("<!DOCTYPE html><html><head><title>Speed Analysis</title></head><body>");
    HTML += FString::Printf(TEXT("<h1>Speed Cluster Analysis</h1><p>Database: %s</p>"), 
        *DatabaseToAnalyze->GetName());
    HTML += FString::Printf(TEXT("<p>Generated %d clusters</p>"), Clusters.Num());
    
    HTML += TEXT("<table border='1'><tr><th>Cluster</th><th>Intent</th><th>Speed</th><th>Animations</th></tr>");
    
    for (const FSemanticSpeedCluster& Cluster : Clusters)
    {
        HTML += FString::Printf(TEXT("<tr><td>%s</td><td>%s</td><td>%.1f</td><td>%d</td></tr>"),
            *Cluster.ClusterName,
            *UEnum::GetValueAsString(Cluster.PrimaryIntent),
            Cluster.CenterSpeed,
            Cluster.AnimationCount);
    }
    
    HTML += TEXT("</table></body></html>");
    return HTML;
}
#endif

#include "Editor/MMSpeedAnalysisWidget.h"
#include "Core/MMSemanticEngineCore.h"
#include "Core/MMFuzzyMatcher.h"
#include "Core/MMTokenizer.h"
#include "Core/MMMotionSemanticAnalyzer.h"
#include "Data/MMStringTypes.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

void UMMSpeedAnalysisWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    InitializePluginComponents();
}

void UMMSpeedAnalysisWidget::NativeDestruct()
{
    // Cleanup if needed
    if (MotionAnalyzer)
    {
        MotionAnalyzer->ConditionalBeginDestroy();
        MotionAnalyzer = nullptr;
    }
    
    Super::NativeDestruct();
}

void UMMSpeedAnalysisWidget::InitializePluginComponents()
{
    // Get semantic engine
    SemanticEngine = UMMSemanticEngineCore::Get();
    if (!SemanticEngine)
    {
        UE_LOG(LogTemp, Error, TEXT("SpeedAnalysisWidget: Failed to get Semantic Engine"));
        return;
    }
    
    // Get components from engine
    FuzzyMatcher = SemanticEngine->GetFuzzyMatcher();
    Tokenizer = SemanticEngine->GetTokenizer();
    
    if (!FuzzyMatcher || !Tokenizer)
    {
        UE_LOG(LogTemp, Error, TEXT("SpeedAnalysisWidget: Failed to get required components"));
        return;
    }
    
    // Create motion analyzer
    MotionAnalyzer = NewObject<UMMMotionSemanticAnalyzer>(this);
    if (MotionAnalyzer)
    {
        MotionAnalyzer->Initialize();
    }
    
    UE_LOG(LogTemp, Log, TEXT("SpeedAnalysisWidget: Plugin components initialized"));
}

bool UMMSpeedAnalysisWidget::IsInitialized() const
{
    return SemanticEngine && FuzzyMatcher && Tokenizer && MotionAnalyzer;
}

// ========== ANALYSIS OPERATIONS ==========

FSpeedAnalysisEntry UMMSpeedAnalysisWidget::AnalyzeAnimationName(const FString& AnimationName)
{
    FSpeedAnalysisEntry Entry;
    Entry.AnimationName = AnimationName;
    
    if (!IsInitialized())
    {
        UE_LOG(LogTemp, Warning, TEXT("SpeedAnalysisWidget: Not initialized"));
        return Entry;
    }
    
    // Use motion analyzer to analyze the animation name
    FMotionSemanticResult Result = MotionAnalyzer->AnalyzeAnimationName(AnimationName);
    
    // Extract speed information
    FMotionSpeedResult SpeedResult = MotionAnalyzer->ExtractSpeed(AnimationName);
    
    // Fill entry
    Entry.DetectedSpeed = SpeedResult.EstimatedSpeed;
    Entry.SpeedCategory = SpeedResult.SpeedCategory;
    Entry.Confidence = Result.Confidence;
    Entry.DetectedKeywords = Result.DetectedKeywords;
    Entry.MotionType = UEnum::GetValueAsString(Result.MotionType);
    
    // Get direction
    FMotionDirectionResult DirResult = MotionAnalyzer->ExtractDirection(AnimationName);
    Entry.Direction = DirResult.DirectionName;
    
    return Entry;
}

void UMMSpeedAnalysisWidget::AnalyzeBatch(const TArray<FString>& AnimationNames)
{
    double StartTime = FPlatformTime::Seconds();
    
    AnalysisResults.Empty();
    AnalysisResults.Reserve(AnimationNames.Num());
    
    for (const FString& AnimName : AnimationNames)
    {
        FSpeedAnalysisEntry Entry = AnalyzeAnimationName(AnimName);
        
        // Only add results that meet confidence threshold
        if (Entry.Confidence >= AnalysisConfig.MinConfidenceThreshold)
        {
            AnalysisResults.Add(Entry);
        }
    }
    
    LastAnalysisTime = FPlatformTime::Seconds() - StartTime;
    UpdateStatistics();
    
    UE_LOG(LogTemp, Log, TEXT("Batch analysis complete: %d animations in %.2f seconds"),
        AnimationNames.Num(), LastAnalysisTime);
}

void UMMSpeedAnalysisWidget::AnalyzeDataTable(UDataTable* AnimationDataTable)
{
    if (!AnimationDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid data table"));
        return;
    }
    
    TArray<FString> AnimationNames;
    
    // Extract animation names from data table
    for (auto& Row : AnimationDataTable->GetRowMap())
    {
        AnimationNames.Add(Row.Key.ToString());
    }
    
    AnalyzeBatch(AnimationNames);
}

void UMMSpeedAnalysisWidget::AnalyzeAnimationsInDirectory(const FString& DirectoryPath)
{
    TArray<FString> AnimationFiles;
    IFileManager::Get().FindFilesRecursive(AnimationFiles, *DirectoryPath, 
        TEXT("*.uasset"), true, false);
    
    TArray<FString> AnimationNames;
    
    for (const FString& FilePath : AnimationFiles)
    {
        FString FileName = FPaths::GetBaseFilename(FilePath);
        
        // Filter for animation-like names
        if (FileName.Contains(TEXT("_")) && 
            (FileName.Contains(TEXT("Walk")) || 
             FileName.Contains(TEXT("Run")) || 
             FileName.Contains(TEXT("Sprint")) ||
             FileName.Contains(TEXT("Idle"))))
        {
            AnimationNames.Add(FileName);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Found %d animations in %s"), 
        AnimationNames.Num(), *DirectoryPath);
    
    AnalyzeBatch(AnimationNames);
}

// ========== FUZZY MATCHING ==========

TArray<FString> UMMSpeedAnalysisWidget::FindSimilarAnimations(const FString& QueryAnimation, 
                                                              const TArray<FString>& CandidateAnimations,
                                                              float MinSimilarity)
{
    if (!FuzzyMatcher)
    {
        return TArray<FString>();
    }
    
    return FuzzyMatcher->FindBestMatches(QueryAnimation, CandidateAnimations, 
        AnalysisConfig.MaxResultsPerCategory, MinSimilarity);
}

float UMMSpeedAnalysisWidget::CompareAnimationNames(const FString& AnimA, const FString& AnimB)
{
    if (!FuzzyMatcher)
    {
        return 0.0f;
    }
    
    FMMStringMatch Match = FuzzyMatcher->CompareStrings(AnimA, AnimB);
    return Match.GetBestSimilarity();
}

// ========== REPORTING ==========

FString UMMSpeedAnalysisWidget::GenerateAnalysisReport()
{
    FString Report = TEXT("=== Speed Analysis Report ===\n\n");
    
    Report += FString::Printf(TEXT("Total Animations Analyzed: %d\n"), AnalysisResults.Num());
    Report += FString::Printf(TEXT("Analysis Time: %.2f seconds\n"), LastAnalysisTime);
    Report += FString::Printf(TEXT("Average Confidence: %.2f%%\n\n"), GetAverageConfidence() * 100.0f);
    
    // Speed distribution
    Report += TEXT("Speed Distribution:\n");
    TMap<FString, int32> SpeedDist = GetSpeedDistribution();
    for (const auto& Pair : SpeedDist)
    {
        Report += FString::Printf(TEXT("  %s: %d animations\n"), *Pair.Key, Pair.Value);
    }
    
    // Motion type distribution  
    Report += TEXT("\nMotion Type Distribution:\n");
    TMap<FString, int32> MotionDist = GetMotionTypeDistribution();
    for (const auto& Pair : MotionDist)
    {
        Report += FString::Printf(TEXT("  %s: %d animations\n"), *Pair.Key, Pair.Value);
    }
    
    // Top keywords
    Report += TEXT("\nTop Detected Keywords:\n");
    TMap<FString, int32> KeywordCounts;
    for (const auto& Entry : AnalysisResults)
    {
        for (const FString& Keyword : Entry.DetectedKeywords)
        {
            KeywordCounts.FindOrAdd(Keyword)++;
        }
    }
    
    // Sort by count
    KeywordCounts.ValueSort([](int32 A, int32 B) { return A > B; });
    
    int32 Count = 0;
    for (const auto& Pair : KeywordCounts)
    {
        Report += FString::Printf(TEXT("  %s: %d occurrences\n"), *Pair.Key, Pair.Value);
        if (++Count >= 10) break;  // Top 10 only
    }
    
    LastAnalysisReport = Report;
    return Report;
}

void UMMSpeedAnalysisWidget::ExportResultsToCSV(const FString& FilePath)
{
    FString CSV = TEXT("Animation Name,Detected Speed,Speed Category,Confidence,Motion Type,Direction,Keywords\n");
    
    for (const auto& Entry : AnalysisResults)
    {
        FString KeywordsStr = FString::Join(Entry.DetectedKeywords, TEXT(";"));
        
        CSV += FString::Printf(TEXT("%s,%.1f,%s,%.2f,%s,%s,%s\n"),
            *Entry.AnimationName,
            Entry.DetectedSpeed,
            *Entry.SpeedCategory,
            Entry.Confidence,
            *Entry.MotionType,
            *Entry.Direction,
            *KeywordsStr
        );
    }
    
    if (FFileHelper::SaveStringToFile(CSV, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("Results exported to %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to export results to %s"), *FilePath);
    }
}

void UMMSpeedAnalysisWidget::ClearResults()
{
    AnalysisResults.Empty();
    LastAnalysisReport.Empty();
    LastAnalysisTime = 0.0f;
}

// ========== STATISTICS ==========

float UMMSpeedAnalysisWidget::GetAverageConfidence() const
{
    if (AnalysisResults.Num() == 0)
        return 0.0f;
    
    float Sum = 0.0f;
    for (const auto& Entry : AnalysisResults)
    {
        Sum += Entry.Confidence;
    }
    
    return Sum / AnalysisResults.Num();
}

TMap<FString, int32> UMMSpeedAnalysisWidget::GetSpeedDistribution() const
{
    TMap<FString, int32> Distribution;
    
    for (const auto& Entry : AnalysisResults)
    {
        Distribution.FindOrAdd(Entry.SpeedCategory)++;
    }
    
    return Distribution;
}

TMap<FString, int32> UMMSpeedAnalysisWidget::GetMotionTypeDistribution() const
{
    TMap<FString, int32> Distribution;
    
    for (const auto& Entry : AnalysisResults)
    {
        Distribution.FindOrAdd(Entry.MotionType)++;
    }
    
    return Distribution;
}

// ========== TESTING ==========

void UMMSpeedAnalysisWidget::TestSemanticEngine()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Testing Semantic Engine ==="));
    
    if (!IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("Engine not initialized"));
        return;
    }
    
    // Test engine status
    UE_LOG(LogTemp, Warning, TEXT("Engine Status:"));
    UE_LOG(LogTemp, Warning, TEXT("  Semantic Engine: %s"), 
        SemanticEngine ? TEXT("OK") : TEXT("FAILED"));
    UE_LOG(LogTemp, Warning, TEXT("  Fuzzy Matcher: %s"), 
        FuzzyMatcher ? TEXT("OK") : TEXT("FAILED"));
    UE_LOG(LogTemp, Warning, TEXT("  Tokenizer: %s"), 
        Tokenizer ? TEXT("OK") : TEXT("FAILED"));
    UE_LOG(LogTemp, Warning, TEXT("  Motion Analyzer: %s"), 
        MotionAnalyzer && MotionAnalyzer->IsInitialized() ? TEXT("OK") : TEXT("FAILED"));
    
    // Get performance stats
    FString Stats = SemanticEngine->GetPerformanceStats();
    UE_LOG(LogTemp, Warning, TEXT("\n%s"), *Stats);
}

void UMMSpeedAnalysisWidget::TestFuzzyMatching()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Testing Fuzzy Matching ==="));
    
    if (!FuzzyMatcher)
    {
        UE_LOG(LogTemp, Error, TEXT("Fuzzy Matcher not available"));
        return;
    }
    
    // Test cases
    TArray<TPair<FString, FString>> TestPairs = {
        {TEXT("MM_Walk_Forward_01"), TEXT("MM_Walk_Forward_02")},
        {TEXT("Walking"), TEXT("Walkng")},  // Typo
        {TEXT("Run"), TEXT("Sprint")},
        {TEXT("Idle_Stand"), TEXT("Stand_Idle")}
    };
    
    for (const auto& Pair : TestPairs)
    {
        FMMStringMatch Match = FuzzyMatcher->CompareStrings(Pair.Key, Pair.Value);
        
        UE_LOG(LogTemp, Warning, TEXT("\n'%s' vs '%s':"), *Pair.Key, *Pair.Value);
        UE_LOG(LogTemp, Warning, TEXT("  Levenshtein: %.2f"), Match.NormalizedLevenshtein);
        UE_LOG(LogTemp, Warning, TEXT("  Jaro-Winkler: %.2f"), Match.JaroWinklerSimilarity);
        UE_LOG(LogTemp, Warning, TEXT("  Dice: %.2f"), Match.DiceCoefficient);
        UE_LOG(LogTemp, Warning, TEXT("  Best: %.2f"), Match.GetBestSimilarity());
    }
}

void UMMSpeedAnalysisWidget::TestTokenization()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Testing Tokenization ==="));
    
    if (!Tokenizer)
    {
        UE_LOG(LogTemp, Error, TEXT("Tokenizer not available"));
        return;
    }
    
    // Test cases
    TArray<FString> TestStrings = {
        TEXT("MM_Walk_Forward_01"),
        TEXT("ALS_Sprint_Left_Start"),
        TEXT("walkForwardSlowly"),
        TEXT("Run-Jump-Land-Sequence")
    };
    
    for (const FString& TestStr : TestStrings)
    {
        FMMTokenStream Stream = Tokenizer->TokenizeAnimationName(TestStr);
        
        UE_LOG(LogTemp, Warning, TEXT("\nTokenizing '%s':"), *TestStr);
        UE_LOG(LogTemp, Warning, TEXT("  Token Count: %d"), Stream.Tokens.Num());
        
        for (const FMMFuzzyToken& Token : Stream.Tokens)
        {
            UE_LOG(LogTemp, Warning, TEXT("    [%s] '%s' (Weight: %.2f)"),
                *UEnum::GetValueAsString(Token.Type),
                *Token.Text,
                Token.Weight);
        }
    }
}

void UMMSpeedAnalysisWidget::ProcessAnalysisResult(const FSpeedAnalysisEntry& Entry)
{
    // Additional processing if needed
}

void UMMSpeedAnalysisWidget::UpdateStatistics()
{
    // Update any cached statistics
    LastAnalysisReport = GenerateAnalysisReport();
}

