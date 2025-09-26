#include "FunctionLibraries/MMPoseSearchUtils.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MMAnimInstance.h"


float UMMPoseSearchUtils::GetTargetAnimationSpeed(
    float ActualSpeed,
    FDynamicSpeedAnalysis& InOutAnalysis,
    const FDynamicSpeedMatchConfig& Config,
    float DeltaTime)
{
    // Update current state
    InOutAnalysis.CurrentActualSpeed = ActualSpeed;
    InOutAnalysis.TimeSinceLastTransition += DeltaTime;
    
    // Store last target for smooth transitions
    InOutAnalysis.LastTargetSpeed = InOutAnalysis.CurrentTargetSpeed;
    
    // If no clusters detected, return actual speed
    if (InOutAnalysis.DetectedClusters.Num() == 0)
    {
        InOutAnalysis.CurrentTargetSpeed = ActualSpeed;
        return ActualSpeed;
    }
    
    // Find nearest cluster
    int32 NewClusterIndex = FindNearestCluster(ActualSpeed, InOutAnalysis.DetectedClusters);
    
    // Apply hysteresis to prevent oscillation
    if (InOutAnalysis.CurrentClusterIndex >= 0 && 
        InOutAnalysis.CurrentClusterIndex < InOutAnalysis.DetectedClusters.Num())
    {
        const FSpeedCluster& CurrentCluster = InOutAnalysis.DetectedClusters[InOutAnalysis.CurrentClusterIndex];
        float DistanceToCurrent = CurrentCluster.DistanceTo(ActualSpeed);
        
        // Stay in current cluster if within hysteresis zone
        if (DistanceToCurrent < Config.HysteresisZone)
        {
            NewClusterIndex = InOutAnalysis.CurrentClusterIndex;
        }
    }
    
    // Prevent micro-transitions
    if (Config.bPreventMicroTransitions && 
        NewClusterIndex != InOutAnalysis.CurrentClusterIndex &&
        InOutAnalysis.TimeSinceLastTransition < Config.MinTransitionTime)
    {
        NewClusterIndex = InOutAnalysis.CurrentClusterIndex;
    }
    
    // Handle cluster change
    if (NewClusterIndex != InOutAnalysis.CurrentClusterIndex)
    {
        InOutAnalysis.CurrentClusterIndex = NewClusterIndex;
        InOutAnalysis.TimeSinceLastTransition = 0.f;
        InOutAnalysis.bIsTransitioning = true;
        
        if (Config.bDebugLog)
        {
            UE_LOG(LogTemp, Warning, TEXT("Speed cluster changed: %d -> %d (Speed: %.1f)"),
                InOutAnalysis.CurrentClusterIndex, NewClusterIndex, ActualSpeed);
        }
    }
    
    // Calculate target speed from cluster
    float TargetSpeed = ActualSpeed; // Default to actual if no cluster
    
    if (NewClusterIndex >= 0 && NewClusterIndex < InOutAnalysis.DetectedClusters.Num())
    {
        const FSpeedCluster& TargetCluster = InOutAnalysis.DetectedClusters[NewClusterIndex];
        
        // Apply speed match strength
        if (Config.bUseContinuousBlending)
        {
            // Continuous blend based on distance to cluster center
            float DistanceToCenter = FMath::Abs(ActualSpeed - TargetCluster.CenterSpeed);
            float BlendFactor = FMath::Clamp(1.0f - (DistanceToCenter / Config.ClusterMergeThreshold), 
                                            0.0f, Config.SpeedMatchStrength);
            TargetSpeed = FMath::Lerp(ActualSpeed, TargetCluster.CenterSpeed, BlendFactor);
        }
        else
        {
            // Snap to cluster with configured strength
            TargetSpeed = FMath::Lerp(ActualSpeed, TargetCluster.CenterSpeed, Config.SpeedMatchStrength);
        }
    }
    
    // Smooth transition between speeds
    if (InOutAnalysis.bIsTransitioning)
    {
        InOutAnalysis.CurrentTargetSpeed = FMath::FInterpTo(
            InOutAnalysis.LastTargetSpeed,
            TargetSpeed,
            DeltaTime,
            Config.TransitionSmoothness
        );
        
        // Check if transition is complete
        if (FMath::IsNearlyEqual(InOutAnalysis.CurrentTargetSpeed, TargetSpeed, 1.0f))
        {
            InOutAnalysis.bIsTransitioning = false;
        }
    }
    else
    {
        InOutAnalysis.CurrentTargetSpeed = TargetSpeed;
    }
    
    return InOutAnalysis.CurrentTargetSpeed;
}

void UMMPoseSearchUtils::ApplySpeedMatchingToTrajectory(
    FTransformTrajectory& Trajectory,
    const FDynamicSpeedAnalysis& Analysis,
    const FDynamicSpeedMatchConfig& Config)
{
    if (Trajectory.Samples.Num() < 2)
    {
        return;
    }
    
    // Don't apply if no target speed set
    if (FMath::IsNearlyZero(Analysis.CurrentTargetSpeed))
    {
        return;
    }
    
    // Process each trajectory sample pair
    for (int32 i = 1; i < Trajectory.Samples.Num(); i++)
    {
        FTransformTrajectorySample& Sample = Trajectory.Samples[i];
        const FTransformTrajectorySample& PrevSample = Trajectory.Samples[i - 1];
        
        const float TimeDelta = Sample.TimeInSeconds - PrevSample.TimeInSeconds;
        if (TimeDelta <= KINDA_SMALL_NUMBER) continue;
        
        // Calculate current implied velocity
        FVector ImpliedVelocity = (Sample.Position - PrevSample.Position) / TimeDelta;
        float ImpliedSpeed = ImpliedVelocity.Size();
        
        if (ImpliedSpeed > SMALL_NUMBER)
        {
            // Calculate speed adjustment ratio
            float SpeedRatio = Analysis.CurrentTargetSpeed / ImpliedSpeed;
            
            // Clamp ratio to prevent extreme adjustments
            SpeedRatio = FMath::Clamp(SpeedRatio, 0.5f, 2.0f);
            
            // For future samples, apply stronger adjustment
            // For past samples, apply weaker adjustment to maintain continuity
            float AdjustmentStrength = Config.SpeedMatchStrength;
            if (Sample.TimeInSeconds < 0.0f)
            {
                // Past samples - use reduced strength
                AdjustmentStrength *= 0.3f;
            }
            
            // Apply adjustment
            float FinalRatio = FMath::Lerp(1.0f, SpeedRatio, AdjustmentStrength);
            
            // Adjust position to imply target speed
            FVector Direction = ImpliedVelocity.GetSafeNormal();
            FVector AdjustedVelocity = Direction * (ImpliedSpeed * FinalRatio);
            Sample.Position = PrevSample.Position + AdjustedVelocity * TimeDelta;
        }
    }
    
    if (Config.bDebugLog)
    {
        TArray<FVector> DerivedVelocities = CalculateDerivedVelocities(Trajectory);
        if (DerivedVelocities.Num() > 0)
        {
            float AvgDerivedSpeed = 0.f;
            for (const FVector& Vel : DerivedVelocities)
            {
                AvgDerivedSpeed += Vel.Size();
            }
            AvgDerivedSpeed /= DerivedVelocities.Num();
            
            UE_LOG(LogTemp, VeryVerbose, TEXT("Trajectory adjusted - Target: %.1f, Avg Derived: %.1f"),
                Analysis.CurrentTargetSpeed, AvgDerivedSpeed);
        }
    }
}

void UMMPoseSearchUtils::DetectSpeedClusters(
    const TArray<float>& SpeedSamples,
    FDynamicSpeedAnalysis& InOutAnalysis,
    const FDynamicSpeedMatchConfig& Config)
{
    InOutAnalysis.DetectedClusters.Empty();
    
    if (SpeedSamples.Num() < 10)
    {
        // Not enough samples for clustering
        return;
    }
    
    // Filter out zero speeds and outliers
    TArray<float> FilteredSpeeds;
    for (float Speed : SpeedSamples)
    {
        if (Speed > 1.0f) // Ignore near-zero speeds
        {
            FilteredSpeeds.Add(Speed);
        }
    }
    
    if (FilteredSpeeds.Num() < 5)
    {
        return;
    }
    
    // Perform k-means clustering
    TArray<FSpeedCluster> Clusters;
    PerformKMeansClustering(FilteredSpeeds, Config.MaxSpeedClusters, Clusters);
    
    // Merge similar clusters
    MergeSimilarClusters(Clusters, Config.ClusterMergeThreshold);
    
    // Filter out low-weight clusters
    Clusters.RemoveAll([&Config, &FilteredSpeeds](const FSpeedCluster& Cluster) {
        float Weight = static_cast<float>(Cluster.SampleCount) / FilteredSpeeds.Num();
        return Weight < Config.ClusterWeightThreshold;
    });
    
    // Sort by speed
    Clusters.Sort([](const FSpeedCluster& A, const FSpeedCluster& B) {
        return A.CenterSpeed < B.CenterSpeed;
    });
    
    InOutAnalysis.DetectedClusters = Clusters;
    
    if (Config.bDebugLog)
    {
        UE_LOG(LogTemp, Warning, TEXT("Detected %d speed clusters from %d samples:"),
            Clusters.Num(), SpeedSamples.Num());
        for (int32 i = 0; i < Clusters.Num(); i++)
        {
            const FSpeedCluster& Cluster = Clusters[i];
            UE_LOG(LogTemp, Warning, TEXT("  Cluster %d: %.0f cm/s (range: %.0f-%.0f, samples: %d)"),
                i, Cluster.CenterSpeed, Cluster.MinSpeed, Cluster.MaxSpeed, Cluster.SampleCount);
        }
    }
}

float UMMPoseSearchUtils::SampleAnimationSpeed(
    const UAnimInstance* AnimInstance)
{
    if (!AnimInstance || !AnimInstance->GetSkelMeshComponent())
    {
        return 0.f;
    }
    
    // Since we can't track state in a static function without statics,
    // and this is meant to sample the CURRENT animation speed,
    // just return the character's actual velocity
    
    // This is the correct approach for ForceRootLock anyway -
    // the character moves at CMC speeds, animations should match
    if (const APawn* Pawn = AnimInstance->TryGetPawnOwner())
    {
        return Pawn->GetVelocity().Size();
    }
    
    return 0.f;
}

float UMMPoseSearchUtils::GetMotionMatchingAnimationSpeed(const UMMAnimInstance* AnimInstance)
{
    if (!AnimInstance || AnimInstance->LastSelectedPoseIndex < 0)
    {
        return 0.f;
    }
    
    const UPoseSearchDatabase* Database = AnimInstance->MotionMatchingDatabase;
    if (!Database)
    {
        return 0.f;
    }
    
    // Get the search index
    const UE::PoseSearch::FSearchIndex& SearchIndex = Database->GetSearchIndex();
    
    // Check if pose index is valid
    if (AnimInstance->LastSelectedPoseIndex >= SearchIndex.GetNumPoses())
    {
        return 0.f;
    }
    
    // Try using a different approach - get the asset directly from pose index
    const int32 PoseIdx = AnimInstance->LastSelectedPoseIndex;
    
    // Use the database's method to get the asset time
    const float AnimTime = Database->GetRealAssetTime(PoseIdx);
    
    // We need to find which asset this pose belongs to
    // This might require iterating through assets to find the one containing this pose
    for (int32 AssetIdx = 0; AssetIdx < Database->GetNumAnimationAssets(); AssetIdx++)
    {
        UObject* AssetObj = Database->GetAnimationAsset(AssetIdx);
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(AssetObj);
        
        if (AnimSeq && AnimSeq->HasRootMotion())
        {
            // Check if this animation contains our pose time
            // This is a simplified check - you might need more sophisticated logic
            if (AnimTime >= 0.f && AnimTime <= AnimSeq->GetPlayLength())
            {
                const float DeltaTime = 0.033f; // One frame at 30fps
                
                FAnimExtractContext Context;
                Context.bLooping = AnimSeq->bLoop;
                
                FTransform RootMotion = AnimSeq->ExtractRootMotionFromRange(
                    AnimTime,
                    AnimTime + DeltaTime,
                    Context
                );
                
                return RootMotion.GetTranslation().Size() / DeltaTime;
            }
        }
    }
    
    // Fallback to character velocity
    if (const APawn* Pawn = AnimInstance->TryGetPawnOwner())
    {
        return Pawn->GetVelocity().Size();
    }
    
    return 0.f;
}


TArray<float> UMMPoseSearchUtils::AnalyzeDatabaseSpeeds(const UPoseSearchDatabase* Database)
{
    TArray<float> DetectedSpeeds;
    
    if (!Database)
    {
        return DetectedSpeeds;
    }
    
    const int32 NumAssets = Database->GetNumAnimationAssets();
    
    UE_LOG(LogTemp, Warning, TEXT("Analyzing %d assets in database"), NumAssets);
    
    for (int32 AssetIdx = 0; AssetIdx < NumAssets; AssetIdx++)
    {
        UObject* AssetObj = Database->GetAnimationAsset(AssetIdx);
        
        if (AssetObj)
        {
            UE_LOG(LogTemp, Warning, TEXT("Asset %d: %s (%s)"), 
                AssetIdx, 
                *AssetObj->GetName(),
                *AssetObj->GetClass()->GetName());
        }
        
        UAnimSequence* AnimSeq = Cast<UAnimSequence>(AssetObj);
        
        if (!AnimSeq)
        {
            UE_LOG(LogTemp, Warning, TEXT("  Skipped - not an AnimSequence"));
            continue;
        }
        
        if (!AnimSeq->HasRootMotion())
        {
            UE_LOG(LogTemp, Warning, TEXT("  Skipped - no root motion"));
            continue;
        }
        
        const float AnimLength = AnimSeq->GetPlayLength();
        const int32 NumSamples = FMath::Max(10, FMath::CeilToInt(AnimLength * 30.f));
        
        TArray<float> AnimSpeeds;
        
        for (int32 i = 0; i < NumSamples - 1; i++)
        {
            double StartTime = (i / static_cast<double>(NumSamples)) * AnimLength;
            double EndTime = ((i + 1) / static_cast<double>(NumSamples)) * AnimLength;
            double SampleDeltaTime = EndTime - StartTime;
            
            if (SampleDeltaTime <= 0.001) continue;
            
            FAnimExtractContext Context;
            Context.bLooping = AnimSeq->bLoop;
            
            FTransform RootMotion = AnimSeq->ExtractRootMotionFromRange(
                StartTime,
                EndTime,
                Context
            );
            
            float Speed = RootMotion.GetTranslation().Size() / SampleDeltaTime;
            
            if (Speed > 10.f)
            {
                AnimSpeeds.Add(Speed);
            }
        }
        
        // Calculate steady-state speed for this animation
        float SteadyStateSpeed = 0.f;
        if (AnimSpeeds.Num() > 10)
        {
            // Sort speeds first
            AnimSpeeds.Sort();
            
            // Take middle 60% to avoid acceleration/deceleration phases
            int32 StartIdx = AnimSpeeds.Num() * 0.2f;
            int32 EndIdx = AnimSpeeds.Num() * 0.8f;
            
            for (int32 i = StartIdx; i < EndIdx; i++)
            {
                SteadyStateSpeed += AnimSpeeds[i];
            }
            SteadyStateSpeed /= (EndIdx - StartIdx);
            
            UE_LOG(LogTemp, Warning, TEXT("  Animation '%s': Steady-State Speed = %.1f cm/s (samples: %d)"),
                *AnimSeq->GetName(), SteadyStateSpeed, AnimSpeeds.Num());
            
            DetectedSpeeds.Add(SteadyStateSpeed);
        }
        else if (AnimSpeeds.Num() > 0)
        {
            // Fallback for short animations - use median
            AnimSpeeds.Sort();
            SteadyStateSpeed = AnimSpeeds[AnimSpeeds.Num() / 2];
            
            UE_LOG(LogTemp, Warning, TEXT("  Animation '%s': Median Speed = %.1f cm/s (samples: %d)"),
                *AnimSeq->GetName(), SteadyStateSpeed, AnimSpeeds.Num());
            
            DetectedSpeeds.Add(SteadyStateSpeed);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  Animation '%s': No valid speed samples"),
                *AnimSeq->GetName());
        }
    }
    
    // Sort and remove duplicates
    DetectedSpeeds.Sort();
    
    // Log all detected speeds before clustering
    UE_LOG(LogTemp, Warning, TEXT("Raw detected speeds (%d total):"), DetectedSpeeds.Num());
    for (float Speed : DetectedSpeeds)
    {
        UE_LOG(LogTemp, Warning, TEXT("  %.1f cm/s"), Speed);
    }
    
    // Find clusters with larger threshold for better band separation
    TArray<float> ClusteredSpeeds;
    const float ClusterThreshold = 100.f;  // Increased from 50.f
    
    for (float Speed : DetectedSpeeds)
    {
        bool bFoundCluster = false;
        for (float& ClusterSpeed : ClusteredSpeeds)
        {
            if (FMath::Abs(Speed - ClusterSpeed) < ClusterThreshold)
            {
                // Average into cluster
                ClusterSpeed = (ClusterSpeed + Speed) * 0.5f;
                bFoundCluster = true;
                break;
            }
        }
        
        if (!bFoundCluster)
        {
            ClusteredSpeeds.Add(Speed);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Database '%s' contains %d speed clusters:"), 
        *Database->GetName(), ClusteredSpeeds.Num());
    
    for (int32 i = 0; i < ClusteredSpeeds.Num(); i++)
    {
        UE_LOG(LogTemp, Warning, TEXT("  Speed %d: %.1f cm/s"), 
            i, ClusteredSpeeds[i]);
    }
    
    return ClusteredSpeeds;
}


float UMMPoseSearchUtils::GetSpeedMatchRatio(const UMMAnimInstance* AnimInstance)
{
    float AnimSpeed = GetMotionMatchingAnimationSpeed(AnimInstance);
    float CharSpeed = AnimInstance ? AnimInstance->GetSpeed() : 0.f;
    
    if (CharSpeed > 10.f)
    {
        return AnimSpeed / CharSpeed;
    }
    
    return 1.0f;
}

void UMMPoseSearchUtils::InitializeSpeedMatchingFromDatabase(FDynamicSpeedAnalysis& InOutAnalysis,
    const UPoseSearchDatabase* Database)
{
    InOutAnalysis.Reset();
    
    TArray<float> DatabaseSpeeds = AnalyzeDatabaseSpeeds(Database);
    
    for (float Speed : DatabaseSpeeds)
    {
        FSpeedCluster Cluster;
        Cluster.CenterSpeed = Speed;
        Cluster.MinSpeed = Speed * 0.9f;  // 10% tolerance
        Cluster.MaxSpeed = Speed * 1.1f;
        Cluster.SampleCount = 1;
        Cluster.Weight = 1.0f;
        InOutAnalysis.DetectedClusters.Add(Cluster);
    }
}

bool UMMPoseSearchUtils::IsSpeedMatchingValid(const FDynamicSpeedAnalysis& Analysis, float CharacterSpeed)
{
    // Check if character speed matches any cluster
    for (const FSpeedCluster& Cluster : Analysis.DetectedClusters)
    {
        if (Cluster.Contains(CharacterSpeed))
        {
            return true;
        }
    }
    
    return false;
}

float UMMPoseSearchUtils::GetPoseSearchQuerySpeed(const FTransformTrajectory& Trajectory)
{
    if (Trajectory.Samples.Num() < 2)
    {
        return 0.f;
    }
    
    // Find current (t=0) and next future sample
    int32 CurrentIdx = INDEX_NONE;
    int32 NextIdx = INDEX_NONE;
    
    for (int32 i = 0; i < Trajectory.Samples.Num(); i++)
    {
        if (FMath::IsNearlyZero(Trajectory.Samples[i].TimeInSeconds))
        {
            CurrentIdx = i;
        }
        else if (Trajectory.Samples[i].TimeInSeconds > 0.f && NextIdx == INDEX_NONE)
        {
            NextIdx = i;
        }
    }
    
    if (CurrentIdx != INDEX_NONE && NextIdx != INDEX_NONE)
    {
        const float DeltaTime = Trajectory.Samples[NextIdx].TimeInSeconds;
        const float Distance = FVector::Distance(
            Trajectory.Samples[CurrentIdx].Position,
            Trajectory.Samples[NextIdx].Position
        );
        
        if (DeltaTime > 0.001f)
        {
            return Distance / DeltaTime;
        }
    }
    
    return 0.f;
}


void UMMPoseSearchUtils::DrawSpeedMatchDebug(
    const UObject* WorldContextObject,
    const FVector& Location,
    const FDynamicSpeedAnalysis& Analysis,
    const FDynamicSpeedMatchConfig& Config)
{
    if (!Config.bDebugDraw)
    {
        return;
    }
    
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    const float BarHeight = 200.0f;
    const float MaxSpeed = 800.0f;
    
    // Draw speed clusters as vertical bars
    for (int32 i = 0; i < Analysis.DetectedClusters.Num(); i++)
    {
        const float BarWidth = 20.0f;
        const FSpeedCluster& Cluster = Analysis.DetectedClusters[i];
        
        float ClusterHeight = (Cluster.CenterSpeed / MaxSpeed) * BarHeight;
        FVector BarLocation = Location + FVector(i * 30.0f, 0, ClusterHeight * 0.5f);
        
        FColor Color = (i == Analysis.CurrentClusterIndex) ? FColor::Green : FColor::Yellow;
        DrawDebugBox(World, BarLocation, FVector(BarWidth * 0.5f, 2.0f, ClusterHeight * 0.5f),
                    Color, false, -1.0f, 0, 2.0f);
        
        // Draw cluster range
        float MinHeight = (Cluster.MinSpeed / MaxSpeed) * BarHeight;
        float MaxHeight = (Cluster.MaxSpeed / MaxSpeed) * BarHeight;
        DrawDebugLine(World, 
                     Location + FVector(i * 30.0f, 0, MinHeight),
                     Location + FVector(i * 30.0f, 0, MaxHeight),
                     FColor::White, false, -1.0f, 0, 1.0f);
        
        // Draw label
        DrawDebugString(World, BarLocation + FVector(0, 0, ClusterHeight * 0.5f + 20.0f),
                       FString::Printf(TEXT("%.0f"), Cluster.CenterSpeed),
                       nullptr, Color, 0.0f, true);
    }
    
    // Draw current actual speed
    float ActualHeight = (Analysis.CurrentActualSpeed / MaxSpeed) * BarHeight;
    DrawDebugLine(World,
                 Location + FVector(-30.0f, 0, ActualHeight),
                 Location + FVector(Analysis.DetectedClusters.Num() * 30.0f, 0, ActualHeight),
                 FColor::Red, false, -1.0f, 0, 3.0f);
    
    // Draw current target speed
    float TargetHeight = (Analysis.CurrentTargetSpeed / MaxSpeed) * BarHeight;
    DrawDebugLine(World,
                 Location + FVector(-30.0f, 0, TargetHeight),
                 Location + FVector(Analysis.DetectedClusters.Num() * 30.0f, 0, TargetHeight),
                 FColor::Blue, false, -1.0f, 0, 3.0f);
    
    // Draw text info
    DrawDebugString(World, Location + FVector(0, 0, BarHeight + 50.0f),
                   FString::Printf(TEXT("Actual: %.1f | Target: %.1f | Cluster: %d"),
                                  Analysis.CurrentActualSpeed,
                                  Analysis.CurrentTargetSpeed,
                                  Analysis.CurrentClusterIndex),
                   nullptr, FColor::White, 0.0f, true);
}

TArray<FVector> UMMPoseSearchUtils::CalculateDerivedVelocities(const FTransformTrajectory& Trajectory)
{
    TArray<FVector> Velocities;
    
    for (int32 i = 1; i < Trajectory.Samples.Num(); i++)
    {
        const FTransformTrajectorySample& Sample = Trajectory.Samples[i];
        const FTransformTrajectorySample& PrevSample = Trajectory.Samples[i - 1];
        
        const float TimeDelta = Sample.TimeInSeconds - PrevSample.TimeInSeconds;
        if (TimeDelta > KINDA_SMALL_NUMBER)
        {
            FVector Velocity = (Sample.Position - PrevSample.Position) / TimeDelta;
            Velocities.Add(Velocity);
        }
    }
    
    return Velocities;
}

float UMMPoseSearchUtils::MeasureFootSliding(
    const USkeletalMeshComponent* Mesh,
    const FName& FootBoneName,
    float DeltaTime)
{
    if (!Mesh)
    {
        return 0.f;
    }
    
    static TMap<const USkeletalMeshComponent*, FVector> LastFootPositions;
    
    const FVector CurrentFootPos = Mesh->GetSocketLocation(FootBoneName);
    
    float SlideAmount = 0.f;
    if (FVector* LastPos = LastFootPositions.Find(Mesh))
    {
        // Only measure when foot should be planted (simplified check)
        const float FootHeight = CurrentFootPos.Z - Mesh->GetComponentLocation().Z;
        if (FootHeight < 10.0f) // Foot near ground
        {
            SlideAmount = (*LastPos - CurrentFootPos).Size2D() / DeltaTime;
        }
        *LastPos = CurrentFootPos;
    }
    else
    {
        LastFootPositions.Add(Mesh, CurrentFootPos);
    }
    
    return SlideAmount;
}

float UMMPoseSearchUtils::GetSpeedMatchQuality(
    float ActualSpeed,
    float TargetSpeed,
    float AnimationSpeed)
{
    if (FMath::IsNearlyZero(TargetSpeed))
    {
        return 0.f;
    }
    
    // Calculate how close the animation speed is to target
    float AnimError = FMath::Abs(AnimationSpeed - TargetSpeed) / TargetSpeed;
    
    // Calculate how close the actual speed is to target
    float ActualError = FMath::Abs(ActualSpeed - TargetSpeed) / TargetSpeed;
    
    // Combined quality score (0-1, where 1 is perfect match)
    float Quality = 1.0f - FMath::Min((AnimError + ActualError) * 0.5f, 1.0f);
    
    return FMath::Clamp(Quality, 0.f, 1.f);
}

// ============================================================================
// Private Implementation
// ============================================================================

void UMMPoseSearchUtils::PerformKMeansClustering(
    const TArray<float>& Speeds,
    int32 K,
    TArray<FSpeedCluster>& OutClusters,
    int32 MaxIterations)
{
    OutClusters.Empty();
    
    if (Speeds.Num() < K)
    {
        // Fewer samples than clusters requested
        K = Speeds.Num();
    }
    
    // Initialize cluster centers using k-means++ method
    TArray<float> Centers;
    Centers.Add(Speeds[FMath::RandRange(0, Speeds.Num() - 1)]);
    
    for (int32 i = 1; i < K; i++)
    {
        float MaxDist = 0.f;
        int32 BestIdx = 0;
        
        for (int32 j = 0; j < Speeds.Num(); j++)
        {
            float MinDistToCenter = FLT_MAX;
            for (float Center : Centers)
            {
                float Dist = FMath::Abs(Speeds[j] - Center);
                MinDistToCenter = FMath::Min(MinDistToCenter, Dist);
            }
            
            if (MinDistToCenter > MaxDist)
            {
                MaxDist = MinDistToCenter;
                BestIdx = j;
            }
        }
        
        Centers.Add(Speeds[BestIdx]);
    }
    
    // Perform k-means iterations
    TArray<TArray<float>> ClusterMembers;
    ClusterMembers.SetNum(K);
    
    for (int32 Iter = 0; Iter < MaxIterations; Iter++)
    {
        // Clear cluster members
        for (auto& Members : ClusterMembers)
        {
            Members.Empty();
        }
        
        // Assign points to nearest cluster
        for (float Speed : Speeds)
        {
            int32 NearestCluster = 0;
            float MinDist = FLT_MAX;
            
            for (int32 c = 0; c < Centers.Num(); c++)
            {
                float Dist = FMath::Abs(Speed - Centers[c]);
                if (Dist < MinDist)
                {
                    MinDist = Dist;
                    NearestCluster = c;
                }
            }
            
            ClusterMembers[NearestCluster].Add(Speed);
        }
        
        // Update cluster centers
        bool bConverged = true;
        for (int32 c = 0; c < K; c++)
        {
            if (ClusterMembers[c].Num() > 0)
            {
                float NewCenter = 0.f;
                for (float Speed : ClusterMembers[c])
                {
                    NewCenter += Speed;
                }
                NewCenter /= ClusterMembers[c].Num();
                
                if (FMath::Abs(NewCenter - Centers[c]) > 1.0f)
                {
                    bConverged = false;
                }
                
                Centers[c] = NewCenter;
            }
        }
        
        if (bConverged)
        {
            break;
        }
    }
    
    // Create cluster structures
    for (int32 c = 0; c < Centers.Num(); c++)
    {
        if (ClusterMembers[c].Num() > 0)
        {
            FSpeedCluster Cluster;
            Cluster.CenterSpeed = Centers[c];
            Cluster.SampleCount = ClusterMembers[c].Num();
            
            // Calculate min/max
            Cluster.MinSpeed = FLT_MAX;
            Cluster.MaxSpeed = -FLT_MAX;
            for (float Speed : ClusterMembers[c])
            {
                Cluster.MinSpeed = FMath::Min(Cluster.MinSpeed, Speed);
                Cluster.MaxSpeed = FMath::Max(Cluster.MaxSpeed, Speed);
            }
            
            Cluster.Weight = static_cast<float>(Cluster.SampleCount) / Speeds.Num();
            
            OutClusters.Add(Cluster);
        }
    }
}

int32 UMMPoseSearchUtils::FindNearestCluster(float Speed, const TArray<FSpeedCluster>& Clusters)
{
    int32 NearestIdx = -1;
    float MinDistance = FLT_MAX;
    
    for (int32 i = 0; i < Clusters.Num(); i++)
    {
        float Distance = Clusters[i].DistanceTo(Speed);
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            NearestIdx = i;
        }
    }
    
    return NearestIdx;
}

void UMMPoseSearchUtils::MergeSimilarClusters(TArray<FSpeedCluster>& Clusters, float MergeThreshold)
{
    bool bMerged = true;
    
    while (bMerged && Clusters.Num() > 1)
    {
        bMerged = false;
        
        for (int32 i = 0; i < Clusters.Num() - 1; i++)
        {
            for (int32 j = i + 1; j < Clusters.Num(); j++)
            {
                float Distance = FMath::Abs(Clusters[i].CenterSpeed - Clusters[j].CenterSpeed);
                
                if (Distance < MergeThreshold)
                {
                    // Merge clusters
                    int32 TotalSamples = Clusters[i].SampleCount + Clusters[j].SampleCount;
                    Clusters[i].CenterSpeed = (Clusters[i].CenterSpeed * Clusters[i].SampleCount +
                                              Clusters[j].CenterSpeed * Clusters[j].SampleCount) / TotalSamples;
                    Clusters[i].MinSpeed = FMath::Min(Clusters[i].MinSpeed, Clusters[j].MinSpeed);
                    Clusters[i].MaxSpeed = FMath::Max(Clusters[i].MaxSpeed, Clusters[j].MaxSpeed);
                    Clusters[i].SampleCount = TotalSamples;
                    Clusters[i].Weight = Clusters[i].Weight + Clusters[j].Weight;
                    
                    Clusters.RemoveAt(j);
                    bMerged = true;
                    break;
                }
            }
            
            if (bMerged) break;
        }
    }
}

