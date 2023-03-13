# Set CMake minimum version
cmake_minimum_required (VERSION 2.8.12)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set project name
project (Turso3D)

# Define CMake options
include (CMakeDependentOption)
option (TURSO3D_TRACY "Enable Tracy profiler" FALSE)

# Set default configuration to Release for single-configuration generators
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

# Compiler-specific setup
if (MSVC)
    set (RELEASE_RUNTIME /MT)
    set (DEBUG_RUNTIME /MTd)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_RUNTIME}")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_RUNTIME} /fp:fast /Zi /GS-")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_RUNTIME} /W4")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_RUNTIME} /W4 /fp:fast /Zi /GS- ")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /DEBUG")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
elseif (NOT XCODE)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wno-invalid-offsetof -ffast-math")
    if (WIN32)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
        set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
        set (CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
        set (CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    endif ()
endif ()

# Macro to set output directories on all configurations
macro (set_output_directories OUTPUT_PATH)
    foreach (TYPE ${ARGN})
        set (CMAKE_${TYPE}_OUTPUT_DIRECTORY ${OUTPUT_PATH})
        foreach (CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string (TOUPPER ${CONFIG} CONFIG)
            set (CMAKE_${TYPE}_OUTPUT_DIRECTORY_${CONFIG} ${OUTPUT_PATH})
        endforeach ()
    endforeach ()
endmacro ()

# Set output directory for executables
set_output_directories (${PROJECT_SOURCE_DIR}/Bin RUNTIME PDB)

add_subdirectory (ThirdParty)
add_subdirectory (Turso3D)
add_subdirectory (Turso3DTest)