# DX12 モジュール

option(AETHER_ENABLE_DX12_DEBUG "Enable D3D12/DXGI debug layer for all build configurations" OFF)

if(AETHER_ENABLE_PIX AND NOT TARGET WinPixEventRuntime)
    message(FATAL_ERROR "AETHER_ENABLE_PIX=ON requires WinPixEventRuntime (ThirdParty/WinPixEventRuntime)")
endif()

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE DX12_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DX12_SRC})

# ライブラリとして追加（STATIC or OBJECT）
add_library(DX12 STATIC ${DX12_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

# インクルードディレクトリは Engine/ 全体を対象に
target_include_directories(DX12
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ThirdParty/DirectXTex/include
        ${CMAKE_SOURCE_DIR}/ThirdParty/DirectX-Headers/include/directX
        ${CMAKE_SOURCE_DIR}/ThirdParty/WinPixEventRuntime/include
)

# 依存する内部モジュール・外部ライブラリ
target_link_libraries(DX12
    PUBLIC
        RHI
    PRIVATE
        dxgi
        d3d12
        d3dcompiler
        dxguid
        DirectXTex
)

target_precompile_headers(DX12 PRIVATE "${CMAKE_SOURCE_DIR}/Engine/RHI/DX12/pch.h")

if(AETHER_ENABLE_DX12_DEBUG)
    target_compile_definitions(DX12 PRIVATE AETHER_DX12_DEBUG=1)
else()
    target_compile_definitions(DX12 PRIVATE
        $<$<CONFIG:Debug>:AETHER_DX12_DEBUG=1>
    )
endif()

if(AETHER_ENABLE_PIX)
    target_link_libraries(DX12 PRIVATE WinPixEventRuntime)
    target_compile_definitions(DX12 PRIVATE AETHER_PIX=1 USE_PIX=1)
endif()

# Visual Studio 等での仮想フォルダ分け（任意）
set_target_properties(DX12 PROPERTIES FOLDER "Engine/RHI/")