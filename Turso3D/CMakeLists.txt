# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3D)

file (GLOB SOURCE_FILES *.cpp *.h)

add_library (${TARGET_NAME} STATIC ${SOURCE_FILES})
