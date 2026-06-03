# Resolves dxc.exe with the following priority:
#   1. ${CMAKE_SOURCE_DIR}/ThirdParty/DirectXShaderCompiler/bin/dxc.exe
#   2. ${CMAKE_BINARY_DIR}/compileShaders/bin/dxc.exe
#   3. Windows SDK / PATH

function(aether_resolve_dxc out_var)
    set(_dxc "")

    set(_bundled "${CMAKE_SOURCE_DIR}/ThirdParty/DirectXShaderCompiler/bin/dxc.exe")
    if(EXISTS "${_bundled}")
        set(_dxc "${_bundled}")
        message(STATUS "HLSL compiler (ThirdParty bundled): ${_dxc}")
    else()
        set(_manual "${CMAKE_BINARY_DIR}/compileShaders/bin/dxc.exe")
        if(EXISTS "${_manual}")
            set(_dxc "${_manual}")
            message(STATUS "HLSL compiler (build tree bundled): ${_dxc}")
        else()
            find_program(_dxc_found NAMES dxc
                HINTS
                    "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
                    "$ENV{WindowsSdkDir}Bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
            )
            if(_dxc_found)
                set(_dxc "${_dxc_found}")
                message(STATUS "HLSL compiler (system): ${_dxc}")
            else()
                file(GLOB _sdk_candidates "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
                if(_sdk_candidates)
                    list(SORT _sdk_candidates ORDER ASCENDING COMPARE STRING)
                    list(GET _sdk_candidates -1 _dxc)
                    message(STATUS "HLSL compiler (system SDK): ${_dxc}")
                endif()
            endif()
        endif()
    endif()

    if(NOT _dxc)
        message(FATAL_ERROR
            "dxc.exe not found. Place it at:\n"
            "  ${_bundled}\n"
            "  ${_manual}\n"
            "or install the Windows SDK Desktop C++ workload.")
    endif()

    set(${out_var} "${_dxc}" PARENT_SCOPE)
endfunction()
