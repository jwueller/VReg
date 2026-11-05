# The single-header form of VReg (vreg.h), generated on
# every build by cmake/VRegAmalgamate.cmake so a commit made since configuring
# cannot leave the version in it stale.
set(VREG_AMALGAMATION_DIR ${PROJECT_BINARY_DIR}/amalgamation)

add_custom_target(
    VReg_Amalgamation
    COMMAND
        ${CMAKE_COMMAND}
        -D SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -D OUTPUT_DIR=${VREG_AMALGAMATION_DIR}
        -D COMMIT=${VREG_SOURCE_COMMIT}
        -D STRICT=${PROJECT_IS_TOP_LEVEL}
        -P ${PROJECT_SOURCE_DIR}/cmake/VRegAmalgamate.cmake
    BYPRODUCTS
        ${VREG_AMALGAMATION_DIR}/vreg.h
    VERBATIM
)
