
add_library(ThirdParty INTERFACE)

# DirectXTex（ソースあり → add_subdirectory）
add_subdirectory(DirectXTex)
set_target_properties(DirectXTex PROPERTIES FOLDER "ThirdParty")

add_subdirectory(DirectX-Headers)
set_target_properties(DirectX-Headers PROPERTIES FOLDER "ThirdParty")

add_subdirectory(Fbxsdk)
set_target_properties(Fbxsdk PROPERTIES FOLDER "ThirdParty")

add_subdirectory(Jolt)
set_target_properties(Jolt PROPERTIES FOLDER "ThirdParty")

target_include_directories(ThirdParty INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

