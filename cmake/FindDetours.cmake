# Finds Microsoft Detours as the vcpkg port lays it out (the header, a release
# library under lib/ and a debug one under debug/lib/, no CMake config) and
# defines `Detours::Detours`.

find_path(Detours_INCLUDE_DIR NAMES detours/detours.h)
get_filename_component(_detours_root "${Detours_INCLUDE_DIR}" DIRECTORY)
find_library(Detours_LIBRARY_RELEASE NAMES detours PATHS "${_detours_root}/lib" NO_DEFAULT_PATH)
find_library(Detours_LIBRARY_DEBUG NAMES detours PATHS "${_detours_root}/debug/lib" NO_DEFAULT_PATH)
unset(_detours_root)

include(SelectLibraryConfigurations)
select_library_configurations(Detours)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Detours REQUIRED_VARS Detours_LIBRARY Detours_INCLUDE_DIR)

if(Detours_FOUND AND NOT TARGET Detours::Detours)
    add_library(Detours::Detours UNKNOWN IMPORTED)
    set_target_properties(Detours::Detours PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${Detours_INCLUDE_DIR}")
    if(Detours_LIBRARY_RELEASE)
        set_property(TARGET Detours::Detours APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(Detours::Detours PROPERTIES IMPORTED_LOCATION_RELEASE "${Detours_LIBRARY_RELEASE}")
    endif()
    if(Detours_LIBRARY_DEBUG)
        set_property(TARGET Detours::Detours APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(Detours::Detours PROPERTIES IMPORTED_LOCATION_DEBUG "${Detours_LIBRARY_DEBUG}")
    endif()
    set_target_properties(
        Detours::Detours
        PROPERTIES
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
    )
endif()
