# Graphics モジュール

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE Graphics_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Graphics_SRC})

# ライブラリとして追加（STATIC or OBJECT）
add_library(Graphics STATIC ${Graphics_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

# インクルードディレクトリは Engine/ 全体を対象に
target_include_directories(Graphics
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# 依存する内部モジュール・外部ライブラリ
target_link_libraries(Graphics
    PUBLIC
        RHI
        dxgi
        d3d12
        d3dcompiler

    PRIVATE
        DirectXTex
)

target_precompile_headers(Graphics PRIVATE ${COMMON_PCH})

target_compile_options(Graphics PRIVATE /FI"Engine/Core/pch.h")
# Visual Studio 等での仮想フォルダ分け（任意）
set_target_properties(Graphics PROPERTIES FOLDER "Engine/Graphics")