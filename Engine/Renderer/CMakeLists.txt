# Renderer モジュール

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE Renderer_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Renderer_SRC})

# ライブラリとして追加（STATIC or OBJECT）
add_library(Renderer STATIC ${Renderer_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

# インクルードディレクトリは Engine/ 全体を対象に
target_include_directories(Renderer
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# 依存する内部モジュール・外部ライブラリ
target_link_libraries(Renderer
    PUBLIC
        RHI
        RHI_Interface
        DX12
)

target_precompile_headers(Renderer PRIVATE ${COMMON_PCH})

target_compile_options(Renderer PRIVATE /FI"Engine/Core/pch.h")
# Visual Studio 等での仮想フォルダ分け（任意）
set_target_properties(Renderer PROPERTIES FOLDER "Engine/Renderer")