project(PhysX)

# Create an imported interface library
add_library(PhysX STATIC IMPORTED GLOBAL)
set_target_properties(PhysX PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysX_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysX_64.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(LowLevel STATIC IMPORTED GLOBAL)
set_target_properties(LowLevel PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/LowLevel_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/LowLevel_static_64.lib"
)

add_library(LowLevelAABB STATIC IMPORTED GLOBAL)
set_target_properties(LowLevelAABB PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/LowLevelAABB_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/LowLevelAABB_static_64.lib"
)

add_library(LowLevelDynamics STATIC IMPORTED GLOBAL)
set_target_properties(LowLevelDynamics PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/LowLevelDynamics_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/LowLevelDynamics_static_64.lib"
)

add_library(PhysXCharacterKinematic STATIC IMPORTED GLOBAL)
set_target_properties(PhysXCharacterKinematic PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysXCharacterKinematic_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysXCharacterKinematic_static_64.lib"
)

add_library(PhysXCommon STATIC IMPORTED GLOBAL)
set_target_properties(PhysXCommon PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysXCommon_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysXCommon_64.lib"
)

add_library(PhysXCooking STATIC IMPORTED GLOBAL)
set_target_properties(PhysXCooking PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysXCooking_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysXCooking_64.lib"
)

add_library(PhysXExtensions STATIC IMPORTED GLOBAL)
set_target_properties(PhysXExtensions PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysXExtensions_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysXExtensions_static_64.lib"
)

add_library(PhysXFoundation STATIC IMPORTED GLOBAL)
set_target_properties(PhysXFoundation PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysXFoundation_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysXFoundation_64.lib"
)

add_library(PhysXTask STATIC IMPORTED GLOBAL)
set_target_properties(PhysXTask PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/PhysXTask_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/PhysXTask_static_64.lib"
)

add_library(SceneQuery STATIC IMPORTED GLOBAL)
set_target_properties(SceneQuery PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/SceneQuery_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/SceneQuery_static_64.lib"
)

add_library(SimulationController STATIC IMPORTED GLOBAL)
set_target_properties(SimulationController PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/SimulationController_static_64.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/SimulationController_static_64.lib"
)

target_link_libraries(PhysX INTERFACE
    LowLevel
    LowLevelAABB
    LowLevelDynamics
    PhysXCharacterKinematic
    PhysXCommon
    PhysXCooking
    PhysXExtensions
    PhysXFoundation
    PhysXTask
    SceneQuery
    SimulationController
)

set_target_properties(PhysX LowLevel LowLevelAABB LowLevelDynamics PhysXCharacterKinematic PhysXCommon PhysXCooking PhysXExtensions PhysXFoundation PhysXTask SceneQuery SimulationController PROPERTIES FOLDER "ThirdParty/PhysX")