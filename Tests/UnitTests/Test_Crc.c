/**
 * \author Mr.Nobody
 * \file Test_Crc.c
 * \ingroup Crc
 * \brief Unit tests of CRC calculation unit (CRC) module.
 *
 * Crc.c is compiled unchanged with real LL drivers. CRC registers are emulated
 * by RegMem, RCC and DMA modules are mocked by CMock.
 *
 * \note Emulated data register is plain RAM - it keeps the last written word
 *       instead of calculated CRC, and the reset bit (CR.RESET) is not cleared
 *       by HW. Tests therefore verify the words written to the data register
 *       (grouped items, bit reversal, pre-load word of the initial value).
 *       Calculated values are verified by integration tests on target.
 *
 * \note Expected pre-load words W = f^-1( value ) ^ 0xFFFFFFFF and SW calculated
 *       items were computed by an independent reference model.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Crc_Port.h"                       /* Module under test              */
#include "MockRcc_Port.h"                   /* RCC module mock                */
#include "MockDma_Port.h"                   /* DMA module mock                */
#include "Stm32_crc.h"                      /* CRC registers definition       */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static dma_RequestState_t   Ut_Crc_DmaInitStub              ( dma_ConfigStruct_t * const dmaConfig, int callCnt );
static dma_RequestState_t   Ut_Crc_DmaStartErrorStub        ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, int callCnt );
static void                 Ut_Crc_TransferCpltCallback     ( void );
static void                 Ut_Crc_HalfTransferCallback     ( void );
static void                 Ut_Crc_TransferErrorCallback    ( crc_XferErrorId_t errorId );
static void                 Ut_Crc_Ignore_Mocks             ( void );
static void                 Ut_Crc_Reset_Mocks              ( void );
static void                 Ut_Crc_Expect_RccInit           ( void );
static void                 Ut_Crc_Expect_DmaOff            ( dma_ChannelId_t channelId );
static crc_Config_t         Ut_Crc_Get_Config               ( void );
static crc_Config_t         Ut_Crc_Get_DmaConfig            ( crc_DmaChannelId_t channelId );
static void                 Ut_Crc_Init                     ( crc_Config_t * const config );
static void                 Ut_Crc_Init_Dma                 ( crc_Config_t * const config );
static void                 Ut_Crc_Start_Transfer           ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Peripheral under test */
#define UT_CRC_ID                           ( CRC_PERIPH_1 )

/** CRC-32 polynomial (fixed by HW) */
#define UT_CRC_POLY32                       ( 0x04C11DB7u )

/** Initial value used by tests (pre-load word needed) */
#define UT_CRC_INIT                         ( 0x12345678u )

/** Pre-load word of \ref UT_CRC_INIT ( f^-1( 0x12345678 ) ^ 0xFFFFFFFF ) */
#define UT_CRC_INIT_PRELOAD                 ( 0x7A3CB0E1u )

/** Value of data register after reset of the calculation unit */
#define UT_CRC_RESET_VALUE                  ( 0xFFFFFFFFu )

/** Marker written to the data register to detect writes */
#define UT_CRC_DR_MARKER                    ( 0xDEADBEEFu )

/** DMA stream used by DMA tests */
#define UT_CRC_DMA_CHANNEL                  ( CRC_DMA_CHANNEL_0 )

/** Count of data bytes of non-blocking calculation */
#define UT_CRC_XFER_CNT                     ( 64u )

/** Offset of DMA data buffer in emulated SRAM (32-bit address for DMA) */
#define UT_CRC_XFER_SRAM_OFFSET             ( 0x100u )

/* ========================== LOCAL VARIABLES =============================== */

/** DMA configuration captured by Dma_Init() stub */
static dma_ConfigStruct_t       utCrc_DmaConfig;

/** Count of Dma_Init() calls */
static uint32_t                 utCrc_DmaInitCnt;

/** Count of transfer complete callback calls */
static uint32_t                 utCrc_TransferCpltCnt;

/** Count of half transfer callback calls */
static uint32_t                 utCrc_HalfTransferCnt;

/** Count of transfer error callback calls */
static uint32_t                 utCrc_ErrorCnt;

/** Last error reported by transfer error callback */
static crc_XferErrorId_t        utCrc_ErrorId;

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
    utCrc_ErrorId         = CRC_XFER_ERROR_CNT;
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
 * - Version is 1.0.0.
 */
void Ut_Crc_Get_ModuleVersion_ReturnsVersion( void )
{
    crc_ModuleVersion_t version = Crc_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Minor );
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
 *   by byte, output reversed, 8-bit input data, DMA inactive (DMA2 stream 0),
 *   no callbacks.
 * - NULL pointer: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Get_DefaultConfig_ReturnsDefaults( void )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( CRC_PERIPH_1, config.PeriphId );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_POLY32, config.Polynomial );
    TEST_ASSERT_EQUAL( CRC_DATA_SIZE_32BITS, config.PolySize );
    TEST_ASSERT_EQUAL_HEX32( 0xFFFFFFFFu, config.InitValue );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_BYTE, config.InputDataOrder );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_WORD, config.OutputDataOrder );
    TEST_ASSERT_EQUAL( CRC_INPUT_SIZE_8BITS, config.InputDataSize );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, config.DmaState );
    TEST_ASSERT_EQUAL( CRC_DMA_PERIPH_2, config.DmaPeriphId );
    TEST_ASSERT_EQUAL( CRC_DMA_CHANNEL_0, config.DmaChannelId );
    TEST_ASSERT_EQUAL( CRC_DMA_PRIORITY_LOW, config.DmaPriority );
    TEST_ASSERT_NULL( config.TransferCpltIsr );
    TEST_ASSERT_NULL( config.HalfTransferIsr );
    TEST_ASSERT_NULL( config.TransferErrorIsr );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_DefaultConfig( NULL ) );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Crc_Init() with default initial value only resets the calculation unit.
 *
 * \details Initializes default configuration (initial value 0xFFFFFFFF equals
 *          the HW reset value), data register preset with a marker.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, RCC clock enabled and peripheral reset.
 * - CR.RESET written, data register not written (no pre-load word).
 * - No DMA stream initialized.
 */
void Ut_Crc_Init_DefaultInitValue_ResetsWithoutPreload( void )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );
    CRC->DR = UT_CRC_DR_MARKER;

    Ut_Crc_Expect_RccInit();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET, CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_DR_MARKER, CRC->DR );
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_DmaInitCnt );
}


/**
 * \brief   Crc_Init() loads the initial value by pre-load word.
 *
 * \details Initializes with initial value 0x12345678.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, CR.RESET written.
 * - Data register written with pre-load word 0x7A3CB0E1.
 * - Getters return the configuration.
 */
void Ut_Crc_Init_InitValue_WritesPreloadWord( void )
{
    crc_Config_t    config      = Ut_Crc_Get_Config();
    crc_Data_t      value       = 0u;
    crc_DataOrder_t order       = CRC_DATA_ORDER_CNT;
    crc_InputSize_t inputSize   = CRC_INPUT_SIZE_CNT;

    Ut_Crc_Expect_RccInit();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET, CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT_PRELOAD, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT, value );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_NORMAL, order );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_NORMAL, order );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
    TEST_ASSERT_EQUAL( CRC_INPUT_SIZE_32BITS, inputSize );
}


/**
 * \brief   Crc_Init() rejects invalid configuration without any access.
 *
 * \details Calls Crc_Init() with NULL, invalid peripheral, polynomial / size not
 *          supported by HW, output data order not supported, invalid input data
 *          order / size / DMA state, DMA1 (no memory to memory), invalid DMA
 *          stream and priority. No RCC / DMA call is expected (strict mocks).
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
    config.Polynomial = 0x8005u;                    /* Not supported by HW */
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config          = Ut_Crc_Get_Config();
    config.PolySize = CRC_DATA_SIZE_16BITS;         /* Not supported by HW */
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config          = Ut_Crc_Get_Config();
    config.PolySize = CRC_DATA_SIZE_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config                 = Ut_Crc_Get_Config();
    config.OutputDataOrder = CRC_DATA_ORDER_REV_BYTE;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config                 = Ut_Crc_Get_Config();
    config.OutputDataOrder = CRC_DATA_ORDER_REV_HALFWORD;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config                = Ut_Crc_Get_Config();
    config.InputDataOrder = CRC_DATA_ORDER_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config               = Ut_Crc_Get_Config();
    config.InputDataSize = CRC_INPUT_SIZE_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config          = Ut_Crc_Get_Config();
    config.DmaState = (crc_FunctionState_t)2u;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config             = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    config.DmaPeriphId = (crc_DmaPeriphId_t)DMA_PERIPH_1;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config             = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    config.DmaPeriphId = CRC_DMA_PERIPH_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config              = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    config.DmaChannelId = CRC_DMA_CHANNEL_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    config             = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    config.DmaPriority = (crc_DmaPriority_t)DMA_PRIORITY_CNT;
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->DR );
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

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->CR );
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
    const uint8_t           data[ 4u ] = { 0x01u, 0x02u, 0x03u, 0x04u };
    crc_Data_t              value      = 0u;
    crc_DataSize_t          polySize   = CRC_DATA_SIZE_CNT;
    crc_DataOrder_t         order      = CRC_DATA_ORDER_CNT;
    crc_InputSize_t         inputSize  = CRC_INPUT_SIZE_CNT;
    crc_IndepData_t         indepData  = 0u;
    crc_TransferState_t     xferState  = CRC_TRANSFER_BUSY;
    crc_FunctionState_t     funcState  = CRC_FUNCTION_INACTIVE;
    crc_CalcCompleteIsr_t * callback   = NULL;
    crc_TransferErrorIsr_t *errCallback = NULL;

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_IndependentData( UT_CRC_ID, 0x12u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_IndependentData( UT_CRC_ID, &indepData ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( UT_CRC_ID, data, sizeof( data ), &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, UT_CRC_POLY32, CRC_DATA_SIZE_32BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Polynomial( UT_CRC_ID, &value, &polySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InitValue( UT_CRC_ID, UT_CRC_INIT ) );
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
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferCpltInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferCpltState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferCpltIsr( UT_CRC_ID, Ut_Crc_TransferCpltCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferCpltIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_HalfTransferActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_HalfTransferInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_HalfTransferState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_HalfTransferIsr( UT_CRC_ID, Ut_Crc_HalfTransferCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_HalfTransferIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferErrorActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferErrorInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferErrorIsr( UT_CRC_ID, Ut_Crc_TransferErrorCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorIsr( UT_CRC_ID, &errCallback ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, CRC->DR );
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

    Rcc_Set_ResetActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_ResetInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_PeriphInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Deinit() );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
}


/**
 * \brief   Crc_Deinit() releases the DMA stream and reports RCC errors.
 *
 * \details Initializes the module with DMA, deinitializes it with RCC clock
 *          disable failing.
 *
 * \par Expected results
 * - DMA stream disabled, its interrupts and NVIC line disabled.
 * - CRC_REQUEST_ERROR (clock could not be disabled), module deinitialized.
 */
void Ut_Crc_Deinit_DmaReleasedRccError_ReturnsError( void )
{
    crc_Config_t        config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_3 );
    crc_TransferState_t xferState = CRC_TRANSFER_IDLE;

    Ut_Crc_Init_Dma( &config );

    Ut_Crc_Expect_DmaOff( DMA_STREAM_3 );
    Rcc_Set_ResetActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_ResetInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    Rcc_Set_PeriphInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Deinit() );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
}

/* ========================= BLOCKING CALCULATION =========================== */

/**
 * \brief   32-bit items are written unchanged or bit-reversed by word.
 *
 * \details Feeds 8 bytes as 32-bit items without and with input reversal by word.
 *
 * \par Expected results
 * - NORMAL: DR = 0x88776655 (last word, little endian assembly).
 * - REV_WORD: DR = 0xAA66EE11 (bit-reversed last word).
 * - NULL data: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Set_Data_32BitItems_WritesWords( void )
{
    const uint8_t data[ 8u ] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
    crc_Config_t  config     = Ut_Crc_Get_Config();

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0x88776655u, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0xAA66EE11u, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, NULL, 4u ) );
}


/**
 * \brief   8-bit items are grouped by four into one word, first item in MSB.
 *
 * \details Feeds 8 bytes as 8-bit items without and with input reversal by byte.
 *
 * \par Expected results
 * - NORMAL: DR = 0x55667788 (second group).
 * - REV_BYTE: DR = 0xAA66EE11 (every byte bit-reversed).
 */
void Ut_Crc_Set_Data_8BitItems_GroupsToWords( void )
{
    const uint8_t data[ 8u ] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
    crc_Config_t  config     = Ut_Crc_Get_Config();

    config.InputDataSize = CRC_INPUT_SIZE_8BITS;
    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0x55667788u, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_BYTE ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0xAA66EE11u, CRC->DR );
}


/**
 * \brief   Remaining 8-bit item is calculated by SW and loaded to the unit.
 *
 * \details Feeds 5 bytes - one group (0x11223344) is written to the emulated
 *          data register, the last byte 0x55 is calculated by SW from the data
 *          register value and loaded by reset and pre-load word.
 *
 * \par Expected results
 * - CR.RESET written, DR = 0x49B6D3C1 (pre-load word of the SW result 0x05B142AB).
 */
void Ut_Crc_Set_Data_8BitRemainder_CalculatedBySw( void )
{
    const uint8_t data[ 5u ] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u };
    crc_Config_t  config     = Ut_Crc_Get_Config();

    config.InputDataSize = CRC_INPUT_SIZE_8BITS;
    Ut_Crc_Init( &config );

    CRC->CR &= ~CRC_CR_RESET;   /* Cleared by HW after reset */

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );

    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET, CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_HEX32( 0x49B6D3C1u, CRC->DR );
}


/**
 * \brief   16-bit items are grouped by two, remaining item calculated by SW.
 *
 * \details Feeds 8 bytes (two groups) and 6 bytes (one group 0x22114433 and
 *          remaining item 0x6655) as 16-bit items.
 *
 * \par Expected results
 * - 8 bytes: DR = 0x66558877 (second group).
 * - 6 bytes: DR = 0x4153A278 (pre-load word of the SW result).
 * - REV_HALFWORD: DR = 0xAA66EE11 (8 bytes, every half-word bit-reversed).
 */
void Ut_Crc_Set_Data_16BitItems_GroupsAndRemainder( void )
{
    const uint8_t data[ 8u ] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
    crc_Config_t  config     = Ut_Crc_Get_Config();

    config.InputDataSize = CRC_INPUT_SIZE_16BITS;
    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0x66558877u, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, 6u ) );
    TEST_ASSERT_EQUAL_HEX32( 0x4153A278u, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_HALFWORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0xAA66EE11u, CRC->DR );
}


/**
 * \brief   Data feed rejects incomplete items and bit reversal wider than the item.
 *
 * \details Feeds 6 bytes as 32-bit items, 3 bytes as 16-bit items, 8-bit items
 *          with reversal by half-word and 16-bit items with reversal by word.
 *          Zero count is accepted.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, nothing is written to the data register.
 * - Zero count: CRC_REQUEST_OK, nothing is written.
 */
void Ut_Crc_Set_Data_InvalidFeed_ReturnsErrorWithoutWrite( void )
{
    const uint8_t data[ 8u ] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
    crc_Config_t  config     = Ut_Crc_Get_Config();

    Ut_Crc_Init( &config );
    CRC->DR = UT_CRC_DR_MARKER;

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, 6u ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, 3u ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_HALFWORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_NORMAL ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( UT_CRC_ID, data, 0u ) );

    TEST_ASSERT_EQUAL_HEX32( UT_CRC_DR_MARKER, CRC->DR );
}


/**
 * \brief   Crc_Get_Data() returns data register value, bit-reversed on request.
 *
 * \details Presets DR = 0x12345678, reads it with normal and reversed output.
 *
 * \par Expected results
 * - NORMAL: 0x12345678, REV_WORD: 0x1E6A2C48.
 * - NULL pointer: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Get_Data_OutputOrder( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();
    crc_Data_t   value  = 0u;

    Ut_Crc_Init( &config );

    CRC->DR = 0x12345678u;
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( 0x12345678u, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( 0x1E6A2C48u, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Crc_Calc_Block() resets the unit, feeds data and reads the result.
 *
 * \details Clears CR.RESET after initialization, calculates CRC of 4 bytes.
 *
 * \par Expected results
 * - CRC_REQUEST_OK, CR.RESET written, result is the last word (emulated DR).
 * - NULL data or result pointer: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Calc_Block_ResetsFeedsAndReads( void )
{
    const uint8_t data[ 4u ] = { 0xA5u, 0x5Au, 0x0Fu, 0xF0u };
    crc_Config_t  config     = Ut_Crc_Get_Config();
    crc_Data_t    value      = 0u;

    Ut_Crc_Init( &config );

    CRC->CR &= ~CRC_CR_RESET;   /* Cleared by HW after reset */

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( UT_CRC_ID, data, sizeof( data ), &value ) );

    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET, CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_HEX32( 0xF00F5AA5u, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( UT_CRC_ID, NULL, sizeof( data ), &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( UT_CRC_ID, data, sizeof( data ), NULL ) );
}

/* ============================ CONFIGURATION =============================== */

/**
 * \brief   Only the CRC-32 polynomial (fixed by HW) is accepted.
 *
 * \details Sets CRC-32 polynomial, then polynomials / sizes not supported by HW.
 *
 * \par Expected results
 * - CRC-32 polynomial with 32-bit size: OK.
 * - Other polynomial or size: error.
 * - Getter returns 0x04C11DB7 / 32 bits, NULL pointers: error.
 */
void Ut_Crc_Set_Polynomial_OnlyCrc32Supported( void )
{
    crc_Config_t   config     = Ut_Crc_Get_Config();
    crc_Data_t     polynomial = 0u;
    crc_DataSize_t polySize   = CRC_DATA_SIZE_CNT;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Polynomial( UT_CRC_ID, UT_CRC_POLY32, CRC_DATA_SIZE_32BITS ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, 0x07u,         CRC_DATA_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, 0x1EDC6F41u,   CRC_DATA_SIZE_32BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, UT_CRC_POLY32, CRC_DATA_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( UT_CRC_ID, UT_CRC_POLY32, CRC_DATA_SIZE_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Polynomial( UT_CRC_ID, &polynomial, &polySize ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_POLY32, polynomial );
    TEST_ASSERT_EQUAL( CRC_DATA_SIZE_32BITS, polySize );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Polynomial( UT_CRC_ID, NULL, &polySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Polynomial( UT_CRC_ID, &polynomial, NULL ) );
}


/**
 * \brief   Initial value is applied by the next reset of the calculation.
 *
 * \details Initializes with initial value 0xFFFFFFFF, sets 0x12345678 (data
 *          register not changed), resets the calculation, sets 0xFFFFFFFF back
 *          and resets again.
 *
 * \par Expected results
 * - Set_InitValue does not write the data register, getter returns the value.
 * - Set_Reset writes pre-load word 0x7A3CB0E1.
 * - Initial value 0xFFFFFFFF: reset without pre-load word (marker kept).
 * - NULL pointer: error.
 */
void Ut_Crc_Set_InitValue_AppliedByReset( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();
    crc_Data_t   value  = 0u;

    config.InitValue = UT_CRC_RESET_VALUE;
    Ut_Crc_Init( &config );
    CRC->DR = UT_CRC_DR_MARKER;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( UT_CRC_ID, UT_CRC_INIT ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_DR_MARKER, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT_PRELOAD, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( UT_CRC_ID, UT_CRC_RESET_VALUE ) );
    CRC->DR  = UT_CRC_DR_MARKER;
    CRC->CR &= ~CRC_CR_RESET;
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL_HEX32( CRC_CR_RESET, CRC->CR & CRC_CR_RESET );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_DR_MARKER, CRC->DR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InitValue( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Input data order is configured and read back for every option.
 *
 * \details Sets every input data order option.
 *
 * \par Expected results
 * - Getter returns the same option.
 * - Invalid option: error, option kept. NULL pointer: error.
 */
void Ut_Crc_Set_InputDataOrder_AllOptions( void )
{
    crc_Config_t    config = Ut_Crc_Get_Config();
    crc_DataOrder_t order  = CRC_DATA_ORDER_CNT;

    Ut_Crc_Init( &config );

    for( crc_DataOrder_t orderId = CRC_DATA_ORDER_NORMAL; CRC_DATA_ORDER_CNT > orderId; orderId ++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, orderId ) );

        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataOrder( UT_CRC_ID, &order ) );
        TEST_ASSERT_EQUAL( orderId, order );
    }

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_WORD, order );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataOrder( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Output data order supports only normal and bit-reversed result.
 *
 * \details Sets every output data order option.
 *
 * \par Expected results
 * - NORMAL / REV_WORD: OK, getter returns the same option.
 * - REV_BYTE / REV_HALFWORD / invalid: error, option kept.
 */
void Ut_Crc_Set_OutputDataOrder_NormalAndReversedOnly( void )
{
    crc_Config_t    config = Ut_Crc_Get_Config();
    crc_DataOrder_t order  = CRC_DATA_ORDER_CNT;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_REV_WORD, order );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_NORMAL ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_NORMAL, order );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_BYTE ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_HALFWORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_OutputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( UT_CRC_ID, &order ) );
    TEST_ASSERT_EQUAL( CRC_DATA_ORDER_NORMAL, order );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_OutputDataOrder( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Input data size is configured and read back.
 *
 * \details Sets every size, then invalid size.
 *
 * \par Expected results
 * - Getter returns the same size, invalid size: error, size kept.
 * - NULL pointer: error.
 */
void Ut_Crc_Set_InputDataSize_ConfiguresAndReadsBack( void )
{
    crc_Config_t    config    = Ut_Crc_Get_Config();
    crc_InputSize_t inputSize = CRC_INPUT_SIZE_CNT;

    Ut_Crc_Init( &config );

    for( crc_InputSize_t sizeId = CRC_INPUT_SIZE_8BITS; CRC_INPUT_SIZE_CNT > sizeId; sizeId ++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, sizeId ) );
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
        TEST_ASSERT_EQUAL( sizeId, inputSize );
    }

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataSize( UT_CRC_ID, &inputSize ) );
    TEST_ASSERT_EQUAL( CRC_INPUT_SIZE_32BITS, inputSize );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InputDataSize( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Independent data register (8 bits) is written and read back.
 *
 * \details Writes 0xA5, then value wider than 8 bits.
 *
 * \par Expected results
 * - IDR = 0xA5, getter returns the same value.
 * - 0x1A5: error, register kept. NULL pointer: error.
 */
void Ut_Crc_Set_IndependentData_8BitsOnly( void )
{
    crc_Config_t    config    = Ut_Crc_Get_Config();
    crc_IndepData_t indepData = 0u;

    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_IndependentData( UT_CRC_ID, 0xA5u ) );
    TEST_ASSERT_EQUAL_HEX8( 0xA5u, CRC->IDR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_IndependentData( UT_CRC_ID, &indepData ) );
    TEST_ASSERT_EQUAL_HEX32( 0xA5u, indepData );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_IndependentData( UT_CRC_ID, 0x1A5u ) );
    TEST_ASSERT_EQUAL_HEX8( 0xA5u, CRC->IDR );

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
 * - Services work again after activation, RCC error is reported.
 */
void Ut_Crc_Set_PeriphInactive_BlocksAccessUntilActive( void )
{
    const uint8_t   data[ 4u ] = { 0x55u, 0x55u, 0x55u, 0x55u };
    crc_Config_t    config     = Ut_Crc_Get_Config();
    crc_Data_t      value      = 0u;
    crc_IndepData_t indepData  = 0u;

    Ut_Crc_Init( &config );

    Rcc_Set_PeriphInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphInactive( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InitValue( UT_CRC_ID, 0u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_IndependentData( UT_CRC_ID, 0x12u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_IndependentData( UT_CRC_ID, &indepData ) );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_OK );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphActive( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( UT_CRC_INIT, value );

    Rcc_Set_PeriphInactive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_PeriphInactive( UT_CRC_ID ) );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_CRC, RCC_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_PeriphActive( UT_CRC_ID ) );
}

/* ======================= NON-BLOCKING CALCULATION ========================= */

/**
 * \brief   Crc_Init() with DMA initializes DMA2 stream in memory to memory mode.
 *
 * \details Initializes DMA2 stream 3 with high priority and captures the DMA
 *          configuration.
 *
 * \par Expected results
 * - Dma_Init() called once with DMA2 stream 3, high priority.
 * - Memory to memory, 32-bit source (peripheral address, increment) ->
 *   32-bit CRC DR (memory address, static), no peripheral request.
 * - Transfer complete, half transfer and error handlers registered.
 * - Transfer complete, half transfer, error interrupts and NVIC line enabled.
 */
void Ut_Crc_Init_Dma_InitializesDmaStream( void )
{
    crc_Config_t config = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_3 );

    config.DmaPriority = CRC_DMA_PRIORITY_HIGH;

    Ut_Crc_Expect_RccInit();
    Dma_Get_DefaultConfig_ExpectAnyArgsAndReturn( DMA_REQUEST_OK );
    Dma_Init_StubWithCallback( Ut_Crc_DmaInitStub );
    Dma_Set_TransferCompleteIrqActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_3, DMA_REQUEST_OK );
    Dma_Set_HalfTransferIrqActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_3, DMA_REQUEST_OK );
    Dma_Set_TransferErrorIrqActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_3, DMA_REQUEST_OK );
    Dma_Set_InterruptActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_3, DMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_DmaInitCnt );
    TEST_ASSERT_EQUAL( DMA_PERIPH_2,              utCrc_DmaConfig.DmaPeriphId );
    TEST_ASSERT_EQUAL( DMA_STREAM_3,              utCrc_DmaConfig.DmaChannel );
    TEST_ASSERT_EQUAL( DMA_PRIORITY_HIGH,         utCrc_DmaConfig.Priority );
    TEST_ASSERT_EQUAL( DMA_DIR_MEMORY_TO_MEMORY,  utCrc_DmaConfig.Direction );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_NORMAL,  utCrc_DmaConfig.TransferMode );
    TEST_ASSERT_EQUAL( DMA_PERIPH_ADDR_INCREMENT, utCrc_DmaConfig.PeriphAddrIncrement );
    TEST_ASSERT_EQUAL( DMA_MEMORY_ADDR_STATIC,    utCrc_DmaConfig.MemoryAddrIncrement );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_SIZE_32BIT,   utCrc_DmaConfig.PeriphTransferSize );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_SIZE_32BIT,   utCrc_DmaConfig.MemoryTransferSize );
    TEST_ASSERT_EQUAL( DMA_REQ_MEM2MEM,           utCrc_DmaConfig.PeripheralReqId );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)(uintptr_t)&CRC->DR, utCrc_DmaConfig.MemoryAddress );
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.TransferCompleteCallback );
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.HalfTransferCallback );
    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.TransferErrorCallback );
}


/**
 * \brief   Repeated Crc_Init() releases the DMA stream of the previous initialization.
 *
 * \details Initializes DMA2 stream 4, then stream 5.
 *
 * \par Expected results
 * - Stream 4 disabled, its interrupts and NVIC line disabled before RCC init.
 * - Stream 5 initialized.
 */
void Ut_Crc_Init_DmaRepeated_ReleasesPreviousStream( void )
{
    crc_Config_t config = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_4 );

    Ut_Crc_Init_Dma( &config );

    config = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_5 );

    Ut_Crc_Expect_DmaOff( DMA_STREAM_4 );
    Ut_Crc_Expect_RccInit();
    Dma_Get_DefaultConfig_ExpectAnyArgsAndReturn( DMA_REQUEST_OK );
    Dma_Init_StubWithCallback( Ut_Crc_DmaInitStub );
    Dma_Set_TransferCompleteIrqActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_5, DMA_REQUEST_OK );
    Dma_Set_HalfTransferIrqActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_5, DMA_REQUEST_OK );
    Dma_Set_TransferErrorIrqActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_5, DMA_REQUEST_OK );
    Dma_Set_InterruptActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_5, DMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_DmaInitCnt );
    TEST_ASSERT_EQUAL( DMA_STREAM_5, utCrc_DmaConfig.DmaChannel );
}


/**
 * \brief   Crc_Init() fails when the DMA stream can not be initialized.
 *
 * \details Dma_Init() returns error.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, module stays uninitialized.
 */
void Ut_Crc_Init_DmaError_ReturnsError( void )
{
    crc_Config_t        config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;

    Ut_Crc_Expect_RccInit();
    Dma_Get_DefaultConfig_ExpectAnyArgsAndReturn( DMA_REQUEST_OK );
    Dma_Init_ExpectAnyArgsAndReturn( DMA_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Init( &config ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
}


/**
 * \brief   Crc_Set_TransferStart() arms DMA stream and signals busy state.
 *
 * \details Starts non-blocking calculation of 64 bytes from emulated SRAM.
 *
 * \par Expected results
 * - Source (peripheral) address of the buffer, data count 16 words, stream enabled.
 * - Transfer state CRC_TRANSFER_BUSY.
 * - Configuration, data feed / read and second start return error while busy.
 */
void Ut_Crc_Set_TransferStart_ArmsStreamAndBlocksAccess( void )
{
    const uint8_t * const data      = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t   xferState = CRC_TRANSFER_IDLE;
    crc_Data_t            value     = 0u;

    Ut_Crc_Init_Dma( &config );

    Dma_Set_PeriphAddr_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, (dma_PeriphAddr_t)(uintptr_t)data, DMA_REQUEST_OK );
    Dma_Set_DataCount_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, UT_CRC_XFER_CNT / 4u, DMA_REQUEST_OK );
    Dma_Set_TransferActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, DMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_BUSY, xferState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, 4u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_InitValue( UT_CRC_ID, 0u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_PeriphInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );
}


/**
 * \brief   Crc_Set_TransferStart() rejects invalid requests without DMA access.
 *
 * \details Starts with NULL data, zero count, count above DMA limit, count not
 *          multiple of 4, misaligned data, 8-bit input items, input bit
 *          reversal and without DMA configured.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, no DMA call (strict mocks), state CRC_TRANSFER_IDLE.
 */
void Ut_Crc_Set_TransferStart_InvalidRequest_ReturnsError( void )
{
    const uint8_t * const data      = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t   xferState = CRC_TRANSFER_BUSY;

    Ut_Crc_Init_Dma( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, NULL, UT_CRC_XFER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, 0u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, CRC_TRANSFER_DATA_CNT_MAX + 4u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, 6u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, &data[ 2u ], UT_CRC_XFER_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( UT_CRC_ID, CRC_INPUT_SIZE_32BITS ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( UT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );

    config = Ut_Crc_Get_Config();   /* DMA inactive */
    Ut_Crc_Init( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStop( UT_CRC_ID ) );
}


/**
 * \brief   Maximal transfer (65535 words) is accepted.
 *
 * \details Starts the transfer of CRC_TRANSFER_DATA_CNT_MAX bytes.
 *
 * \par Expected results
 * - Data count 65535 words, CRC_REQUEST_OK.
 */
void Ut_Crc_Set_TransferStart_MaxCount_Accepted( void )
{
    const uint8_t * const data   = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );

    Ut_Crc_Init_Dma( &config );

    Dma_Set_PeriphAddr_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, (dma_PeriphAddr_t)(uintptr_t)data, DMA_REQUEST_OK );
    Dma_Set_DataCount_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, 0xFFFFu, DMA_REQUEST_OK );
    Dma_Set_TransferActive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, DMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( UT_CRC_ID, data, CRC_TRANSFER_DATA_CNT_MAX ) );
}


/**
 * \brief   Failed DMA configuration or activation leaves the transfer idle.
 *
 * \details Source address configuration fails, then stream activation fails.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, state CRC_TRANSFER_IDLE, result can be read.
 */
void Ut_Crc_Set_TransferStart_DmaError_StaysIdle( void )
{
    const uint8_t * const data      = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t   xferState = CRC_TRANSFER_BUSY;
    crc_Data_t            value     = 0u;

    Ut_Crc_Init_Dma( &config );

    Dma_Set_PeriphAddr_ExpectAnyArgsAndReturn( DMA_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    Dma_Set_PeriphAddr_ExpectAnyArgsAndReturn( DMA_REQUEST_OK );
    Dma_Set_DataCount_ExpectAnyArgsAndReturn( DMA_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    Dma_Set_PeriphAddr_ExpectAnyArgsAndReturn( DMA_REQUEST_OK );
    Dma_Set_DataCount_ExpectAnyArgsAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferActive_ExpectAnyArgsAndReturn( DMA_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
}


/**
 * \brief   Transfer error reported during stream activation is kept.
 *
 * \details Dma_Set_TransferActive() stub executes the captured DMA error handler
 *          (error interrupt during activation) and returns error.
 *
 * \par Expected results
 * - CRC_REQUEST_ERROR, state CRC_TRANSFER_ERROR (not overwritten to idle).
 * - Error callback called once with CRC_XFER_ERROR_DMA_TRANSFER.
 */
void Ut_Crc_Set_TransferStart_ErrorDuringActivation_KeepsErrorState( void )
{
    const uint8_t * const data      = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );
    crc_Config_t          config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t   xferState = CRC_TRANSFER_IDLE;

    config.TransferErrorIsr = Ut_Crc_TransferErrorCallback;
    Ut_Crc_Init_Dma( &config );

    Dma_Set_PeriphAddr_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_DataCount_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferActive_StubWithCallback( Ut_Crc_DmaStartErrorStub );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_ERROR, xferState );
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_TRANSFER, utCrc_ErrorId );
}


/**
 * \brief   DMA transfer complete ends the transfer and calls user callback.
 *
 * \details Starts the transfer, calls captured DMA transfer complete handler.
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
    Ut_Crc_Init_Dma( &config );

    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.TransferCompleteCallback );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferCpltState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    Ut_Crc_Start_Transfer();
    utCrc_DmaConfig.TransferCompleteCallback();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferCpltInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferCpltState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    Ut_Crc_Start_Transfer();
    utCrc_DmaConfig.TransferCompleteCallback();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_TransferCpltCnt );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferCpltActive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferCpltState( UT_CRC_ID, NULL ) );
}


/**
 * \brief   DMA half transfer calls user callback only when active.
 *
 * \details Initializes without half transfer callback, sets it and activates it
 *          by API, calls captured DMA half transfer handler.
 *
 * \par Expected results
 * - Callback not configured: state inactive, handler does nothing.
 * - Callback set and activated: getters return it, handler calls it once,
 *   transfer stays busy.
 * - Deactivated: handler does not call it.
 */
void Ut_Crc_DmaHalfTransfer_CallsActiveCallback( void )
{
    crc_Config_t            config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_6 );
    crc_TransferState_t     xferState = CRC_TRANSFER_IDLE;
    crc_FunctionState_t     funcState = CRC_FUNCTION_ACTIVE;
    crc_CalcCompleteIsr_t * callback  = NULL;

    Ut_Crc_Init_Dma( &config );

    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.HalfTransferCallback );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_HalfTransferState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    Ut_Crc_Start_Transfer();
    utCrc_DmaConfig.HalfTransferCallback();
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_HalfTransferCnt );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_HalfTransferIsr( UT_CRC_ID, Ut_Crc_HalfTransferCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_HalfTransferActive( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_HalfTransferIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Crc_HalfTransferCallback, callback );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_HalfTransferState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    utCrc_DmaConfig.HalfTransferCallback();
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_HalfTransferCnt );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_BUSY, xferState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_HalfTransferInactive( UT_CRC_ID ) );
    utCrc_DmaConfig.HalfTransferCallback();
    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_HalfTransferCnt );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_HalfTransferIsr( UT_CRC_ID, NULL ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_HalfTransferState( UT_CRC_ID, NULL ) );
}


/**
 * \brief   DMA error stops the transfer, error state is cleared by reset.
 *
 * \details Initializes with transfer complete and transfer error callbacks,
 *          starts the transfer, calls captured DMA error handler.
 *
 * \par Expected results
 * - DMA stream disabled, state CRC_TRANSFER_ERROR, no transfer complete
 *   callback, result read and data feed return error.
 * - Transfer error callback active after init, called once with DMA transfer error.
 * - Crc_Set_Reset() clears the state to CRC_TRANSFER_IDLE.
 */
void Ut_Crc_DmaError_StopsTransferAndCallsErrorCallback( void )
{
    const uint8_t       data[ 4u ] = { 0x01u, 0x02u, 0x03u, 0x04u };
    crc_Config_t        config     = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_7 );
    crc_TransferState_t xferState  = CRC_TRANSFER_IDLE;
    crc_FunctionState_t funcState  = CRC_FUNCTION_INACTIVE;
    crc_Data_t          value      = 0u;

    config.TransferCpltIsr  = Ut_Crc_TransferCpltCallback;
    config.TransferErrorIsr = Ut_Crc_TransferErrorCallback;
    Ut_Crc_Init_Dma( &config );

    TEST_ASSERT_NOT_NULL( utCrc_DmaConfig.TransferErrorCallback );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_ACTIVE, funcState );

    Ut_Crc_Start_Transfer();

    Dma_Set_TransferInactive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_7, DMA_REQUEST_OK );
    utCrc_DmaConfig.TransferErrorCallback();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_ERROR, xferState );
    TEST_ASSERT_EQUAL_UINT32( 0u, utCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( UT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Data( UT_CRC_ID, data, sizeof( data ) ) );

    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_TRANSFER, utCrc_ErrorId );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( UT_CRC_ID, &value ) );
}


/**
 * \brief   Transfer error callback is configured at runtime and gated by its state.
 *
 * \details Initializes without error callback, sets it by API, reads it back,
 *          reports DMA error with callback inactive, then active.
 *
 * \par Expected results
 * - Callback not configured: state inactive.
 * - Getter returns the configured callback.
 * - Inactive callback: error state set, callback not called.
 * - Active callback: called once.
 * - NULL output pointers: error.
 */
void Ut_Crc_Set_TransferErrorIsr_CallbackGatedByState( void )
{
    crc_Config_t             config    = Ut_Crc_Get_DmaConfig( CRC_DMA_CHANNEL_2 );
    crc_FunctionState_t      funcState = CRC_FUNCTION_ACTIVE;
    crc_TransferState_t      xferState = CRC_TRANSFER_IDLE;
    crc_TransferErrorIsr_t * callback  = NULL;

    Ut_Crc_Init_Dma( &config );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferErrorIsr( UT_CRC_ID, Ut_Crc_TransferErrorCallback ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorIsr( UT_CRC_ID, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Crc_TransferErrorCallback, callback );

    /* Callback configured, but not active */
    Ut_Crc_Start_Transfer();
    Dma_Set_TransferInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    utCrc_DmaConfig.TransferErrorCallback();
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
    Dma_Set_TransferInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    utCrc_DmaConfig.TransferErrorCallback();
    Ut_Crc_Reset_Mocks();

    TEST_ASSERT_EQUAL_UINT32( 1u, utCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_TRANSFER, utCrc_ErrorId );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferErrorInactive( UT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferErrorState( UT_CRC_ID, &funcState ) );
    TEST_ASSERT_EQUAL( CRC_FUNCTION_INACTIVE, funcState );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorState( UT_CRC_ID, NULL ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_TransferErrorIsr( UT_CRC_ID, NULL ) );
}


/**
 * \brief   Crc_Set_TransferStop() disables DMA stream and ends busy state.
 *
 * \details Starts the transfer and stops it, then stop fails in DMA.
 *
 * \par Expected results
 * - DMA stream disabled, state CRC_TRANSFER_IDLE.
 * - DMA error: CRC_REQUEST_ERROR.
 */
void Ut_Crc_Set_TransferStop_DisablesStream( void )
{
    crc_Config_t        config    = Ut_Crc_Get_DmaConfig( UT_CRC_DMA_CHANNEL );
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;

    Ut_Crc_Init_Dma( &config );
    Ut_Crc_Start_Transfer();

    Dma_Set_TransferInactive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, DMA_REQUEST_OK );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStop( UT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( UT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );

    Dma_Set_TransferInactive_ExpectAndReturn( DMA_PERIPH_2, DMA_STREAM_0, DMA_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStop( UT_CRC_ID ) );
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


/**
 * \brief   Crc_Task() has no effect.
 *
 * \details Calls the task with initialized module.
 *
 * \par Expected results
 * - No register or mock access, data register kept.
 */
void Ut_Crc_Task_NoEffect( void )
{
    crc_Config_t config = Ut_Crc_Get_Config();

    Ut_Crc_Init( &config );
    CRC->DR = UT_CRC_DR_MARKER;

    Crc_Task();

    TEST_ASSERT_EQUAL_HEX32( UT_CRC_DR_MARKER, CRC->DR );
}

/* ========================== HELPER FUNCTIONS ============================== */

/**
 * \brief Dma_Init() stub - captures configuration (handlers, transfer).
 */
static dma_RequestState_t Ut_Crc_DmaInitStub( dma_ConfigStruct_t * const dmaConfig, int callCnt )
{
    (void)callCnt;

    utCrc_DmaConfig = *dmaConfig;
    utCrc_DmaInitCnt ++;

    return ( DMA_REQUEST_OK );
}


/**
 * \brief Dma_Set_TransferActive() stub - DMA error interrupt during activation.
 */
static dma_RequestState_t Ut_Crc_DmaStartErrorStub( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, int callCnt )
{
    (void)dmaBus;
    (void)dmaChannel;
    (void)callCnt;

    utCrc_DmaConfig.TransferErrorCallback();

    return ( DMA_REQUEST_ERROR );
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
 * \brief Transfer error user callback - counts calls and stores reported error.
 */
static void Ut_Crc_TransferErrorCallback( crc_XferErrorId_t errorId )
{
    utCrc_ErrorId = errorId;
    utCrc_ErrorCnt ++;
}


/**
 * \brief Ignores all RCC / DMA calls used by initialization and deinitialization.
 */
static void Ut_Crc_Ignore_Mocks( void )
{
    Rcc_Set_PeriphActive_IgnoreAndReturn( RCC_REQUEST_OK );
    Rcc_Set_PeriphInactive_IgnoreAndReturn( RCC_REQUEST_OK );
    Rcc_Set_ResetActive_IgnoreAndReturn( RCC_REQUEST_OK );
    Rcc_Set_ResetInactive_IgnoreAndReturn( RCC_REQUEST_OK );
    Dma_Get_DefaultConfig_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Init_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferCompleteIrqActive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_HalfTransferIrqActive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferErrorIrqActive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_InterruptActive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_InterruptInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferCompleteIrqInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_HalfTransferIrqInactive_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferErrorIrqInactive_IgnoreAndReturn( DMA_REQUEST_OK );
}


/**
 * \brief Re-initializes the mocks (clears ignore / expectation state).
 */
static void Ut_Crc_Reset_Mocks( void )
{
    MockRcc_Port_Destroy();
    MockDma_Port_Destroy();
    MockRcc_Port_Init();
    MockDma_Port_Init();
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
 * \brief Expects release of DMA2 stream (stream, NVIC line and interrupts disabled).
 */
static void Ut_Crc_Expect_DmaOff( dma_ChannelId_t channelId )
{
    Dma_Set_TransferInactive_ExpectAndReturn( DMA_PERIPH_2, channelId, DMA_REQUEST_OK );
    Dma_Set_InterruptInactive_ExpectAndReturn( DMA_PERIPH_2, channelId, DMA_REQUEST_OK );
    Dma_Set_TransferCompleteIrqInactive_ExpectAndReturn( DMA_PERIPH_2, channelId, DMA_REQUEST_OK );
    Dma_Set_HalfTransferIrqInactive_ExpectAndReturn( DMA_PERIPH_2, channelId, DMA_REQUEST_OK );
    Dma_Set_TransferErrorIrqInactive_ExpectAndReturn( DMA_PERIPH_2, channelId, DMA_REQUEST_OK );
}


/**
 * \brief Returns CRC-32 configuration with initial value 0x12345678, 32-bit
 *        items without bit reversal and without DMA.
 */
static crc_Config_t Ut_Crc_Get_Config( void )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );

    config.PeriphId        = UT_CRC_ID;
    config.InitValue       = UT_CRC_INIT;
    config.InputDataOrder  = CRC_DATA_ORDER_NORMAL;
    config.OutputDataOrder = CRC_DATA_ORDER_NORMAL;
    config.InputDataSize   = CRC_INPUT_SIZE_32BITS;

    return ( config );
}


/**
 * \brief Returns configuration of \ref Ut_Crc_Get_Config with DMA (DMA2, low priority).
 */
static crc_Config_t Ut_Crc_Get_DmaConfig( crc_DmaChannelId_t channelId )
{
    crc_Config_t config = Ut_Crc_Get_Config();

    config.DmaState     = CRC_FUNCTION_ACTIVE;
    config.DmaPeriphId  = CRC_DMA_PERIPH_2;
    config.DmaChannelId = channelId;
    config.DmaPriority  = CRC_DMA_PRIORITY_LOW;

    return ( config );
}


/**
 * \brief Initializes the module with ignored RCC / DMA calls and re-initializes the mocks.
 */
static void Ut_Crc_Init( crc_Config_t * const config )
{
    Ut_Crc_Ignore_Mocks();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( config ) );

    Ut_Crc_Reset_Mocks();
}


/**
 * \brief Initializes the module with DMA (ignored RCC / DMA calls, DMA handlers
 *        captured) and re-initializes the mocks.
 */
static void Ut_Crc_Init_Dma( crc_Config_t * const config )
{
    Ut_Crc_Ignore_Mocks();
    Dma_Init_StubWithCallback( Ut_Crc_DmaInitStub );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( config ) );

    Ut_Crc_Reset_Mocks();

    utCrc_DmaInitCnt = 0u;
}


/**
 * \brief Starts non-blocking calculation of UT_CRC_XFER_CNT bytes with ignored DMA calls.
 */
static void Ut_Crc_Start_Transfer( void )
{
    const uint8_t * const data = REGMEM_SRAM_PTR( uint8_t, UT_CRC_XFER_SRAM_OFFSET );

    Dma_Set_PeriphAddr_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_DataCount_IgnoreAndReturn( DMA_REQUEST_OK );
    Dma_Set_TransferActive_IgnoreAndReturn( DMA_REQUEST_OK );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( UT_CRC_ID, data, UT_CRC_XFER_CNT ) );

    Ut_Crc_Reset_Mocks();
}
