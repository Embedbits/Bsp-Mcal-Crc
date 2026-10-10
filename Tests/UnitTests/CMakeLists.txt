#======================== Unit tests configuration ============================#
# Unit tests of Crc module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).

get_filename_component(CRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MCAL_DIR "${CRC_DIR}/.." ABSOLUTE)

UnitTesting_Add_Test(
    NAME            Crc
    TEST_SOURCE     Test_Crc.c
    UUT_SOURCES     ${CRC_DIR}/Crc.c
    MOCK_HEADERS    ${MCAL_DIR}/Rcc/Rcc_Port.h
                    ${MCAL_DIR}/Dma/Dma_Port.h
    INCLUDE_DIRS    ${CRC_DIR}
                    ${MCAL_DIR}/Rcc
                    ${MCAL_DIR}/Dma
    LINK_LIBS       ut_RegMem                           # Registers in host memory
                    ut_CmsisHost                        # CMSIS core intrinsics on host
    FORCE_INCLUDES  ${UNIT_TESTING_CMSIS_HOST_HEADER}   # Replaces cmsis_gcc.h
    HOST_LIBS       Ral_Lib                             # ST LL compiled for host
    COVERAGE_EXCLUDE ".*/Ral/.*"
)
