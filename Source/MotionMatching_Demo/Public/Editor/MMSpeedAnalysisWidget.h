// Editor/MMSpeedAnalysisWidget.h
#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Data/MMSemanticTypes.h"
#include "PoseSearch/MMPoseSearchSpeedAnalyzer.h"
#include "MMSpeedAnalysisWidget.generated.h"

/**
 * Editor widget for analyzing and visualizing speed clusters
 * Uses the new semantic speed analysis system
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
    UMMPoseSearchSpeedAnalyzer* SpeedAnalyzer;
    
    TArray<FSemanticSpeedCluster> LastAnalysisResults;

    static FString GenerateCSVContent(const TArray<FSemanticSpeedCluster>& Clusters);
    FString GenerateHTMLContent(const TArray<FSemanticSpeedCluster>& Clusters);
    static void LogClusterSummary(const FSemanticSpeedCluster& Cluster);
};