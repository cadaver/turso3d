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

add_engine_directory (Graphics)
add_engine_directory (Input)
add_engine_directory (IO)
add_engine_directory (Math)
add_engine_directory (Object)
add_engine_directory (Renderer)
add_engine_directory (Resource)
add_engine_directory (Scene)
add_engine_directory (Thread)
add_engine_directory (Time)

add_definitions (-DSDL_MAIN_HANDLED)

if (TURSO3D_TRACY)
    add_definitions (-DTRACY_ENABLE)
endif ()

add_library (${TARGET_NAME} ${SOURCE_FILES})

target_include_directories (${TARGET_NAME} PUBLIC .)

target_link_libraries (${TARGET_NAME} PUBLIC SDL2-static Tracy GLEW STB)