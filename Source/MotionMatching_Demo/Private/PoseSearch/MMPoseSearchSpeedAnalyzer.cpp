#if 0
#include "PoseSearch/MMPoseSearchSpeedAnalyzer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMSpeedAnalysis, Log, All);

UMMPoseSearchSpeedAnalyzer::UMMPoseSearchSpeedAnalyzer()
{
}

void UMMPoseSearchSpeedAnalyzer::Initialize(UMMSemanticSubsystem* InSemanticSubsystem)
{
    SemanticSubsystem = InSemanticSubsystem;
    
    if (!SemanticSubsystem)
    {
        UE_LOG(LogMMSpeedAnalysis, Warning, TEXT("Initialized without semantic subsystem - will use speed-based clustering only"));
    }
    else
    {
        UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Speed Analyzer initialized with semantic subsystem"));
    }
}


void UMMPoseSearchSpeedAnalyzer::Shutdown()
{
    FScopeLock Lock(&CacheMutex);
    DatabaseProfiles.Empty();
    DatabaseClusters.Empty();
    SemanticSubsystem = nullptr;
}

int32 UMMPoseSearchSpeedAnalyzer::IndexDatabaseWithSemantics(
    UPoseSearchDatabase* Database,
    const FSemanticSpeedConfig& Config,
    bool bForceReindex)
{
    SCOPE_CYCLE_COUNTER(STAT_MMSpeedIndexing);
    
    if (!Database)
    {
        UE_LOG(LogMMSpeedAnalysis, Error, TEXT("Invalid database"));
        return 0;
    }
    
    const FObjectKey DatabaseKey(Database);
    
    FScopeLock Lock(&CacheMutex);
    
    // Check if already indexed
    if (!bForceReindex && DatabaseProfiles.Contains(DatabaseKey))
    {
        UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Database %s already indexed with %d profiles"),
            *Database->GetName(),
            DatabaseProfiles[DatabaseKey].Num());
        return DatabaseProfiles[DatabaseKey].Num();
    }
    
    // Clear existing profiles
    DatabaseProfiles.Remove(DatabaseKey);
    
    TMap<int32, FSemanticSpeedProfile>& Profiles = DatabaseProfiles.Add(DatabaseKey);
    int32 AnimationsIndexed = 0;
    
    const UE::PoseSearch::FSearchIndex& SearchIndex = Database->GetSearchIndex();
    
    // Map to track unique source assets
    TMap<int32, UAnimationAsset*> SourceAssetMap;
    
    const int32 NumAnimations = Database->GetNumAnimationAssets();
    
    for (int32 DbIndex = 0; DbIndex < NumAnimations; ++DbIndex)
    {
        if (const FPoseSearchDatabaseSequence* DbSequence = 
            Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseSequence>(DbIndex))
        {
            if (DbSequence->Sequence)
            {
                SourceAssetMap.Add(DbIndex, DbSequence->Sequence);
            }
        }
        else if (const FPoseSearchDatabaseAnimComposite* DbComposite = 
            Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimComposite>(DbIndex))
        {
            if (DbComposite->AnimComposite)
            {
                SourceAssetMap.Add(DbIndex, DbComposite->AnimComposite);
            }
        }
        else if (const FPoseSearchDatabaseBlendSpace* DbBlendSpace = 
            Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseBlendSpace>(DbIndex))
        {
            if (DbBlendSpace->BlendSpace)
            {
                SourceAssetMap.Add(DbIndex, DbBlendSpace->BlendSpace);
            }
        }
        else if (const FPoseSearchDatabaseAnimMontage* DbMontage = 
            Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimMontage>(DbIndex))
        {
            if (DbMontage->AnimMontage)
            {
                SourceAssetMap.Add(DbIndex, DbMontage->AnimMontage);
            }
        }
    }
    
    // Process unique source assets
    TSet<int32> ProcessedSourceAssets;
    
    for (int32 AssetIdx = 0; AssetIdx < SearchIndex.Assets.Num(); ++AssetIdx)
    {
        const UE::PoseSearch::FSearchIndexAsset& SearchAsset = SearchIndex.Assets[AssetIdx];
        const int32 SourceAssetIdx = SearchAsset.GetSourceAssetIdx();
        
        if (ProcessedSourceAssets.Contains(SourceAssetIdx))
        {
            continue;
        }
        
        UAnimationAsset** AnimAssetPtr = SourceAssetMap.Find(SourceAssetIdx);
        if (!AnimAssetPtr || !(*AnimAssetPtr))
        {
            continue;
        }
        
        UAnimationAsset* AnimAsset = *AnimAssetPtr;
        ProcessedSourceAssets.Add(SourceAssetIdx);
        
        // Analyze the animation
        FSemanticSpeedProfile Profile = AnalyzeAnimationSpeed(AnimAsset);
        
        if (Profile.IsValid())
        {
            Profile.DatabaseIndex = AssetIdx;
            Profile.LastUpdateTime = FPlatformTime::Seconds();
            
            // Handle semantic analysis
            if (SemanticSubsystem)
            {
                FMotionSemanticResult SemanticResult = SemanticSubsystem->ProcessQuery(Profile.AnimationName);
                Profile.SemanticAnalysis = SemanticResult;
                Profile.SemanticSpeedLevel = SemanticResult.Speed.SpeedLevel;
            }
            else
            {
                // Fallback: Determine speed level from raw speed
                if (Profile.AverageSpeed < 50.0f)
                {
                    Profile.SemanticSpeedLevel = EMotionSpeed::Idle;
                }
                else if (Profile.AverageSpeed < 200.0f)
                {
                    Profile.SemanticSpeedLevel = EMotionSpeed::Walk;
                }
                else if (Profile.AverageSpeed < 400.0f)
                {
                    Profile.SemanticSpeedLevel = EMotionSpeed::Jog;
                }
                else if (Profile.AverageSpeed < 600.0f)
                {
                    Profile.SemanticSpeedLevel = EMotionSpeed::Run;
                }
                else
                {
                    Profile.SemanticSpeedLevel = EMotionSpeed::Sprint;
                }
                
                // Set default semantic values with correct structure
                Profile.SemanticAnalysis.Intent.PrimaryIntent = EMotionIntent::Locomotion;
                Profile.SemanticAnalysis.Intent.SecondaryIntents.Empty();  // Clear the array
                Profile.SemanticAnalysis.Intent.IntentConfidences.Add(EMotionIntent::Locomotion, 0.75f);
                Profile.SemanticAnalysis.Speed.SpeedLevel = Profile.SemanticSpeedLevel;
                Profile.SemanticAnalysis.OverallConfidence = 0.5f;
            }
            
            // Calculate transition suitability scores
            for (int32 i = 0; i <= static_cast<int32>(EMotionSpeed::Sprint); ++i)
            {
                EMotionSpeed TargetSpeed = static_cast<EMotionSpeed>(i);
                Profile.TransitionSuitability.Add(TargetSpeed, 
                    CalculateTransitionScore(Profile.SemanticSpeedLevel, TargetSpeed));
            }
            
            Profiles.Add(AssetIdx, Profile);
            AnimationsIndexed++;
            
            if (Config.bVerboseLogging)
            {
                UE_LOG(LogMMSpeedAnalysis, Verbose, 
                    TEXT("Indexed %s: Avg Speed=%.1f, Semantic=%s"),
                    *Profile.AnimationName,
                    Profile.AverageSpeed,
                    *UEnum::GetValueAsString(Profile.SemanticSpeedLevel));
            }
        }
    }
    
    UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Indexed %d animations from database %s"),
        AnimationsIndexed, *Database->GetName());
    
    return AnimationsIndexed;
}

TArray<FSemanticSpeedCluster> UMMPoseSearchSpeedAnalyzer::GenerateSemanticSpeedClusters(
    UPoseSearchDatabase* Database,
    const FSemanticSpeedConfig& Config)
{
    SCOPE_CYCLE_COUNTER(STAT_MMGenerateClusters);
    
    if (!Database)
    {
        UE_LOG(LogMMSpeedAnalysis, Error, TEXT("Invalid database"));
        return TArray<FSemanticSpeedCluster>();
    }
    
    if (!SemanticSubsystem)
    {
        UE_LOG(LogMMSpeedAnalysis, Warning, TEXT("Running without semantic analysis - using speed-based clustering"));
    }
    
    // Ensure database is indexed
    if (IndexDatabaseWithSemantics(Database, Config, false) == 0)
    {
        UE_LOG(LogMMSpeedAnalysis, Warning, TEXT("No animations to cluster in database %s"),
            *Database->GetName());
        return TArray<FSemanticSpeedCluster>();
    }
    
    const FObjectKey DatabaseKey(Database);
    
    FScopeLock Lock(&CacheMutex);
    
    // Check cache
    if (Config.bEnableCaching && DatabaseClusters.Contains(DatabaseKey))
    {
        const TArray<FSemanticSpeedCluster>& CachedClusters = DatabaseClusters[DatabaseKey];
        if (CachedClusters.Num() > 0)
        {
            UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Returning %d cached clusters for %s"),
                CachedClusters.Num(), *Database->GetName());
            return CachedClusters;
        }
    }
    
    // Get all profiles
    const TMap<int32, FSemanticSpeedProfile>* ProfileMap = DatabaseProfiles.Find(DatabaseKey);
    if (!ProfileMap || ProfileMap->Num() == 0)
    {
        return TArray<FSemanticSpeedCluster>();
    }
    
    // Convert to array for clustering
    TArray<FSemanticSpeedProfile> AllProfiles;
    AllProfiles.Reserve(ProfileMap->Num());
    
    for (const auto& ProfilePair : *ProfileMap)
    {
        AllProfiles.Add(ProfilePair.Value);
    }
    
    // Perform hierarchical clustering
    TArray<FSemanticSpeedCluster> Clusters = PerformHierarchicalClustering(AllProfiles, Config);
    
    // Merge similar clusters
    MergeSimilarClusters(Clusters, Config);
    
    // Calculate final statistics and thresholds
    for (FSemanticSpeedCluster& Cluster : Clusters)
    {
        CalculateClusterStatistics(Cluster);
        DetermineSemanticThresholds(Cluster, Config);
    }
    
    // Sort by center speed
    Clusters.Sort([](const FSemanticSpeedCluster& A, const FSemanticSpeedCluster& B) {
        return A.CenterSpeed < B.CenterSpeed;
    });
    
    // Assign priorities based on semantic importance
    for (int32 i = 0; i < Clusters.Num(); ++i)
    {
        FSemanticSpeedCluster& Cluster = Clusters[i];
        
        // Higher priority for common movement types
        if (Cluster.PrimaryIntent == EMotionIntent::Locomotion)
        {
            if (Cluster.SpeedLevel == EMotionSpeed::Walk)
                Cluster.Priority = 10;
            else if (Cluster.SpeedLevel == EMotionSpeed::Run)
                Cluster.Priority = 9;
            else if (Cluster.SpeedLevel == EMotionSpeed::Jog)
                Cluster.Priority = 8;
        }
        else if (Cluster.PrimaryIntent == EMotionIntent::Idle)
        {
            Cluster.Priority = 7;
        }
        
        // Generate cluster name
        Cluster.ClusterName = FString::Printf(TEXT("%s_%s_%d"),
            *UEnum::GetValueAsString(Cluster.PrimaryIntent),
            *UEnum::GetValueAsString(Cluster.SpeedLevel),
            i);
    }
    
    // Cache the results
    if (Config.bEnableCaching)
    {
        DatabaseClusters.Add(DatabaseKey, Clusters);
    }
    
    UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Generated %d semantic speed clusters for %s"),
        Clusters.Num(), *Database->GetName());
    
    if (Config.bVerboseLogging)
    {
        for (const FSemanticSpeedCluster& Cluster : Clusters)
        {
            UE_LOG(LogMMSpeedAnalysis, Verbose,
                TEXT("  Cluster '%s': %.1f±%.1f cm/s, %d anims, Intent=%s, Speed=%s"),
                *Cluster.ClusterName,
                Cluster.CenterSpeed,
                Cluster.StandardDeviation,
                Cluster.AnimationCount,
                *UEnum::GetValueAsString(Cluster.PrimaryIntent),
                *UEnum::GetValueAsString(Cluster.SpeedLevel));
        }
    }
    
    return Clusters;
}

FSemanticSpeedProfile UMMPoseSearchSpeedAnalyzer::AnalyzeAnimationSpeed(UAnimationAsset* AnimationAsset)
{
    SCOPE_CYCLE_COUNTER(STAT_MMAnalyzeSpeed);
    
    FSemanticSpeedProfile Profile;
    
    if (!AnimationAsset)
    {
        return Profile;
    }
    
    Profile.AnimationName = AnimationAsset->GetName();
    Profile.AnimationAsset = AnimationAsset;
    
    // Handle different animation types
    if (UAnimSequence* Sequence = Cast<UAnimSequence>(AnimationAsset))
    {
        Profile = ExtractSpeedFromSequence(Sequence);
    }
    else if (UAnimComposite* Composite = Cast<UAnimComposite>(AnimationAsset))
    {
        // For composites, analyze the primary segment
        if (Composite->AnimationTrack.AnimSegments.Num() > 0)
        {
            if (UAnimSequence* Seq = Cast<UAnimSequence>(
                Composite->AnimationTrack.AnimSegments[0].GetAnimReference()))
            {
                Profile = ExtractSpeedFromSequence(Seq);
                Profile.AnimationName = Composite->GetName(); // Keep composite name
                Profile.AnimationAsset = Composite;
            }
        }
    }
    else if (UAnimMontage* Montage = Cast<UAnimMontage>(AnimationAsset))
    {
        // For montages, analyze the first slot's first segment
        if (Montage->SlotAnimTracks.Num() > 0 && 
            Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
        {
            if (UAnimSequence* Seq = Cast<UAnimSequence>(
                Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()))
            {
                Profile = ExtractSpeedFromSequence(Seq);
                Profile.AnimationName = Montage->GetName();
                Profile.AnimationAsset = Montage;
            }
        }
    }
    
    return Profile;
}

FSemanticSpeedProfile UMMPoseSearchSpeedAnalyzer::ExtractSpeedFromSequence(UAnimSequence* Sequence)
{
    FSemanticSpeedProfile Profile;
    
    if (!Sequence || !Sequence->GetSkeleton())
    {
        return Profile;
    }
    
    Profile.AnimationName = Sequence->GetName();
    Profile.AnimationAsset = Sequence;
    
    const float SequenceLength = Sequence->GetPlayLength();
    const int32 NumFrames = Sequence->GetNumberOfSampledKeys();
    
    if (NumFrames < 2 || SequenceLength <= 0.0f)
    {
        return Profile;
    }
    
    // Sample root motion to calculate speeds
    TArray<float> SpeedSamples;
    TArray<float> AccelerationSamples;
    
    FVector LastPosition = FVector::ZeroVector;
    float LastSpeed = 0.0f;
    const float DeltaTime = SequenceLength / static_cast<float>(NumFrames - 1);
    
    // CORRECTED: Simpler approach without PoseBones
    for (int32 Frame = 0; Frame < NumFrames; ++Frame)
    {
        const float CurrentTime = (static_cast<float>(Frame) / static_cast<float>(NumFrames - 1)) * SequenceLength;
        
        if (Frame > 0)
        {
            const float PrevTime = (static_cast<float>(Frame - 1) / static_cast<float>(NumFrames - 1)) * SequenceLength;
            
            // Create extraction context
            FAnimExtractContext ExtractionContext;
            ExtractionContext.CurrentTime = CurrentTime;
            // No PoseBones setup needed - just use the context as is
            
            // Extract root motion for this frame range
            FTransform RootMotion = Sequence->ExtractRootMotionFromRange(PrevTime, CurrentTime, ExtractionContext);
            FVector CurrentPosition = LastPosition + RootMotion.GetLocation();
            
            // Calculate speed
            FVector Delta = RootMotion.GetLocation();
            Delta.Z = 0.0f; // Ignore vertical movement for ground locomotion
            float Speed = Delta.Size() / DeltaTime;
            
            SpeedSamples.Add(Speed);
            Profile.SpeedCurve.Add(Speed);
            
            // Calculate acceleration
            if (Frame > 1)
            {
                float Acceleration = (Speed - LastSpeed) / DeltaTime;
                AccelerationSamples.Add(Acceleration);
                Profile.MaxAcceleration = FMath::Max(Profile.MaxAcceleration, FMath::Abs(Acceleration));
            }
            
            LastSpeed = Speed;
            LastPosition = CurrentPosition;
        }
    }
    
    // Calculate statistics
    if (SpeedSamples.Num() > 0)
    {
        CalculateSpeedStatistics(SpeedSamples, Profile);
        
        if (AccelerationSamples.Num() > 0)
        {
            float AccelSum = 0.0f;
            for (float Accel : AccelerationSamples)
            {
                AccelSum += FMath::Abs(Accel);
            }
            Profile.AverageAcceleration = AccelSum / AccelerationSamples.Num();
        }
    }
    
    return Profile;
}


void UMMPoseSearchSpeedAnalyzer::CalculateSpeedStatistics(
    TArray<float>& Speeds,
    FSemanticSpeedProfile& OutProfile)
{
    if (Speeds.Num() == 0) return;
    
    // Sort for median calculation
    Speeds.Sort();
    
    // Basic statistics
    OutProfile.MinSpeed = Speeds[0];
    OutProfile.MaxSpeed = Speeds.Last();
    
    // Average
    float Sum = 0.0f;
    for (float Speed : Speeds)
    {
        Sum += Speed;
    }
    OutProfile.AverageSpeed = Sum / Speeds.Num();
    
    // Variance and standard deviation
    float VarianceSum = 0.0f;
    for (float Speed : Speeds)
    {
        float Diff = Speed - OutProfile.AverageSpeed;
        VarianceSum += Diff * Diff;
    }
    OutProfile.SpeedVariance = VarianceSum / Speeds.Num();
    OutProfile.SpeedStandardDeviation = FMath::Sqrt(OutProfile.SpeedVariance);
}

TArray<FSemanticSpeedCluster> UMMPoseSearchSpeedAnalyzer::PerformHierarchicalClustering(
    const TArray<FSemanticSpeedProfile>& Profiles,
    const FSemanticSpeedConfig& Config)
{
    TArray<FSemanticSpeedCluster> Clusters;
    
    if (Profiles.Num() == 0)
    {
        return Clusters;
    }
    
    // Start with each profile as its own cluster
    for (const FSemanticSpeedProfile& Profile : Profiles)
    {
        FSemanticSpeedCluster NewCluster;
        NewCluster.CenterSpeed = Profile.AverageSpeed;
        NewCluster.MinSpeed = Profile.MinSpeed;
        NewCluster.MaxSpeed = Profile.MaxSpeed;
        NewCluster.AnimationCount = 1;
        NewCluster.AnimationProfiles.Add(Profile);
        NewCluster.SpeedLevel = Profile.SemanticSpeedLevel;
        NewCluster.PrimaryIntent = EMotionIntent::Locomotion;
        Clusters.Add(NewCluster);
    }
    
    // Hierarchical merging until we reach MaxClusters
    while (Clusters.Num() > Config.MaxClusters)
    {
        float MinDistance = FLT_MAX;
        int32 MergeIdx1 = -1;
        int32 MergeIdx2 = -1;
        
        // Find closest pair of clusters
        for (int32 i = 0; i < Clusters.Num() - 1; ++i)
        {
            for (int32 j = i + 1; j < Clusters.Num(); ++j)
            {
                float Distance = FMath::Abs(Clusters[i].CenterSpeed - Clusters[j].CenterSpeed);
                
                // Apply semantic weight if both clusters have the same intent
                if (Clusters[i].PrimaryIntent == Clusters[j].PrimaryIntent)
                {
                    Distance *= (1.0f - Config.SemanticWeight);
                }
                
                if (Distance < MinDistance)
                {
                    MinDistance = Distance;
                    MergeIdx1 = i;
                    MergeIdx2 = j;
                }
            }
        }
        
        // Check if merge would violate merge threshold
        if (MinDistance > Config.ClusterMergeThreshold)
        {
            UE_LOG(LogMMSpeedAnalysis, Verbose, 
                TEXT("Stopping merge: distance %.1f exceeds threshold %.1f"),
                MinDistance, Config.ClusterMergeThreshold);
            break;
        }
        
        // Merge the closest pair
        if (MergeIdx1 >= 0 && MergeIdx2 >= 0)
        {
            FSemanticSpeedCluster& Target = Clusters[MergeIdx1];
            const FSemanticSpeedCluster& Source = Clusters[MergeIdx2];
            
            // Weighted average for center
            float TotalWeight = Target.AnimationCount + Source.AnimationCount;
            Target.CenterSpeed = (Target.CenterSpeed * Target.AnimationCount + 
                                 Source.CenterSpeed * Source.AnimationCount) / TotalWeight;
            
            // Expand ranges
            Target.MinSpeed = FMath::Min(Target.MinSpeed, Source.MinSpeed);
            Target.MaxSpeed = FMath::Max(Target.MaxSpeed, Source.MaxSpeed);
            
            // Combine animations
            Target.AnimationCount += Source.AnimationCount;
            Target.AnimationProfiles.Append(Source.AnimationProfiles);
            
            // Update semantic classification based on new speed
            if (Target.CenterSpeed < 50.0f)
                Target.SpeedLevel = EMotionSpeed::Idle;
            else if (Target.CenterSpeed < 150.0f)
                Target.SpeedLevel = EMotionSpeed::Walk;
            else if (Target.CenterSpeed < 250.0f)
                Target.SpeedLevel = EMotionSpeed::Jog;
            else if (Target.CenterSpeed < 400.0f)
                Target.SpeedLevel = EMotionSpeed::Run;
            else
                Target.SpeedLevel = EMotionSpeed::Sprint;
            
            // Remove the merged cluster
            Clusters.RemoveAt(MergeIdx2);
            
            UE_LOG(LogMMSpeedAnalysis, Verbose, 
                TEXT("Merged clusters: %.1f cm/s + %.1f cm/s -> %.1f cm/s (Count: %d)"),
                Clusters[MergeIdx1].CenterSpeed, Source.CenterSpeed,
                Target.CenterSpeed, Target.AnimationCount);
        }
        else
        {
            break;
        }
    }
    
    // Remove clusters with too few animations
    if (Config.MinAnimationsPerCluster > 1)
    {
        Clusters.RemoveAll([&Config](const FSemanticSpeedCluster& Cluster) {
            bool bRemove = Cluster.AnimationCount < Config.MinAnimationsPerCluster;
            if (bRemove)
            {
                UE_LOG(LogMMSpeedAnalysis, Verbose,
                    TEXT("Removing cluster with %d animations (min required: %d)"),
                    Cluster.AnimationCount, Config.MinAnimationsPerCluster);
            }
            return bRemove;
        });
    }
    
    // Final check - if we have too few clusters after filtering, warn
    if (Clusters.Num() == 0)
    {
        UE_LOG(LogMMSpeedAnalysis, Warning,
            TEXT("No clusters remain after applying MinAnimationsPerCluster=%d filter"),
            Config.MinAnimationsPerCluster);
    }
    
    return Clusters;
}


void UMMPoseSearchSpeedAnalyzer::MergeSimilarClusters(
    TArray<FSemanticSpeedCluster>& Clusters,
    const FSemanticSpeedConfig& Config)
{
    bool bMerged = true;
    
    while (bMerged && Clusters.Num() > 1)
    {
        bMerged = false;
        
        for (int32 i = 0; i < Clusters.Num() - 1; ++i)
        {
            for (int32 j = i + 1; j < Clusters.Num(); ++j)
            {
                FSemanticSpeedCluster& ClusterA = Clusters[i];
                FSemanticSpeedCluster& ClusterB = Clusters[j];
                
                // Check if clusters should merge
                bool bShouldMerge = false;
                
                // Same semantic intent and speed level
                if (ClusterA.PrimaryIntent == ClusterB.PrimaryIntent &&
                    ClusterA.SpeedLevel == ClusterB.SpeedLevel)
                {
                    // Check speed proximity
                    float SpeedDiff = FMath::Abs(ClusterA.CenterSpeed - ClusterB.CenterSpeed);
                    if (SpeedDiff < Config.ClusterMergeThreshold)
                    {
                        bShouldMerge = true;
                    }
                }
                
                if (bShouldMerge)
                {
                    // Merge B into A
                    ClusterA.AnimationProfiles.Append(ClusterB.AnimationProfiles);
                    ClusterA.ClusterName = FString::Printf(TEXT("%s_merged"), *ClusterA.ClusterName);
                    
                    // Remove B
                    Clusters.RemoveAt(j);
                    bMerged = true;
                    break;
                }
            }
            
            if (bMerged) break;
        }
    }
    
    // Limit to maximum clusters
    if (Clusters.Num() > Config.MaxClusters)
    {
        // Sort by animation count and keep the most populated clusters
        Clusters.Sort([](const FSemanticSpeedCluster& A, const FSemanticSpeedCluster& B) {
            return A.AnimationProfiles.Num() > B.AnimationProfiles.Num();
        });
        
        Clusters.SetNum(Config.MaxClusters);
    }
}

void UMMPoseSearchSpeedAnalyzer::CalculateClusterStatistics(FSemanticSpeedCluster& Cluster)
{
    if (Cluster.AnimationProfiles.Num() == 0) return;
    
    TArray<float> Speeds;
    float TotalConfidence = 0.0f;
    
    for (const FSemanticSpeedProfile& Profile : Cluster.AnimationProfiles)
    {
        Speeds.Add(Profile.AverageSpeed);
        TotalConfidence += Profile.SemanticAnalysis.OverallConfidence;
    }
    
    Cluster.AnimationCount = Cluster.AnimationProfiles.Num();
    Cluster.SemanticConfidence = TotalConfidence / Cluster.AnimationCount;
    
    // Calculate speed statistics
    Speeds.Sort();
    
    Cluster.MinSpeed = Speeds[0];
    Cluster.MaxSpeed = Speeds.Last();
    
    // Calculate median (more robust than mean for center)
    if (Speeds.Num() % 2 == 0)
    {
        int32 Mid = Speeds.Num() / 2;
        Cluster.CenterSpeed = (Speeds[Mid - 1] + Speeds[Mid]) / 2.0f;
    }
    else
    {
        Cluster.CenterSpeed = Speeds[Speeds.Num() / 2];
    }
    
    // Calculate standard deviation
    float Sum = 0.0f;
    for (float Speed : Speeds)
    {
        Sum += Speed;
    }
    float Mean = Sum / Speeds.Num();
    
    float VarianceSum = 0.0f;
    for (float Speed : Speeds)
    {
        float Diff = Speed - Mean;
        VarianceSum += Diff * Diff;
    }
    Cluster.StandardDeviation = FMath::Sqrt(VarianceSum / Speeds.Num());
}

void UMMPoseSearchSpeedAnalyzer::DetermineSemanticThresholds(
    FSemanticSpeedCluster& Cluster,
    const FSemanticSpeedConfig& Config)
{
    // Base thresholds on standard deviation
    float BaseThreshold = Cluster.StandardDeviation;
    
    // Adjust based on semantic speed level
    float ThresholdMultiplier = 1.0f;
    
    switch (Cluster.SpeedLevel)
    {
        case EMotionSpeed::Idle:
        case EMotionSpeed::Slow:
            // Tight thresholds for slow movements
            ThresholdMultiplier = 0.5f;
            Cluster.HysteresisWidth = 20.0f;
            Cluster.PreferredBlendTime = 0.5f;
            break;
            
        case EMotionSpeed::Walk:
            // Moderate thresholds for walking
            ThresholdMultiplier = 0.75f;
            Cluster.HysteresisWidth = 30.0f;
            Cluster.PreferredBlendTime = 0.4f;
            break;
            
        case EMotionSpeed::Jog:
            // Standard thresholds for jogging
            ThresholdMultiplier = 1.0f;
            Cluster.HysteresisWidth = 40.0f;
            Cluster.PreferredBlendTime = 0.3f;
            break;
            
        case EMotionSpeed::Run:
            // Wider thresholds for running
            ThresholdMultiplier = 1.25f;
            Cluster.HysteresisWidth = 50.0f;
            Cluster.PreferredBlendTime = 0.25f;
            break;
            
        case EMotionSpeed::Sprint:
            // Widest thresholds for sprinting
            ThresholdMultiplier = 1.5f;
            Cluster.HysteresisWidth = 60.0f;
            Cluster.PreferredBlendTime = 0.2f;
            break;
            
        default:
            break;
    }
    
    // Calculate final thresholds
    float ThresholdRange = BaseThreshold * ThresholdMultiplier;
    
    Cluster.EntryThreshold = FMath::Max(0.0f, Cluster.CenterSpeed - ThresholdRange);
    Cluster.ExitThreshold = Cluster.CenterSpeed + ThresholdRange;
    
    // Ensure thresholds don't overlap too much with adjacent clusters
    // This would be done in the calling function after all clusters are processed
}

float UMMPoseSearchSpeedAnalyzer::GetOptimalSpeedForSemanticContext(
    const FMotionSemanticResult& CurrentSemantics,
    const TArray<FSemanticSpeedCluster>& AvailableClusters)
{
    if (AvailableClusters.Num() == 0)
    {
        return 0.0f;
    }
    
    // Find best matching cluster based on semantics
    const FSemanticSpeedCluster* BestCluster = nullptr;
    float BestScore = -1.0f;
    
    for (const FSemanticSpeedCluster& Cluster : AvailableClusters)
    {
        float Score = 0.0f;
        
        // Intent matching (40% weight)
        if (Cluster.PrimaryIntent == CurrentSemantics.Intent.PrimaryIntent)
        {
            Score += 0.4f;
        }
        else if (CurrentSemantics.Intent.HasIntent(Cluster.PrimaryIntent))
        {
            Score += 0.2f;
        }
        
        // Speed level matching (40% weight)
        if (Cluster.SpeedLevel == CurrentSemantics.Speed.SpeedLevel)
        {
            Score += 0.4f;
        }
        else
        {
            // Partial score based on speed level distance
            int32 SpeedDiff = FMath::Abs(
                static_cast<int32>(Cluster.SpeedLevel) - 
                static_cast<int32>(CurrentSemantics.Speed.SpeedLevel)
            );
            Score += FMath::Max(0.0f, 0.4f - (SpeedDiff * 0.1f));
        }
        
        // Confidence weighting (20% weight)
        Score += Cluster.SemanticConfidence * 0.2f;
        
        if (Score > BestScore)
        {
            BestScore = Score;
            BestCluster = &Cluster;
        }
    }
    
    return BestCluster ? BestCluster->CenterSpeed : 0.0f;
}

int32 UMMPoseSearchSpeedAnalyzer::FindBestCluster(
    float Speed,
    const FMotionSemanticResult& Semantics,
    const TArray<FSemanticSpeedCluster>& Clusters)
{
    int32 BestIndex = -1;
    float BestScore = -1.0f;
    
    for (int32 i = 0; i < Clusters.Num(); ++i)
    {
        const FSemanticSpeedCluster& Cluster = Clusters[i];
        
        // Calculate combined score
        float Score = 0.0f;
        
        // Speed proximity (50% weight)
        if (Cluster.ContainsSpeed(Speed))
        {
            Score += 0.5f;
        }
        else
        {
            float Distance = Cluster.GetDistanceToSpeed(Speed);
            Score += FMath::Max(0.0f, 0.5f - (Distance / 500.0f)); // Decay over 500 cm/s
        }
        
        // Semantic matching (30% weight)
        if (Cluster.PrimaryIntent == Semantics.Intent.PrimaryIntent)
        {
            Score += 0.2f;
        }
        if (Cluster.SpeedLevel == Semantics.Speed.SpeedLevel)
        {
            Score += 0.1f;
        }
        
        // Priority (20% weight)
        Score += (Cluster.Priority / 10.0f) * 0.2f;
        
        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = i;
        }
    }
    
    return BestIndex;
}

FSpeedTransitionCurve UMMPoseSearchSpeedAnalyzer::GenerateSemanticSpeedTransition(
    const FSemanticSpeedCluster& FromCluster,
    const FSemanticSpeedCluster& ToCluster,
    const FMotionSemanticResult& SemanticContext)
{
    FSpeedTransitionCurve Transition;
    
    Transition.FromCluster = FromCluster.ClusterName;
    Transition.ToCluster = ToCluster.ClusterName;
    
    // Calculate speed delta
    float SpeedDelta = FMath::Abs(ToCluster.CenterSpeed - FromCluster.CenterSpeed);
    Transition.SpeedDeltaThreshold = SpeedDelta;
    
    // Determine optimal duration based on semantic context
    float BaseDuration = 0.3f;
    
    // Adjust for speed change magnitude
    BaseDuration *= (1.0f + (SpeedDelta / 1000.0f)); // Add 0.1s per 100 cm/s difference
    
    // Adjust for semantic intent
    if (SemanticContext.Intent.PrimaryIntent == EMotionIntent::Combat ||
        SemanticContext.Speed.NormalizedIntensity > 0.8f)
    {
        // Faster transitions for urgent contexts
        BaseDuration *= 0.7f;
    }
    else if (SemanticContext.Intent.PrimaryIntent == EMotionIntent::Idle)
    {
        // Slower transitions for relaxed contexts
        BaseDuration *= 1.3f;
    }
    
    // Clamp duration
    Transition.OptimalDuration = FMath::Clamp(BaseDuration, 0.1f, 1.0f);
    
    // Calculate semantic smoothness
    float Smoothness = 1.0f;
    
    // Reduce smoothness if intents don't match
    if (FromCluster.PrimaryIntent != ToCluster.PrimaryIntent)
    {
        Smoothness *= 0.7f;
    }
    
    // Reduce smoothness for large speed level jumps
    int32 SpeedLevelDiff = FMath::Abs(
        static_cast<int32>(FromCluster.SpeedLevel) - 
        static_cast<int32>(ToCluster.SpeedLevel)
    );
    Smoothness *= FMath::Max(0.3f, 1.0f - (SpeedLevelDiff * 0.15f));
    
    Transition.SemanticSmoothness = Smoothness;
    
    // TODO: Generate or assign actual blend curve asset
    // For now, this would use a default curve or generate one procedurally
    
    return Transition;
}

float UMMPoseSearchSpeedAnalyzer::CalculateProfileDistance(
    const FSemanticSpeedProfile& ProfileA,
    const FSemanticSpeedProfile& ProfileB)
{
    float Distance = 0.0f;
    float WeightSum = 0.0f;
    
    // Speed difference (40% weight)
    float SpeedDiff = FMath::Abs(ProfileA.AverageSpeed - ProfileB.AverageSpeed);
    Distance += (SpeedDiff / 1000.0f) * 0.4f; // Normalize to ~1000 cm/s range
    WeightSum += 0.4f;
    
    // CORRECTED: Direct semantic comparison using the analyzer
    if (SemanticSubsystem)
    {
        // Access the motion semantic analyzer directly
        if (UMMMotionSemanticAnalyzer* Analyzer = GEngine->GetEngineSubsystem<UMMMotionSemanticAnalyzer>())
        {
            float SemanticSimilarity = Analyzer->CompareSemanticResults(
                ProfileA.SemanticAnalysis,
                ProfileB.SemanticAnalysis
            );
            Distance += (1.0f - SemanticSimilarity) * 0.4f;
            WeightSum += 0.4f;
        }
    }
    
    // Speed variance difference (10% weight)
    float VarianceDiff = FMath::Abs(ProfileA.SpeedVariance - ProfileB.SpeedVariance);
    Distance += FMath::Min(1.0f, VarianceDiff / 500.0f) * 0.1f;
    WeightSum += 0.1f;
    
    // Acceleration difference (10% weight)
    float AccelDiff = FMath::Abs(ProfileA.MaxAcceleration - ProfileB.MaxAcceleration);
    Distance += FMath::Min(1.0f, AccelDiff / 1000.0f) * 0.1f;
    WeightSum += 0.1f;
    
    return WeightSum > 0.0f ? (Distance / WeightSum) : 1.0f;
}

void UMMPoseSearchSpeedAnalyzer::ClearCache()
{
    FScopeLock Lock(&CacheMutex);
    DatabaseProfiles.Empty();
    DatabaseClusters.Empty();
    
    UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Speed analyzer cache cleared"));
}

int32 UMMPoseSearchSpeedAnalyzer::GetMemoryUsage() const
{
    FScopeLock Lock(&CacheMutex);
    
    int32 TotalBytes = 0;
    
    // Estimate memory usage
    for (const auto& DatabasePair : DatabaseProfiles)
    {
        TotalBytes += DatabasePair.Value.Num() * sizeof(FSemanticSpeedProfile);
    }
    
    for (const auto& ClusterPair : DatabaseClusters)
    {
        for (const FSemanticSpeedCluster& Cluster : ClusterPair.Value)
        {
            TotalBytes += sizeof(FSemanticSpeedCluster);
            TotalBytes += Cluster.AnimationProfiles.Num() * sizeof(FSemanticSpeedProfile);
        }
    }
    
    return TotalBytes;
}

void UMMPoseSearchSpeedAnalyzer::CleanupCache(float MaxAge)
{
    FScopeLock Lock(&CacheMutex);
    
    float CurrentTime = FPlatformTime::Seconds();
    
    // Clean old profiles
    for (auto& DatabasePair : DatabaseProfiles)
    {
        TArray<int32> KeysToRemove;
        
        for (const auto& ProfilePair : DatabasePair.Value)
        {
            if (CurrentTime - ProfilePair.Value.LastUpdateTime > MaxAge)
            {
                KeysToRemove.Add(ProfilePair.Key);
            }
        }
        
        for (int32 Key : KeysToRemove)
        {
            DatabasePair.Value.Remove(Key);
        }
    }
    
    UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Cache cleanup completed"));
}

void UMMPoseSearchSpeedAnalyzer::DrawDebugClusters(
    UWorld* World,
    const FVector& Location,
    const TArray<FSemanticSpeedCluster>& Clusters,
    int32 CurrentClusterIndex)
{
    if (!World) return;

    // Removed unused BarHeight variable

    for (int32 i = 0; i < Clusters.Num(); ++i)
    {
        const float Spacing = 25.0f;
        const float BarWidth = 200.0f;
        const FSemanticSpeedCluster& Cluster = Clusters[i];
        FVector BarLocation = Location + FVector(0, 0, -i * Spacing);
        
        // Draw cluster speed range
        FColor BarColor = (i == CurrentClusterIndex) ? FColor::Green : FColor::White;
        
        // Draw center line
        DrawDebugLine(World,
            BarLocation - FVector(BarWidth/2, 0, 0),
            BarLocation + FVector(BarWidth/2, 0, 0),
            BarColor, false, -1, 0, 2.0f);
        
        // Draw threshold markers
        float EntryPos = (Cluster.EntryThreshold / 1000.0f) * BarWidth - BarWidth/2;
        float ExitPos = (Cluster.ExitThreshold / 1000.0f) * BarWidth - BarWidth/2;
        float CenterPos = (Cluster.CenterSpeed / 1000.0f) * BarWidth - BarWidth/2;
        
        DrawDebugLine(World,
            BarLocation + FVector(EntryPos, 0, -5),
            BarLocation + FVector(EntryPos, 0, 5),
            FColor::Blue, false, -1, 0, 3.0f);
        
        DrawDebugLine(World,
            BarLocation + FVector(ExitPos, 0, -5),
            BarLocation + FVector(ExitPos, 0, 5),
            FColor::Red, false, -1, 0, 3.0f);
        
        DrawDebugLine(World,
            BarLocation + FVector(CenterPos, 0, -7),
            BarLocation + FVector(CenterPos, 0, 7),
            FColor::Yellow, false, -1, 0, 4.0f);
        
        // Draw cluster name and info
        FString InfoText = FString::Printf(TEXT("%s: %.0f±%.0f cm/s (%d anims)"),
            *Cluster.ClusterName,
            Cluster.CenterSpeed,
            Cluster.StandardDeviation,
            Cluster.AnimationCount);
        
        DrawDebugString(World,
            BarLocation + FVector(BarWidth/2 + 10, 0, 0),
            InfoText,
            nullptr, BarColor, 0.0f, true);
    }
}


FString UMMPoseSearchSpeedAnalyzer::ExportClustersToCSV(const TArray<FSemanticSpeedCluster>& Clusters)
{
    FString CSV = TEXT("ClusterName,Intent,SpeedLevel,CenterSpeed,MinSpeed,MaxSpeed,StdDev,AnimCount,EntryThreshold,ExitThreshold,Confidence\n");
    
    for (const FSemanticSpeedCluster& Cluster : Clusters)
    {
        CSV += FString::Printf(TEXT("%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f\n"),
            *Cluster.ClusterName,
            *UEnum::GetValueAsString(Cluster.PrimaryIntent),
            *UEnum::GetValueAsString(Cluster.SpeedLevel),
            Cluster.CenterSpeed,
            Cluster.MinSpeed,
            Cluster.MaxSpeed,
            Cluster.StandardDeviation,
            Cluster.AnimationCount,
            Cluster.EntryThreshold,
            Cluster.ExitThreshold,
            Cluster.SemanticConfidence);
    }
    
    return CSV;
}

float UMMPoseSearchSpeedAnalyzer::CalculateTransitionScore(
    EMotionSpeed FromSpeed,
    EMotionSpeed ToSpeed)
{
    // Calculate how suitable a transition is between speed levels
    int32 Distance = FMath::Abs(static_cast<int32>(FromSpeed) - static_cast<int32>(ToSpeed));
    
    if (Distance == 0) return 1.0f;  // Same level, perfect
    if (Distance == 1) return 0.8f;  // Adjacent levels, good
    if (Distance == 2) return 0.5f;  // Two levels apart, okay
    if (Distance == 3) return 0.3f;  // Three levels, poor
    return 0.1f;  // Very different, bad transition
}
#endif