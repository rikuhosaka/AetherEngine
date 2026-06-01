# WinPixEventRuntime (MIT) — https://www.nuget.org/packages/WinPixEventRuntime
#
# Fetched from NuGet on first configure when missing, or use a local tree:
#   -DCMAKE_WINPIX_EVENT_RUNTIME_ROOT="path/to/extracted/nupkg"

set(WINPIX_EVENT_RUNTIME_VERSION "1.0.240308001" CACHE STRING "WinPixEventRuntime NuGet version")

if(WINPIX_EVENT_RUNTIME_ROOT)
    set(_winpix_root "${WINPIX_EVENT_RUNTIME_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/package/Include/WinPixEventRuntime/pix3.h")
    set(_winpix_root "${CMAKE_CURRENT_SOURCE_DIR}/package")
else()
    set(_winpix_root "${CMAKE_CURRENT_BINARY_DIR}/nuget/extracted")
    set(_winpix_nupkg "${CMAKE_CURRENT_BINARY_DIR}/nuget/WinPixEventRuntime.${WINPIX_EVENT_RUNTIME_VERSION}.nupkg")
    set(_winpix_url
        "https://api.nuget.org/v3-flatcontainer/winpixeventruntime/${WINPIX_EVENT_RUNTIME_VERSION}/winpixeventruntime.${WINPIX_EVENT_RUNTIME_VERSION}.nupkg")

    if(NOT EXISTS "${_winpix_root}/Include/WinPixEventRuntime/pix3.h")
        if(NOT EXISTS "${_winpix_nupkg}")
            message(STATUS "Downloading WinPixEventRuntime ${WINPIX_EVENT_RUNTIME_VERSION} from NuGet...")
            file(DOWNLOAD "${_winpix_url}" "${_winpix_nupkg}" STATUS _winpix_download_status)
            list(GET _winpix_download_status 0 _winpix_download_code)
            if(NOT _winpix_download_code EQUAL 0)
                message(FATAL_ERROR "Failed to download WinPixEventRuntime (${_winpix_url})")
            endif()
        endif()

        file(ARCHIVE_EXTRACT INPUT "${_winpix_nupkg}" DESTINATION "${_winpix_root}")
    endif()
endif()

if(NOT EXISTS "${_winpix_root}/Include/WinPixEventRuntime/pix3.h")
    message(FATAL_ERROR "WinPixEventRuntime headers not found under '${_winpix_root}'")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_winpix_arch "x64")
else()
    message(FATAL_ERROR "WinPixEventRuntime is only supported on 64-bit builds")
endif()

set(_winpix_lib "${_winpix_root}/bin/${_winpix_arch}/WinPixEventRuntime.lib")
set(_winpix_dll "${_winpix_root}/bin/${_winpix_arch}/WinPixEventRuntime.dll")

if(NOT EXISTS "${_winpix_lib}")
    message(FATAL_ERROR "WinPixEventRuntime import library not found: ${_winpix_lib}")
endif()

add_library(WinPixEventRuntime SHARED IMPORTED GLOBAL)
set_target_properties(WinPixEventRuntime PROPERTIES
    IMPORTED_IMPLIB "${_winpix_lib}"
    IMPORTED_LOCATION "${_winpix_dll}"
    INTERFACE_INCLUDE_DIRECTORIES "${_winpix_root}/Include"
)

message(STATUS "WinPixEventRuntime: ${_winpix_root} (${_winpix_arch})")
