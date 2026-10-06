cmake_minimum_required(VERSION 3.16)
project(AetherEngine)

option(AETHER_ENABLE_PIX "Link WinPixEventRuntime for PIX GPU markers (NuGet fetch on configure)" OFF)
option(AETHER_PRECOMPILED_SHADERS_ONLY
    "Load only precompiled .cso shaders at runtime (no HLSL fallback)"
    OFF)

#変数

set(CMAKE_CXX_STANDARD 20)
# 指定したバージョンが必須であることを伝える（古いコンパイラでエラーにする）
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(COMMON_PCH
    ${CMAKE_SOURCE_DIR}/Engine/Core/pch.h
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Visual Studio IDE が設定する VS_UNICODE_OUTPUT があると cl.exe は /showIncludes を標準出力ではなく
# IDE へ直接書き出すため、Ninja がヘッダー依存を記録できずヘッダー変更で再コンパイルされなくなる。
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    foreach(lang C CXX)
        if(NOT CMAKE_${lang}_COMPILER_LAUNCHER)
            set(CMAKE_${lang}_COMPILER_LAUNCHER "${CMAKE_COMMAND}" -E env --unset=VS_UNICODE_OUTPUT)
        endif()
    endforeach()
endif()

if(WIN32)
    include(ResolveAetherDxc)
    aether_resolve_dxc(AETHER_DXC_EXECUTABLE)
endif()

# 出力ディレクトリを構成別に設定
foreach(OUTPUTCONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG})
endforeach()

# サブプロジェクト（ThirdParty は Engine より先 — DirectXTex / WinPixEventRuntime 等）
add_subdirectory(ThirdParty)
set_target_properties(ThirdParty PROPERTIES FOLDER "ThirdParty")

add_subdirectory(Engine)
set_target_properties(Engine PROPERTIES FOLDER "Engine")

# Assets フォルダ内のすべてのファイルを再帰的に取得
file(GLOB_RECURSE ASSETS_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Assets/*.*"
)

# 仮想プロジェクト "Assets" を作成（ビルド対象ではない）
add_custom_target(AssetsFolder SOURCES ${ASSETS_FILES})

# 階層構造を維持して source_group を設定
foreach(file_path IN LISTS ASSETS_FILES)
    # 相対パスを取得（例：Assets/Shaders/xxx.hlsl）
    file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}" "${file_path}")

    # ディレクトリ部分を抽出
    get_filename_component(dir "${rel_path}" PATH)

    # Visual Studio 向けに \ 区切りに変換
    string(REPLACE "/" "\\" group "${dir}")

    # ソースグループとして登録
    source_group("${group}" FILES "${file_path}")
endforeach()

# シェーダーファイルを全探索
file(GLOB_RECURSE HLSL_SHADERS "${CMAKE_SOURCE_DIR}/Assets/Shaders/*.hlsl")

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/CompiledShaders")
set(SHADER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Assets/Shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

file(GLOB HLSL_INCLUDES CONFIGURE_DEPENDS "${SHADER_INCLUDE_DIR}/Include/*.hlsli")

if(WIN32)
    set(DXC_EXECUTABLE "${AETHER_DXC_EXECUTABLE}")

    #region agent log
    file(APPEND "${CMAKE_SOURCE_DIR}/debug-4f4115.log"
        "{\"sessionId\":\"4f4115\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H1\",\"location\":\"CMakeLists.txt:shader-config\",\"message\":\"Resolved DXC path\",\"data\":{\"DXC_EXECUTABLE\":\"${DXC_EXECUTABLE}\",\"SHADER_OUTPUT_DIR\":\"${SHADER_OUTPUT_DIR}\",\"CMAKE_BINARY_DIR\":\"${CMAKE_BINARY_DIR}\"},\"timestamp\":$ENV{SOURCE_DATE_EPOCH}}\n"
    )
    #endregion
else()
    message(FATAL_ERROR "HLSL compilation via dxc is only supported on Windows in this project.")
endif()

set(COMPILED_SHADERS "")

foreach(HLSL_FILE IN LISTS HLSL_SHADERS)
    get_filename_component(FILENAME "${HLSL_FILE}" NAME)
    get_filename_component(FILE_NAME_WE "${HLSL_FILE}" NAME_WE)

    # ターゲットプロファイルをファイル名に基づいて決定
    set(SHADER_TARGET "")
    if(FILENAME MATCHES ".*VS\\.hlsl$")
        set(SHADER_TARGET "vs_6_6")
    elseif(FILENAME MATCHES ".*PS\\.hlsl$")
        set(SHADER_TARGET "ps_6_6")
    elseif(FILENAME MATCHES ".*CS\\.hlsl$")
        set(SHADER_TARGET "cs_6_6")
    else()
        message(WARNING "Unknown shader type: ${FILENAME}")
        continue()
    endif()

    set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${FILE_NAME_WE}.cso")

    #region agent log
    file(APPEND "${CMAKE_SOURCE_DIR}/debug-4f4115.log"
        "{\"sessionId\":\"4f4115\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H3\",\"location\":\"CMakeLists.txt:shader-loop\",\"message\":\"Shader compile parameters\",\"data\":{\"HLSL_FILE\":\"${HLSL_FILE}\",\"FILENAME\":\"${FILENAME}\",\"ENTRY\":\"${FILE_NAME_WE}\",\"TARGET\":\"${SHADER_TARGET}\",\"OUTPUT\":\"${OUTPUT_FILE}\"},\"timestamp\":$ENV{SOURCE_DATE_EPOCH}}\n"
    )
    #endregion

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND "${DXC_EXECUTABLE}"
            -T ${SHADER_TARGET}
            -E ${FILE_NAME_WE}
            -I ${SHADER_INCLUDE_DIR}
            -Fo ${OUTPUT_FILE}
            ${HLSL_FILE}
        DEPENDS ${HLSL_FILE} ${HLSL_INCLUDES}
        COMMENT "Compiling ${FILENAME} -> ${OUTPUT_FILE}"
        VERBATIM
    )

    list(APPEND COMPILED_SHADERS ${OUTPUT_FILE})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
set(AETHER_COMPILED_SHADERS ${COMPILED_SHADERS})

add_subdirectory(Runtime)
set_target_properties(Runtime PROPERTIES FOLDER "Runtime")

add_subdirectory(Game)
set_target_properties(Game PROPERTIES FOLDER "Game")