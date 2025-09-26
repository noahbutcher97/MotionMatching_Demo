#pragma once
#if 0
#include "CoreMinimal.h"
#include "Editor/Blutility/Classes/EditorUtilityWidget.h"
#include "PoseSearch/MMPoseSearchSpeedAnalyzer.h"
#include "MMSpeedAnalysisWidget.generated.h"

/**
 * Editor widget for analyzing speed clusters
 */
UCLASS()
class MOTIONMATCHING_DEMO_API UMMSpeedAnalysisWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    // ========== Configuration ==========
    
    /** Database to analyze */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration", meta=(DisplayName="Database"))
    class UPoseSearchDatabase* DatabaseToAnalyze;
    
    /** Clustering configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration", meta=(ShowOnlyInnerProperties))
    FSemanticSpeedConfig AnalysisConfig;
    
    /** Output directory for reports */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
    FString OutputDirectory = TEXT("Saved/SpeedAnalysis");
    
    /** Auto-open files after generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
    bool bAutoOpenResults = true;
    
    // ========== Results Display ==========
    
    /** Last analysis results for display */
    UPROPERTY(BlueprintReadOnly, Category="Results")
    FString LastAnalysisStatus = TEXT("No analysis run yet");
    
    /** Number of clusters generated */
    UPROPERTY(BlueprintReadOnly, Category="Results")
    int32 NumClustersGenerated = 0;
    
    /** Path to last CSV file */
    UPROPERTY(BlueprintReadOnly, Category="Results")
    FString LastCSVPath;
    
    /** Path to last HTML file */
    UPROPERTY(BlueprintReadOnly, Category="Results")
    FString LastHTMLPath;
    
    // ========== Functions ==========
    
    /** Run the speed cluster analysis */
    UFUNCTION(BlueprintCallable, Category="Analysis", meta=(CallInEditor="true"))
    void RunAnalysis();
    
    /** Clear cached results */
    UFUNCTION(BlueprintCallable, Category="Analysis", meta=(CallInEditor="true"))
    void ClearResults();
    
    /** Open the output folder */
    UFUNCTION(BlueprintCallable, Category="Analysis", meta=(CallInEditor="true"))
    void OpenOutputFolder();
    
    /** Open last generated CSV */
    UFUNCTION(BlueprintCallable, Category="Analysis", meta=(CallInEditor="true"))
    void OpenLastCSV();
    
    /** Open last generated HTML */
    UFUNCTION(BlueprintCallable, Category="Analysis", meta=(CallInEditor="true"))
    void OpenLastHTML();
    
    /** Initialize default config */
    UFUNCTION(BlueprintCallable, Category="Configuration")
    void InitializeDefaults();
    
protected:
    virtual void NativeConstruct() override;
    
private:
    UPROPERTY()
    class UMMPoseSearchSpeedAnalyzer* SpeedAnalyzer;
    
    UPROPERTY()
    class UMMSemanticSubsystem* SemanticSubsystem;
    
    TArray<FSemanticSpeedCluster> LastAnalysisResults;

    static FString GenerateCSVContent(const TArray<FSemanticSpeedCluster>& Clusters);
    FString GenerateHTMLContent(const TArray<FSemanticSpeedCluster>& Clusters);
    static void LogClusterSummary(const FSemanticSpeedCluster& Cluster);
};

#endif

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Engine/DataTable.h"
#include "Semantic/Data/RESemanticTypes.h"
#include "MMSpeedAnalysisWidget.generated.h"

// Forward declarations
class UMMSemanticEngineCore;
class UMMFuzzyMatcher;
class UMMTokenizer;
class UMMMotionSemanticAnalyzer;

/**
 * Speed analysis entry
 */
USTRUCT(BlueprintType)
struct FSpeedAnalysisEntry
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString AnimationName;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float DetectedSpeed = 0.0f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString SpeedCategory;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float Confidence = 0.0f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FString> DetectedKeywords;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString MotionType;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Direction;
};

/**
 * Speed analysis configuration
 */
USTRUCT(BlueprintType)
struct FSpeedAnalysisConfig
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float MinConfidenceThreshold = 0.5f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bUsePatternMatching = true;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bAnalyzeVariants = true;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 MaxResultsPerCategory = 10;
};

/**
 * Speed Analysis Widget using Plugin Systems
 * Analyzes animation speeds using semantic engine
 */
UCLASS()
class MOTIONMATCHING_DEMO_API UMMSpeedAnalysisWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()
    
protected:
    // ========== COMPONENTS ==========
    
    UPROPERTY()
    URECore* SemanticEngine;
    
    UPROPERTY()
    UREFuzzy* FuzzyMatcher;
    
    UPROPERTY()
    URETokenizer* Tokenizer;
    
    UPROPERTY()
    UMMMotionSemanticAnalyzer* MotionAnalyzer;
    
    // ========== CONFIGURATION ==========
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Configuration")
    FSpeedAnalysisConfig AnalysisConfig;
    
    // ========== RESULTS ==========
    
    UPROPERTY(BlueprintReadWrite, Category="Results")
    TArray<FSpeedAnalysisEntry> AnalysisResults;
    
    UPROPERTY(BlueprintReadWrite, Category="Results")
    FString LastAnalysisReport;
    
    UPROPERTY(BlueprintReadWrite, Category="Results")
    float LastAnalysisTime = 0.0f;
    
public:
    // ========== INITIALIZATION ==========
    
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    
    /**
     * Initialize plugin components
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void InitializePluginComponents();
    
    /**
     * Check if widget is ready
     */
    UFUNCTION(BlueprintPure, Category="MM|Speed Analysis")
    bool IsInitialized() const;
    
    // ========== ANALYSIS OPERATIONS ==========
    
    /**
     * Analyze single animation name
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    FSpeedAnalysisEntry AnalyzeAnimationName(const FString& AnimationName);
    
    /**
     * Analyze batch of animation names
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void AnalyzeBatch(const TArray<FString>& AnimationNames);
    
    /**
     * Analyze from data table
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis", 
              meta=(CallInEditor="true"))
    void AnalyzeDataTable(UDataTable* AnimationDataTable);
    
    /**
     * Analyze all animations in directory
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis",
              meta=(CallInEditor="true"))
    void AnalyzeAnimationsInDirectory(const FString& DirectoryPath);
    
    // ========== FUZZY MATCHING ==========
    
    /**
     * Find similar animation names
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    TArray<FString> FindSimilarAnimations(const FString& QueryAnimation, 
                                          const TArray<FString>& CandidateAnimations,
                                          float MinSimilarity = 0.7f);
    
    /**
     * Compare two animation names
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    float CompareAnimationNames(const FString& AnimA, const FString& AnimB);
    
    // ========== REPORTING ==========
    
    /**
     * Generate analysis report
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    FString GenerateAnalysisReport();
    
    /**
     * Export results to CSV
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis",
              meta=(CallInEditor="true"))
    void ExportResultsToCSV(const FString& FilePath);
    
    /**
     * Clear all results
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    void ClearResults();
    
    // ========== STATISTICS ==========
    
    /**
     * Get average confidence
     */
    UFUNCTION(BlueprintPure, Category="MM|Speed Analysis")
    float GetAverageConfidence() const;
    
    /**
     * Get speed distribution
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    TMap<FString, int32> GetSpeedDistribution() const;
    
    /**
     * Get motion type distribution
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis")
    TMap<FString, int32> GetMotionTypeDistribution() const;
    
    // ========== TESTING ==========
    
    /**
     * Test semantic engine components
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis|Testing",
              meta=(CallInEditor="true"))
    void TestSemanticEngine();
    
    /**
     * Test fuzzy matching
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis|Testing",
              meta=(CallInEditor="true"))
    void TestFuzzyMatching();
    
    /**
     * Test tokenization
     */
    UFUNCTION(BlueprintCallable, Category="MM|Speed Analysis|Testing",
              meta=(CallInEditor="true"))
    void TestTokenization();
    
private:
    /**
     * Process single result
     */
    void ProcessAnalysisResult(const FSpeedAnalysisEntry& Entry);
    
    /**
     * Update statistics
     */
    void UpdateStatistics();
};