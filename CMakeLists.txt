cmake_minimum_required(VERSION 3.16)
project(AetherEngine)

#変数

set(CMAKE_CXX_STANDARD 20)
# 指定したバージョンが必須であることを伝える（古いコンパイラでエラーにする）
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(COMMON_PCH
    ${CMAKE_SOURCE_DIR}/Engine/Core/pch.h
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 出力ディレクトリを構成別に設定
foreach(OUTPUTCONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG})
endforeach()

# サブプロジェクト
add_subdirectory(Engine)
set_target_properties(Engine PROPERTIES FOLDER "Engine")

add_subdirectory(Runtime)
set_target_properties(Runtime PROPERTIES FOLDER "Runtime")

add_subdirectory(Game)
set_target_properties(Game PROPERTIES FOLDER "Game")

add_subdirectory(ThirdParty)
set_target_properties(ThirdParty PROPERTIES FOLDER "ThirdParty")

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
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# CSO output dir must not be confused with the HLSL compiler: dxc comes from Windows SDK or PATH.
if(WIN32)
    find_program(DXC_EXECUTABLE NAMES dxc
        HINTS
            "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
            "$ENV{WindowsSdkDir}Bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    )
    if(NOT DXC_EXECUTABLE)
        file(GLOB _DXC_SDK_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
        if(_DXC_SDK_CANDIDATES)
            list(SORT _DXC_SDK_CANDIDATES ORDER ASCENDING COMPARE STRING)
            list(GET _DXC_SDK_CANDIDATES -1 DXC_EXECUTABLE)
        endif()
    endif()
    if(NOT DXC_EXECUTABLE)
        message(FATAL_ERROR "dxc.exe not found (Windows SDK HLSL compiler). Install the Windows 10/11 SDK with the Desktop C++ workload, or add dxc to PATH and re-configure.")
    endif()
    message(STATUS "HLSL compiler: ${DXC_EXECUTABLE}")

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
        set(SHADER_TARGET "vs_6_0")
    elseif(FILENAME MATCHES ".*PS\\.hlsl$")
        set(SHADER_TARGET "ps_6_0")
    elseif(FILENAME MATCHES ".*CS\\.hlsl$")
        set(SHADER_TARGET "cs_6_0")
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
            -Fo ${OUTPUT_FILE}
            ${HLSL_FILE}
        DEPENDS ${HLSL_FILE}
        COMMENT "Compiling ${FILENAME} -> ${OUTPUT_FILE}"
        VERBATIM
    )

    list(APPEND COMPILED_SHADERS ${OUTPUT_FILE})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})

add_dependencies(Runtime CompileShaders)