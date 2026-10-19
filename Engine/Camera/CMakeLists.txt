file(GLOB_RECURSE Camera_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Camera_SRC})

add_library(Camera STATIC ${Camera_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(Camera
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Camera
    PUBLIC
        Math
        Platform
        Renderer
)

target_compile_definitions(Camera PRIVATE NOMINMAX)

target_precompile_headers(Camera PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(Camera PROPERTIES FOLDER "Engine/Camera")
