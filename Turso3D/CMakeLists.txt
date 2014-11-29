# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3D)

# Macro to add files from a directory grouped to the solution file
macro (add_engine_directory DIRECTORY)
    file (GLOB DIR_FILES ${DIRECTORY}/*.h ${DIRECTORY}/*.c ${DIRECTORY}/*.cpp)
    get_filename_component (GROUP ${DIRECTORY} NAME)
    source_group ("${GROUP}" FILES ${DIR_FILES})
    list (APPEND SOURCE_FILES ${DIR_FILES})
endmacro ()

macro (add_engine_directory_group DIRECTORY GROUP)
    file (GLOB DIR_FILES ${DIRECTORY}/*.h ${DIRECTORY}/*.c ${DIRECTORY}/*.cpp)
    source_group ("${GROUP}" FILES ${DIR_FILES})
    list (APPEND SOURCE_FILES ${DIR_FILES})
endmacro ()

# Set definitions
if (TURSO3D_SHARED)
    add_definitions (-DTURSO3D_EXPORTS)
    set (TURSO3D_LIB_TYPE SHARED)
else ()
    set (TURSO3D_LIB_TYPE STATIC)
endif ()
if (WIN32 AND NOT TURSO3D_OPENGL)
    set (TURSO3D_D3D11 TRUE)
endif ()

# Generate the Turso3DConfig.h file
configure_file (Turso3DConfig.h.in ${CMAKE_CURRENT_SOURCE_DIR}/Turso3DConfig.h)

# Add source files from subdirectories
add_engine_directory (Base)
add_engine_directory (Debug)
add_engine_directory (Graphics)
add_engine_directory (IO)
add_engine_directory (Math)
add_engine_directory (Object)
add_engine_directory (Resource)
add_engine_directory (Scene)
add_engine_directory (Thread)
add_engine_directory (ThirdParty/STB)
add_engine_directory (Window)
if (TURSO3D_D3D11)
    add_engine_directory_group (Graphics/D3D11 Graphics)
endif ()
if (TURSO3D_OPENGL)
    add_engine_directory_group (Graphics/GL Graphics)
    add_engine_directory (ThirdParty/FlextGL)
endif ()
if (WIN32)
    add_engine_directory_group (Window/Win32 Window)
endif ()

# Add root-level files
file (GLOB TURSO3D_FILES *.h *.c *.cpp)
list (APPEND SOURCE_FILES ${TURSO3D_FILES})

# Define the Turso3D library target
add_library (${TARGET_NAME} ${TURSO3D_LIB_TYPE} ${SOURCE_FILES})

# Add third party include directories. Only used internally
include_directories (ThirdParty/STB)

# Add rendering API specific includes and library dependencies
if (TURSO3D_D3D11)
    if (DIRECT3D_FOUND)
        include_directories (${DIRECT3D_INCLUDE_DIRS})
    endif ()
    target_link_libraries (${TARGET_NAME} ${DIRECT3D_LIBRARIES})
endif ()
if (TURSO3D_OPENGL)
    include_directories (ThirdParty/FlextGL)
    find_package (OpenGL REQUIRED)
    target_link_libraries (${TARGET_NAME} ${OPENGL_gl_LIBRARY})
endif ()

# Add OS-specific library dependencies
if (WIN32)
    target_link_libraries (${TARGET_NAME} winmm)
else ()
    target_link_libraries (${TARGET_NAME} pthread)
endif ()
