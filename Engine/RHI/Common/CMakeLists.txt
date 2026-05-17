# RHI モジュール

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE RHI_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${RHI_SRC})

# ライブラリとして追加（STATIC or OBJECT）
add_library(RHI_Common INTERFACE ${RHI_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

# インクルードディレクトリは Engine/ 全体を対象に
target_include_directories(RHI_Common
    INTERFACE
        ${CMAKE_SOURCE_DIR}
)

# 依存する内部モジュール・外部ライブラリ
target_link_libraries(RHI_Common
    INTERFACE
        Core
)

target_precompile_headers(RHI_Common INTERFACE ${COMMON_PCH})

# Visual Studio 等での仮想フォルダ分け（任意）
set_target_properties(RHI_Common PROPERTIES FOLDER "Engine/RHI/")