# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3D)

# Macro to add files from a directory grouped to the solution file
macro (add_engine_directory DIRECTORY)
    file (GLOB DIR_FILES ${DIRECTORY}/*.h ${DIRECTORY}/*.c ${DIRECTORY}/*.cpp)
    get_filename_component (GROUP ${DIRECTORY} NAME)
    source_group ("${GROUP}" FILES ${DIR_FILES})
    list (APPEND SOURCE_FILES ${DIR_FILES})
endmacro ()

# Add source files from subdirectories
add_engine_directory (Base)
add_engine_directory (Debug)
add_engine_directory (IO)
add_engine_directory (Math)
add_engine_directory (Thread)

# Add root-level files
file (GLOB TURSO3D_FILES *.h *.c *.cpp)
list (APPEND SOURCE_FILES ${TURSO3D_FILES})

add_library (${TARGET_NAME} STATIC ${SOURCE_FILES})
