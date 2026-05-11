
# Create an imported interface library
add_library(DirectXTex STATIC IMPORTED GLOBAL)

set_target_properties(DirectXTex PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/DirectXTex.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/DirectXTex.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set_target_properties(DirectXTex PROPERTIES FOLDER "ThirdParty/DirectXTex")