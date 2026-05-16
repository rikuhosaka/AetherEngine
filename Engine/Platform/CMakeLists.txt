# Platform モジュール
project(Platform)

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE Platform_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# ライブラリとして追加（STATIC or OBJECT）
add_library(Platform STATIC ${Platform_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

# インクルードディレクトリは Engine/ 全体を対象に
target_include_directories(Platform
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)


# Visual Studio 等での仮想フォルダ分け（任意）
set_target_properties(Platform PROPERTIES FOLDER "Engine/Platform")

target_precompile_headers(Platform PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")