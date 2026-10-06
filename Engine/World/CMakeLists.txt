file(GLOB_RECURSE World_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${World_SRC})

add_library(World STATIC ${World_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(World
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(World
    PUBLIC
        Core
        Math
        Renderer
)

target_precompile_headers(World PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(World PROPERTIES FOLDER "Engine/World")
