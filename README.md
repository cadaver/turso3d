# Turso3D

Experimental 3D / game engine technology partially based on the Urho3D codebase. Expected to remain in an immature or "toy" state for the time being.

- OpenGL 3.2 / SDL3
- Forward+ rendering, currently up to 255 lights in view
- Threaded work queue to speed up animation and view preparation
- Caching of static shadow maps
- Hardware occlusion queries that work on the octree hierarchy
- SSAO

## Building

Execute one of the provided CMake scripts to generate build files in .build subdirectory (will be created). Execute with command line option -DTURSO3D_TRACY=1
to enable Tracy profiling.

## Test application controls

- WSAD + mouse to move
- SHIFT move faster
- F1-F3 switch scene preset
- SPACE toggle scene animation
- 1 toggle shadow modes
- 2 toggle SSAO
- 3 toggle occlusion culling
- 4 toggle scene debug draw
- 5 toggle shadow debug draw
- F toggle windowed, fullscreen and borderless fullscreen
- V toggle vsync
