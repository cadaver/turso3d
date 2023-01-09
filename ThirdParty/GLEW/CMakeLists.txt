set (TARGET_NAME GLEW)

add_definitions (-DGLEW_STATIC -DGLEW_NO_GLU)

file (GLOB SOURCE_FILES *.h *.c *.cpp)

add_library (${TARGET_NAME} ${SOURCE_FILES})
include_directories (.)