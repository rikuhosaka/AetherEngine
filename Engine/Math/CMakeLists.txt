# Math module (DirectXMath wrappers)

file(GLOB_RECURSE Math_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Math_SRC})

add_library(Math STATIC ${Math_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(Math
    PUBLIC
        ${CMAKE_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Math
    PUBLIC
        Core)

target_precompile_headers(Math PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")

set_target_properties(Math PROPERTIES FOLDER "Engine/Math")
