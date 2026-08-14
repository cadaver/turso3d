# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3DTest)

file (GLOB SOURCE_FILES *.h *.cpp)

add_definitions (-DGLEW_STATIC -DSDL_MAIN_HANDLED)

if (TURSO3D_TRACY)
    add_definitions (-DTRACY_ENABLE)
endif ()

if (EMSCRIPTEN)
    set (CMAKE_EXE_LINKER_FLAGS "-s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH --preload-file ${PROJECT_SOURCE_DIR}/Bin/Data@Data")
    set (CMAKE_EXECUTABLE_SUFFIX ".html")
endif ()

add_executable (${TARGET_NAME} ${SOURCE_FILES})

target_link_libraries (${TARGET_NAME} Turso3D)

if (WIN32)
    target_link_libraries (${TARGET_NAME} winmm imm32 ole32 oleaut32 setupapi version uuid opengl32)
elseif (APPLE)
    target_link_libraries (${TARGET_NAME} "-framework Carbon" "-framework Cocoa" "-framework OpenGL")
else ()
    target_link_libraries (${TARGET_NAME} -lGL -lpthread)
endif ()
