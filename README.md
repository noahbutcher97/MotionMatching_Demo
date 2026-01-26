# Motion Matching Demo

A comprehensive Unreal Engine 5.6 demonstration project showcasing advanced motion matching techniques using the Pose Search system with intelligent trajectory prediction, speed matching, and environmental awareness.

## Overview

This project demonstrates a production-ready implementation of motion matching technology for character animation in Unreal Engine. It features a sophisticated animation system that dynamically selects the most appropriate animation pose based on the character's current motion, desired trajectory, and environmental context.

## Key Features

### 🎯 Advanced Motion Matching
- **Pose Search Integration**: Utilizes Unreal Engine's PoseSearch plugin for efficient pose database queries
- **Dynamic Trajectory Prediction**: Intelligent trajectory generation based on player input and character state
- **Speed-Aware Matching**: Semantic speed clustering system that matches animation speeds to character velocity
- **Direction Change Detection**: Automatic detection and handling of rapid direction changes for responsive animation

### 🏃 Intelligent Motion Analysis
- **Adaptive Motion Buffering**: Dynamic history tracking with configurable buffer sizes
- **Locomotion State Detection**: Automatic classification of movement states (Idle, Walk, Run, Sprint, etc.)
- **Motion Complexity Analysis**: Real-time analysis of movement patterns for animation selection
- **Pivot Detection**: Specialized handling for quick directional changes

### 🌍 Environmental Awareness
- **Async Environment Scanning**: Non-blocking environment queries using async trace system
- **Spatial Hash Caching**: Efficient spatial cache for environment data to minimize redundant traces
- **Adaptive LOD System**: Dynamic level-of-detail for environment scanning based on motion complexity
- **Multi-layer Detection**: Ground, edge, wall, and ceiling detection for context-aware animation

### 🔄 Flexible Rotation Systems
- **Dual Rotation Modes**: 
  - Character Movement Component (CMC) rotation with multiple modes
  - Manual rotation system with custom behavior
- **Rotation Lag System**: Smooth rotation transitions with configurable lag
- **Look-At Targeting**: Optional look-at system for focused character orientation
- **Control-to-Mesh Tracking**: Precise tracking of control vs mesh rotation for animation blending

### 🎮 Enhanced Input System
- **Enhanced Input Actions**: Modern UE5 input system integration
- **2D Movement Input**: Smooth directional movement with full 360° control
- **Camera-Relative Controls**: Input automatically oriented to camera direction
- **Input Context Management**: Flexible input mapping with priority support

### 🐛 Comprehensive Debug Tools
- **Trajectory Visualization**: Real-time visualization of predicted trajectory
- **Environment Cache Display**: Visual representation of cached environment data
- **Performance Metrics**: Detailed performance statistics and profiling
- **Rotation Debug Overlay**: Visual debugging for rotation systems
- **Animation State Display**: Real-time animation state and transition information

## Project Structure

```
MotionMatching_Demo/
├── Source/
│   └── MotionMatching_Demo/
│       ├── Private/
│       │   ├── Animation/
│       │   │   └── MMAnimInstance.cpp          # Core animation logic
│       │   ├── Character/
│       │   │   └── MMCharacter.cpp             # Character implementation
│       │   ├── Data/
│       │   │   ├── MMPrimaryStructs.cpp        # Core data structures
│       │   │   └── MMSemanticTypes.cpp         # Semantic typing system
│       │   ├── Editor/
│       │   │   └── MMSpeedAnalysisWidget.cpp   # Editor tools
│       │   ├── FunctionLibraries/
│       │   │   └── MMPoseSearchUtils.cpp       # Utility functions
│       │   └── PoseSearch/
│       │       ├── MMTrajectoryComponent.cpp   # Trajectory system
│       │       └── MMPoseSearchSpeedAnalyzer.cpp # Speed analysis
│       └── Public/
│           └── [Header files mirror Private structure]
├── Content/
│   ├── Assets/                                 # Game assets
│   ├── Project/                                # Project-specific content
│   └── ThirdPerson/                            # Third-person template content
├── Config/                                     # Configuration files
└── MotionMatching_Demo.uproject               # Project file
```

## System Requirements

- **Unreal Engine Version**: 5.6 or higher
- **Platform Support**: Windows 64-bit, Linux (MetaHuman plugins limited to Win64/Linux)
- **Required Plugins**:
  - PoseSearch (Motion Matching core)
  - MotionTrajectory
  - AnimationWarping
  - MotionWarping
  - Locomotor
  - BlendStack
  - Chooser
  - AnimatorKit
  - AnimationLocomotionLibrary
  - GameplayStateTree
  - ContextualAnimation
  - Enhanced Input (included in Engine)

## Getting Started

### Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/noahbutcher97/MotionMatching_Demo.git
   cd MotionMatching_Demo
   ```

2. **Open the project**:
   - Double-click `MotionMatching_Demo.uproject` to open in Unreal Engine 5.6
   - If prompted to rebuild modules, select "Yes"

3. **Generate project files** (if needed):
   - Right-click on `MotionMatching_Demo.uproject`
   - Select "Generate Visual Studio project files"

### Building the Project

#### From Unreal Editor:
- Open the project and let it compile on first launch
- Any code changes can be hot-reloaded or compiled from the Editor

#### From Visual Studio:
1. Open the generated `.sln` file
2. Set the build configuration to "Development Editor"
3. Build the solution (Ctrl+Shift+B)

#### From Command Line:
```bash
# Windows
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" MotionMatching_DemoEditor Win64 Development "Path\To\MotionMatching_Demo.uproject"

# Linux
/path/to/UE5/Engine/Build/BatchFiles/Linux/Build.sh MotionMatching_DemoEditor Linux Development "/path/to/MotionMatching_Demo.uproject"
```

## Usage

### Setting Up a Character

1. **Create a Character Blueprint** based on `AMMCharacter`
2. **Configure Animation Blueprint**:
   - Parent class: `UMMAnimInstance`
   - Set up Motion Matching Database reference
   - Configure trajectory time offsets
3. **Set up Input**:
   - Create Enhanced Input mapping context
   - Define IA_Move and IA_Look input actions
   - Assign in character blueprint

### Configuring Motion Matching

In your Animation Blueprint (derived from `UMMAnimInstance`):

- **Motion Buffer Config**: Controls history tracking
  - `MaxSamples`: Maximum samples to store (default: 120)
  - `SampleInterval`: Time between samples (default: 0.033s)

- **Trajectory Config**: Controls trajectory prediction
  - `TrajectoryTimeOffsets`: Array of time points for trajectory (e.g., [-0.4, -0.2, 0.0, 0.15, 0.3, 0.45, 0.6, 0.8])
  - `RotationConfig`: Rotation system settings

- **Speed Match Config**: Controls speed matching behavior
  - `bEnableSpeedMatching`: Enable/disable speed matching
  - Various clustering and matching parameters

- **Environment Cache Config**: Controls environment scanning
  - `CellSize`: Size of spatial hash cells (default: 100cm)
  - `MaxCacheAge`: How long cache entries remain valid (default: 0.5s)
  - LOD-specific scan configurations

### Debug Visualization

Enable debug options in your Animation Blueprint instance:
- `bDebugDrawTrajectory`: Visualize predicted trajectory
- `bDebugDrawEnvironmentCache`: Show cached environment data
- `bDebugDrawRotation`: Display rotation information
- `bShowPerformanceStats`: Show performance metrics

## Architecture

### Core Components

#### MMCharacter
The character class handling:
- Enhanced Input integration
- Player control input
- Camera control rotation
- Movement component integration

#### MMAnimInstance
The animation instance managing:
- Motion data gathering and analysis
- Trajectory generation and prediction
- Environment scanning and caching
- Pose search queries and animation selection
- Speed matching and clustering
- Debug visualization

#### Data Structures
- **FMotionData**: Adaptive circular buffer for motion history
- **FCharacterMotionContext**: Current motion state and analysis
- **FEnvironmentContext**: Environmental awareness data
- **FTransformTrajectory**: Predicted future trajectory
- **FSemanticSpeedCluster**: Speed clustering for animation matching

### Motion Matching Pipeline

1. **Motion Gathering** (`GatherMotionData`)
   - Capture current velocity, acceleration, position
   - Store in adaptive motion buffer
   - Update motion analysis (speed, direction, state)

2. **Environment Scanning** (`UpdateEnvironmentCache`)
   - Async trace queries for ground, walls, edges, ceiling
   - Spatial hash caching for efficiency
   - Adaptive LOD based on motion complexity

3. **Trajectory Building** (`BuildTrajectory`)
   - Generate future trajectory predictions
   - Apply rotation system (CMC or manual)
   - Incorporate speed matching if enabled
   - Apply direction change detection

4. **Animation Context** (`ExtractAnimationContext`)
   - Extract current animation state
   - Determine interruptibility
   - Track foot plant events

5. **Pose Search Query**
   - Query motion matching database
   - Weight trajectory vs pose importance
   - Apply continuing pose bias
   - Select optimal animation

## Performance Considerations

### Optimization Features
- **Async Tracing**: All environment queries use async traces
- **Spatial Caching**: Reduces redundant environment checks
- **Adaptive LOD**: Scales environment scanning based on need
- **Motion Buffer**: Circular buffer with adaptive sizing
- **Speed Clustering**: Pre-computed clusters for fast matching

### Performance Metrics
Monitor performance using `bShowPerformanceStats`:
- Cache hit rates
- Traces per frame
- Average frame time
- Current LOD level

## Customization

### Extending the System

1. **Custom Motion States**: Add new locomotion states in `ELocomotionState`
2. **Custom Speed Clusters**: Define semantic speed clusters for your animations
3. **Custom Rotation Modes**: Extend rotation system with new modes
4. **Custom Environment Checks**: Add new trace types for specific needs

### Blueprint Integration

All major systems are exposed to Blueprint:
- Motion state queries
- Trajectory manipulation
- Environment context access
- Debug visualization controls
- Performance monitoring

## Troubleshooting

### Common Issues

**Animation not matching properly**:
- Verify Motion Matching Database is assigned
- Check trajectory time offsets are appropriate for your animations
- Ensure pose database is properly configured with channels

**Performance issues**:
- Reduce environment scan frequency via LOD settings
- Decrease motion buffer size if memory is a concern
- Disable debug visualization in shipping builds

**Input not responding**:
- Verify Enhanced Input mapping context is added on BeginPlay
- Check Input Actions are properly assigned
- Ensure character has a Player Controller

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

This project is provided as-is for educational and demonstration purposes.

## Credits

- Built with Unreal Engine 5.6
- Uses Epic Games' PoseSearch and Motion Matching systems
- Implements concepts from modern game animation research

## Additional Resources

- [Unreal Engine Motion Matching Documentation](https://docs.unrealengine.com/5.6/en-US/motion-matching-in-unreal-engine/)
- [PoseSearch Plugin Documentation](https://docs.unrealengine.com/5.6/en-US/pose-search-plugin-in-unreal-engine/)
- [Enhanced Input Documentation](https://docs.unrealengine.com/5.6/en-US/enhanced-input-in-unreal-engine/)

## Contact

For questions or feedback, please open an issue on the GitHub repository.

---

**Note**: This is a demonstration project showcasing motion matching techniques. For production use, additional optimization and testing may be required based on your specific requirements.
