include(CMakePackageConfigHelpers)

get_property(vreg_libraries GLOBAL PROPERTY VREG_LIBRARIES)
list(REMOVE_ITEM vreg_libraries VReg_Detours)

install(
    TARGETS ${vreg_libraries}
    EXPORT VRegTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# VReg::Detours is exported on its own, so a consumer that does not ask for it
# needs no Detours to find the package.
if(TARGET VReg_Detours)
    install(
        TARGETS VReg_Detours
        EXPORT VRegDetoursTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

set(vreg_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/VReg)

install(
    EXPORT VRegTargets
    NAMESPACE VReg::
    DESTINATION ${vreg_cmake_dir}
)

configure_package_config_file(
    cmake/VRegConfig.cmake.in
    ${PROJECT_BINARY_DIR}/VRegConfig.cmake
    INSTALL_DESTINATION ${vreg_cmake_dir}
)

# Numeric: find_package cannot compare PROJECT_VERSION's semver string.
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/VRegConfigVersion.cmake
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    COMPATIBILITY SameMinorVersion
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/VRegConfig.cmake
        ${PROJECT_BINARY_DIR}/VRegConfigVersion.cmake
    DESTINATION ${vreg_cmake_dir}
)

if(TARGET VReg_Detours)
    install(
        EXPORT VRegDetoursTargets
        NAMESPACE VReg::
        DESTINATION ${vreg_cmake_dir}
    )

    install(FILES cmake/FindDetours.cmake DESTINATION ${vreg_cmake_dir})
endif()
