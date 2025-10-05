// Editor/MMSpeedAnalysisWidget.cpp
#include "Editor/MMSpeedAnalysisWidget.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"

void UMMSpeedAnalysisWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Initialize default configuration
    InitializeDefaults();
    
    // Create speed analyzer
    if (!SpeedAnalyzer)
    {
        SpeedAnalyzer = NewObject<UMMPoseSearchSpeedAnalyzer>(this);
        SpeedAnalyzer->Initialize();
    }
}

void UMMSpeedAnalysisWidget::InitializeDefaults()
{
    // Set up default analysis configuration
    AnalysisConfig.MaxClusters = 10;
    AnalysisConfig.MinAnimationsPerCluster = 2;
    AnalysisConfig.ClusterMergeThreshold = 75.0f;
    AnalysisConfig.SemanticWeight = 0.4f;
    AnalysisConfig.bVerboseLogging = true;
    AnalysisConfig.bEnableCaching = true;
}

void UMMSpeedAnalysisWidget::RunAnalysis()
{
    if (!DatabaseToAnalyze)
    {
        LastAnalysisStatus = TEXT("ERROR: No database selected");
        UE_LOG(LogTemp, Error, TEXT("No database selected for analysis"));
        return;
    }
    
    if (!SpeedAnalyzer)
    {
        SpeedAnalyzer = NewObject<UMMPoseSearchSpeedAnalyzer>(this);
        SpeedAnalyzer->Initialize();
    }
    
    LastAnalysisStatus = TEXT("Running analysis...");
    
    // Generate clusters
    const double StartTime = FPlatformTime::Seconds();
    
    LastAnalysisResults = SpeedAnalyzer->GenerateSemanticSpeedClusters(
        DatabaseToAnalyze,
        AnalysisConfig
    );
    
    const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
    
    NumClustersGenerated = LastAnalysisResults.Num();
    
    if (NumClustersGenerated > 0)
    {
        LastAnalysisStatus = FString::Printf(
            TEXT("SUCCESS: Generated %d clusters in %.2fms"),
            NumClustersGenerated,
            ElapsedTime * 1000.0
        );
        
        // Log summary
        UE_LOG(LogTemp, Warning, TEXT("=== Speed Cluster Analysis Complete ==="));
        UE_LOG(LogTemp, Warning, TEXT("Database: %s"), *DatabaseToAnalyze->GetName());
        UE_LOG(LogTemp, Warning, TEXT("Clusters: %d"), NumClustersGenerated);
        UE_LOG(LogTemp, Warning, TEXT("Time: %.2fms"), ElapsedTime * 1000.0);
        UE_LOG(LogTemp, Warning, TEXT(""));
        
        for (const FSemanticSpeedCluster& Cluster : LastAnalysisResults)
        {
            LogClusterSummary(Cluster);
        }
        
        // Generate reports
        const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        const FString BaseFilename = FString::Printf(TEXT("%s_SpeedAnalysis_%s"),
            *DatabaseToAnalyze->GetName(), *Timestamp);
        
        // Create output directory
        const FString FullOutputDir = FPaths::ProjectDir() / OutputDirectory;
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*FullOutputDir))
        {
            PlatformFile.CreateDirectoryTree(*FullOutputDir);
        }
        
        // Generate CSV
        LastCSVPath = FullOutputDir / (BaseFilename + TEXT(".csv"));
        const FString CSVContent = GenerateCSVContent(LastAnalysisResults);
        if (FFileHelper::SaveStringToFile(CSVContent, *LastCSVPath))
        {
            UE_LOG(LogTemp, Log, TEXT("CSV saved: %s"), *LastCSVPath);
        }
        
        // Generate HTML
        LastHTMLPath = FullOutputDir / (BaseFilename + TEXT(".html"));
        const FString HTMLContent = GenerateHTMLContent(LastAnalysisResults);
        if (FFileHelper::SaveStringToFile(HTMLContent, *LastHTMLPath))
        {
            UE_LOG(LogTemp, Log, TEXT("HTML saved: %s"), *LastHTMLPath);
        }
        
        // Auto-open if enabled
        if (bAutoOpenResults)
        {
            FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastHTMLPath);
        }
    }
    else
    {
        LastAnalysisStatus = TEXT("ERROR: No clusters generated");
        UE_LOG(LogTemp, Error, TEXT("Failed to generate clusters"));
    }
}

void UMMSpeedAnalysisWidget::ClearResults()
{
    LastAnalysisResults.Empty();
    NumClustersGenerated = 0;
    LastAnalysisStatus = TEXT("Results cleared");
    LastCSVPath.Empty();
    LastHTMLPath.Empty();
}

void UMMSpeedAnalysisWidget::OpenOutputFolder()
{
    const FString FullOutputDir = FPaths::ProjectDir() / OutputDirectory;
    FPlatformProcess::ExploreFolder(*FullOutputDir);
}

void UMMSpeedAnalysisWidget::OpenLastCSV()
{
    if (!LastCSVPath.IsEmpty() && FPaths::FileExists(LastCSVPath))
    {
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastCSVPath);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No CSV file to open"));
    }
}

void UMMSpeedAnalysisWidget::OpenLastHTML()
{
    if (!LastHTMLPath.IsEmpty() && FPaths::FileExists(LastHTMLPath))
    {
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*LastHTMLPath);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No HTML file to open"));
    }
}

void UMMSpeedAnalysisWidget::LogClusterSummary(const FSemanticSpeedCluster& Cluster)
{
    UE_LOG(LogTemp, Log, TEXT("  %s:"), *Cluster.ClusterName);
    UE_LOG(LogTemp, Log, TEXT("    Speed: %.1f cm/s (%.1f - %.1f)"),
        Cluster.CenterSpeed, Cluster.MinSpeed, Cluster.MaxSpeed);
    UE_LOG(LogTemp, Log, TEXT("    Animations: %d"), Cluster.AnimationCount);
    UE_LOG(LogTemp, Log, TEXT("    Intent: %s, Level: %s"),
        *UEnum::GetValueAsString(Cluster.PrimaryIntent),
        *UEnum::GetValueAsString(Cluster.SpeedLevel));
    UE_LOG(LogTemp, Log, TEXT("    Hysteresis: %.1f cm/s (Entry=%.1f, Exit=%.1f)"),
        Cluster.HysteresisWidth, Cluster.EntryThreshold, Cluster.ExitThreshold);
}

FString UMMSpeedAnalysisWidget::GenerateCSVContent(const TArray<FSemanticSpeedCluster>& Clusters)
{
    FString CSV = TEXT("Cluster Name,Intent,Speed Level,Animation Count,Min Speed,Max Speed,Center Speed,Std Dev,Entry Threshold,Exit Threshold,Animation List\n");
    
    for (const FSemanticSpeedCluster& Cluster : Clusters)
    {
        // Build animation list from profiles
        FString AnimList;
        for (int32 i = 0; i < Cluster.AnimationProfiles.Num(); ++i)
        {
            AnimList += Cluster.AnimationProfiles[i].AnimationName;
            if (i < Cluster.AnimationProfiles.Num() - 1)
            {
                AnimList += TEXT(";");
            }
        }
        
        CSV += FString::Printf(TEXT("%s,%s,%s,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,\"%s\"\n"),
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
    FString HTML = TEXT("<!DOCTYPE html>\n<html>\n<head>\n");
    HTML += TEXT("<title>Speed Cluster Analysis</title>\n");
    HTML += TEXT("<style>\n");
    HTML += TEXT("body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }\n");
    HTML += TEXT("h1 { color: #333; }\n");
    HTML += TEXT("table { width: 100%; border-collapse: collapse; background: white; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n");
    HTML += TEXT("th { background: #4CAF50; color: white; padding: 12px; text-align: left; }\n");
    HTML += TEXT("td { padding: 10px; border-bottom: 1px solid #ddd; }\n");
    HTML += TEXT("tr:hover { background: #f9f9f9; }\n");
    HTML += TEXT(".cluster-name { font-weight: bold; color: #2196F3; }\n");
    HTML += TEXT(".stats { color: #666; font-size: 0.9em; }\n");
    HTML += TEXT("</style>\n");
    HTML += TEXT("</head>\n<body>\n");
    
    HTML += FString::Printf(TEXT("<h1>Speed Cluster Analysis: %s</h1>\n"), 
        *DatabaseToAnalyze->GetName());
    HTML += FString::Printf(TEXT("<p>Generated %d clusters | %s</p>\n"), 
        Clusters.Num(), 
        *FDateTime::Now().ToString());
    
    HTML += TEXT("<table>\n");
    HTML += TEXT("<tr><th>Cluster</th><th>Intent</th><th>Speed Level</th><th>Speed Range</th><th>Animations</th><th>Details</th></tr>\n");
    
    for (const FSemanticSpeedCluster& Cluster : Clusters)
    {
        HTML += TEXT("<tr>\n");
        HTML += FString::Printf(TEXT("<td class='cluster-name'>%s</td>\n"), *Cluster.ClusterName);
        HTML += FString::Printf(TEXT("<td>%s</td>\n"), *UEnum::GetValueAsString(Cluster.PrimaryIntent));
        HTML += FString::Printf(TEXT("<td>%s</td>\n"), *UEnum::GetValueAsString(Cluster.SpeedLevel));
        HTML += FString::Printf(TEXT("<td>%.1f - %.1f cm/s<br/><span class='stats'>Center: %.1f</span></td>\n"), 
            Cluster.MinSpeed, Cluster.MaxSpeed, Cluster.CenterSpeed);
        HTML += FString::Printf(TEXT("<td>%d</td>\n"), Cluster.AnimationCount);
        HTML += FString::Printf(TEXT("<td class='stats'>σ: %.1f<br/>Entry: %.1f<br/>Exit: %.1f</td>\n"),
            Cluster.StandardDeviation, Cluster.EntryThreshold, Cluster.ExitThreshold);
        HTML += TEXT("</tr>\n");
    }
    
    HTML += TEXT("</table>\n");
    HTML += TEXT("</body>\n</html>");
    
    return HTML;
}