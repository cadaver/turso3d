# For conditions of distribution and use, see copyright notice in License.txt

project (Turso3D)

cmake_minimum_required (VERSION 2.8)

# Compiler-specific setup
if (MSVC)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT /fp:fast /Zi /GS-")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W4 /MTd")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} /W4 /MT /fp:fast /Zi /GS- ")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /DEBUG")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
elseif (NOT XCODE)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-invalid-offsetof -ffast-math")
    if (WIN32)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
        set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
        set (CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
        set (CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
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

# Set Turso3D as a global include directory
include_directories (Turso3D)

# Set output for executables
set_output_directories (${PROJECT_SOURCE_DIR}/Bin RUNTIME PDB)

add_subdirectory (Turso3D)
add_subdirectory (Tests)
