# For conditions of distribution and use, see copyright notice in License.txt

project (Turso3D)

cmake_minimum_required (VERSION 2.8)

# Define CMake options
include (CMakeDependentOption)
option (TURSO3D_SHARED "Build Turso3D as a shared library" FALSE)
option (TURSO3D_LOGGING "Enable logging macros" TRUE)
option (TURSO3D_PROFILING "Enable performance profiling" TRUE)
cmake_dependent_option (TURSO3D_STATIC_RUNTIME "Use static C/C++ runtime library with MSVC" FALSE "MSVC" FALSE)

# Set default configuration to Release for single-configuration generators
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

if (MSVC)
    if (TURSO3D_STATIC_RUNTIME)
        set (RELEASE_RUNTIME /MT)
        set (DEBUG_RUNTIME /MTd)
    else ()
        set (RELEASE_RUNTIME "")
        set (DEBUG_RUNTIME "")
    endif ()
endif ()

# Find DirectX SDK
# Based on realXtend Tundra CMake build system (https://github.com/realXtend/tundra)
if (WIN32)
    # Do not search for the SDK from Visual Studio 2012 onward to avoid incompatibility
    # between DirectX SDK and Windows SDK defines and the resulting warning spam
    if (MSVC AND MSVC_VERSION LESS 1700)
        set (DIRECTX_INC_SEARCH_PATH
            "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Include"
            "C:/Program Files/Microsoft DirectX SDK (June 2010)/Include"
            "$ENV{DIRECTX_ROOT}/Include"
            "$ENV{DXSDK_DIR}/Include")
        find_path (DIRECT3D_INCLUDE_DIRS d3d11.h ${DIRECTX_INC_SEARCH_PATH})

        if (CMAKE_CL_64)
            set (DIRECTX_LIB_SEARCH_PATH
                "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Lib/x64"
                "C:/Program Files/Microsoft DirectX SDK (June 2010)/Lib/x64"
                "$ENV{DIRECTX_ROOT}/Lib/x64"
                "$ENV{DXSDK_DIR}/Lib/x64")
        else ()
            set (DIRECTX_LIB_SEARCH_PATH
                "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Lib"
                "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Lib/x86"
                "C:/Program Files/Microsoft DirectX SDK (June 2010)/Lib"
                "C:/Program Files/Microsoft DirectX SDK (June 2010)/Lib/x86"
                "$ENV{DIRECTX_ROOT}/Lib"
                "$ENV{DIRECTX_ROOT}/Lib/x86"
                "$ENV{DXSDK_DIR}/Lib"
                "$ENV{DXSDK_DIR}/Lib/x86")
        endif ()

        find_library (D3D11_LIBRARY d3d11 ${DIRECTX_LIB_SEARCH_PATH})
        find_library (DXGI_LIBRARY dxgi ${DIRECTX_LIB_SEARCH_PATH})
        find_library (DXGUID_LIBRARY dxguid ${DIRECTX_LIB_SEARCH_PATH})
        find_library (D3DCOMPILER_LIBRARY d3dcompiler ${DIRECTX_LIB_SEARCH_PATH})
    else ()
        message (STATUS "Skipping DirectX SDK search as the used compiler does not require it")
    endif ()

    if (DIRECT3D_INCLUDE_DIRS AND D3D11_LIBRARY AND DXGI_LIBRARY AND DXGUID_LIBRARY AND D3DCOMPILER_LIBRARY)
        set (DIRECT3D_FOUND 1)
        set (DIRECT3D_LIBRARIES ${D3D11_LIBRARY} ${DXGI_LIBRARY} ${DXGUID_LIBRARY} ${D3DCOMPILER_LIBRARY})
        include (FindPackageMessage)
        FIND_PACKAGE_MESSAGE (Direct3D "Found DirectX SDK: ${DIRECT3D_LIBRARIES} ${DIRECT3D_INCLUDE_DIRS}" "[${DIRECT3D_LIBRARIES}][${DIRECT3D_INCLUDE_DIRS}]")
    else ()
        # If DirectX SDK not found, assume the includes & libs are found in the compiler default directories
        set (DIRECT3D_LIBRARIES d3d11 dxgi dxguid d3dcompiler)
    endif ()
endif ()

# Compiler-specific setup
if (MSVC)
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
