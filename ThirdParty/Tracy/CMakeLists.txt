set (TARGET_NAME Tracy)

add_definitions (-DTRACY_ENABLE)

file (GLOB SOURCE_FILES *.cpp)

add_library (${TARGET_NAME} ${SOURCE_FILES})
include_directories (.)