# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME 02_Scene)

file (GLOB SOURCE_FILES *.cpp *.h)

add_executable (${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries (${TARGET_NAME} Turso3D)