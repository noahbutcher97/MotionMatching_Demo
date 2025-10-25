// Animation/.cpp - OPTIMIZED WITH CACHING

#include "Animation/MMAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/MMCharacter.h"
#include "FunctionLibraries/MMPoseSearchUtils.h"
#include "PoseSearch/MMPoseSearchSpeedAnalyzer.h"

void UMMAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    
    MMCharacter = Cast<AMMCharacter>(TryGetPawnOwner());
    if (MMCharacter)
    {
        CachedMovementComponent = MMCharacter->GetCharacterMovement();
    }
    
    MotionHistory.Initialize(MotionBufferConfig);
    LocalTimeSeconds = 0.f;
    
    if (CachedMovementComponent && MMCharacter)
    {
        TrajectoryConfig.RotationConfig.InitializeOnPlay(MMCharacter, CachedMovementComponent);
    }
    
    // Initialize async trace state
    bIsGroundTracePending = false;
    bIsEdgeTracePending = false;
    bIsWallTracePending = false;
    bIsCeilingTracePending = false;
    
    MotionHistory.ResetSmoothingState();
    LastMeshRotation = FQuat::Identity;
    LastControlYaw = 0.f;
    LastLocalVelocity = FVector::ZeroVector;
    DirectionChangeAccumulator = 0.f;
    
    if (MotionMatchingDatabase)
    {
        TArray<float> DatabaseSpeeds = UMMPoseSearchUtils::AnalyzeDatabaseSpeeds(MotionMatchingDatabase);
        
        if (DatabaseSpeeds.Num() > 0)
        {
            SpeedAnalysis.Reset();
            for (float Speed : DatabaseSpeeds)
            {
                FSpeedCluster Cluster;
                Cluster.CenterSpeed = Speed;
                Cluster.MinSpeed = Speed * 0.85f;
                Cluster.MaxSpeed = Speed * 1.15f;
                Cluster.SampleCount = 1;
                Cluster.Weight = 1.0f;
                SpeedAnalysis.DetectedClusters.Add(Cluster);
            }
            bEnableSpeedMatching = true;
        }
        else
        {
            bEnableSpeedMatching = false;
        }
    }
    else
    {
        bEnableSpeedMatching = false;
    }
    
    SpeedSampleTimer = 0.f;
    SpeedSampleHistory.Empty();
    if (MotionMatchingDatabase && bEnableSpeedMatching)
    {
        InitializeSpeedClusters();
    }
}



void UMMAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!MMCharacter)
    {
        MMCharacter = Cast<AMMCharacter>(TryGetPawnOwner());
        if (!MMCharacter) return;
    }
    
    DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.001f, 0.1f);
    LocalTimeSeconds += DeltaSeconds;

    // This order is critical
    ExtractAnimationContext(DeltaSeconds);
    UpdateEnvironmentCache(DeltaSeconds);
    GatherMotionData(DeltaSeconds);
    UpdateRotationSystem(DeltaSeconds);
    
    // SEMANTIC SPEED MATCHING - Replace old cluster detection with semantic analysis
    if (bEnableSpeedMatching)
    {
        // Initialize semantic clusters if not done yet
        if (CachedSemanticClusters.Num() == 0 && MotionMatchingDatabase)
        {
            InitializeSpeedClusters();
        }
        
        // Convert semantic clusters to the format GetTargetAnimationSpeed expects
        if (CachedSemanticClusters.Num() > 0)
        {
            SpeedAnalysis.DetectedClusters = ConvertSemanticToSpeedClusters(CachedSemanticClusters);
        }
        
        // Use existing target speed calculation for smooth transitions
        if (SpeedAnalysis.DetectedClusters.Num() > 0)
        {
            float CurrentSpeed = MMCharacter->GetVelocity().Size();
            
            float TargetSpeed = UMMPoseSearchUtils::GetTargetAnimationSpeed(
                CurrentSpeed,
                SpeedAnalysis,
                SpeedMatchConfig,
                DeltaSeconds
            );
            
            SpeedAnalysis.CurrentTargetSpeed = TargetSpeed;
        }
    }
    
    BuildTrajectory(DeltaSeconds);
    
    CurrentMotionAnalysis = MotionHistory.AnalyzeMotion(TrajectoryConfig);
    CurrentMotionContext = MotionHistory.GetCharacterContext(
        MMCharacter->GetControlYawRotation(),
        GetMeshRotation(),
        TrajectoryConfig
    );
    
    const FMotionSample CurrentSample = MotionHistory.GetNewestSample();
    const FVector LocalVelocity = GetMeshRotation().UnrotateVector(CurrentSample.WorldVelocity);
    
    if (LastLocalVelocity.Size() > 50.f && LocalVelocity.Size() > 50.f)
    {
        const float VelDot = FVector::DotProduct(
            LastLocalVelocity.GetSafeNormal(),
            LocalVelocity.GetSafeNormal()
        );
        const float AngleChange = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(VelDot, -1.f, 1.f)));
        
        const bool bWasForward = FMath::Abs(LastLocalVelocity.Y / LastLocalVelocity.Size()) < 0.3f;
        const bool bNowDiagonal = FMath::Abs(LocalVelocity.Y / LocalVelocity.Size()) > 0.4f;
        
        if (bWasForward && bNowDiagonal)
        {
            DirectionChangeAccumulator = 1.0f;
        }
        else if (AngleChange > 20.f)
        {
            DirectionChangeAccumulator = FMath::Min(DirectionChangeAccumulator + DeltaSeconds * 5.f, 1.f);
        }
        else
        {
            DirectionChangeAccumulator = FMath::Max(DirectionChangeAccumulator - DeltaSeconds * 2.f, 0.f);
        }
    }
    
    LastLocalVelocity = LocalVelocity;
    DirectionChangeWeight = DirectionChangeAccumulator;
    
    if (DirectionChangeWeight > 0.5f)
    {
        bForceMotionMatchingUpdate = true;
        MotionMatchingUpdateUrgency = DirectionChangeWeight;
    }
    else
    {
        bForceMotionMatchingUpdate = false;
        MotionMatchingUpdateUrgency = 0.0f;
    }
    
    if (MotionMatchingDatabase && TrajectoryForMM.Samples.Num() > 0)
    {
        bShouldTriggerMotionMatching = true;
    }

    if (bDebugDrawTrajectory) DebugDrawTrajectory();
    if (bDebugDrawRotation) DebugDrawRotation();
    if (bDebugDrawEnvironmentCache) DebugDrawEnvironmentCache();
    
    if (SpeedMatchConfig.bDebugDraw && bEnableSpeedMatching)
    {
        UMMPoseSearchUtils::DrawSpeedMatchDebug(
            GetWorld(),
            MMCharacter->GetActorLocation() + FVector(0, 200, 0),
            SpeedAnalysis,
            SpeedMatchConfig
        );
    }
}


void UMMAnimInstance::InitializeSpeedClusters()
{
    if (!MotionMatchingDatabase)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot initialize speed clusters: No database assigned"));
        return;
    }
    
    // Create analyzer if needed
    if (!SpeedAnalyzer)
    {
        SpeedAnalyzer = NewObject<UMMPoseSearchSpeedAnalyzer>(this);
        SpeedAnalyzer->Initialize();
    }
    
    // Configure clustering
    FSemanticSpeedConfig Config;
    Config.MaxClusters = 10;                   // Max number of speed tiers
    Config.MinAnimationsPerCluster = 2;        // Minimum anims per cluster
    Config.ClusterMergeThreshold = 75.0f;      // Merge if speeds within this range (cm/s)
    Config.SemanticWeight = 0.4f;              // Weight for semantic similarity (0-1)
    Config.bVerboseLogging = true;             // Enable for initial testing
    Config.bEnableCaching = true;              // Cache results
    
    // Generate clusters from database
    const double StartTime = FPlatformTime::Seconds();
    
    CachedSemanticClusters = SpeedAnalyzer->GenerateSemanticSpeedClusters(
        MotionMatchingDatabase, 
        Config
    );
    
    const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
    
    if (CachedSemanticClusters.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Speed Clustering Complete: %d clusters generated in %.2fms"),
            CachedSemanticClusters.Num(),
            ElapsedTime * 1000.0);
        
        // Log cluster details
        for (int32 i = 0; i < CachedSemanticClusters.Num(); ++i)
        {
            const FSemanticSpeedCluster& Cluster = CachedSemanticClusters[i];
            
            UE_LOG(LogTemp, Log, TEXT("  Cluster %d: %s"), i, *Cluster.ClusterName);
            UE_LOG(LogTemp, Log, TEXT("    Speed: %.1f cm/s (%.1f - %.1f)"),
                Cluster.CenterSpeed,
                Cluster.MinSpeed,
                Cluster.MaxSpeed);
            UE_LOG(LogTemp, Log, TEXT("    Animations: %d, Intent: %s, Level: %s"),
                Cluster.AnimationCount,
                *UEnum::GetValueAsString(Cluster.PrimaryIntent),
                *UEnum::GetValueAsString(Cluster.SpeedLevel));
            UE_LOG(LogTemp, Log, TEXT("    Thresholds: Entry=%.1f, Exit=%.1f, Hysteresis=%.1f"),
                Cluster.EntryThreshold,
                Cluster.ExitThreshold,
                Cluster.HysteresisWidth);
        }
        
        // Memory usage report
        const int32 MemoryUsage = SpeedAnalyzer->GetMemoryUsage();
        UE_LOG(LogTemp, Log, TEXT("  Memory Usage: %.2f KB"), MemoryUsage / 1024.0f);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Speed Clustering Failed: No clusters generated"));
    }
}


TArray<FSpeedCluster> UMMAnimInstance::ConvertSemanticToSpeedClusters(
    const TArray<FSemanticSpeedCluster>& SemanticClusters)
{
    TArray<FSpeedCluster> Result;
    
    if (SemanticClusters.Num() == 0)
    {
        return Result;
    }
    
    // Calculate total animations for weight normalization
    float TotalAnimations = 0.0f;
    for (const FSemanticSpeedCluster& Semantic : SemanticClusters)
    {
        TotalAnimations += Semantic.AnimationCount;
    }
    
    // Convert each semantic cluster
    for (const FSemanticSpeedCluster& Semantic : SemanticClusters)
    {
        FSpeedCluster Cluster;
        Cluster.CenterSpeed = Semantic.CenterSpeed;
        Cluster.MinSpeed = Semantic.MinSpeed;
        Cluster.MaxSpeed = Semantic.MaxSpeed;
        Cluster.SampleCount = Semantic.AnimationCount;
        
        // Normalize weight based on animation count
        Cluster.Weight = TotalAnimations > 0.0f 
            ? Semantic.AnimationCount / TotalAnimations 
            : 0.0f;
        
        Result.Add(Cluster);
    }
    
    return Result;
}

void UMMAnimInstance::UpdateRotationSystem(float DeltaSeconds)
{
    if (!MMCharacter || !CachedMovementComponent) return;
    
    FRotationConfig& RotConfig = TrajectoryConfig.RotationConfig;
    
    if (RotConfig.bUseRawInputForRotation)
    {
        RotConfig.RawInputForRotation = MMCharacter->CurrentMoveInput;
    }
    
    if (RotConfig.bUseCharacterMovementRotation)
    {
        static float ConfigCheckTimer = 0.f;
        ConfigCheckTimer += DeltaSeconds;
        if (ConfigCheckTimer > 1.0f)
        {
            RotConfig.ConfigureCMC(CachedMovementComponent);
            ConfigCheckTimer = 0.f;
        }
        
        RotConfig.CurrentYaw = MMCharacter ->GetActorRotation().Yaw;
    }
    
    if (MotionHistory.GetSampleCount() > 1)
    {
        FMotionSample PrevSample;
        if (MotionHistory.GetSampleByAge(1, PrevSample))
        {
            RotConfig.UpdateAlphaCorrection(RotConfig.BlendAlpha, PrevSample.RotationAlpha, DeltaSeconds);
        }
    }
}



void UMMAnimInstance::UpdateEnvironmentCache(float DeltaSeconds)
{
    if (!MMCharacter || !GetWorld() || !EnvironmentCacheConfig.bUseAsyncTraces) return;
    
    // First, process any completed traces from the previous frame
    ProcessAsyncTraces();
    
    const FVector CharPos = MMCharacter->GetActorLocation();
    
    // Try to use spatially hashed cache to avoid re-scanning the same area
    if (TryGetCachedEnvironment(CharPos, EnvironmentCache))
    {
        ScanStats.CachedTraces++;
        EnvContext = EnvironmentCache.Data;
        return;
    }

    // If no valid cache, determine LOD and request new async scans
    const EEnvironmentLOD SelectedLOD = SelectEnvironmentLOD();
    EnvironmentCache.CurrentLOD = SelectedLOD;
    
    RequestAsyncScans(SelectedLOD, CharPos, MMCharacter->GetActorForwardVector());

    // IMPORTANT: Use the *last known* data for the current frame.
    // The results of the new traces will be available on the next frame.
    EnvContext = EnvironmentCache.Data;
    
    ScanStats.TotalTraces++;
    ScanStats.CacheHitRate = EnvironmentCache.GetCacheHitRate();
    ScanStats.CurrentLOD = SelectedLOD;
}

void UMMAnimInstance::RequestAsyncScans(EEnvironmentLOD LOD, const FVector& CharPos, const FVector& CharForward)
{
    UWorld* World = GetWorld();
    if (!World) return;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(MMCharacter);
    Params.bTraceComplex = false;

    // --- Ground Trace ---
    if (!bIsGroundTracePending && !EnvironmentCache.IsGroundValid(LocalTimeSeconds, EnvironmentCacheConfig.GroundCacheTTL))
    {
        const FVector GroundTraceStart = CharPos + FVector(0, 0, 50.f);
        const FVector GroundTraceEnd = CharPos - FVector(0, 0, 100.f);
        GroundTraceHandle = World->AsyncLineTraceByChannel(EAsyncTraceType::Single, GroundTraceStart, GroundTraceEnd, ECC_Visibility, Params);
        bIsGroundTracePending = true;
        ScanStats.LastFrameTraces++;
    }

    if (LOD == EEnvironmentLOD::Minimal) return;

    // --- Wall Sweep (LOD0 and LOD1) ---
    // REPLACES 8 line traces with 1 sphere sweep
    if (!bIsWallTracePending && !EnvironmentCache.IsWallsValid(LocalTimeSeconds, EnvironmentCacheConfig.WallCacheTTL))
    {
        const float WallCheckRadius = (LOD == EEnvironmentLOD::Full) ? 200.f : 150.f;
        const FVector WallTraceStart = CharPos + FVector(0, 0, 50.f);
        WallTraceHandle = World->AsyncSweepByChannel(EAsyncTraceType::Single, WallTraceStart, WallTraceStart, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(WallCheckRadius), Params);
        bIsWallTracePending = true;
        ScanStats.LastFrameTraces++;
    }

    if (LOD == EEnvironmentLOD::Reduced) return;

    // --- Edge and Ceiling Traces (LOD0 Only) ---
    if (!bIsEdgeTracePending && !EnvironmentCache.IsEdgeValid(LocalTimeSeconds, EnvironmentCacheConfig.EdgeCacheTTL))
    {
        const FVector EdgeCheckPos = CharPos + CharForward * 150.f;
        const FVector EdgeTraceStart = EdgeCheckPos + FVector(0, 0, 10.f);
        const FVector EdgeTraceEnd = EdgeCheckPos - FVector(0, 0, 200.f);
        EdgeTraceHandle = World->AsyncLineTraceByChannel(EAsyncTraceType::Single, EdgeTraceStart, EdgeTraceEnd, ECC_Visibility, Params);
        bIsEdgeTracePending = true;
        ScanStats.LastFrameTraces++;
    }
    
    if (!bIsCeilingTracePending && !EnvironmentCache.IsCeilingValid(LocalTimeSeconds, EnvironmentCacheConfig.CeilingCacheTTL))
    {
        const FVector CeilingTraceStart = CharPos + FVector(0, 0, 50.f);
        const FVector CeilingTraceEnd = CharPos + FVector(0, 0, 300.f);
        CeilingTraceHandle = World->AsyncLineTraceByChannel(EAsyncTraceType::Single, CeilingTraceStart, CeilingTraceEnd, ECC_Visibility, Params);
        bIsCeilingTracePending = true;
        ScanStats.LastFrameTraces++;
    }
}
void UMMAnimInstance::ProcessAsyncTraces()
{
    if (!IsValid(MMCharacter)) 
    {
        // Cancel all pending traces
        bIsGroundTracePending = false;
        bIsEdgeTracePending = false;
        bIsWallTracePending = false;
        bIsCeilingTracePending = false;
        return;
    }
    UWorld* World = GetWorld();
    if (!World) return;

    FTraceDatum Datum;
    
    // --- Process Ground Trace ---
    if (bIsGroundTracePending && World->QueryTraceData(GroundTraceHandle, Datum))
    {
        if (Datum.OutHits.Num() > 0)
        {
            const FHitResult& Hit = Datum.OutHits[0];
            EnvironmentCache.Data.GroundNormal = Hit.Normal;
            EnvironmentCache.Data.GroundSlope = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Hit.Normal, FVector::UpVector)));
        }
        else
        {
            EnvironmentCache.Data.GroundNormal = FVector::UpVector;
            EnvironmentCache.Data.GroundSlope = 0.f;
        }
        EnvironmentCache.GroundUpdateTime = LocalTimeSeconds;
        bIsGroundTracePending = false;
        ScanStats.AsyncTraces++;
    }
    
    // --- Process Wall Sweep ---
    if (bIsWallTracePending && World->QueryTraceData(WallTraceHandle, Datum))
    {
        if (Datum.OutHits.Num() > 0)
        {
            const FHitResult& Hit = Datum.OutHits[0];
            EnvironmentCache.Data.NearestWallDistance = Hit.Distance;
            EnvironmentCache.Data.NearestWallNormal = Hit.Normal;
        }
        else
        {
            EnvironmentCache.Data.NearestWallDistance = 1000.f;
            EnvironmentCache.Data.NearestWallNormal = FVector::ZeroVector;
        }
        EnvironmentCache.WallsUpdateTime = LocalTimeSeconds;
        bIsWallTracePending = false;
        ScanStats.AsyncTraces++;
    }

    // --- Process Edge Trace ---
    if (bIsEdgeTracePending && World->QueryTraceData(EdgeTraceHandle, Datum))
    {
        EnvironmentCache.Data.bIsNearEdge = (Datum.OutHits.Num() == 0);
        EnvironmentCache.EdgeUpdateTime = LocalTimeSeconds;
        bIsEdgeTracePending = false;
        ScanStats.AsyncTraces++;
    }

    // --- Process Ceiling Trace ---
    if (bIsCeilingTracePending && World->QueryTraceData(CeilingTraceHandle, Datum))
    {
        if (Datum.OutHits.Num() > 0)
        {
            EnvironmentCache.Data.CeilingHeight = Datum.OutHits[0].Distance + 50.f;
            EnvironmentCache.Data.bIsInTightSpace = EnvironmentCache.Data.CeilingHeight < 180.f;
        }
        else
        {
            EnvironmentCache.Data.CeilingHeight = 250.f;
            EnvironmentCache.Data.bIsInTightSpace = false;
        }
        EnvironmentCache.CeilingUpdateTime = LocalTimeSeconds;
        bIsCeilingTracePending = false;
        ScanStats.AsyncTraces++;
    }

    // After processing, store the newly updated cache data
    StoreCachedEnvironment(MMCharacter->GetActorLocation(), EnvironmentCache);
}

EEnvironmentLOD UMMAnimInstance::SelectEnvironmentLOD() const
{
    if (!CurrentMotionContext.bIsMoving)
    {
        return EEnvironmentLOD::Minimal;
    }
    
    const float Speed = CurrentMotionContext.Speed;
    const float AngularVel = FMath::Abs(MotionHistory.EstimateAngularAcceleration(0.05f));
    
    if (Speed > EnvironmentCacheConfig.LOD0SpeedThreshold || 
        AngularVel > EnvironmentCacheConfig.LODAngularThreshold)
    {
        return EEnvironmentLOD::Full;
    }
    
    if (Speed > EnvironmentCacheConfig.LOD1SpeedThreshold)
    {
        return EEnvironmentLOD::Reduced;
    }
    
    return EEnvironmentLOD::Minimal;
}



FIntVector UMMAnimInstance::GetSpatialHashKey(const FVector& Position) const
{
    const float GridSize = EnvironmentCacheConfig.SpatialHashGridSize;
    return FIntVector(
        FMath::FloorToInt(Position.X / GridSize),
        FMath::FloorToInt(Position.Y / GridSize),
        FMath::FloorToInt(Position.Z / GridSize)
    );
}

bool UMMAnimInstance::TryGetCachedEnvironment(const FVector& Position, FCachedEnvironmentData& OutData)
{
    const FIntVector Key = GetSpatialHashKey(Position);
    
    if (FSpatialHashEntry* Entry = SpatialCache.Find(Key))
    {
        const float Distance = FVector::Distance(Position, Entry->Position);
        
        if (Distance < EnvironmentCacheConfig.SpatialReuseRadius)
        {
            const float Age = LocalTimeSeconds - Entry->TimeStamp;
            
            if (Age < EnvironmentCacheConfig.GroundCacheTTL * 2.f)
            {
                OutData = Entry->CachedData;
                OutData.CacheHits++;
                return true;
            }
        }
    }
    
    OutData.CacheMisses++;
    return false;
}

void UMMAnimInstance::StoreCachedEnvironment(const FVector& Position, const FCachedEnvironmentData& Data)
{
    const FIntVector Key = GetSpatialHashKey(Position);
    
    FSpatialHashEntry& Entry = SpatialCache.FindOrAdd(Key);
    Entry.Position = Position;
    Entry.CachedData = Data;
    Entry.TimeStamp = LocalTimeSeconds;
    
    if (SpatialCache.Num() > EnvironmentCacheConfig.MaxCacheEntries)
    {
        float OldestTime = FLT_MAX;
        FIntVector OldestKey;
        
        for (const auto& Pair : SpatialCache)
        {
            if (Pair.Value.TimeStamp < OldestTime)
            {
                OldestTime = Pair.Value.TimeStamp;
                OldestKey = Pair.Key;
            }
        }
        
        SpatialCache.Remove(OldestKey);
    }
}

void UMMAnimInstance::UpdatePerformanceStats(float DeltaSeconds)
{
    FrameCounter++;
    FrameDeltaAccumulator += DeltaSeconds;
    
    if (FrameDeltaAccumulator > 1.0f)
    {
        ScanStats.AverageTracesPerFrame = static_cast<float>(ScanStats.TotalTraces) / static_cast<float>(FrameCounter);
        
        FrameCounter = 0;
        FrameDeltaAccumulator = 0.f;
    }
}

FString UMMAnimInstance::GetPerformanceReport() const
{
    return FString::Printf(
        TEXT("=== PERFORMANCE REPORT ===\n")
        TEXT("Motion Buffer: %s\n")
        TEXT("Environment Cache:\n")
        TEXT("  LOD: %s\n")
        TEXT("  Hit Rate: %.1f%%\n")
        TEXT("  Traces/Frame: %.1f\n")
        TEXT("  Spatial Cache: %d entries\n")
        TEXT("Frame Stats:\n")
        TEXT("  Last Frame Traces: %d\n")
        TEXT("  Skipped Traces: %d\n")
        TEXT("  Async Traces: %d"),
        *MotionHistory.GetDebugStats(),
        *UEnum::GetValueAsString(EnvironmentCache.CurrentLOD),
        EnvironmentCache.GetCacheHitRate() * 100.f,
        ScanStats.AverageTracesPerFrame,
        SpatialCache.Num(),
        ScanStats.LastFrameTraces,
        ScanStats.SkippedTraces,
        ScanStats.AsyncTraces
    );
}

void UMMAnimInstance::DebugDrawEnvironmentCache() const
{
    if (!MMCharacter || !GetWorld()) return;
    
    UWorld* World = GetWorld();
    const FVector CharPos = MMCharacter->GetActorLocation();
    
    FColor LODColor;
    FString LODText;
    
    switch (EnvironmentCache.CurrentLOD)
    {
        case EEnvironmentLOD::Full:
            LODColor = FColor::Green;
            LODText = TEXT("FULL QUALITY");
            break;
        case EEnvironmentLOD::Reduced:
            LODColor = FColor::Yellow;
            LODText = TEXT("REDUCED");
            break;
        case EEnvironmentLOD::Minimal:
            LODColor = FColor::Red;
            LODText = TEXT("MINIMAL");
            break;
    }
    
    DrawDebugString(World, CharPos + FVector(0, 0, 200), 
        FString::Printf(TEXT("ENV LOD: %s"), *LODText),
        nullptr, LODColor, 0.f, true, 1.5f);
    
    const float CurrentTime = LocalTimeSeconds;
    
    if (EnvironmentCache.IsGroundValid(CurrentTime, EnvironmentCacheConfig.GroundCacheTTL))
    {
        DrawDebugSphere(World, CharPos - FVector(0, 0, 50), 10.f, 8, FColor::Green, false, 0.f);
    }
    else
    {
        DrawDebugSphere(World, CharPos - FVector(0, 0, 50), 10.f, 8, FColor::Red, false, 0.f);
    }
    
    if (EnvironmentCache.Data.NearestWallDistance < 200.f)
    {
        const FVector WallPoint = CharPos + (-EnvironmentCache.Data.NearestWallNormal * EnvironmentCache.Data.NearestWallDistance);
        FColor WallColor = EnvironmentCache.IsWallsValid(CurrentTime, EnvironmentCacheConfig.WallCacheTTL) 
            ? FColor::Green : FColor::Red;
        DrawDebugLine(World, CharPos, WallPoint, WallColor, false, 0.f, 0, 2.f);
    }
    
    const float GridSize = EnvironmentCacheConfig.SpatialHashGridSize;
    const FIntVector CurrentGrid = GetSpatialHashKey(CharPos);
    
    const FVector GridMin(
        CurrentGrid.X * GridSize,
        CurrentGrid.Y * GridSize,
        CurrentGrid.Z * GridSize
    );
    const FVector GridMax = GridMin + FVector(GridSize, GridSize, GridSize);
    DrawDebugBox(World, (GridMin + GridMax) * 0.5f, FVector(GridSize * 0.5f), 
        FColor::Cyan, false, 0.f, 0, 2.f);
    
    DrawDebugString(World, CharPos + FVector(0, 0, 250), 
        FString::Printf(TEXT("Cache Hit: %.1f%% | Traces: %d | Spatial: %d"),
            EnvironmentCache.GetCacheHitRate() * 100.f,
            ScanStats.LastFrameTraces,
            SpatialCache.Num()),
        nullptr, FColor::White, 0.f, true);
}


void UMMAnimInstance::GatherMotionData(float DeltaSeconds)
{
    if (!MMCharacter) return;
    
    FMotionSample Sample;
    Sample.TimeStamp = LocalTimeSeconds;
    
    Sample.WorldPosition = MMCharacter->GetActorLocation();
    Sample.WorldVelocity = MMCharacter->GetVelocity();
    
    if (CachedMovementComponent)
    {
        Sample.WorldAcceleration = CachedMovementComponent->GetCurrentAcceleration();
        Sample.CachedAcceleration = Sample.WorldAcceleration;
    }
    else if (MotionHistory.GetSampleCount() > 0)
    {
        const FMotionSample PrevSample = MotionHistory.GetNewestSample();
        Sample.WorldAcceleration = (Sample.WorldVelocity - PrevSample.WorldVelocity) / DeltaSeconds;
        Sample.CachedAcceleration = Sample.WorldAcceleration;
    }
    
    FQuat RawRotation = FQuat::Identity;
    if (USkeletalMeshComponent* Mesh = MMCharacter->GetMesh())
    {
        RawRotation = Mesh->GetComponentQuat();
    }
    else
    {
        RawRotation = MMCharacter->GetActorQuat();
        const float MeshOffset = MMCharacter->GetMeshYawOffset();
        if (FMath::Abs(MeshOffset) > 0.1f)
        {
            RawRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(MeshOffset)) * RawRotation;
        }
    }
    
    Sample.WorldRotation = RawRotation;
    
    if (MotionHistory.GetSampleCount() > 0)
    {
        const FQuat DeltaRot = Sample.WorldRotation * LastMeshRotation.Inverse();
        FVector Axis;
        float Angle;
        DeltaRot.ToAxisAndAngle(Axis, Angle);
        
        while (Angle > PI) Angle -= 2.f * PI;
        while (Angle < -PI) Angle += 2.f * PI;
        
        Sample.AngularVelocityRadians = Axis * (Angle / DeltaSeconds);
    }
    LastMeshRotation = Sample.WorldRotation;
    
    Sample.RawInput = MMCharacter->CurrentMoveInput;
    
    if (TrajectoryConfig.RotationConfig.bUseRawInputForRotation)
    {
        TrajectoryConfig.RotationConfig.RawInputForRotation = Sample.RawInput;
    }
    
    Sample.RotationAlpha = TrajectoryConfig.RotationConfig.BlendAlpha;
    
    if (!FMath::IsFinite(Sample.RawInput.X) || !FMath::IsFinite(Sample.RawInput.Y))
    {
        Sample.RawInput = FVector2D::ZeroVector;
    }
    
    if (UCharacterMovementComponent* Movement = MMCharacter->GetCharacterMovement())
    {
        Sample.bIsOnGround = Movement->IsMovingOnGround();
    }
    
    HandleRootMotion(Sample);
    
    MotionHistory.AddSample(Sample);
}


void UMMAnimInstance::BuildTrajectory(float DeltaSeconds)
{
    if (!MMCharacter || !CachedMovementComponent) return;
    
    FCharacterMotionContext CharContext = MotionHistory.GetCharacterContext(
        MMCharacter->GetControlYawRotation(),
        GetMeshRotation(),
        TrajectoryConfig
    );
    
    const float ControlAngularVel = MotionHistory.GetSampleCount() > 1 ?
        FMath::FindDeltaAngleDegrees(LastControlYaw, CharContext.ControlRotation.Yaw) / DeltaSeconds : 0.f;
    
    TrajectoryConfig.RotationConfig.UpdateRotationState(
        CharContext.ControlRotation.Yaw,
        ControlAngularVel,
        DeltaSeconds
    );
    
    if (!TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation)
    {
        FVector CurrentAcceleration = FVector::ZeroVector;
        if (CachedMovementComponent)
        {
            CurrentAcceleration = CachedMovementComponent->GetCurrentAcceleration();
        }
        
        TrajectoryConfig.RotationConfig.UpdateManualRotation(MMCharacter
            ,
            CharContext.ControlRotation,
            MMCharacter->GetVelocity(),
            MMCharacter->CurrentMoveInput,
            CurrentAcceleration,
            DeltaSeconds
        );
    }
    
    LastControlYaw = CharContext.ControlRotation.Yaw;
    
    const FDynamicSpeedMatchConfig* ConfigPtr = nullptr;
    const FDynamicSpeedAnalysis* AnalysisPtr = nullptr;
    
    if (bEnableSpeedMatching && SpeedAnalysis.DetectedClusters.Num() > 0)
    {
        ConfigPtr = &SpeedMatchConfig;
        AnalysisPtr = &SpeedAnalysis;
    }
    
    MotionHistory.BuildMMTrajectory(
        TrajectoryTimeOffsets,
        TrajectoryForMM,
        TrajectoryConfig,
        CharContext,
        AnimContext,
        EnvContext,
        CachedMovementComponent,
        DeltaSeconds,
        ConfigPtr,
        AnalysisPtr
    );
}

void UMMAnimInstance::BuildTrajectoryWithSpeedMatch(float DeltaSeconds)
{
    if (!MMCharacter || !CachedMovementComponent)
    {
        return;
    }
    
    // Initialize clusters if not already done
    if (CachedSemanticClusters.Num() == 0 && MotionMatchingDatabase)
    {
        InitializeSpeedClusters();
    }
    
    // Extract character context
    FCharacterMotionContext CharContext = MotionHistory.GetCharacterContext(
        MMCharacter->GetControlYawRotation(),
        GetMeshRotation(),
        TrajectoryConfig
    );
    
    // Update rotation system
    const float ControlAngularVel = MotionHistory.GetSampleCount() > 1 ?
        FMath::FindDeltaAngleDegrees(LastControlYaw, CharContext.ControlRotation.Yaw) / DeltaSeconds : 0.f;
    
    TrajectoryConfig.RotationConfig.UpdateRotationState(
        CharContext.ControlRotation.Yaw,
        ControlAngularVel,
        DeltaSeconds
    );
    
    if (!TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation)
    {
        FVector CurrentAcceleration = FVector::ZeroVector;
        if (CachedMovementComponent)
        {
            CurrentAcceleration = CachedMovementComponent->GetCurrentAcceleration();
        }
        
        TrajectoryConfig.RotationConfig.UpdateManualRotation(
            MMCharacter,
            CharContext.ControlRotation,
            MMCharacter->GetVelocity(),
            MMCharacter->CurrentMoveInput,
            CurrentAcceleration,
            DeltaSeconds
        );
    }
    
    LastControlYaw = CharContext.ControlRotation.Yaw;
    
    // Set up speed matching configuration
    FDynamicSpeedMatchConfig CustomConfig = SpeedMatchConfig;
    
    // Populate speed analysis with semantic clusters
    FDynamicSpeedAnalysis CustomAnalysis;
    CustomAnalysis.DetectedClusters = ConvertSemanticToSpeedClusters(CachedSemanticClusters);
    CustomAnalysis.CurrentActualSpeed = MMCharacter->GetVelocity().Size();
    CustomAnalysis.CurrentClusterIndex = -1;
    CustomAnalysis.TimeSinceLastTransition = 0.f;
    CustomAnalysis.bIsTransitioning = false;
    
    // Find current cluster if we're moving
    if (CustomAnalysis.CurrentActualSpeed > 10.0f && SpeedAnalyzer)
    {
        CustomAnalysis.CurrentClusterIndex = SpeedAnalyzer->FindBestCluster(
            CustomAnalysis.CurrentActualSpeed,
            CachedSemanticClusters
        );
        
        if (CustomAnalysis.CurrentClusterIndex != INDEX_NONE)
        {
            const FSemanticSpeedCluster& CurrentCluster = CachedSemanticClusters[CustomAnalysis.CurrentClusterIndex];
            
            // Log cluster match for debugging (disable after testing)
            if (GFrameCounter % 60 == 0) // Every 60 frames (~1 second)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Speed Match: %.1f cm/s -> Cluster '%s' (%.1f cm/s)"),
                    CustomAnalysis.CurrentActualSpeed,
                    *CurrentCluster.ClusterName,
                    CurrentCluster.CenterSpeed);
            }
        }
    }
    
    // Build trajectory with speed matching enabled
    // EnvContext is already a member variable, no need to extract it
    MotionHistory.BuildMMTrajectory(
        TrajectoryTimeOffsets,
        TrajectoryForMM,
        TrajectoryConfig,
        CharContext,
        AnimContext,
        EnvContext,          // This is the cached member variable
        CachedMovementComponent,
        DeltaSeconds,
        &CustomConfig,       // Speed match config
        &CustomAnalysis      // Speed analysis with semantic clusters
    );
}

void UMMAnimInstance::BuildTrajectoryWithCustomSpeedModel(
    const TArray<FSpeedCluster>& CustomSpeedClusters,
    float SpeedMatchStrength)
{
    if (!MMCharacter || !CachedMovementComponent) return;
    
    FCharacterMotionContext CharContext = MotionHistory.GetCharacterContext(
        MMCharacter->GetControlYawRotation(),
        GetMeshRotation(),
        TrajectoryConfig
    );
    
    const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
    const float ControlAngularVel = MotionHistory.GetSampleCount() > 1 ?
        FMath::FindDeltaAngleDegrees(LastControlYaw, CharContext.ControlRotation.Yaw) / DeltaSeconds : 0.f;
    
    TrajectoryConfig.RotationConfig.UpdateRotationState(
        CharContext.ControlRotation.Yaw,
        ControlAngularVel,
        DeltaSeconds
    );
    
    if (!TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation)
    {
        FVector CurrentAcceleration = FVector::ZeroVector;
        if (CachedMovementComponent)
        {
            CurrentAcceleration = CachedMovementComponent->GetCurrentAcceleration();
        }
        
        TrajectoryConfig.RotationConfig.UpdateManualRotation(MMCharacter
            ,
            CharContext.ControlRotation,
            MMCharacter->GetVelocity(),
            MMCharacter->CurrentMoveInput,
            CurrentAcceleration,
            DeltaSeconds
        );
    }
    
    LastControlYaw = CharContext.ControlRotation.Yaw;
    
    FDynamicSpeedMatchConfig CustomConfig = SpeedMatchConfig;
    CustomConfig.SpeedMatchStrength = SpeedMatchStrength;
    
    FDynamicSpeedAnalysis CustomAnalysis;
    CustomAnalysis.DetectedClusters = CustomSpeedClusters;
    CustomAnalysis.CurrentActualSpeed = MMCharacter->GetVelocity().Size();
    CustomAnalysis.CurrentClusterIndex = -1;
    CustomAnalysis.TimeSinceLastTransition = 0.f;
    CustomAnalysis.bIsTransitioning = false;
    
    MotionHistory.BuildMMTrajectory(
        TrajectoryTimeOffsets,
        TrajectoryForMM,
        TrajectoryConfig,
        CharContext,
        AnimContext,
        EnvContext,
        CachedMovementComponent,
        DeltaSeconds,
        &CustomConfig,
        &CustomAnalysis
    );
}

void UMMAnimInstance::BuildTrajectoryNoSpeedMatch(float DeltaSeconds)
{
    if (!MMCharacter || !CachedMovementComponent) return;
    
    FCharacterMotionContext CharContext = MotionHistory.GetCharacterContext(
        MMCharacter->GetControlYawRotation(),
        GetMeshRotation(),
        TrajectoryConfig
    );
    
    const float ControlAngularVel = MotionHistory.GetSampleCount() > 1 ?
        FMath::FindDeltaAngleDegrees(LastControlYaw, CharContext.ControlRotation.Yaw) / DeltaSeconds : 0.f;
    
    TrajectoryConfig.RotationConfig.UpdateRotationState(
        CharContext.ControlRotation.Yaw,
        ControlAngularVel,
        DeltaSeconds
    );
    
    if (!TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation)
    {
        FVector CurrentAcceleration = FVector::ZeroVector;
        if (CachedMovementComponent)
        {
            CurrentAcceleration = CachedMovementComponent->GetCurrentAcceleration();
        }
        
        TrajectoryConfig.RotationConfig.UpdateManualRotation(MMCharacter
            ,
            CharContext.ControlRotation,
            MMCharacter->GetVelocity(),
            MMCharacter->CurrentMoveInput,
            CurrentAcceleration,
            DeltaSeconds
        );
    }
    
    LastControlYaw = CharContext.ControlRotation.Yaw;
    
    MotionHistory.BuildMMTrajectory(
        TrajectoryTimeOffsets,
        TrajectoryForMM,
        TrajectoryConfig,
        CharContext,
        AnimContext,
        EnvContext,
        CachedMovementComponent,
        DeltaSeconds,
        nullptr,
        nullptr
    );
}



void UMMAnimInstance::SetRotationMode(bool bUseCMC, ECMCRotationMode CMCMode, EManualRotationMode ManualMode)
{
    TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation = bUseCMC;
    TrajectoryConfig.RotationConfig.CMCMode = CMCMode;
    TrajectoryConfig.RotationConfig.ManualMode = ManualMode;
    
    if (CachedMovementComponent)
    {
        TrajectoryConfig.RotationConfig.ConfigureCMC(CachedMovementComponent);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Rotation Mode Changed - %s"), 
        *TrajectoryConfig.RotationConfig.GetCurrentModeDescription());
}


void UMMAnimInstance::SetLookAtTarget(const FVector& Target, bool bEnable)
{
    TrajectoryConfig.RotationConfig.LookAtTarget = Target;
    TrajectoryConfig.RotationConfig.bHasLookAtTarget = bEnable;
    
    if (bEnable && !TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation)
    {
        TrajectoryConfig.RotationConfig.ManualMode = EManualRotationMode::LookAt;
    }
}

void UMMAnimInstance::ExtractAnimationContext(float DeltaSeconds)
{
    if (UAnimMontage* CurrentMontage = GetCurrentActiveMontage())
    {
        AnimContext.CurrentAnimationName = CurrentMontage->GetFName();
        AnimContext.TimeInAnimation = Montage_GetPosition(CurrentMontage);
        AnimContext.AnimationLength = CurrentMontage->GetPlayLength();
        AnimContext.bHasRootMotion = CurrentMontage->HasRootMotion();
        
        const float CurrentTime = AnimContext.TimeInAnimation;
        for (const FAnimNotifyEvent& Notify : CurrentMontage->Notifies)
        {
            if (Notify.GetTriggerTime() > CurrentTime && 
                Notify.NotifyName == TEXT("FootPlant"))
            {
                AnimContext.NextFootPlantTime = LocalTimeSeconds + (Notify.GetTriggerTime() - CurrentTime);
                AnimContext.FootPlantFoot = (Notify.TrackIndex == 0) ? 0 : 1;
                break;
            }
        }
    }
    else
    {
        AnimContext.CurrentAnimationName = TEXT("StateMachine");
        AnimContext.bHasRootMotion = false;
        AnimContext.TimeInAnimation = 0.f;
    }
    
    AnimContext.bIsInterruptible = !IsAnyMontagePlaying() || 
        (AnimContext.TimeInAnimation > AnimContext.AnimationLength * 0.7f);
    
    AnimContext.RootMotionVelocity = FVector::ZeroVector;
    AnimContext.RootMotionRotation = FQuat::Identity;
    
    if (AnimContext.bHasRootMotion)
    {
        FRootMotionMovementParams CurrentExtractedRootMotion = ConsumeExtractedRootMotion(1.0f);
        
        if (CurrentExtractedRootMotion.bHasRootMotion)
        {
            const FTransform RootMotionTransform = CurrentExtractedRootMotion.GetRootMotionTransform();
            
            if (DeltaSeconds > 0.001f)
            {
                AnimContext.RootMotionVelocity = RootMotionTransform.GetTranslation() / DeltaSeconds;
                AnimContext.RootMotionRotation = RootMotionTransform.GetRotation();
            }
        }
    }
}

void UMMAnimInstance::HandleRootMotion(FMotionSample& Sample) const
{
    if (AnimContext.bHasRootMotion && AnimContext.RootMotionVelocity.Size() > 0.1f)
    {
        Sample.WorldVelocity = AnimContext.RootMotionVelocity;
        
        if (!AnimContext.RootMotionRotation.IsIdentity())
        {
            Sample.WorldRotation = AnimContext.RootMotionRotation * Sample.WorldRotation;
        }
    }
}

FQuat UMMAnimInstance::GetMeshRotation() const
{
    if (USkeletalMeshComponent* Mesh = MMCharacter ? MMCharacter->GetMesh() : nullptr)
    {
        return Mesh->GetComponentQuat();
    }
    return  MMCharacter? MMCharacter->GetActorQuat() : FQuat::Identity;
}

void UMMAnimInstance::DebugDrawTrajectory() const
{
    if (!MMCharacter || !GetWorld()) return;

    UWorld* World = GetWorld();
    const FVector CharPos = MMCharacter->GetActorLocation();
    
    // --- Calculate a screen-space-aware position for the debug text ---
    FVector TextLocation = CharPos;
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if(APlayerCameraManager* CamManager = PC->PlayerCameraManager)
        {
            const FRotator CameraRotation = CamManager->GetCameraRotation();
            // Position the text to the right of the character from the camera's perspective
            TextLocation += CameraRotation.RotateVector(FVector(0, 120, 0));
        }
    }

    // --- Trajectory Path Visualization ---
    if (TrajectoryForMM.Samples.Num() > 0)
    {
        for (int32 i = 0; i < TrajectoryForMM.Samples.Num(); i++)
        {
            const FTransformTrajectorySample& Sample = TrajectoryForMM.Samples[i];
            
            // Color code: Red for past, Green for present, Blue for future
            const FColor Color = (Sample.TimeInSeconds < 0.f) ? FColor::Red : ((FMath::IsNearlyZero(Sample.TimeInSeconds)) ? FColor::Green : FColor::Blue);
            const float Radius = FMath::IsNearlyZero(Sample.TimeInSeconds) ? 15.f : 8.f;
            
            DrawDebugSphere(World, Sample.Position, Radius, 8, Color, false, 0.f, 0, 1.f);
            
            // Draw facing direction arrow for each sample point
            const FVector FacingDir = Sample.Facing.GetForwardVector();
            DrawDebugDirectionalArrow(World, Sample.Position, Sample.Position + FacingDir * 25.f, 10.f, Color, false, 0.f, 0, 1.f);

            // Connect samples with a line to show the path
            if (i > 0)
            {
                const FTransformTrajectorySample& PrevSample = TrajectoryForMM.Samples[i - 1];
                DrawDebugLine(World, PrevSample.Position, Sample.Position, Color, false, 0.f, 0, 1.f);
            }
        }
    }

    // --- Consolidated Debug Text Block ---
    const FVector CurrentAccel = MotionHistory.EstimateAcceleration(0.05f);
    
    FString StateText = FString::Printf(
        TEXT("--- MOTION ---\n")
        TEXT("State: %s (%.2fs)\n")
        TEXT("Speed: %.1f cm/s\n")
        TEXT("Accel: %.1f cm/s²\n")
        TEXT("Move Dir: %s\n")
        TEXT("\n--- ANIMATION ---\n")
        TEXT("Pivot: %s | Loop: %s | Trans: %s\n")
        TEXT("Dir Change Weight: %.2f\n")
        TEXT("Force Update: %s (Urgency: %.2f)\n")
        TEXT("\n--- PREDICTION ---\n")
        TEXT("Stop Time: %.2fs\n")
        TEXT("Stop Dist: %.1f cm"),
        *UEnum::GetValueAsString(CurrentMotionAnalysis.CurrentState),
        CurrentMotionAnalysis.TimeInState,
        CurrentMotionContext.Speed,
        CurrentAccel.Size(),
        *UEnum::GetValueAsString(CurrentMotionContext.MovementDirection),
        CurrentMotionAnalysis.bIsPivoting ? TEXT("Y") : TEXT("N"),
        CurrentMotionAnalysis.bIsLooping ? TEXT("Y") : TEXT("N"),
        CurrentMotionAnalysis.bIsTransitioning ? TEXT("Y") : TEXT("N"),
        DirectionChangeWeight,
        bForceMotionMatchingUpdate ? TEXT("YES") : TEXT("NO"),
        MotionMatchingUpdateUrgency,
        CurrentMotionAnalysis.PredictedStopTime > 0.f ? CurrentMotionAnalysis.PredictedStopTime : 0.f,
        CurrentMotionAnalysis.PredictedStopDistance > 0.f ? CurrentMotionAnalysis.PredictedStopDistance : 0.f
    );
    
    // Use bTestZ=false to prevent the text from being hidden by level geometry.
    DrawDebugString(World, TextLocation, StateText, nullptr, FColor::White, 0.f, false, 1.0f);
}

void UMMAnimInstance::DebugDrawRotation() const
{
    if (!MMCharacter || !GetWorld()) return;

    UWorld* World = GetWorld();
    const FVector CharPos = MMCharacter->GetActorLocation();
    const FRotationConfig& RotConfig = TrajectoryConfig.RotationConfig;
    
    // Calculate a screen-space-aware position for the debug text
    FVector TextLocation = CharPos;
    FVector VisRootLocation = CharPos + FVector(0, 0, 10);
    
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if(APlayerCameraManager* CamManager = PC->PlayerCameraManager)
        {
            const FRotator CameraRotation = CamManager->GetCameraRotation();
            TextLocation += CameraRotation.RotateVector(FVector(0, 150, 80));
        }
    }

    // Visuals for Dynamic Mode
    if (!RotConfig.bUseCharacterMovementRotation && 
        RotConfig.ManualMode == EManualRotationMode::Dynamic && 
        MMCharacter->GetVelocity().SizeSquared() > 10.f)
    {
        const FVector VelocityDir = MMCharacter->GetVelocity().GetSafeNormal();
        const FVector CameraDir = MMCharacter->GetControlYawRotation().Vector();
        
        // Orbit Target (Velocity) - Orange
        DrawDebugDirectionalArrow(World, VisRootLocation, 
            VisRootLocation + VelocityDir * 120.f, 20.f, FColor::Orange, false, 0.f, 0, 2.f);
        DrawDebugString(World, VisRootLocation + VelocityDir * 125.f, 
            "Orbit", nullptr, FColor::Orange, 0.f, true, 1.f);
        
        // Strafe Target (Camera) - Cyan
        DrawDebugDirectionalArrow(World, VisRootLocation, 
            VisRootLocation + CameraDir * 120.f, 20.f, FColor::Cyan, false, 0.f, 0, 2.f);
        DrawDebugString(World, VisRootLocation + CameraDir * 125.f, 
            "Strafe", nullptr, FColor::Cyan, 0.f, true, 1.f);

        // Angle Arc between Orbit and Strafe (manual implementation)
        const float AngleRad = FMath::Acos(FVector::DotProduct(VelocityDir, CameraDir));
        const FVector ArcAxis = FVector::CrossProduct(VelocityDir, CameraDir).GetSafeNormal();
        
        const int32 ArcSegments = 16;
        const float ArcRadius = 120.f;
        FVector LastArcPoint = VisRootLocation + VelocityDir * ArcRadius;

        for (int32 i = 1; i <= ArcSegments; ++i)
        {
            const float Alpha = static_cast<float>(i) / static_cast<float>(ArcSegments);
            const float CurrentAngleRad = AngleRad * Alpha;
            const FQuat RotQuat(ArcAxis, CurrentAngleRad);
            const FVector CurrentArcPoint = VisRootLocation + RotQuat.RotateVector(VelocityDir) * ArcRadius;
            DrawDebugLine(World, LastArcPoint, CurrentArcPoint, 
                FColor(255, 255, 255, 64), false, 0.f, 0, 1.f);
            LastArcPoint = CurrentArcPoint;
        }

        // Angle Text
        const float AngleDeg = FMath::RadiansToDegrees(AngleRad);
        DrawDebugString(World, CharPos + FVector(0,0,120), 
            FString::Printf(TEXT("%.1f°"), AngleDeg), nullptr, FColor::White, 0.f, true, 1.2f);
    }
    
    // General Rotation Visuals
    
    // Current Facing Direction (Green)
    const FVector CurrentForward = MMCharacter->GetActorForwardVector();
    DrawDebugDirectionalArrow(World, VisRootLocation, 
        VisRootLocation + CurrentForward * 150.f, 25.f, FColor::Green, false, 0.f, 0, 3.f);
    DrawDebugString(World, VisRootLocation + CurrentForward * 155.f, 
        "Current", nullptr, FColor::Green, 0.f, true, 1.f);

    // Target Rotation Direction
    const FColor TargetColor = (RotConfig.ManualMode == EManualRotationMode::Dynamic) ? 
        FColor::White : FColor::Red;
    const FRotator TargetRot(0, RotConfig.TargetYaw, 0);
    const FVector TargetForward = TargetRot.Vector();
    DrawDebugDirectionalArrow(World, VisRootLocation, 
        VisRootLocation + TargetForward * 100.f, 15.f, TargetColor, false, 0.f, 0, 2.f);
    DrawDebugString(World, VisRootLocation + TargetForward * 105.f, 
        "Target", nullptr, TargetColor, 0.f, true, 1.f);

    // Consolidated Debug Text
    FString RotationText = FString::Printf(
        TEXT("=== ROTATION ===\n")
        TEXT("Mode: %s\n")
        TEXT("Rate: %.1f°/s\n")
        TEXT("Yaw: %.1f° → %.1f°\n")
        TEXT("Lag: %s | Limited: %s"),
        *RotConfig.GetCurrentModeDescription(),
        RotConfig.GetCurrentRotationRate(),
        FMath::UnwindDegrees(RotConfig.CurrentYaw),
        FMath::UnwindDegrees(RotConfig.TargetYaw),
        RotConfig.bIsLagging ? TEXT("YES") : TEXT("NO"),
        RotConfig.bIsRotationRateLimited ? TEXT("YES") : TEXT("NO")
    );
    
    if (!RotConfig.bUseCharacterMovementRotation)
    {
        if (RotConfig.ManualMode == EManualRotationMode::Dynamic)
        {
            RotationText += FString::Printf(
                TEXT("\n\n=== DYNAMIC ===\n")
                TEXT("Blend Weight: %.2f\n")
                TEXT("Alpha: %.2f\n")
                TEXT("Movement: %s"),
                RotConfig.DynamicBlendWeight,
                RotConfig.BlendAlpha,
                RotConfig.bWasMovingLastFrame ? TEXT("Moving") : TEXT("Idle")
            );
        }
        else if (RotConfig.ManualMode == EManualRotationMode::Cinematic)
        {
            RotationText += FString::Printf(
                TEXT("\n\n=== CINEMATIC ===\n")
                TEXT("Accel: %.2f | Vel: %.2f | Ctrl: %.2f\n")
                TEXT("Alpha: %.2f"),
                RotConfig.CinematicAccelerationInfluence,
                RotConfig.CinematicVelocityInfluence,
                RotConfig.CinematicControlInfluence,
                RotConfig.BlendAlpha
            );
        }
    }
    
    DrawDebugString(World, TextLocation, RotationText, nullptr, FColor::White, 0.f, false, 1.0f);
}

