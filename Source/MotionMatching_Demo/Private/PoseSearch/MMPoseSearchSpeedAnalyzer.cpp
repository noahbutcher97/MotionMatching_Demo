// MMPoseSearchSpeedAnalyzer.cpp - CLEANED VERSION
#include "PoseSearch/MMPoseSearchSpeedAnalyzer.h"
#include "Data/MMSemanticTypes.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"

DEFINE_LOG_CATEGORY(LogMMSpeedAnalysis);

UMMPoseSearchSpeedAnalyzer::UMMPoseSearchSpeedAnalyzer()
{
}

void UMMPoseSearchSpeedAnalyzer::Initialize()
{
    UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Speed Analyzer initialized (standalone mode)"));
}

void UMMPoseSearchSpeedAnalyzer::Shutdown()
{
    FScopeLock Lock(&CacheMutex);
    DatabaseProfiles.Empty();
    DatabaseClusters.Empty();
    
    UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Speed Analyzer shutdown"));
}

int32 UMMPoseSearchSpeedAnalyzer::IndexDatabaseWithSemantics(
    UPoseSearchDatabase* Database,
    const FSemanticSpeedConfig& Config,
    bool bForceReindex)
{
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
        UE_LOG(LogMMSpeedAnalysis, Log, TEXT("Database %s already indexed"), *Database->GetName());
        return DatabaseProfiles[DatabaseKey].Num();
    }
    
    // Get profiles map for this database
    TMap<int32, FSemanticSpeedProfile>& Profiles = DatabaseProfiles.FindOrAdd(DatabaseKey);
    Profiles.Empty();
    
    int32 AnimationsIndexed = 0;
    
    // Use the same approach as MMPoseSearchUtils - iterate GetNumAnimationAssets
    const int32 NumAssets = Database->GetNumAnimationAssets();
    
    for (int32 AssetIdx = 0; AssetIdx < NumAssets; ++AssetIdx)
    {
        UObject* AssetObj = Database->GetAnimationAsset(AssetIdx);
        
        if (!AssetObj)
        {
            continue;
        }
        
        UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(AssetObj);
        if (!AnimAsset)
        {
            continue;
        }
        
        // Analyze the animation
        FSemanticSpeedProfile Profile = AnalyzeAnimationSpeed(AnimAsset);
        
        if (Profile.IsValid())
        {
            Profile.DatabaseIndex = AssetIdx;
            Profile.LastUpdateTime = FPlatformTime::Seconds();
            
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

FSemanticSpeedProfile UMMPoseSearchSpeedAnalyzer::AnalyzeAnimationSpeed(UAnimationAsset* AnimationAsset)
{
    // Explicitly initialize to silence compiler warnings
    FSemanticSpeedProfile Profile;
    Profile.AnimationName = TEXT("");
    Profile.AverageSpeed = 0.0f;
    Profile.MinSpeed = 0.0f;
    Profile.MaxSpeed = 0.0f;
    Profile.SpeedVariance = 0.0f;
    Profile.SpeedStandardDeviation = 0.0f;
    Profile.SemanticSpeedLevel = EMotionSpeed::Idle;
    Profile.DatabaseIndex = INDEX_NONE;
    Profile.LastUpdateTime = 0.0f;
    
    if (!AnimationAsset)
    {
        return Profile;
    }
    
    Profile.AnimationName = AnimationAsset->GetName();
    Profile.AnimationAsset = AnimationAsset;
    
    // USE SIMPLE SEMANTIC ANALYZER - No plugin dependencies
    FMotionSemanticResult SemanticResult = UMMSemanticNameAnalyzer::AnalyzeAnimationName(Profile.AnimationName);
    Profile.SemanticAnalysis = SemanticResult;
    Profile.SemanticSpeedLevel = SemanticResult.Speed.SpeedLevel;
    
    // Extract speed from animation if it's a sequence
    if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(AnimationAsset))
    {
        FSemanticSpeedProfile ExtractedProfile = ExtractSpeedFromSequence(AnimSeq);
        
        // Keep the semantic analysis but use extracted speed data
        Profile.AverageSpeed = ExtractedProfile.AverageSpeed;
        Profile.MinSpeed = ExtractedProfile.MinSpeed;
        Profile.MaxSpeed = ExtractedProfile.MaxSpeed;
        Profile.SpeedVariance = ExtractedProfile.SpeedVariance;
        Profile.SpeedStandardDeviation = ExtractedProfile.SpeedStandardDeviation;
    }
    else
    {
        // Fallback: Use semantic estimate
        Profile.AverageSpeed = SemanticResult.Speed.EstimatedSpeed;
        Profile.MinSpeed = Profile.AverageSpeed * 0.8f;
        Profile.MaxSpeed = Profile.AverageSpeed * 1.2f;
    }
    
    return Profile;
}

FSemanticSpeedProfile UMMPoseSearchSpeedAnalyzer::ExtractSpeedFromSequence(UAnimSequence* Sequence)
{
    FSemanticSpeedProfile Profile;
    Profile.AnimationName = TEXT("");
    Profile.AverageSpeed = 0.0f;
    Profile.MinSpeed = 0.0f;
    Profile.MaxSpeed = 0.0f;
    Profile.SpeedVariance = 0.0f;
    Profile.SpeedStandardDeviation = 0.0f;
    
    if (!Sequence)
    {
        return Profile;
    }
    
    Profile.AnimationName = Sequence->GetName();
    Profile.AnimationAsset = Sequence;
    
    // Check if animation has root motion
    if (!Sequence->HasRootMotion())
    {
        return Profile;
    }
    
    // Sample root motion velocity
    const float Duration = Sequence->GetPlayLength();
    if (Duration <= 0.0f)
    {
        return Profile;
    }
    
    const int32 SampleCount = FMath::Max(10, FMath::CeilToInt(Duration * 30.0f));
    
    TArray<float> Speeds;
    Speeds.Reserve(SampleCount);
    
    for (int32 i = 0; i < SampleCount - 1; ++i)
    {
        float StartTime = (Duration * i) / FMath::Max(1, SampleCount - 1);
        float EndTime = (Duration * (i + 1)) / FMath::Max(1, SampleCount - 1);
        float DeltaTime = EndTime - StartTime;
        
        if (DeltaTime <= SMALL_NUMBER)
        {
            continue;
        }
        
        // Extract root motion for this time range using new API
        FAnimExtractContext Context;
        Context.bLooping = Sequence->bLoop;
        
        FTransform RootMotion = Sequence->ExtractRootMotionFromRange(StartTime, EndTime, Context);
        
        // Calculate speed from displacement
        float Speed = RootMotion.GetTranslation().Size() / DeltaTime;
        
        if (Speed > 1.0f) // Filter out near-zero speeds
        {
            Speeds.Add(Speed);
        }
    }
    
    // Calculate statistics
    if (Speeds.Num() > 0)
    {
        CalculateSpeedStatistics(Speeds, Profile);
    }
    
    return Profile;
}

void UMMPoseSearchSpeedAnalyzer::CalculateSpeedStatistics(TArray<float>& Speeds, FSemanticSpeedProfile& OutProfile)
{
    if (Speeds.Num() == 0)
    {
        return;
    }
    
    // Min/Max
    OutProfile.MinSpeed = FLT_MAX;
    OutProfile.MaxSpeed = -FLT_MAX;
    
    for (float Speed : Speeds)
    {
        OutProfile.MinSpeed = FMath::Min(OutProfile.MinSpeed, Speed);
        OutProfile.MaxSpeed = FMath::Max(OutProfile.MaxSpeed, Speed);
    }
    
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

TArray<FSemanticSpeedCluster> UMMPoseSearchSpeedAnalyzer::GenerateSemanticSpeedClusters(
    UPoseSearchDatabase* Database,
    const FSemanticSpeedConfig& Config)
{
    if (!Database)
    {
        UE_LOG(LogMMSpeedAnalysis, Error, TEXT("Invalid database"));
        return TArray<FSemanticSpeedCluster>();
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
    
    // Assign priorities and names
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
        NewCluster.PrimaryIntent = Profile.SemanticAnalysis.Intent.PrimaryIntent;
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
            else if (Target.CenterSpeed < 200.0f)
                Target.SpeedLevel = EMotionSpeed::Walk;
            else if (Target.CenterSpeed < 350.0f)
                Target.SpeedLevel = EMotionSpeed::Jog;
            else if (Target.CenterSpeed < 500.0f)
                Target.SpeedLevel = EMotionSpeed::Run;
            else
                Target.SpeedLevel = EMotionSpeed::Sprint;
            
            // Remove the merged cluster
            Clusters.RemoveAt(MergeIdx2);
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
            return Cluster.AnimationCount < Config.MinAnimationsPerCluster;
        });
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
                
                // Same semantic intent and speed level?
                if (ClusterA.PrimaryIntent == ClusterB.PrimaryIntent &&
                    ClusterA.SpeedLevel == ClusterB.SpeedLevel)
                {
                    // Check speed proximity
                    float SpeedDiff = FMath::Abs(ClusterA.CenterSpeed - ClusterB.CenterSpeed);
                    if (SpeedDiff < Config.ClusterMergeThreshold)
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
    if (Cluster.AnimationProfiles.Num() == 0)
    {
        return;
    }
    
    // Recalculate center and ranges
    float Sum = 0.0f;
    float Min = FLT_MAX;
    float Max = -FLT_MAX;
    
    for (const FSemanticSpeedProfile& Profile : Cluster.AnimationProfiles)
    {
        Sum += Profile.AverageSpeed;
        Min = FMath::Min(Min, Profile.MinSpeed);
        Max = FMath::Max(Max, Profile.MaxSpeed);
    }
    
    Cluster.CenterSpeed = Sum / Cluster.AnimationProfiles.Num();
    Cluster.MinSpeed = Min;
    Cluster.MaxSpeed = Max;
    Cluster.AnimationCount = Cluster.AnimationProfiles.Num();
    
    // Calculate standard deviation
    float VarianceSum = 0.0f;
    for (const FSemanticSpeedProfile& Profile : Cluster.AnimationProfiles)
    {
        float Diff = Profile.AverageSpeed - Cluster.CenterSpeed;
        VarianceSum += Diff * Diff;
    }
    Cluster.StandardDeviation = FMath::Sqrt(VarianceSum / Cluster.AnimationCount);
    
    // Calculate semantic confidence (average of profile confidences)
    float ConfidenceSum = 0.0f;
    for (const FSemanticSpeedProfile& Profile : Cluster.AnimationProfiles)
    {
        ConfidenceSum += Profile.SemanticAnalysis.OverallConfidence;
    }
    Cluster.SemanticConfidence = ConfidenceSum / Cluster.AnimationCount;
}

void UMMPoseSearchSpeedAnalyzer::DetermineSemanticThresholds(
    FSemanticSpeedCluster& Cluster,
    const FSemanticSpeedConfig& Config)
{
    // Entry/exit thresholds with hysteresis
    Cluster.HysteresisWidth = Cluster.StandardDeviation * 0.5f;
    Cluster.EntryThreshold = Cluster.CenterSpeed - Cluster.HysteresisWidth;
    Cluster.ExitThreshold = Cluster.CenterSpeed + Cluster.HysteresisWidth;
    
    // Ensure thresholds don't go negative
    Cluster.EntryThreshold = FMath::Max(0.0f, Cluster.EntryThreshold);
}

int32 UMMPoseSearchSpeedAnalyzer::FindBestCluster(
    float CurrentSpeed,
    const TArray<FSemanticSpeedCluster>& Clusters)
{
    if (Clusters.Num() == 0)
    {
        return INDEX_NONE;
    }
    
    int32 BestIndex = INDEX_NONE;
    float BestDistance = FLT_MAX;
    
    for (int32 i = 0; i < Clusters.Num(); ++i)
    {
        const FSemanticSpeedCluster& Cluster = Clusters[i];
        
        // Exact match?
        if (Cluster.ContainsSpeed(CurrentSpeed))
        {
            return i;
        }
        
        // Track closest
        float Distance = Cluster.GetDistanceToSpeed(CurrentSpeed);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestIndex = i;
        }
    }
    
    return BestIndex;
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