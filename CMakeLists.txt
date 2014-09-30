# For conditions of distribution and use, see copyright notice in License.txt

project (Turso3D)

cmake_minimum_required (VERSION 2.8)

# Find DirectX SDK
# Based on realXtend Tundra CMake build system (https://github.com/realXtend/tundra)
if (MSVC)
    # Use only Windows SDK from Visual Studio 2012 onward to avoid incompatibility
    # between DirectX SDK and Windows SDK defines and the resulting warning spam
    if (MSVC_VERSION LESS 1700)
        set (DIRECTX_INC_SEARCH_PATH
            "C:/Program Files (x86)/Microsoft DirectX SDK*/Include"
            "C:/Program Files/Microsoft DirectX SDK*/Include"
            "$ENV{DIRECTX_ROOT}/Include"
            "$ENV{DXSDK_DIR}/Include")
        find_path (DIRECT3D_INCLUDE_DIRS d3d11.h ${DIRECTX_INC_SEARCH_PATH})
    
        if (CMAKE_CL_64)
            set (DIRECTX_LIB_SEARCH_PATH
                "C:/Program Files (x86)/Microsoft DirectX SDK*/Lib/x64"
                "C:/Program Files/Microsoft DirectX SDK*/Lib/x64"
                "$ENV{DIRECTX_ROOT}/Lib/x64"
                "$ENV{DXSDK_DIR}/Lib/x64")
        else ()
            set (DIRECTX_LIB_SEARCH_PATH
                "C:/Program Files (x86)/Microsoft DirectX SDK*/Lib"
                "C:/Program Files (x86)/Microsoft DirectX SDK*/Lib/x86"
                "C:/Program Files/Microsoft DirectX SDK*/Lib"
                "C:/Program Files/Microsoft DirectX SDK*/Lib/x86"
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
        message (STATUS "MSVC 2012 or newer detected, skipping DirectX SDK search")
    endif ()

    if (DIRECT3D_INCLUDE_DIRS AND D3D11_LIBRARY AND DXGI_LIBRARY AND DXGUID_LIBRARY AND D3DCOMPILER_LIBRARY)
        set (DIRECT3D_FOUND 1)
        set (DIRECT3D_LIBRARIES ${D3D11_LIBRARY} ${DXGI_LIBRARY} ${DXGUID_LIBRARY} ${D3DCOMPILER_LIBRARY})
        include (FindPackageMessage)
        FIND_PACKAGE_MESSAGE (Direct3D "Found DirectX SDK: ${DIRECT3D_LIBRARIES} ${DIRECT3D_INCLUDE_DIRS}" "[${DIRECT3D_LIBRARIES}][${DIRECT3D_INCLUDE_DIRS}]")
    else ()
        # If DirectX SDK not found, assume the includes & libs are found in Windows SDK
        set (DIRECT3D_LIBRARIES d3d11 dxgi dxguid d3dcompiler)
    endif ()
endif ()

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
