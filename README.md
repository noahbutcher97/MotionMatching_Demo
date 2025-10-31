# Motion Matching Demo

An Unreal Engine 5.6 demonstration project showcasing advanced Motion Matching techniques using Unreal's Pose Search system. This project implements a sophisticated locomotion system with semantic speed analysis, trajectory prediction, and environment-aware animation selection.

## Overview

This demo showcases a production-ready implementation of Motion Matching in Unreal Engine 5.6, featuring:

- **Semantic Speed Analysis**: Intelligent animation clustering and speed-matching based on animation naming conventions and actual motion data
- **Trajectory-Based Matching**: Future trajectory prediction with configurable time offsets for smooth, responsive character movement
- **Environment-Aware Animation**: Asynchronous environment scanning with spatial caching for optimized performance
- **Adaptive Motion System**: Dynamic motion analysis with state detection (looping, transitioning, pivoting)
- **Flexible Rotation Control**: Multiple rotation modes including Character Movement Component integration and manual control with look-at targeting

## Features

### Core Motion Matching
- Full integration with Unreal Engine's **Pose Search** plugin
- Real-time pose database queries for natural animation transitions
- Trajectory matching with configurable weights (position, rotation, velocity)
- Support for continuous animation loops and transitions

### Semantic Speed Clustering
- Automatic animation speed analysis from both naming conventions and root motion data
- Hierarchical clustering of animations into semantic speed levels (Idle, Walk, Jog, Run, Sprint)
- Dynamic speed matching with configurable transition thresholds
- Support for multiple motion intents (Locomotion, Combat, Interaction, Idle)

### Advanced Trajectory System
- Multi-sample trajectory prediction (default: 8 time offsets from -0.4s to 0.8s)
- Rotation-aware trajectory generation
- Configurable rotation modes:
  - Character Movement Component rotation (Orient to Movement, Orient to Controller)
  - Manual rotation (Control Rotation, Velocity Direction, Look At Target)
- Rotation lag simulation for more natural turning animations

### Environment Awareness
- Asynchronous ray-casting for ground, edge, wall, and ceiling detection
- Spatial hash-based caching system for performance optimization
- Level-of-Detail (LOD) system for environment scanning
- Cache hit rate tracking and performance monitoring

### Motion Analysis
- Real-time locomotion state detection
- Movement direction classification (8 directions + pivots)
- Motion complexity analysis for adaptive animation selection
- Animation interruptibility detection based on foot plants

### Debug & Visualization
- Visual trajectory debugging with sample points
- Environment cache visualization
- Rotation system debugging with gizmos
- Performance statistics display

## System Requirements

### Unreal Engine
- **Version**: Unreal Engine 5.6 or later
- **Build Type**: Development or Shipping

### Platform Support
- **Windows**: Win64 (Recommended)
- **Linux**: Limited support (some plugins may require Win64)

### Hardware Requirements (Recommended)
- **CPU**: Multi-core processor (4+ cores recommended)
- **RAM**: 16 GB or more
- **GPU**: DirectX 12 compatible GPU
- **Storage**: At least 5 GB free space for project and assets

## Required Plugins

The following Unreal Engine plugins are required (automatically enabled in the project):

### Animation & Motion Matching
- **PoseSearch** - Core motion matching functionality
- **MotionTrajectory** - Trajectory generation system
- **MotionWarping** - Animation warping capabilities
- **AnimationWarping** - Additional warping features
- **AnimationLocomotionLibrary** - Locomotion utilities
- **AnimNextPoseSearch** - Next-gen pose search features
- **AnimNextWarping** - Next-gen warping features
- **Locomotor** - Advanced locomotion system

### Animation Tools
- **AnimatorKit** - Animation tooling
- **BlendStack** - Animation blending
- **Chooser** - Selection system
- **ContextualAnimation** - Context-aware animations
- **RigMapper** - Rig retargeting

### State & Behavior
- **GameplayStateTree** - State machine system
- **GameplayBehaviors** - Behavior tree integration

### Character & Rendering
- **MetaHuman** - MetaHuman character support
- **MetaHumanCharacter** - MetaHuman integration
- **MetaHumanCoreTech** - Core MetaHuman technology
- **MetaHumanCalibrationProcessing** - MetaHuman calibration
- **MetaHumanLiveLink** - Live Link support for MetaHumans
- **PhysicsControl** - Physics-based character control

### Additional Features
- **MovieScenePoseSearchTracks** - Sequencer integration
- **MovieSceneAnimMixer** - Animation mixing in Sequencer
- **CurveExpression** - Curve manipulation
- **TrajectoryTools** - Trajectory debugging tools

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/noahbutcher97/MotionMatching_Demo.git
cd MotionMatching_Demo
```

### 2. Install Unreal Engine 5.6

If you haven't already:
1. Download and install the [Epic Games Launcher](https://www.epicgames.com/store/en-US/download)
2. Install **Unreal Engine 5.6** through the launcher
3. Ensure all required engine plugins are available (most are built-in)

### 3. Generate Project Files

**Windows:**
```bash
# Right-click on MotionMatching_Demo.uproject
# Select "Generate Visual Studio project files"
```

**Linux:**
```bash
# Use the UnrealBuildTool to generate makefiles
/path/to/UE5/Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh -project="/path/to/MotionMatching_Demo.uproject"
```

### 4. Build the Project

**Visual Studio (Windows):**
1. Open `MotionMatching_Demo.sln`
2. Set the configuration to `Development Editor`
3. Build the solution (Ctrl+Shift+B)

**Command Line (Windows):**
```bash
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" MotionMatching_Demo Win64 Development -Project="path\to\MotionMatching_Demo.uproject"
```

**Linux:**
```bash
/path/to/UE5/Engine/Build/BatchFiles/Linux/Build.sh MotionMatching_Demo Linux Development -project="/path/to/MotionMatching_Demo.uproject"
```

### 5. Open the Project

1. Launch Unreal Engine 5.6
2. Open `MotionMatching_Demo.uproject`
3. Wait for shaders and assets to compile (first launch may take several minutes)

## Project Structure

```
MotionMatching_Demo/
├── Config/                      # Project configuration files
│   ├── DefaultEngine.ini        # Engine settings
│   ├── DefaultGame.ini          # Game settings
│   └── DefaultInput.ini         # Input mappings
├── Content/                     # All content assets
│   ├── Assets/                  # Third-party animation assets
│   │   ├── ARPG_Unarmed/       # ARPG animations
│   │   ├── CombatMasterAnimBundle/
│   │   ├── CyberNinja/
│   │   ├── MeleeLocomotion/
│   │   └── MetaHumans/         # MetaHuman characters
│   ├── Project/                 # Project-specific content
│   │   ├── Animation/          # Animation Blueprints, ABPs
│   │   ├── Blueprints/         # Blueprint classes
│   │   ├── Input/              # Input Actions and Contexts
│   │   ├── Metahuman/          # MetaHuman configurations
│   │   └── UI/                 # User interface
│   └── ThirdPerson/             # Third-person template content
├── Source/                      # C++ source code
│   └── MotionMatching_Demo/
│       ├── Public/              # Public headers
│       │   ├── Character/       # Character classes
│       │   ├── Animation/       # Animation instances
│       │   ├── PoseSearch/      # Pose search utilities
│       │   ├── Data/            # Data structures
│       │   ├── FunctionLibraries/
│       │   └── Editor/          # Editor tools
│       └── Private/             # Implementation files
└── MotionMatching_Demo.uproject # Unreal project file
```

## Key Components

### C++ Classes

#### `AMMCharacter`
Main character class with Enhanced Input support.
- **Location**: `Source/MotionMatching_Demo/Public/Character/MMCharacter.h`
- **Features**:
  - Enhanced Input system integration
  - Movement and look input handling
  - Control rotation management
  - Mesh yaw offset calculations

#### `UMMAnimInstance`
Core Animation Blueprint instance class managing motion matching.
- **Location**: `Source/MotionMatching_Demo/Public/Animation/MMAnimInstance.h`
- **Features**:
  - Motion history tracking with adaptive buffer
  - Trajectory generation and prediction
  - Environment-aware animation selection
  - Asynchronous environment scanning
  - Speed-matched animation clustering
  - Dynamic rotation control
  - Performance monitoring and statistics

#### `UMMPoseSearchSpeedAnalyzer`
Semantic speed analysis and clustering system.
- **Location**: `Source/MotionMatching_Demo/Public/PoseSearch/MMPoseSearchSpeedAnalyzer.h`
- **Features**:
  - Animation name-based semantic analysis
  - Speed profile generation from root motion
  - Hierarchical speed clustering
  - Database indexing with caching
  - Runtime speed matching

#### `UMMTrajectoryComponent`
Component for trajectory visualization and prediction.
- **Location**: `Source/MotionMatching_Demo/Public/PoseSearch/MMTrajectoryComponent.h`
- **Features**:
  - Debug trajectory visualization
  - Trajectory sample management
  - Integration with motion matching system

#### `UMMSemanticNameAnalyzer`
Animation name parsing for semantic understanding.
- **Location**: `Source/MotionMatching_Demo/Public/Data/MMSemanticTypes.h`
- **Features**:
  - Keyword-based motion intent detection
  - Speed level extraction from names
  - Direction classification
  - No external dependencies (pure string parsing)

### Data Structures

#### `FMotionData` & `FMotionSample`
Motion history tracking and sampling.
- **Location**: `Source/MotionMatching_Demo/Public/Data/MMPrimaryStructs.h`
- **Features**:
  - Circular buffer for motion samples
  - Velocity, acceleration, and rotation tracking
  - Root motion extraction
  - Motion complexity analysis

#### `FSemanticSpeedProfile` & `FSemanticSpeedCluster`
Speed analysis and clustering structures.
- **Features**:
  - Animation speed statistics (min, max, average, variance)
  - Semantic speed level classification
  - Transition suitability scoring
  - Cluster-based speed matching

#### `FTransformTrajectory`
Trajectory sample container for motion matching.
- **Features**:
  - Multiple time offset samples
  - World and local space transforms
  - Velocity and facing direction per sample

### Editor Tools

#### `UMMSpeedAnalysisWidget`
Editor utility widget for analyzing and visualizing animation speeds in Pose Search databases.
- **Location**: `Source/MotionMatching_Demo/Public/Editor/MMSpeedAnalysisWidget.h`

## Usage

### Basic Setup

1. **Open the Main Level**
   - Navigate to `Content/ThirdPerson/Maps/`
   - Open the main demo level

2. **Place the Character**
   - The character Blueprint should already be placed in the level
   - If not, drag `Content/Project/Blueprints/BP_MMCharacter` into the level

3. **Configure Pose Search Database**
   - Open `Content/Project/Animation/` to find Pose Search databases
   - Configure database in the Animation Blueprint or Character settings
   - Ensure animations are properly indexed

### Input Controls

Default Enhanced Input mappings:

- **Movement**: W/A/S/D or Left Stick
- **Camera Look**: Mouse or Right Stick
- **Jump**: Spacebar / Gamepad Button

*Input mappings can be customized in `Content/Project/Input/`*

### Animation Configuration

#### Setting Up a Pose Search Database

1. Create a new **Pose Search Database** asset
2. Add animation sequences to the database
3. Configure search parameters:
   - Trajectory samples and weights
   - Pose features and bones
   - Blend parameters
4. Build/index the database

#### Configuring Speed Matching

In the Animation Blueprint (ABP):

```cpp
// Enable speed matching
SpeedMatchConfig.bEnableSpeedMatching = true;

// Configure clustering
SpeedMatchConfig.SemanticConfig.MinAnimationsPerCluster = 2;
SpeedMatchConfig.SemanticConfig.MaxClusters = 10;

// Set speed matching strength
SpeedMatchConfig.SpeedMatchStrength = 0.7f; // 0.0 to 1.0
```

#### Trajectory Configuration

In the Animation Instance:

```cpp
// Configure trajectory time offsets (in seconds)
TrajectoryTimeOffsets = { -0.4f, -0.2f, 0.0f, 0.15f, 0.3f, 0.45f, 0.6f, 0.8f };

// Set trajectory weights
TrajectoryConfig.TrajectoryWeight = 3.0f;
TrajectoryConfig.PoseWeight = 0.5f;

// Configure rotation
TrajectoryConfig.RotationConfig.bUseCharacterMovementRotation = true;
TrajectoryConfig.RotationConfig.CMCRotationMode = ECMCRotationMode::OrientToMovement;
```

### Debug Visualization

Enable debug drawing in the Animation Blueprint or via console commands:

```cpp
// In Animation Instance
bDebugDrawTrajectory = true;           // Show trajectory samples
bDebugDrawEnvironmentCache = true;     // Show environment scan cache
bDebugDrawRotation = true;             // Show rotation vectors
bShowPerformanceStats = true;          // Display performance metrics
```

### Runtime API

Key Blueprint-accessible functions:

```cpp
// Character
FRotator GetControlYawRotation();
float GetMeshYawOffset();

// Animation Instance - Motion Analysis
ELocomotionState GetCurrentLocomotionState();
bool IsInContinuousLoop();
bool IsTransitioning();
bool IsPivoting();
EMovementDirection GetMovementDirection();
float GetSpeed();

// Animation Instance - Environment
float GetEnvironmentCacheHitRate();
EEnvironmentLOD GetCurrentEnvironmentLOD();

// Animation Instance - Rotation
void SetRotationMode(bool bUseCMC, ECMCRotationMode CMCMode, EManualRotationMode ManualMode);
void SetLookAtTarget(const FVector& Target, bool bEnable);
bool IsRotationLagging();

// Animation Instance - Debug
FString GetPerformanceReport();
```

## Performance Optimization

### Environment Scanning

The system uses several optimization techniques:

1. **Spatial Caching**: Results are cached in a spatial hash grid
2. **LOD System**: Scan frequency adapts based on motion complexity
3. **Async Traces**: Ray-casts are performed asynchronously
4. **Selective Updates**: Only necessary scans are performed per frame

### Motion Matching

To optimize motion matching queries:

1. **Database Size**: Keep databases focused and under ~1000 poses
2. **Trajectory Samples**: Fewer samples = faster queries (balance with quality)
3. **Search Exclusions**: Use channel exclusions to ignore irrelevant bones
4. **Speed Clustering**: Pre-filter animations by speed for better performance

### Monitoring Performance

```cpp
// Get performance statistics
FString PerfReport = AnimInstance->GetPerformanceReport();
UE_LOG(LogTemp, Log, TEXT("%s"), *PerfReport);

// Check cache efficiency
float CacheHitRate = AnimInstance->GetEnvironmentCacheHitRate();
```

## Extending the System

### Adding Custom Speed Clusters

```cpp
// Create custom speed cluster configuration
FSemanticSpeedConfig CustomConfig;
CustomConfig.MinAnimationsPerCluster = 3;
CustomConfig.MaxClusters = 5;
CustomConfig.SemanticWeight = 0.5f;

// Generate clusters from database
TArray<FSemanticSpeedCluster> Clusters = 
    SpeedAnalyzer->GenerateSemanticSpeedClusters(Database, CustomConfig);
```

### Custom Environment Scanning

Extend `UMMAnimInstance` and override:

```cpp
void UpdateEnvironmentCache(float DeltaSeconds);
void ProcessAsyncTraces();
```

### Custom Motion Analysis

Modify `FMotionAnalysis` calculations in:

```cpp
void GatherMotionData(float DeltaSeconds);
```

## Troubleshooting

### Common Issues

1. **Animations Not Matching Properly**
   - Ensure Pose Search Database is built/indexed
   - Check trajectory weights and sample offsets
   - Verify animation root motion is enabled
   - Review speed clustering results

2. **Character Not Rotating Smoothly**
   - Check `TrajectoryConfig.RotationConfig` settings
   - Adjust rotation lag and interpolation speed
   - Verify rotation mode (CMC vs Manual)

3. **Performance Issues**
   - Reduce trajectory sample count
   - Adjust environment cache LOD settings
   - Monitor cache hit rate
   - Profile using Unreal's profiling tools (stat fps, stat unit)

4. **Compilation Errors**
   - Ensure all required plugins are enabled
   - Verify Unreal Engine 5.6 is installed
   - Clean and rebuild the project
   - Check for missing dependencies

### Debug Commands

Useful console commands for debugging:

```
stat fps                    # Show frame rate
stat unit                   # Show frame time breakdown
stat game                   # Show game thread stats
ShowDebug Animation         # Show animation debug info
p.NetShowCorrections 1      # Show network corrections
```

## Known Limitations

- **Platform**: Some MetaHuman plugins are Windows/Linux only
- **Animation Assets**: Demo uses third-party assets (see Credits)
- **Network**: Network replication not fully implemented
- **VR**: Not currently optimized for VR

## Credits

### Animation Assets

This project uses various third-party animation packs available on the Unreal Engine Marketplace:

- ARPG Unarmed Animations
- Combat Master Animation Bundle
- Cyber Ninja Animations
- Cyberpunk Runner Animations
- GASP (General Animation Set Pack)
- Karate Animation V1
- Kickboxing Animation V2
- Melee Combat Basics
- Melee Counters
- Melee Locomotion
- Melee Strikes

### Epic Games

- Unreal Engine 5.6
- MetaHuman Creator and MetaHuman SDK
- Pose Search Plugin
- Animation Locomotion Library
- All built-in Unreal Engine plugins

### Developer

- **GitHub**: [noahbutcher97](https://github.com/noahbutcher97)

## License

This project is provided as a demonstration/educational resource. Please review and comply with:

1. **Unreal Engine**: Subject to Epic Games' [Unreal Engine EULA](https://www.unrealengine.com/en-US/eula/unreal)
2. **Third-Party Assets**: Each animation pack has its own license terms
3. **Project Code**: The C++ code and blueprints in this project are provided as-is for educational purposes

**Note**: This is a demo project. If you plan to use any part of this in a commercial project, ensure you have proper licenses for all assets and comply with Epic Games' licensing terms.

## Contributing

This is a demonstration project. If you find bugs or have suggestions:

1. Open an issue on GitHub
2. Provide detailed reproduction steps
3. Include Unreal Engine version and platform information

## Future Improvements

Potential areas for enhancement:

- [ ] Network replication support for multiplayer
- [ ] IK (Inverse Kinematics) integration for foot placement
- [ ] Advanced terrain adaptation
- [ ] Combat system integration
- [ ] VR optimization
- [ ] Mobile platform support
- [ ] Additional animation sets
- [ ] Machine learning-based motion prediction
- [ ] Advanced physics integration with Physics Control

## Additional Resources

### Unreal Engine Documentation
- [Pose Search Documentation](https://docs.unrealengine.com/5.6/en-US/pose-search-in-unreal-engine/)
- [Animation Documentation](https://docs.unrealengine.com/5.6/en-US/animation-in-unreal-engine/)
- [Motion Matching Overview](https://docs.unrealengine.com/5.6/en-US/motion-matching-in-unreal-engine/)

### Learning Resources
- [Unreal Engine Learning Portal](https://dev.epicgames.com/community/learning)
- [Motion Matching Samples](https://www.unrealengine.com/marketplace/en-US/product/motion-matching)

---

**Last Updated**: October 2025
**Project Version**: 1.0
**Unreal Engine Version**: 5.6
