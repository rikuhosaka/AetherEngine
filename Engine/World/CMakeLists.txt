file(GLOB_RECURSE World_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${World_SRC})

add_library(World STATIC ${World_SRC})

# 型の宣言は Camera にあるが、実装は World を使うためこちらでコンパイルする。
# Camera 側の GLOB からは外してある。片方だけ戻すと未定義か二重定義になる。
target_sources(World PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Camera/CameraSystemComponent.cpp")

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(World
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(World
    PUBLIC
        Application
        Camera
        Core
        Math
        Physics
)

# pch が windows.h を先に取り込む。NOMINMAX はコンパイル定義として先に渡す。
target_compile_definitions(World PRIVATE NOMINMAX)

target_precompile_headers(World PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(World PROPERTIES FOLDER "Engine/World")
