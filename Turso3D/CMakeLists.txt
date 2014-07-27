# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3D)

macro (add_source_group GROUP GROUP_FILES)
    source_group (${GROUP} FILES ${GROUP_FILES})
    list (APPEND SOURCE_FILES ${GROUP_FILES})
endmacro ()

set (BASE_FILES
    AutoPtr.h
    DebugNew.h
    SharedPtr.h
    Swap.h
    WeakPtr.cpp
    WeakPtr.h
    Vector.cpp
    Vector.h
)

add_source_group (Base "${BASE_FILES}")

add_library (${TARGET_NAME} STATIC ${SOURCE_FILES})
