/**
 * \author Mr.Nobody
 * \file Test_Crc.c
 * \ingroup Crc
 * \brief Unit tests of CRC calculation unit (CRC) module.
 *
 * Crc.c is compiled unchanged with real LL drivers. CRC registers are emulated
 * by RegMem, RCC and GPDMA modules are mocked by CMock.
 *
 * \note Emulated data register is plain RAM - it keeps the last written data
 *       instead of calculated CRC, and the reset bit (CR.RESET) is not cleared
 *       by HW. Calculated values are verified by integration tests on target.
 *
 * \note GPDMA channel ownership is static module state kept across
 *       \ref Crc_Deinit (as in the module). Tests expecting GPDMA channel
 *       initialization therefore initialize a different channel first.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Crc_Port.h"                       /* Module under test              */
#include "MockRcc_Port.h"                   /* RCC module mock                */
#include "MockGpdma_Port.h"                 /* GPDMA module mock              */
#include "Stm32_crc.h"                      /* CRC registers definition       */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static gpdma_RequestState_t Ut_Crc_GpdmaInitStub        ( gpdma_ConfigStruct_t * const configStruct, int callCnt );
static void                 Ut_Crc_TransferCpltCallback ( void );
static void                 Ut_Crc_HalfTransferCallback ( void );
static void                 Ut_Crc_TransferErrorCallback( crc_XferErrorId_t errorId );
static void                 Ut_Crc_Ignore_Mocks         ( void );
static void                 Ut_Crc_Reset_Mocks          ( void );
static void                 Ut_Crc_Expect_RccInit       ( void );
static crc_Config_t         Ut_Crc_Get_Config           ( void );
static crc_Config_t         Ut_Crc_Get_DmaConfig        ( crc_DmaChannelId_t channelId );
static void                 Ut_Crc_Init                 ( crc_Config_t * const config );
static void                 Ut_Crc_Start_Transfer       ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Peripheral under test */
#define UT_CRC_ID                           ( CRC_PERIPH_1 )

/** CRC-16/MODBUS polynomial */
#define UT_CRC_POLY16                       ( 0x8005u )

/** CRC-16/MODBUS initial value */
#define UT_CRC_INIT16                       ( 0xFFFFu )

/** GPDMA channel used by DMA tests */
#define UT_CRC_DMA_CHANNEL                  ( CRC_DMA_CHANNEL_0 )

/** Count of data bytes of non-blocking calculation */
#define UT_CRC_XFER_CNT                     ( 64u )

/** Offset of DMA data buffer in emulated SRAM (32-bit address for GPDMA) */
#define UT_CRC_XFER_SRAM_OFFSET             ( 0x100u )

/* ========================== LOCAL VARIABLES =============================== */

/** GPDMA configuration captured by Gpdma_Init() stub */
static gpdma_ConfigStruct_t     utCrc_DmaConfig;

/** GPDMA transfer configuration captured by Gpdma_Init() stub */
static gpdma_TransferConfig_t   utCrc_DmaXferConfig;

/** Count of Gpdma_Init() calls */
static uint32_t                 utCrc_DmaInitCnt;

/** Count of transfer complete callback calls */
static uint32_t                 utCrc_TransferCpltCnt;

/** Count of half transfer callback calls */
static uint32_t                 utCrc_HalfTransferCnt;

/** Count of transfer error callback calls */
static uint32_t                 utCrc_ErrorCnt;

/** Errors reported by transfer error callback (in call order) */
static crc_XferErrorId_t        utCrc_ErrorIds[ CRC_XFER_ERROR_CNT ];

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    /* Release static module context of the previous test */
    Ut_Crc_Ignore_Mocks();
    (void)Crc_Deinit();
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    utCrc_DmaInitCnt      = 0u;
    utCrc_TransferCpltCnt = 0u;
    utCrc_HalfTransferCnt = 0u;
    utCrc_ErrorCnt        = 0u;
}


void tearDown( void )
{
    /* Mocks are verified by generated runner */
}

/* ========================== MODULE VERSION ================================ */

/**
 * \brief   Crc_Get_ModuleVersion() returns version of the module.
 *
 * \details Reads the module version structure.
 *
 * \par Expected results
 * - Version is 1.1.0.
 */
void Ut_Crc_Get_ModuleVersion_ReturnsVersion( void )
{
    crc_ModuleVersion_t version = Crc_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 1u, version.Minor );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Patch );
}

/* =========================== DEFAULT CONFIG =============================== */

/**
 * \brief   Crc_Get_DefaultConfig() fills CRC-32 Ethernet configuration.
 *
 * \details Reads default configuration, then calls the function with NULL.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, CRC-32 polynomial, initial value 0xFFFFFFFF, input reversed
 *   by byte, output reversed, 8-bit input data, DMA inactive, no callbacks.
 * - NULL pointer: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Get_DefaultConfig_ReturnsDefaults( void )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( CRC_PERIPH_1, config.PeriphId );
    TEST_ASSERT_EQUAL_HEX32( 0x04C11DB7u, config.Polynomial );
    TEST_ASSERT_EQUAL( CRC_DATA_SIZE_32BITS, config.PolySize );
    TEST_ASSERT_EQUAL_HEX32( 0xFFFFFFFFu, config.InitValue );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_BYTE, config.InputDataOrder );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_WORD, config.OutputDataOrder );
    TEST_ASSERT_EQUAL( CRC_INPUT_SIZE_8BITS, config.InputDataSize );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, config.DmaState );
    TEST_ASSERT_NULL( config.TransferCpltIsr );
    TEST_ASSERT_NULL( config.HalfTransferIsr );
    TEST_ASSERT_NULL( config.TransferErrorIsr );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_DefaultConfig( NULL ) );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Crc_Init() configures registers and resets calculation unit.
 *
 * \details Initializes CRC-16/MODBUS (polynomial 0x8005, init 0xFFFF, input
 *          reversed by byte, output reversed) without DMA.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, RCC clock enabled and peripheral reset.
 * - POL = 0x8005, POLYSIZE = 16 bits, INIT = 0xFFFF.
 * - REV_IN = by byte, REV_OUT = bit reversal, RESET bit written.
 * - No GPDMA channel initialized.
 */
void Ut_Crc_Init_Crc16_ConfiguresRegisters( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();

    Gpdma_Set_ChannelInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Ut_Crc_Expect_RccInit();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( UT_CRC_POLY16,                  CRC->POL );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_POLYLENGTH_16B,          CRC->CR & CRC_CR_POLYSIZE );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT16,                  CRC->INIT );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_INDATA_REVERSE_BYTE,     CRC->CR & CRC_CR_REV_IN );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_OUTDATA_REVERSE_BIT,     CRC->CR & CRC_CR_REV_OUT );
    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET,                   CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_DmaInitCnt );
}


/**
 * \brief   Crc_Init() rejects invalid configuration without any access.
 *
 * \details Calls Crc_Init() with NULL, invalid peripheral, even polynomial,
 *          polynomial wider than its size, invalid polynomial size, output data
 *          order not supported by HW and invalid DMA channel. No RCC / GPDMA
 *          call is expected (strict mocks).
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR for every configuration, registers untouched.
 */
void Ut_Crc_Init_InvalidConfig_ReturnsErrorWithoutAccess( void )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( NULL ) );

    config          = Ut_Crc_Get_Config();
    config.PeriphId = CRC_PERIPH_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config            = Ut_Crc_Get_Config();
    config.Polynomial = 0x8004u;                    /* Even polynomial */
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config            = Ut_Crc_Get_Config();
    config.Polynomial = 0x18005u;                   /* Wider than 16 bits */
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config          = Ut_Crc_Get_Config();
    config.PolySize = CRC_DATA_SIZE_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config                 = Ut_Crc_Get_Config();
    config.OutputDataOrder = CRC_DATA_ORDER_REV_BYTE;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config                = Ut_Crc_Get_Config();
    config.InputDataOrder = CRC_DATA_ORDER_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config               = Ut_Crc_Get_Config();
    config.InputDataSize = CRC_INPUT_SIZE_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config              = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    config.DmaChannelId = CRC_DMA_CHANNEL_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->POL );
    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->CR );
}


/**
 * \brief   Crc_Init() fails when RCC peripheral clock can not be enabled.
 *
 * \details RCC mock returns error for clock activation.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, registers untouched, module stays uninitialized.
 */
void Ut_Crc_Init_RccError_ReturnsError( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();
    crc_Data_t   value  = 0u;

    Gpdma_Set_ChannelInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->POL );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
}


/**
 * \brief   Services return error before Crc_Init().
 *
 * \details Calls calculation, configuration and transfer services without
 *          initialization (module context released by setUp).
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR for every service, registers untouched.
 */
void Ut_Crc_Services_NotInitialized_ReturnError( void )
{
    const uint8_t       data[ 2u ] = { 0x01u, 0x02u };
    crc_Data_t          value      = 0u;
    crc_DataSize_t      polySize   = CRC_DATA_SIZE_CNT;
    crc_DataOrder_t     order      = CRC_DATA_ORDER_CNT;
    crc_InputSize_t     inputSize  = CRC_INPUT_SIZE_CNT;
    crc_IndepData_t     indepData  = 0u;
    crc_TransferState_t xferState  = CRC_TRANSFER_BUSY;
    crc_FunctionState_t funcState  = CRC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_IndependentData( UT_CRC_ID, 0x12u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_IndependentData( UT_CRC_ID, &indepData ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( UT_CRC_ID, data, sizeof( data ), &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, UT_CRC_POLY16, CRC_DATA_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Polynomial( UT_CRC_ID, &value, &polySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InitValue( UT_CRC_ID, UT_CRC_INIT16 ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_BYTE ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_PeriphActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_PeriphInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStop( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferCpltActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferCpltState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_HalfTransferActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_HalfTransferState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferErrorActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->CR );
}

/* ============================== DEINIT ==================================== */

/**
 * \brief   Crc_Deinit() resets the peripheral and disables its clock.
 *
 * \details Initializes the module, then deinitializes it.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, RCC reset pulse and clock disable requested.
 * - Services return error after deinitialization.
 */
void Ut_Crc_Deinit_ResetsPeripheralAndDisablesClock( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();
    crc_Data_t   value  = 0u;

    Ut_Crc_Init( &config );

    Gpdma_Set_ChannelInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Rcc_Set_ResetActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_ResetInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_PeriphInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Deinit() );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
}

/* ========================= BLOCKING CALCULATION =========================== */

/**
 * \brief   Crc_Set_Data() feeds data byte by byte.
 *
 * \details Feeds 3 bytes. Emulated data register keeps the last written byte
 *          (8-bit access into the lowest byte).
 *
 * \par Expected results
 * - CRC_REQUEST_OK, DR lowest byte equals the last data byte.
 * - NULL data: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Set_Data_FeedsBytes( void )
{
    const uint8_t data[ 3u ] = { 0x11u, 0x22u, 0x33u };
    crc_Config_t  config     = Ut_Crc_Get_Config();

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX8( 0x33u, (uint8_t)( CRC->DR & 0xFFu ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, NULL, 1u ) );
}


/**
 * \brief   Crc_Get_Data() masks the result by polynomial size.
 *
 * \details Presets DR = 0x12345678 with 16-bit and 7-bit polynomial.
 *
 * \par Expected results
 * - 16 bits: 0x5678, 7 bits: 0x78.
 * - NULL pointer: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Get_Data_MasksByPolySize( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();
    crc_Data_t   value  = 0u;

    Ut_Crc_Init( &config );

    CRC->DR = 0x12345678u;
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( 0x5678u, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Polynomial( UT_CRC_ID, 0x09u, CRC_DATA_SIZE_7BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( 0x78u, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Crc_Calc_Block() resets the unit, feeds data and reads the result.
 *
 * \details Clears CR.RESET after initialization, calculates CRC of 2 bytes.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, CR.RESET written, result is last byte (emulated DR) masked
 *   by 16 bits.
 * - NULL data or result pointer: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Calc_Block_ResetsFeedsAndReads( void )
{
    const uint8_t data[ 2u ] = { 0xA5u, 0x5Au };
    crc_Config_t  config     = Ut_Crc_Get_Config();
    crc_Data_t    value      = 0u;

    Ut_Crc_Init( &config );

    CRC->CR &= ~CRC_CR_RESET;   /* Cleared by HW after reset */
    CRC->DR  = 0xFFFF0000u;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( UT_CRC_ID, data, sizeof( data ), &value ) );

    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET, CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_HEX32( 0x005Au, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( UT_CRC_ID, NULL, sizeof( data ), &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( UT_CRC_ID, data, sizeof( data ), NULL ) );
}

/* ============================ CONFIGURATION =============================== */

/**
 * \brief   Polynomial is configured and read back.
 *
 * \details Sets CRC-8 polynomial 0x07, reads it back, then tries invalid values.
 *
 * \par Expected results
 * - POL = 0x07, POLYSIZE = 8 bits, getter returns the same values.
 * - Even polynomial, polynomial wider than size, invalid size: error, registers kept.
 */
void Ut_Crc_Set_Polynomial_ConfiguresAndReadsBack( void )
{
    crc_Config_t   config     = Ut_Crc_Get_Config();
    crc_Data_t     polynomial = 0u;
    crc_DataSize_t polySize   = CRC_DATA_SIZE_CNT;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Polynomial( UT_CRC_ID, 0x07u, CRC_DATA_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL_HEX32( 0x07u, CRC->POL );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_POLYLENGTH_8B, CRC->CR & CRC_CR_POLYSIZE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Polynomial( UT_CRC_ID, &polynomial, &polySize ) );
    TEST_ASSERT_EQUAL_HEX32( 0x07u, polynomial );
    TEST_ASSERT_EQUAL( CRC_DATA_SIZE_8BITS, polySize );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, 0x06u,  CRC_DATA_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, 0x107u, CRC_DATA_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, 0x07u,  CRC_DATA_SIZE_CNT ) );
    TEST_ASSERT_EQUAL_HEX32( 0x07u, CRC->POL );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Polynomial( UT_CRC_ID, NULL, &polySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Polynomial( UT_CRC_ID, &polynomial, NULL ) );
}


/**
 * \brief   Initial value is configured and read back.
 *
 * \details Sets initial value 0x1D0F.
 *
 * \par Expected results
 * - INIT = 0x1D0F, getter returns the same value, NULL pointer: error.
 */
void Ut_Crc_Set_InitValue_ConfiguresAndReadsBack( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();
    crc_Data_t   value  = 0u;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( UT_CRC_ID, 0x1D0Fu ) );
    TEST_ASSERT_EQUAL_HEX32( 0x1D0Fu, CRC->INIT );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( 0x1D0Fu, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InitValue( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Input data order is configured and read back for every option.
 *
 * \details Sets every input data order option.
 *
 * \par Expected results
 * - REV_IN field matches LL value, getter returns the same option.
 * - Invalid option: error.
 */
void Ut_Crc_Set_InputDataOrder_AllOptions( void )
{
    const uint32_t regValues[ CRC_DATA_ORDER_CNT ] =
    {
        LL_CRC_INDATA_REVERSE_NONE, LL_CRC_INDATA_REVERSE_BYTE,
        LL_CRC_INDATA_REVERSE_HALFWORD, LL_CRC_INDATA_REVERSE_WORD
    };
    crc_Config_t    config = Ut_Crc_Get_Config();
    crc_DataOrder_t order  = CRC_DATA_ORDER_CNT;

    Ut_Crc_Init( &config );

    for( crc_DataOrder_t orderId = CRC_DATA_ORDER_NORMAL; CRC_DATA_ORDER_CNT > orderId; orderId ++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, orderId ) );
        TEST_ASSERT_EQUAL_HEX32( regValues[ orderId ], CRC->CR & CRC_CR_REV_IN );

        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataOrder( UT_CRC_ID, &order ) );
        TEST_ASSERT_EQUAL( orderId, order );
    }

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataOrder( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Output data order supports only normal and bit-reversed result.
 *
 * \details Sets every output data order option.
 *
 * \par Expected results
 * - NORMAL / REV_WORD: OK, REV_OUT matches, getter returns the same option.
 * - REV_BYTE / REV_HALFWORD / invalid: error, register kept.
 */
void Ut_Crc_Set_OutputDataOrder_NormalAndReversedOnly( void )
{
    crc_Config_t    config = Ut_Crc_Get_Config();
    crc_DataOrder_t order  = CRC_DATA_ORDER_CNT;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_NORMAL ) );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_OUTDATA_REVERSE_NONE, CRC->CR & CRC_CR_REV_OUT );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_NORMAL, order );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_OUTDATA_REVERSE_BIT, CRC->CR & CRC_CR_REV_OUT );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_WORD, order );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_BYTE ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_HALFWORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_CNT ) );
    TEST_ASSERT_EQUAL_HEX32( LL_CRC_OUTDATA_REVERSE_BIT, CRC->CR & CRC_CR_REV_OUT );
}

/**
 * \brief   Input data are fed as items of the configured size.
 *
 * \details Feeds 8 bytes with 32-bit and 16-bit input data size, then requests
 *          incomplete item and invalid size. Emulated data register keeps the
 *          last written item (little endian assembly from bytes).
 *
 * \par Expected results
 * - 32 bits: DR = 0x88776655 (last word), getter returns 32 bits.
 * - 16 bits: DR lower half-word = 0x8877 (last half-word).
 * - 3 bytes with 16-bit items: error, nothing is fed.
 * - Invalid size: error, size kept.
 */
void Ut_Crc_Set_InputDataSize_FeedsItemsOfConfiguredSize( void )
{
    const uint8_t   data[ 8u ] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
    crc_Config_t    config     = Ut_Crc_Get_Config();
    crc_InputSize_t inputSize  = CRC_INPUT_SIZE_CNT;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_32BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
    TEST_ASSERT_EQUAL( CRC_INPUT_SIZE_32BITS, inputSize );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0x88776655u, CRC->DR );

    CRC->DR = 0u;
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0x8877u, CRC->DR & 0xFFFFu );

    CRC->DR = 0u;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, 3u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
    TEST_ASSERT_EQUAL( CRC_INPUT_SIZE_16BITS, inputSize );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataSize( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Independent data register is written and read back.
 *
 * \details Writes 0xA5A55A5A, reads it back.
 *
 * \par Expected results
 * - IDR = 0xA5A55A5A, getter returns the same value, NULL pointer: error.
 */
void Ut_Crc_Set_IndependentData_WritesAndReadsBack( void )
{
    crc_Config_t    config    = Ut_Crc_Get_Config();
    crc_IndepData_t indepData = 0u;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_IndependentData( UT_CRC_ID, 0xA5A55A5Au ) );
    TEST_ASSERT_EQUAL_HEX32( 0xA5A55A5Au, CRC->IDR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_IndependentData( UT_CRC_ID, &indepData ) );
    TEST_ASSERT_EQUAL_HEX32( 0xA5A55A5Au, indepData );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_IndependentData( UT_CRC_ID, NULL ) );
}

/* ========================== PERIPHERAL CONTROL ============================ */

/**
 * \brief   Inactive peripheral (clock disabled) blocks register access.
 *
 * \details Disables the peripheral, accesses it, enables it again.
 *
 * \par Expected results
 * - Set_PeriphInactive / Set_PeriphActive: OK, RCC clock disabled / enabled.
 * - Calculation and configuration services return error while inactive.
 * - Services work again after activation.
 */
void Ut_Crc_Set_PeriphInactive_BlocksAccessUntilActive( void )
{
    const uint8_t data   = 0x55u;
    crc_Config_t  config = Ut_Crc_Get_Config();
    crc_Data_t    value  = 0u;

    Ut_Crc_Init( &config );

    Rcc_Set_PeriphInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphInactive( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, &data, 1u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InitValue( UT_CRC_ID, 0u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InitValue( UT_CRC_ID, &value ) );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphActive( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT16, value );
}

/* ======================= NON-BLOCKING CALCULATION ========================= */

/**
 * \brief   Crc_Init() with DMA initializes GPDMA memory to memory channel.
 *
 * \details Initializes channel 1 first (channel ownership of previous tests),
 *          then initializes channel 3 and captures the GPDMA configuration.
 *
 * \par Expected results
 * - Gpdma_Init() called once with GPDMA1 channel 3, high priority.
 * - Memory to memory, 8-bit source (increment) -> 8-bit CRC DR (static).
 * - Transfer complete, half transfer and error handlers registered.
 * - GPDMA channel interrupt enabled.
 */
void Ut_Crc_Init_Dma_InitializesGpdmaChannel( void )
{
    crc_Config_t config = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_1 );

    Ut_Crc_Init( &config );

    config             = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_3 );
    config.DmaPriority = CRC_DMA_PRIORITY_HIGH;

    Gpdma_Set_ChannelInactive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_1, GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptInactive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_1, GPDMA_REQUEST_OK );
    Ut_Crc_Expect_RccInit();
    Gpdma_Get_DefaultConfig_ExpectAnyArgsAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Init_StubWithCallback( Ut_Crc_GpdmaInitStub );
    Gpdma_Set_InterruptActive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_3, GPDMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_DmaInitCnt );
    TEST_ASSERT_EQUAL( GPDMA_PERIPH_1,       utCrc_DmaConfig.PeriphId );
    TEST_ASSERT_EQUAL( GPDMA_CHANNEL_3,      utCrc_DmaConfig.ChannelId );
    TEST_ASSERT_EQUAL( GPDMA_PRIORITY_HIGH,  utCrc_DmaConfig.ChannelPrio );
    TEST_ASSERT_EQUAL_UINT32( 1u,            utCrc_DmaConfig.TransfersCount );
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.TransferCompleteIsr );
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.HalfTransferIsr );
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.ErrorIsr );

    TEST_ASSERT_EQUAL( GPDMA_DIR_MEMORY_TO_MEMORY, utCrc_DmaXferConfig.Direction );
    TEST_ASSERT_EQUAL( GPDMA_DATA_SIZE_8BITS,      utCrc_DmaXferConfig.SourceDataSize );
    TEST_ASSERT_EQUAL( GPDMA_ADDR_INCREMENT,       utCrc_DmaXferConfig.SourceAddrMode );
    TEST_ASSERT_EQUAL( GPDMA_DATA_SIZE_8BITS,      utCrc_DmaXferConfig.DestinationDataSize );
    TEST_ASSERT_EQUAL( GPDMA_ADDR_STATIC,          utCrc_DmaXferConfig.DestinationAddrMode );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)(uintptr_t)&CRC->DR, utCrc_DmaXferConfig.DestinationAddr );
}


/**
 * \brief   Repeated Crc_Init() with the same GPDMA channel reuses it.
 *
 * \details Initializes channel 4 twice.
 *
 * \par Expected results
 * - Second initialization: Gpdma_Init() not called, only priority updated and
 *   channel interrupt enabled.
 */
void Ut_Crc_Init_DmaSameChannel_ReusesChannel( void )
{
    crc_Config_t config = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_4 );

    Ut_Crc_Init( &config );

    Gpdma_Set_ChannelInactive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_4, GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptInactive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_4, GPDMA_REQUEST_OK );
    Ut_Crc_Expect_RccInit();
    Gpdma_Get_DefaultConfig_ExpectAnyArgsAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_Priority_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_4, GPDMA_PRIORITY_LOW, GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptActive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_4, GPDMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_DmaInitCnt );
}


/**
 * \brief   Crc_Set_TransferStart() arms GPDMA channel and signals busy state.
 *
 * \details Starts non-blocking calculation of 64 bytes from emulated SRAM.
 *
 * \par Expected results
 * - Block size 64, source address of the buffer, channel enabled.
 * - Transfer state CRC_TRANSFER_BUSY.
 * - Configuration, data feed / read and second start return error while busy.
 */
void Ut_Crc_Set_TransferStart_ArmsChannelAndBlocksAccess( void )
{
    const uint8_t * const data      = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t   xferState = CRC_TRANSFER_IDLE;
    crc_Data_t            value     = 0u;

    Ut_Crc_Init( &config );

    Gpdma_Set_SourceDataSize_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_DATA_SIZE_8BITS, GPDMA_REQUEST_OK );
    Gpdma_Set_DestinationDataSize_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_DATA_SIZE_8BITS, GPDMA_REQUEST_OK );
    Gpdma_Set_BlockSize_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, UT_CRC_XFER_CNT, GPDMA_REQUEST_OK );
    Gpdma_Set_SourceAddr_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, (gpdma_SrcAddr_t)(uintptr_t)data, GPDMA_REQUEST_OK );
    Gpdma_Set_ChannelActive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_BUSY, xferState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, 1u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InitValue( UT_CRC_ID, 0u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_PeriphInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );
}


/**
 * \brief   Non-blocking calculation with 32-bit items configures GPDMA data size
 *          and requires aligned data.
 *
 * \details Sets 32-bit input data size, starts with aligned buffer, then with
 *          misaligned buffer and incomplete item.
 *
 * \par Expected results
 * - Source / destination data size 32 bits, block size in bytes.
 * - Misaligned data or count not multiple of 4: error, no GPDMA call.
 */
void Ut_Crc_Set_TransferStart_32BitItems_ConfiguresDataSize( void )
{
    const uint8_t * const data   = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_32BITS ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, &data[ 1u ], UT_CRC_XFER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, 6u ) );

    Gpdma_Set_SourceDataSize_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_DATA_SIZE_32BITS, GPDMA_REQUEST_OK );
    Gpdma_Set_DestinationDataSize_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_DATA_SIZE_32BITS, GPDMA_REQUEST_OK );
    Gpdma_Set_BlockSize_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, UT_CRC_XFER_CNT, GPDMA_REQUEST_OK );
    Gpdma_Set_SourceAddr_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, (gpdma_SrcAddr_t)(uintptr_t)data, GPDMA_REQUEST_OK );
    Gpdma_Set_ChannelActive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );
}


/**
 * \brief   Crc_Set_TransferStart() rejects invalid requests without GPDMA access.
 *
 * \details Starts with NULL data, zero count, count above GPDMA block size limit,
 *          and without DMA configured.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, no GPDMA call (strict mocks), state CRC_TRANSFER_IDLE.
 */
void Ut_Crc_Set_TransferStart_InvalidRequest_ReturnsError( void )
{
    const uint8_t * const data      = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t   xferState = CRC_TRANSFER_BUSY;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, NULL, UT_CRC_XFER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, 0u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, CRC_TRANSFER_DATA_CNT_MAX + 1u ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );

    config = Ut_Crc_Get_Config();   /* DMA inactive */
    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStop( UT_CRC_ID ) );
}


/**
 * \brief   GPDMA transfer complete ends the transfer and calls user callback.
 *
 * \details Starts the transfer, calls captured GPDMA transfer complete handler.
 *          Repeats with the callback disabled.
 *
 * \par Expected results
 * - State CRC_TRANSFER_IDLE, callback called once, result can be read.
 * - Callback disabled: state IDLE, callback not called again.
 */
void Ut_Crc_DmaTransferCplt_EndsTransferAndCallsCallback( void )
{
    crc_Config_t        config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_5 );
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;
    crc_FunctionState_t funcState = CRC_FUNCTION_INACTIVE;
    crc_Data_t          value     = 0u;

    config.TransferCpltIsr = Ut_Crc_TransferCpltCallback;

    Ut_Crc_Ignore_Mocks();
    Gpdma_Init_StubWithCallback( Ut_Crc_GpdmaInitStub );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
    Ut_Crc_Reset_Mocks();

    /* Channel 5 is used only by this test - GPDMA channel is initialized and handlers captured */
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.TransferCompleteIsr );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferCpltState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    Ut_Crc_Start_Transfer();
    utCrc_DmaConfig.TransferCompleteIsr();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferCpltInactive( UT_CRC_ID ) );
    Ut_Crc_Start_Transfer();
    utCrc_DmaConfig.TransferCompleteIsr();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_TransferCpltCnt );
}


/**
 * \brief   GPDMA half transfer calls user callback only when active.
 *
 * \details Initializes without half transfer callback, sets it and activates it
 *          by API, calls captured GPDMA half transfer handler.
 *
 * \par Expected results
 * - Callback not configured: state inactive, handler does nothing.
 * - Callback set and activated: getters return it, handler calls it once,
 *   transfer stays busy.
 */
void Ut_Crc_DmaHalfTransfer_CallsActiveCallback( void )
{
    crc_Config_t            config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_6 );
    crc_TransferState_t     xferState = CRC_TRANSFER_IDLE;
    crc_FunctionState_t     funcState = CRC_FUNCTION_ACTIVE;
    crc_CalcCompleteIsr_t * callback  = NULL;

    Ut_Crc_Ignore_Mocks();
    Gpdma_Init_StubWithCallback( Ut_Crc_GpdmaInitStub );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.HalfTransferIsr );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_HalfTransferState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    Ut_Crc_Start_Transfer();
    utCrc_DmaConfig.HalfTransferIsr();
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_HalfTransferCnt );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_HalfTransferIsr( UT_CRC_ID, Ut_Crc_HalfTransferCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_HalfTransferActive( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_HalfTransferIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Crc_HalfTransferCallback, callback );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_HalfTransferState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    utCrc_DmaConfig.HalfTransferIsr();
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_HalfTransferCnt );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_BUSY, xferState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_HalfTransferIsr( UT_CRC_ID, NULL ) );
}


/**
 * \brief   GPDMA error stops the transfer, error state is cleared by reset.
 *
 * \details Initializes with transfer complete and transfer error callbacks,
 *          starts the transfer, calls captured GPDMA error handler with transfer
 *          error and trigger overrun bits.
 *
 * \par Expected results
 * - GPDMA channel disabled, state CRC_TRANSFER_ERROR, no transfer complete
 *   callback, result read and data feed return error.
 * - Transfer error callback active after init, called twice: DMA transfer error,
 *   then DMA trigger overrun.
 * - Crc_Set_Reset() clears the state to CRC_TRANSFER_IDLE.
 */
void Ut_Crc_DmaError_StopsTransferAndCallsErrorCallback( void )
{
    crc_Config_t        config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_7 );
    crc_TransferState_t xferState = CRC_TRANSFER_IDLE;
    crc_FunctionState_t funcState = CRC_FUNCTION_INACTIVE;
    crc_Data_t          value     = 0u;

    config.TransferCpltIsr  = Ut_Crc_TransferCpltCallback;
    config.TransferErrorIsr = Ut_Crc_TransferErrorCallback;

    Ut_Crc_Ignore_Mocks();
    Gpdma_Init_StubWithCallback( Ut_Crc_GpdmaInitStub );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.ErrorIsr );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    Ut_Crc_Start_Transfer();

    Gpdma_Set_ChannelInactive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_7, GPDMA_REQUEST_OK );
    utCrc_DmaConfig.ErrorIsr( (gpdma_ErrorMaskId_t)( GPDMA_ERROR_TRANSFER | GPDMA_ERROR_TRIG_OVERRUN ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_ERROR, xferState );
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );

    TEST_ASSERT_EQUAL_UINT32( 2u, utCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_TRANSFER,        utCrc_ErrorIds[ 0u ] );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_TRIGGER_OVERRUN, utCrc_ErrorIds[ 1u ] );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
}


/**
 * \brief   Transfer error callback is configured at runtime and gated by its state.
 *
 * \details Initializes without error callback, sets it by API, reads it back,
 *          reports GPDMA error with callback inactive, then active.
 *
 * \par Expected results
 * - Callback not configured: state inactive.
 * - Getter returns the configured callback.
 * - Inactive callback: error state set, callback not called.
 * - Active callback: called once with DMA configuration error.
 * - NULL output pointers: error.
 */
void Ut_Crc_Set_TransferErrorIsr_CallbackGatedByState( void )
{
    crc_Config_t             config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_2 );
    crc_FunctionState_t      funcState = CRC_FUNCTION_ACTIVE;
    crc_TransferState_t      xferState = CRC_TRANSFER_IDLE;
    crc_TransferErrorIsr_t * callback  = NULL;

    Ut_Crc_Ignore_Mocks();
    Gpdma_Init_StubWithCallback( Ut_Crc_GpdmaInitStub );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.ErrorIsr );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferErrorIsr( UT_CRC_ID, Ut_Crc_TransferErrorCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Crc_TransferErrorCallback, callback );

    /* Callback configured, but not active */
    Ut_Crc_Start_Transfer();
    Gpdma_Set_ChannelInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    utCrc_DmaConfig.ErrorIsr( GPDMA_ERROR_CONFIG_ERROR );
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_ERROR, xferState );
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_ErrorCnt );

    /* Callback active */
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferErrorActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( UT_CRC_ID ) );
    Ut_Crc_Start_Transfer();
    Gpdma_Set_ChannelInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    utCrc_DmaConfig.ErrorIsr( GPDMA_ERROR_CONFIG_ERROR );
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_CONFIG, utCrc_ErrorIds[ 0u ] );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferErrorInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorState( UT_CRC_ID, NULL ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorIsr( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Crc_Set_TransferStop() disables GPDMA channel and ends busy state.
 *
 * \details Starts the transfer and stops it.
 *
 * \par Expected results
 * - GPDMA channel disabled, state CRC_TRANSFER_IDLE.
 */
void Ut_Crc_Set_TransferStop_DisablesChannel( void )
{
    crc_Config_t        config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;

    Ut_Crc_Init( &config );
    Ut_Crc_Start_Transfer();

    Gpdma_Set_ChannelInactive_ExpectAndReturn( GPDMA_PERIPH_1, GPDMA_CHANNEL_0, GPDMA_REQUEST_OK );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStop( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
}


/**
 * \brief   Transfer complete callback is configured and read back.
 *
 * \details Initializes without callback, sets it, reads it back, removes it.
 *
 * \par Expected results
 * - Getter returns configured callback, NULL after removal.
 * - NULL output pointer: error.
 */
void Ut_Crc_Set_TransferCpltIsr_ConfiguresAndReadsBack( void )
{
    crc_Config_t            config   = Ut_Crc_Get_Config();
    crc_CalcCompleteIsr_t * callback = NULL;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferCpltIsr( UT_CRC_ID, Ut_Crc_TransferCpltCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferCpltIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Crc_TransferCpltCallback, callback );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferCpltIsr( UT_CRC_ID, NULL ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferCpltIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_NULL( callback );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferCpltIsr( UT_CRC_ID, NULL ) );
}

/* ========================== HELPER FUNCTIONS ============================== */

/**
 * \brief Gpdma_Init() stub - captures configuration (handlers, transfer).
 */
static gpdma_RequestState_t Ut_Crc_GpdmaInitStub( gpdma_ConfigStruct_t * const configStruct, int callCnt )
{
    (void)callCnt;

    utCrc_DmaConfig = *configStruct;

    if( NULL != configStruct->TransferConfig )
    {
        utCrc_DmaXferConfig = *configStruct->TransferConfig;
    }
    else
    {
        /* No transfer configuration */
    }

    utCrc_DmaInitCnt ++;

    return ( GPDMA_REQUEST_OK );
}


/**
 * \brief Transfer complete user callback - counts calls.
 */
static void Ut_Crc_TransferCpltCallback( void )
{
    utCrc_TransferCpltCnt ++;
}


/**
 * \brief Half transfer user callback - counts calls.
 */
static void Ut_Crc_HalfTransferCallback( void )
{
    utCrc_HalfTransferCnt ++;
}


/**
 * \brief Transfer error user callback - counts calls and stores reported errors.
 */
static void Ut_Crc_TransferErrorCallback( crc_XferErrorId_t errorId )
{
    if( CRC_XFER_ERROR_CNT > utCrc_ErrorCnt )
    {
        utCrc_ErrorIds[ utCrc_ErrorCnt ] = errorId;
    }
    else
    {
        /* More errors than storage */
    }

    utCrc_ErrorCnt ++;
}


/**
 * \brief Ignores all RCC / GPDMA calls used by initialization and deinitialization.
 */
static void Ut_Crc_Ignore_Mocks( void )
{
    Rcc_Set_PeriphActive_IgnoreAndReturn( RCC_REQUEST_OK );
    Rcc_Set_PeriphInactive_IgnoreAndReturn( RCC_REQUEST_OK );
    Rcc_Set_ResetActive_IgnoreAndReturn( RCC_REQUEST_OK );
    Rcc_Set_ResetInactive_IgnoreAndReturn( RCC_REQUEST_OK );
    Gpdma_Get_DefaultConfig_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Init_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_Priority_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptActive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_ChannelInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_InterruptInactive_IgnoreAndReturn( GPDMA_REQUEST_OK );
}


/**
 * \brief Re-initializes the mocks (clears ignore / expectation state).
 */
static void Ut_Crc_Reset_Mocks( void )
{
    MockRcc_Port_Destroy();
    MockGpdma_Port_Destroy();
    MockRcc_Port_Init();
    MockGpdma_Port_Init();
}


/**
 * \brief Expects RCC clock activation and peripheral reset of CRC.
 */
static void Ut_Crc_Expect_RccInit( void )
{
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_ResetActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_ResetInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
}


/**
 * \brief Returns CRC-16/MODBUS configuration without DMA.
 */
static crc_Config_t Ut_Crc_Get_Config( void )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );

    config.PeriphId        = UT_CRC_ID;
    config.Polynomial      = UT_CRC_POLY16;
    config.PolySize        = CRC_DATA_SIZE_16BITS;
    config.InitValue       = UT_CRC_INIT16;
    config.InputDataOrder  = CRC_DATA_ORDER_REV_BYTE;
    config.OutputDataOrder = CRC_DATA_ORDER_REV_WORD;

    return ( config );
}


/**
 * \brief Returns CRC-16/MODBUS configuration with DMA (GPDMA1, low priority).
 */
static crc_Config_t Ut_Crc_Get_DmaConfig( crc_DmaChannelId_t channelId )
{
    crc_Config_t config = Ut_Crc_Get_Config();

    config.DmaState     = CRC_FUNCTION_ACTIVE;
    config.DmaPeriphId  = CRC_DMA_PERIPH_1;
    config.DmaChannelId = channelId;
    config.DmaPriority  = CRC_DMA_PRIORITY_LOW;

    return ( config );
}


/**
 * \brief Initializes the module with ignored RCC / GPDMA calls and re-initializes the mocks.
 */
static void Ut_Crc_Init( crc_Config_t * const config )
{
    Ut_Crc_Ignore_Mocks();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( config ) );

    Ut_Crc_Reset_Mocks();
}


/**
 * \brief Starts non-blocking calculation of UT_CRC_XFER_CNT bytes with ignored GPDMA calls.
 */
static void Ut_Crc_Start_Transfer( void )
{
    const uint8_t * const data = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );

    Gpdma_Set_SourceDataSize_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_DestinationDataSize_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_BlockSize_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_SourceAddr_IgnoreAndReturn( GPDMA_REQUEST_OK );
    Gpdma_Set_ChannelActive_IgnoreAndReturn( GPDMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    Ut_Crc_Reset_Mocks();
}
