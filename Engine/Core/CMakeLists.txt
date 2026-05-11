# Core モジュール

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE Core_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Core_SRC})

# インターフェイスライブラリとして作成
add_library(Core STATIC ${Core_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

# インクルードディレクトリ（エンジンルート）
target_include_directories(Core
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(Core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.h)

target_compile_options(Core PRIVATE /FI"Engine/Core/pch.h")

set_target_properties(Core PROPERTIES FOLDER "Engine/Core")

