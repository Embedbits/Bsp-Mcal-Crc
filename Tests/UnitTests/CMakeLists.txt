#======================== Unit tests configuration ============================#
# Unit tests of Crc module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).

get_filename_component(CRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MCAL_DIR "${CRC_DIR}/.." ABSOLUTE)

# DMA functionality: GPDMA on STM32H7R / STM32H7S (tests of the STM32H5 module), DMA streams on the
# classic STM32H7 lines
if(MCU_FAMILY_ID STREQUAL "STM32H7RSxx")
    set(CRC_UT_SOURCE   Test_Crc_Gpdma.c)
    set(CRC_UT_DMA_DIR  ${MCAL_DIR}/Gpdma)
    set(CRC_UT_DMA_MOCK ${MCAL_DIR}/Gpdma/Gpdma_Port.h)
else()
    set(CRC_UT_SOURCE   Test_Crc.c)
    set(CRC_UT_DMA_DIR  ${MCAL_DIR}/Dma)
    set(CRC_UT_DMA_MOCK ${MCAL_DIR}/Dma/Dma_Port.h)
endif()

UnitTesting_Add_Test(
    NAME            Crc
    TEST_SOURCE     ${CRC_UT_SOURCE}
    UUT_SOURCES     ${CRC_DIR}/Crc.c
    MOCK_HEADERS    ${MCAL_DIR}/Rcc/Rcc_Port.h
                    ${CRC_UT_DMA_MOCK}
    INCLUDE_DIRS    ${CRC_DIR}
                    ${MCAL_DIR}/Rcc
                    ${CRC_UT_DMA_DIR}
    LINK_LIBS       ut_RegMem                           # Registers in host memory
                    ut_CmsisHost                        # CMSIS core intrinsics on host
    FORCE_INCLUDES  ${UNIT_TESTING_CMSIS_HOST_HEADER}   # Replaces cmsis_gcc.h
    HOST_LIBS       Ral_Lib                             # ST LL compiled for host
    COVERAGE_EXCLUDE ".*/Ral/.*"
)
