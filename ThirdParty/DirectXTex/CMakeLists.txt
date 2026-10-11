# 同梱の lib/Debug と lib/Release は別ツールチェーンで作られている。
# エンジンと同じコンパイラと /MD で、sorce/DirectXTex から静的ライブラリを作る。
set(DIRECTXTEX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sorce/DirectXTex")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(DIRECTXTEX_SOURCES
    "${DIRECTXTEX_ROOT}/BC.h"
    "${DIRECTXTEX_ROOT}/DDS.h"
    "${DIRECTXTEX_ROOT}/DirectXTex.h"
    "${DIRECTXTEX_ROOT}/DirectXTex.inl"
    "${DIRECTXTEX_ROOT}/DirectXTexP.h"
    "${DIRECTXTEX_ROOT}/filters.h"
    "${DIRECTXTEX_ROOT}/scoped.h"
    "${DIRECTXTEX_ROOT}/BC.cpp"
    "${DIRECTXTEX_ROOT}/BC4BC5.cpp"
    "${DIRECTXTEX_ROOT}/BC6HBC7.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexCompress.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexConvert.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexDDS.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexFlipRotate.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexHDR.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexImage.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexMipmaps.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexMisc.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexNormalMaps.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexPMAlpha.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexResize.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexTGA.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexUtil.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexWIC.cpp"
    "${DIRECTXTEX_ROOT}/BCDirectCompute.h"
    "${DIRECTXTEX_ROOT}/BCDirectCompute.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexCompressGPU.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexD3D11.cpp"
    "${DIRECTXTEX_ROOT}/DirectXTexD3D12.cpp"
)

file(GLOB DIRECTX_FXC_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/fxc.exe")
if(NOT DIRECTX_FXC_CANDIDATES)
    message(FATAL_ERROR "DirectXTex のコンピュートシェーダには Windows SDK の fxc.exe が必要です")
endif()
list(SORT DIRECTX_FXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
list(GET DIRECTX_FXC_CANDIDATES 0 DIRECTX_FXC_TOOL)

set(DIRECTXTEX_COMPILED_SHADERS "${CMAKE_CURRENT_BINARY_DIR}/Shaders/Compiled")
set(DIRECTXTEX_SHADER_INC "${DIRECTXTEX_COMPILED_SHADERS}/BC6HEncode_EncodeBlockCS.inc")
file(MAKE_DIRECTORY "${DIRECTXTEX_COMPILED_SHADERS}")

add_custom_command(
    OUTPUT "${DIRECTXTEX_SHADER_INC}"
    COMMAND ${CMAKE_COMMAND} -E env
        "CompileShadersOutput=${DIRECTXTEX_COMPILED_SHADERS}"
        "LegacyShaderCompiler=${DIRECTX_FXC_TOOL}"
        "${DIRECTXTEX_ROOT}/Shaders/CompileShaders.cmd"
    WORKING_DIRECTORY "${DIRECTXTEX_ROOT}/Shaders"
    DEPENDS
        "${DIRECTXTEX_ROOT}/Shaders/CompileShaders.cmd"
        "${DIRECTXTEX_ROOT}/Shaders/BC6HEncode.hlsl"
        "${DIRECTXTEX_ROOT}/Shaders/BC7Encode.hlsl"
    COMMENT "Compiling DirectXTex compute shaders"
)

list(APPEND DIRECTXTEX_SOURCES "${DIRECTXTEX_SHADER_INC}")

add_library(DirectXTex STATIC ${DIRECTXTEX_SOURCES})

set_target_properties(DirectXTex PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    FOLDER "ThirdParty/DirectXTex"
)

target_include_directories(DirectXTex
    PUBLIC
        "${DIRECTXTEX_ROOT}"
    PRIVATE
        "${DIRECTXTEX_COMPILED_SHADERS}"
        "${CMAKE_SOURCE_DIR}/ThirdParty/DirectX-Headers/include/directX"
)

target_compile_definitions(DirectXTex PRIVATE
    _UNICODE
    UNICODE
    _CRT_STDIO_ARBITRARY_WIDE_SPECIFIERS
)

target_precompile_headers(DirectXTex PRIVATE "${DIRECTXTEX_ROOT}/DirectXTexP.h")

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(DirectXTex PUBLIC OpenMP::OpenMP_CXX)
endif()

target_link_libraries(DirectXTex PUBLIC ole32)

target_link_libraries(DirectXTex PRIVATE
    d3d11
    d3d12
    dxguid
)
