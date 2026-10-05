/**
 * \author Mr.Nobody
 * \file ItTest_Crc.c
 * \ingroup Crc
 * \brief Integration tests of CRC calculation unit (CRC) module on target.
 *
 * Crc module runs on the MCU together with real RCC, GPDMA and NVIC modules.
 * Tests verify behavior which cannot be verified by unit tests (emulated
 * registers): calculated CRC values of standard algorithms (check value of
 * ASCII "123456789"), register read-back on HW, configuration retention while
 * the peripheral clock is disabled and non-blocking calculation by GPDMA.
 *
 * Every test case runs after system reset done by the test runner.
 *
 * Used resources:
 * - CRC
 * - GPDMA1 channel 0 (memory to memory), no external wiring
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Crc_Port.h"                       /* Module under test              */
/* ============================= TYPEDEFS =================================== */

/** \brief Standard CRC algorithm parameters (final XOR applied by test) */
typedef struct
{
    crc_Data_t      Polynomial;     /**< Polynomial                 */
    crc_DataSize_t  PolySize;       /**< Polynomial size            */
    crc_Data_t      InitValue;      /**< Initial value              */
    crc_DataOrder_t InputOrder;     /**< Input data bit order       */
    crc_DataOrder_t OutputOrder;    /**< Output data bit order      */
    crc_Data_t      FinalXor;       /**< Final XOR (software)       */
    crc_Data_t      CheckValue;     /**< CRC of "123456789"         */
}   it_CrcAlgorithm_t;

/* ======================= FORWARD DECLARATIONS ============================= */

static void         It_Crc_Init_Algorithm       ( const it_CrcAlgorithm_t * const algorithm, crc_FunctionState_t dmaState );
static void         It_Crc_Check_Algorithm      ( const it_CrcAlgorithm_t * const algorithm );
static void         It_Crc_Wait_TransferDone    ( void );
static void         It_Crc_TransferCpltCallback ( void );
static void         It_Crc_HalfTransferCallback ( void );
static void         It_Crc_TransferErrorCallback( crc_XferErrorId_t errorId );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Peripheral under test */
#define IT_CRC_ID                           ( CRC_PERIPH_1 )

/** GPDMA channel used for non-blocking calculation */
#define IT_CRC_DMA_CHANNEL                  ( CRC_DMA_CHANNEL_0 )

/** Size of data block of non-blocking calculation */
#define IT_CRC_DMA_DATA_CNT                 ( 1024u )

/** Maximal count of wait loops for the end of non-blocking calculation */
#define IT_CRC_WAIT_LOOPS                   ( 1000000u )

/** Count of bytes of the check string "123456789" */
#define IT_CRC_CHECK_CNT                    ( 9u )

/* ========================== LOCAL VARIABLES =============================== */

/** Check string of CRC catalogues */
static const uint8_t itCrc_CheckData[ IT_CRC_CHECK_CNT ] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

/** CRC-32/MPEG-2 (HW reset configuration) */
static const it_CrcAlgorithm_t itCrc_Crc32Mpeg2 =
{
    .Polynomial = 0x04C11DB7u, .PolySize = CRC_DATA_SIZE_32BITS, .InitValue = 0xFFFFFFFFu,
    .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .FinalXor   = 0x00000000u, .CheckValue = 0x0376E6E7u
};

/** CRC-32/ISO-HDLC (Ethernet, zlib) */
static const it_CrcAlgorithm_t itCrc_Crc32IsoHdlc =
{
    .Polynomial = 0x04C11DB7u, .PolySize = CRC_DATA_SIZE_32BITS, .InitValue = 0xFFFFFFFFu,
    .InputOrder = CRC_DATA_ORDER_REV_BYTE, .OutputOrder = CRC_DATA_ORDER_REV_WORD,
    .FinalXor   = 0xFFFFFFFFu, .CheckValue = 0xCBF43926u
};

/** CRC-16/MODBUS */
static const it_CrcAlgorithm_t itCrc_Crc16Modbus =
{
    .Polynomial = 0x8005u, .PolySize = CRC_DATA_SIZE_16BITS, .InitValue = 0xFFFFu,
    .InputOrder = CRC_DATA_ORDER_REV_BYTE, .OutputOrder = CRC_DATA_ORDER_REV_WORD,
    .FinalXor   = 0x0000u, .CheckValue = 0x4B37u
};

/** CRC-16/IBM-3740 (CCITT-FALSE) */
static const it_CrcAlgorithm_t itCrc_Crc16Ibm3740 =
{
    .Polynomial = 0x1021u, .PolySize = CRC_DATA_SIZE_16BITS, .InitValue = 0xFFFFu,
    .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .FinalXor   = 0x0000u, .CheckValue = 0x29B1u
};

/** CRC-8/SMBUS */
static const it_CrcAlgorithm_t itCrc_Crc8Smbus =
{
    .Polynomial = 0x07u, .PolySize = CRC_DATA_SIZE_8BITS, .InitValue = 0x00u,
    .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .FinalXor   = 0x00u, .CheckValue = 0xF4u
};

/** CRC-7/MMC */
static const it_CrcAlgorithm_t itCrc_Crc7Mmc =
{
    .Polynomial = 0x09u, .PolySize = CRC_DATA_SIZE_7BITS, .InitValue = 0x00u,
    .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .FinalXor   = 0x00u, .CheckValue = 0x75u
};

/** Storage of data block of non-blocking calculation (word aligned for 32-bit GPDMA access) */
static uint32_t itCrc_DmaBuffer[ IT_CRC_DMA_DATA_CNT / sizeof( uint32_t ) ];

/** Data block of non-blocking calculation (accessible by GPDMA) */
static crc_DataByte_t * const itCrc_DmaData = (crc_DataByte_t *)itCrc_DmaBuffer;

/** Count of transfer complete callback calls */
static volatile uint32_t itCrc_TransferCpltCnt = 0u;

/** Count of half transfer callback calls */
static volatile uint32_t itCrc_HalfTransferCnt = 0u;

/** Count of transfer error callback calls */
static volatile uint32_t itCrc_ErrorCnt = 0u;

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    itCrc_TransferCpltCnt = 0u;
    itCrc_HalfTransferCnt = 0u;
    itCrc_ErrorCnt        = 0u;

    for( uint32_t byteIdx = 0u; IT_CRC_DMA_DATA_CNT > byteIdx; byteIdx++ )
    {
        itCrc_DmaData[ byteIdx ] = (uint8_t)( ( byteIdx * 13u ) + 7u );
    }
}


void tearDown( void )
{
    /* Every test case runs after system reset */
}

/* =============================== TESTS ==================================== */

/*------------------------------ Initialization ------------------------------*/

/**
 * \brief   Crc_Init() with default configuration calculates CRC-32 Ethernet.
 *
 * \details Initializes with default configuration, calculates CRC of "123456789".
 *
 * \par Expected results
 * - Crc_Init() returns CRC_REQUEST_OK.
 * - Result XOR 0xFFFFFFFF is 0xCBF43926.
 */
void It_Crc_Init_DefaultConfig_CalculatesCrc32Ethernet( void )
{
    crc_Config_t config;
    crc_Data_t   value = 0u;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT, &value ) );
    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc32IsoHdlc.CheckValue, value ^ itCrc_Crc32IsoHdlc.FinalXor );
}


/**
 * \brief   CRC-32/MPEG-2 check value (HW reset configuration).
 *
 * \details No reversal, no final XOR.
 *
 * \par Expected results
 * - Result is 0x0376E6E7.
 */
void It_Crc_Calc_Crc32Mpeg2_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc32Mpeg2 );
}


/**
 * \brief   Independent data register keeps its value during calculation.
 *
 * \details Writes IDR, calculates CRC with reset, reads IDR back.
 *
 * \par Expected results
 * - IDR reads back the written value after the calculation.
 */
void It_Crc_Set_IndependentData_KeptDuringCalculation( void )
{
    crc_IndepData_t indepData = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc16Modbus, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_IndependentData( IT_CRC_ID, 0x5AA5C33Cu ) );
    It_Crc_Check_Algorithm( &itCrc_Crc16Modbus );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_IndependentData( IT_CRC_ID, &indepData ) );
    TEST_ASSERT_EQUAL_HEX32( 0x5AA5C33Cu, indepData );
}


/**
 * \brief   Configuration reads back from HW registers.
 *
 * \details Initializes CRC-16/MODBUS, reads polynomial, initial value and data
 *          orders back.
 *
 * \par Expected results
 * - All getters return the configured values.
 */
void It_Crc_Init_Crc16Modbus_ConfigReadsBack( void )
{
    crc_Data_t      polynomial  = 0u;
    crc_DataSize_t  polySize    = CRC_DATA_SIZE_CNT;
    crc_Data_t      initValue   = 0u;
    crc_DataOrder_t inputOrder  = CRC_DATA_ORDER_CNT;
    crc_DataOrder_t outputOrder = CRC_DATA_ORDER_CNT;

    It_Crc_Init_Algorithm( &itCrc_Crc16Modbus, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Polynomial( IT_CRC_ID, &polynomial, &polySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( IT_CRC_ID, &initValue ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataOrder( IT_CRC_ID, &inputOrder ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( IT_CRC_ID, &outputOrder ) );

    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc16Modbus.Polynomial, polynomial );
    TEST_ASSERT_EQUAL( itCrc_Crc16Modbus.PolySize, polySize );
    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc16Modbus.InitValue, initValue );
    TEST_ASSERT_EQUAL( itCrc_Crc16Modbus.InputOrder, inputOrder );
    TEST_ASSERT_EQUAL( itCrc_Crc16Modbus.OutputOrder, outputOrder );
}

/*--------------------------- Standard algorithms ----------------------------*/

/**
 * \brief   CRC-32/ISO-HDLC (Ethernet, zlib) check value.
 *
 * \details Input reversed by byte, output reversed, final XOR by software.
 *
 * \par Expected results
 * - Result XOR 0xFFFFFFFF is 0xCBF43926.
 */
void It_Crc_Calc_Crc32IsoHdlc_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32IsoHdlc, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc32IsoHdlc );
}


/**
 * \brief   CRC-16/MODBUS check value.
 *
 * \details 16-bit polynomial 0x8005, initial value 0xFFFF, reversed input / output.
 *
 * \par Expected results
 * - Result is 0x4B37.
 */
void It_Crc_Calc_Crc16Modbus_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc16Modbus, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc16Modbus );
}


/**
 * \brief   CRC-16/IBM-3740 (CCITT-FALSE) check value.
 *
 * \details 16-bit polynomial 0x1021, initial value 0xFFFF, no reversal.
 *
 * \par Expected results
 * - Result is 0x29B1.
 */
void It_Crc_Calc_Crc16Ibm3740_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc16Ibm3740, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc16Ibm3740 );
}


/**
 * \brief   CRC-8/SMBUS check value.
 *
 * \details 8-bit polynomial 0x07, initial value 0, no reversal.
 *
 * \par Expected results
 * - Result is 0xF4.
 */
void It_Crc_Calc_Crc8Smbus_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc8Smbus, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc8Smbus );
}


/**
 * \brief   CRC-7/MMC check value.
 *
 * \details 7-bit polynomial 0x09, initial value 0, no reversal.
 *
 * \par Expected results
 * - Result is 0x75.
 */
void It_Crc_Calc_Crc7Mmc_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc7Mmc, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc7Mmc );
}

/*--------------------------- Runtime configuration --------------------------*/

/**
 * \brief   Data fed in more parts give the same result as one block.
 *
 * \details CRC-16/MODBUS, "1234" and "56789" fed separately after reset.
 *
 * \par Expected results
 * - Result is 0x4B37.
 */
void It_Crc_Set_Data_InParts_SameAsBlock( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc16Modbus, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 0u ], 4u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 4u ], IT_CRC_CHECK_CNT - 4u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );

    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc16Modbus.CheckValue, value );
}


/**
 * \brief   Algorithm is switched at runtime by configuration services.
 *
 * \details Initializes CRC-32/MPEG-2, reconfigures it to CRC-16/IBM-3740 by
 *          Crc_Set_Polynomial(), Crc_Set_InitValue() and data order setters.
 *
 * \par Expected results
 * - Result is 0x29B1.
 */
void It_Crc_Set_Configuration_SwitchesAlgorithm( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Polynomial( IT_CRC_ID, itCrc_Crc16Ibm3740.Polynomial, itCrc_Crc16Ibm3740.PolySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( IT_CRC_ID, itCrc_Crc16Ibm3740.InitValue ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( IT_CRC_ID, itCrc_Crc16Ibm3740.InputOrder ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( IT_CRC_ID, itCrc_Crc16Ibm3740.OutputOrder ) );

    It_Crc_Check_Algorithm( &itCrc_Crc16Ibm3740 );
}


/**
 * \brief   Configuration is kept while the peripheral clock is disabled.
 *
 * \details Initializes CRC-8/SMBUS, disables and enables the peripheral.
 *
 * \par Expected results
 * - Calculation fails while inactive.
 * - After activation the result is 0xF4 without reconfiguration.
 */
void It_Crc_Set_PeriphInactive_ConfigurationKept( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc8Smbus, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphInactive( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT, &value ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphActive( IT_CRC_ID ) );
    It_Crc_Check_Algorithm( &itCrc_Crc8Smbus );
}


/**
 * \brief   Crc_Deinit() releases the peripheral.
 *
 * \details Initializes the module, deinitializes it and calculates CRC.
 *
 * \par Expected results
 * - Crc_Deinit() returns CRC_REQUEST_OK, calculation returns error.
 */
void It_Crc_Deinit_ServicesReturnError( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Deinit() );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT, &value ) );
}

/*------------------------- Non-blocking calculation -------------------------*/

/**
 * \brief   Non-blocking calculation of the check string.
 *
 * \details CRC-32/ISO-HDLC with GPDMA, transfer complete callback configured.
 *
 * \par Expected results
 * - Transfer ends (state CRC_TRANSFER_IDLE), callback called once.
 * - Result XOR 0xFFFFFFFF is 0xCBF43926.
 */
void It_Crc_Set_TransferStart_CheckValue( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32IsoHdlc, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT ) );

    It_Crc_Wait_TransferDone();

    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc32IsoHdlc.CheckValue, value ^ itCrc_Crc32IsoHdlc.FinalXor );
}


/**
 * \brief   Non-blocking calculation of a large block equals blocking calculation.
 *
 * \details CRC-16/MODBUS of 1024 bytes - blocking reference, then GPDMA with
 *          transfer complete and half transfer callbacks.
 *
 * \par Expected results
 * - Transfer complete and half transfer callbacks called once, no transfer error.
 * - Non-blocking result equals blocking result.
 */
void It_Crc_Set_TransferStart_LargeBlockEqualsBlocking( void )
{
    crc_Data_t blockingValue = 0u;
    crc_Data_t dmaValue      = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc16Modbus, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT, &blockingValue ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT ) );

    It_Crc_Wait_TransferDone();

    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_HalfTransferCnt );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &dmaValue ) );
    TEST_ASSERT_EQUAL_HEX32( blockingValue, dmaValue );
}


/**
 * \brief   32-bit input items - non-blocking calculation equals blocking calculation.
 *
 * \details CRC-32/MPEG-2 of 1024 bytes as 32-bit items - blocking reference
 *          (items assembled by CPU), then GPDMA with 32-bit accesses. Repeats the
 *          blocking calculation with 8-bit items.
 *
 * \par Expected results
 * - Non-blocking result equals blocking result with 32-bit items.
 * - Result with 8-bit items differs (items are processed MSB first).
 */
void It_Crc_Set_InputDataSize_32BitItemsDmaEqualsBlocking( void )
{
    crc_Data_t blockingValue = 0u;
    crc_Data_t dmaValue      = 0u;
    crc_Data_t byteValue     = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( IT_CRC_ID, CRC_INPUT_SIZE_32BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT, &blockingValue ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT ) );

    It_Crc_Wait_TransferDone();

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &dmaValue ) );
    TEST_ASSERT_EQUAL_HEX32( blockingValue, dmaValue );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( IT_CRC_ID, CRC_INPUT_SIZE_8BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT, &byteValue ) );
    TEST_ASSERT_NOT_EQUAL( blockingValue, byteValue );
}


/**
 * \brief   Consecutive non-blocking calculations reuse the GPDMA channel.
 *
 * \details Calculates CRC-16/IBM-3740 of the check string twice by GPDMA.
 *
 * \par Expected results
 * - Both results are 0x29B1, transfer complete callback called twice, no transfer error.
 */
void It_Crc_Set_TransferStart_Repeated( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc16Ibm3740, CRC_FUNCTION_ACTIVE );

    for( uint32_t runIdx = 0u; 2u > runIdx; runIdx++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT ) );

        It_Crc_Wait_TransferDone();

        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
        TEST_ASSERT_EQUAL_HEX32( itCrc_Crc16Ibm3740.CheckValue, value );
    }

    TEST_ASSERT_EQUAL_UINT32( 2u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCrc_ErrorCnt );
}

/* ========================== HELPER FUNCTIONS ============================== */

/**
 * \brief Initializes the module with the algorithm parameters.
 *
 * \param algorithm [in]: Algorithm parameters
 * \param dmaState  [in]: Non-blocking calculation state (GPDMA1 channel 0, callbacks)
 */
static void It_Crc_Init_Algorithm( const it_CrcAlgorithm_t * const algorithm, crc_FunctionState_t dmaState )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );

    config.PeriphId         = IT_CRC_ID;
    config.Polynomial       = algorithm->Polynomial;
    config.PolySize         = algorithm->PolySize;
    config.InitValue        = algorithm->InitValue;
    config.InputDataOrder   = algorithm->InputOrder;
    config.OutputDataOrder  = algorithm->OutputOrder;
    config.DmaState         = dmaState;
    config.DmaPeriphId      = CRC_DMA_PERIPH_1;
    config.DmaChannelId     = IT_CRC_DMA_CHANNEL;
    config.DmaPriority      = CRC_DMA_PRIORITY_LOW;
    config.TransferCpltIsr  = It_Crc_TransferCpltCallback;
    config.HalfTransferIsr  = It_Crc_HalfTransferCallback;
    config.TransferErrorIsr = It_Crc_TransferErrorCallback;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Init( &config ) );
}


/**
 * \brief Calculates CRC of the check string (blocking) and verifies the check value.
 *
 * \param algorithm [in]: Algorithm parameters
 */
static void It_Crc_Check_Algorithm( const it_CrcAlgorithm_t * const algorithm )
{
    crc_Data_t value = 0u;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT, &value ) );
    TEST_ASSERT_EQUAL_HEX32( algorithm->CheckValue, value ^ algorithm->FinalXor );
}


/**
 * \brief Waits for the end of non-blocking calculation (with timeout).
 */
static void It_Crc_Wait_TransferDone( void )
{
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;

    for( uint32_t loopIdx = 0u; ( IT_CRC_WAIT_LOOPS > loopIdx ) && ( CRC_TRANSFER_BUSY == xferState ); loopIdx++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( IT_CRC_ID, &xferState ) );
    }

    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
}


/**
 * \brief Transfer complete callback - counts calls.
 */
static void It_Crc_TransferCpltCallback( void )
{
    itCrc_TransferCpltCnt++;
}


/**
 * \brief Half transfer callback - counts calls.
 */
static void It_Crc_HalfTransferCallback( void )
{
    itCrc_HalfTransferCnt++;
}


/**
 * \brief Transfer error callback - counts calls.
 *
 * \param errorId [in]: Reported error
 */
static void It_Crc_TransferErrorCallback( crc_XferErrorId_t errorId )
{
    (void)errorId;

    itCrc_ErrorCnt++;
}
