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
        Core
        Frame
        RHI
        RHI_Interface
        DX12
    PRIVATE
        dxgi
        d3d12
        dxcompiler
)

target_precompile_headers(Renderer PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")

if(WIN32)
    find_program(AETHER_DXC_EXECUTABLE NAMES dxc
        HINTS
            "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
            "$ENV{WindowsSdkDir}Bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    )
    if(NOT AETHER_DXC_EXECUTABLE)
        file(GLOB _AETHER_DXC_SDK_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
        if(_AETHER_DXC_SDK_CANDIDATES)
            list(SORT _AETHER_DXC_SDK_CANDIDATES ORDER ASCENDING COMPARE STRING)
            list(GET _AETHER_DXC_SDK_CANDIDATES -1 AETHER_DXC_EXECUTABLE)
        endif()
    endif()
    if(AETHER_DXC_EXECUTABLE)
        cmake_path(CONVERT "${AETHER_DXC_EXECUTABLE}" TO_CMAKE_PATH_LIST AETHER_DXC_EXECUTABLE_NORMALIZED)
        target_compile_definitions(Renderer PRIVATE "AETHER_DXC_EXECUTABLE=\"${AETHER_DXC_EXECUTABLE_NORMALIZED}\"")
    endif()
endif()

# Visual Studio 等での仮想フォルダ分け（任意）
set_target_properties(Renderer PROPERTIES FOLDER "Engine/Renderer")