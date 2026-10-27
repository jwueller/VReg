include(CMakePackageConfigHelpers)

get_property(vreg_libraries GLOBAL PROPERTY VREG_LIBRARIES)

install(
    TARGETS ${vreg_libraries}
    EXPORT VRegTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    FILES ${PROJECT_BINARY_DIR}/include/VReg/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VReg
)

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
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/VRegConfigVersion.cmake
    COMPATIBILITY SameMinorVersion
)
install(
    FILES
        ${PROJECT_BINARY_DIR}/VRegConfig.cmake
        ${PROJECT_BINARY_DIR}/VRegConfigVersion.cmake
    DESTINATION ${vreg_cmake_dir}
)
