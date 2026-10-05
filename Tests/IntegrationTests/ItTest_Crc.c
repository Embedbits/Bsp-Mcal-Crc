/**
 * \author Mr.Nobody
 * \file ItTest_Crc.c
 * \ingroup Crc
 * \brief Integration tests of CRC calculation unit (CRC) module on target.
 *
 * Crc module runs on the MCU together with real RCC, DMA and NVIC modules.
 * Tests verify behavior which cannot be verified by unit tests (emulated
 * registers): calculated CRC values of standard CRC-32 algorithms (check value
 * of ASCII "123456789"), which verify the SW emulation of initial value, bit
 * order and 8/16-bit input items on the fixed STM32F4 CRC unit, configuration
 * retention while the peripheral clock is disabled and non-blocking calculation
 * by DMA2.
 *
 * Every test case runs after system reset done by the test runner.
 *
 * Used resources:
 * - CRC
 * - DMA2 stream 0 (memory to memory), no external wiring
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Crc_Port.h"                       /* Module under test              */
/* ============================= TYPEDEFS =================================== */

/** \brief Standard CRC algorithm parameters (final XOR applied by test) */
typedef struct
{
    crc_Data_t      InitValue;      /**< Initial value              */
    crc_DataOrder_t InputOrder;     /**< Input data bit order       */
    crc_DataOrder_t OutputOrder;    /**< Output data bit order      */
    crc_InputSize_t InputSize;      /**< Input data item size       */
    crc_Data_t      FinalXor;       /**< Final XOR (software)       */
    crc_Data_t      CheckValue;     /**< CRC of "123456789"         */
}   it_CrcAlgorithm_t;

/* ======================= FORWARD DECLARATIONS ============================= */

static void         It_Crc_Init_Algorithm       ( const it_CrcAlgorithm_t * const algorithm, crc_FunctionState_t dmaState );
static void         It_Crc_Check_Algorithm      ( const it_CrcAlgorithm_t * const algorithm );
static void         It_Crc_Wait_TransferEnd     ( crc_TransferState_t expectedState );
static void         It_Crc_TransferCpltCallback ( void );
static void         It_Crc_HalfTransferCallback ( void );
static void         It_Crc_TransferErrorCallback( crc_XferErrorId_t errorId );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Peripheral under test */
#define IT_CRC_ID                           ( CRC_PERIPH_1 )

/** DMA stream used for non-blocking calculation */
#define IT_CRC_DMA_CHANNEL                  ( CRC_DMA_CHANNEL_0 )

/** Size of data block of non-blocking calculation */
#define IT_CRC_DMA_DATA_CNT                 ( 1024u )

/** Maximal count of wait loops for the end of non-blocking calculation */
#define IT_CRC_WAIT_LOOPS                   ( 1000000u )

/** Count of bytes of the check string "123456789" */
#define IT_CRC_CHECK_CNT                    ( 9u )

/** Count of bytes of the check string used with 32-bit items ("12345678") */
#define IT_CRC_CHECK_WORD_CNT               ( 8u )

/** CRC-32/MPEG-2 of "12345678" as 32-bit little endian items (reference model) */
#define IT_CRC_MPEG2_WORDS_CHECK            ( 0xFEFC54F9u )

/** CRC-32/MPEG-2 of the DMA data block as 32-bit items (reference model) */
#define IT_CRC_MPEG2_BLOCK_CHECK            ( 0xA6EA9642u )

/** CRC-32 (initial value 0, no reversal) of the DMA data block as 32-bit items (reference model) */
#define IT_CRC_INIT0_BLOCK_CHECK            ( 0x2DE0C44Au )

/** CRC-32/ISO-HDLC of the DMA data block without final XOR (zlib) */
#define IT_CRC_ISOHDLC_BLOCK_CHECK          ( 0x72CB6425u )

/* ========================== LOCAL VARIABLES =============================== */

/** Check string of CRC catalogues (word aligned for DMA) */
static const uint8_t itCrc_CheckData[ 12u ] __attribute__(( aligned( 4u ) )) = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

/** CRC-32/ISO-HDLC (Ethernet, zlib) - default configuration */
static const it_CrcAlgorithm_t itCrc_Crc32IsoHdlc =
{
    .InitValue  = 0xFFFFFFFFu, .InputOrder = CRC_DATA_ORDER_REV_BYTE, .OutputOrder = CRC_DATA_ORDER_REV_WORD,
    .InputSize  = CRC_INPUT_SIZE_8BITS, .FinalXor = 0xFFFFFFFFu, .CheckValue = 0xCBF43926u
};

/** CRC-32/MPEG-2 (HW native configuration, byte stream) */
static const it_CrcAlgorithm_t itCrc_Crc32Mpeg2 =
{
    .InitValue  = 0xFFFFFFFFu, .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .InputSize  = CRC_INPUT_SIZE_8BITS, .FinalXor = 0x00000000u, .CheckValue = 0x0376E6E7u
};

/** CRC-32/BZIP2 */
static const it_CrcAlgorithm_t itCrc_Crc32Bzip2 =
{
    .InitValue  = 0xFFFFFFFFu, .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .InputSize  = CRC_INPUT_SIZE_8BITS, .FinalXor = 0xFFFFFFFFu, .CheckValue = 0xFC891918u
};

/** CRC-32/JAMCRC */
static const it_CrcAlgorithm_t itCrc_Crc32Jamcrc =
{
    .InitValue  = 0xFFFFFFFFu, .InputOrder = CRC_DATA_ORDER_REV_BYTE, .OutputOrder = CRC_DATA_ORDER_REV_WORD,
    .InputSize  = CRC_INPUT_SIZE_8BITS, .FinalXor = 0x00000000u, .CheckValue = 0x340BC6D9u
};

/** CRC-32/CKSUM (POSIX) - initial value 0 (emulated by pre-load word) */
static const it_CrcAlgorithm_t itCrc_Crc32Cksum =
{
    .InitValue  = 0x00000000u, .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .InputSize  = CRC_INPUT_SIZE_8BITS, .FinalXor = 0xFFFFFFFFu, .CheckValue = 0x765E7680u
};

/** CRC-32/MPEG-2 with 32-bit items (data written unchanged by CPU or DMA) */
static const it_CrcAlgorithm_t itCrc_Crc32Mpeg2Words =
{
    .InitValue  = 0xFFFFFFFFu, .InputOrder = CRC_DATA_ORDER_NORMAL, .OutputOrder = CRC_DATA_ORDER_NORMAL,
    .InputSize  = CRC_INPUT_SIZE_32BITS, .FinalXor = 0x00000000u, .CheckValue = IT_CRC_MPEG2_WORDS_CHECK
};

/** Storage of data block of non-blocking calculation (word aligned for 32-bit DMA access) */
static uint32_t itCrc_DmaBuffer[ IT_CRC_DMA_DATA_CNT / sizeof( uint32_t ) ];

/** Data block of non-blocking calculation (accessible by DMA) */
static crc_DataByte_t * const itCrc_DmaData = (crc_DataByte_t *)itCrc_DmaBuffer;

/** Count of transfer complete callback calls */
static volatile uint32_t itCrc_TransferCpltCnt = 0u;

/** Count of half transfer callback calls */
static volatile uint32_t itCrc_HalfTransferCnt = 0u;

/** Count of transfer error callback calls */
static volatile uint32_t itCrc_ErrorCnt = 0u;

/** Last error reported by transfer error callback */
static volatile crc_XferErrorId_t itCrc_ErrorId = CRC_XFER_ERROR_CNT;

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    itCrc_TransferCpltCnt = 0u;
    itCrc_HalfTransferCnt = 0u;
    itCrc_ErrorCnt        = 0u;
    itCrc_ErrorId         = CRC_XFER_ERROR_CNT;

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
 * \details Initializes with default configuration (8-bit items, input reversed
 *          by byte, output reversed), calculates CRC of "123456789" - two
 *          groups of 4 bytes by HW, last byte by SW.
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
 * \brief   Independent data register keeps its value during calculation.
 *
 * \details Writes IDR (8 bits), calculates CRC with reset, reads IDR back.
 *          Value wider than 8 bits is rejected.
 *
 * \par Expected results
 * - IDR reads back the written value after the calculation.
 * - 0x15A: CRC_REQUEST_ERROR.
 */
void It_Crc_Set_IndependentData_KeptDuringCalculation( void )
{
    crc_IndepData_t indepData = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_IndependentData( IT_CRC_ID, 0x5Au ) );
    It_Crc_Check_Algorithm( &itCrc_Crc32Mpeg2 );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_IndependentData( IT_CRC_ID, &indepData ) );
    TEST_ASSERT_EQUAL_HEX32( 0x5Au, indepData );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_IndependentData( IT_CRC_ID, 0x15Au ) );
}


/**
 * \brief   Configuration reads back.
 *
 * \details Initializes CRC-32/CKSUM with output reversal, reads polynomial,
 *          initial value, data orders and input size back.
 *
 * \par Expected results
 * - All getters return the configured values, polynomial 0x04C11DB7 / 32 bits.
 */
void It_Crc_Init_ConfigReadsBack( void )
{
    crc_Data_t      polynomial  = 0u;
    crc_DataSize_t  polySize    = CRC_DATA_SIZE_CNT;
    crc_Data_t      initValue   = 0xFFFFFFFFu;
    crc_DataOrder_t inputOrder  = CRC_DATA_ORDER_CNT;
    crc_DataOrder_t outputOrder = CRC_DATA_ORDER_CNT;
    crc_InputSize_t inputSize   = CRC_INPUT_SIZE_CNT;

    It_Crc_Init_Algorithm( &itCrc_Crc32Cksum, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Polynomial( IT_CRC_ID, &polynomial, &polySize ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InitValue( IT_CRC_ID, &initValue ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataOrder( IT_CRC_ID, &inputOrder ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_OutputDataOrder( IT_CRC_ID, &outputOrder ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_InputDataSize( IT_CRC_ID, &inputSize ) );

    TEST_ASSERT_EQUAL_HEX32( CRC_POLYNOMIAL, polynomial );
    TEST_ASSERT_EQUAL( CRC_DATA_SIZE_32BITS, polySize );
    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc32Cksum.InitValue, initValue );
    TEST_ASSERT_EQUAL( itCrc_Crc32Cksum.InputOrder, inputOrder );
    TEST_ASSERT_EQUAL( itCrc_Crc32Cksum.OutputOrder, outputOrder );
    TEST_ASSERT_EQUAL( itCrc_Crc32Cksum.InputSize, inputSize );
}

/*--------------------------- Standard algorithms ----------------------------*/

/**
 * \brief   CRC-32/MPEG-2 check value (HW native polynomial, initial value and order).
 *
 * \details No reversal, no final XOR, 8-bit items.
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
 * \brief   CRC-32/BZIP2 check value.
 *
 * \details No reversal, final XOR 0xFFFFFFFF by software.
 *
 * \par Expected results
 * - Result XOR 0xFFFFFFFF is 0xFC891918.
 */
void It_Crc_Calc_Crc32Bzip2_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32Bzip2, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc32Bzip2 );
}


/**
 * \brief   CRC-32/JAMCRC check value.
 *
 * \details Input reversed by byte, output reversed (emulated), no final XOR.
 *
 * \par Expected results
 * - Result is 0x340BC6D9.
 */
void It_Crc_Calc_Crc32Jamcrc_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32Jamcrc, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc32Jamcrc );
}


/**
 * \brief   CRC-32/CKSUM check value - initial value 0 emulated by pre-load word.
 *
 * \details Initial value 0, no reversal, final XOR 0xFFFFFFFF by software.
 *
 * \par Expected results
 * - Result XOR 0xFFFFFFFF is 0x765E7680.
 */
void It_Crc_Calc_Crc32Cksum_CheckValue( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32Cksum, CRC_FUNCTION_INACTIVE );
    It_Crc_Check_Algorithm( &itCrc_Crc32Cksum );
}


/**
 * \brief   Input data size variants of CRC-32 Ethernet give the same result.
 *
 * \details CRC-32/ISO-HDLC of 1024 bytes with 8-bit items (reversed by byte),
 *          16-bit items (reversed by half-word) and 32-bit items (reversed by word),
 *          then 6 bytes "123456" with 16-bit items (last item by SW).
 *
 * \par Expected results
 * - All results of the block are 0x72CB6425 (zlib without final XOR).
 * - "123456" result XOR 0xFFFFFFFF is 0x0972D361 (zlib).
 */
void It_Crc_Set_InputDataSize_VariantsSameResult( void )
{
    static const crc_DataOrder_t orderLut[ CRC_INPUT_SIZE_CNT ] =
    {
        CRC_DATA_ORDER_REV_BYTE, CRC_DATA_ORDER_REV_HALFWORD, CRC_DATA_ORDER_REV_WORD
    };
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32IsoHdlc, CRC_FUNCTION_INACTIVE );

    for( crc_InputSize_t sizeId = CRC_INPUT_SIZE_8BITS; CRC_INPUT_SIZE_CNT > sizeId; sizeId++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( IT_CRC_ID, sizeId ) );
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( IT_CRC_ID, orderLut[ sizeId ] ) );
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT, &value ) );
        TEST_ASSERT_EQUAL_HEX32( IT_CRC_ISOHDLC_BLOCK_CHECK, value );
    }

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( IT_CRC_ID, CRC_INPUT_SIZE_16BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( IT_CRC_ID, CRC_DATA_ORDER_REV_HALFWORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, 6u, &value ) );
    TEST_ASSERT_EQUAL_HEX32( 0x0972D361u, value ^ 0xFFFFFFFFu );
}

/*--------------------------- Runtime configuration --------------------------*/

/**
 * \brief   Data fed in more parts give the same result as one block.
 *
 * \details CRC-32/ISO-HDLC, "12", "345" and "6789" fed separately after reset
 *          (every part ends with items calculated by SW).
 *
 * \par Expected results
 * - Result XOR 0xFFFFFFFF is 0xCBF43926.
 */
void It_Crc_Set_Data_InParts_SameAsBlock( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32IsoHdlc, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 0u ], 2u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 2u ], 3u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 5u ], 4u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );

    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc32IsoHdlc.CheckValue, value ^ itCrc_Crc32IsoHdlc.FinalXor );
}


/**
 * \brief   Algorithm is switched at runtime by configuration services.
 *
 * \details Initializes CRC-32/MPEG-2, reconfigures it to CRC-32/CKSUM by
 *          Crc_Set_InitValue(), then to CRC-32/JAMCRC by data order setters.
 *
 * \par Expected results
 * - CKSUM result XOR 0xFFFFFFFF is 0x765E7680, JAMCRC result is 0x340BC6D9.
 */
void It_Crc_Set_Configuration_SwitchesAlgorithm( void )
{
    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Polynomial( IT_CRC_ID, CRC_POLYNOMIAL, CRC_DATA_SIZE_32BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( IT_CRC_ID, itCrc_Crc32Cksum.InitValue ) );
    It_Crc_Check_Algorithm( &itCrc_Crc32Cksum );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( IT_CRC_ID, itCrc_Crc32Jamcrc.InitValue ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( IT_CRC_ID, itCrc_Crc32Jamcrc.InputOrder ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( IT_CRC_ID, itCrc_Crc32Jamcrc.OutputOrder ) );
    It_Crc_Check_Algorithm( &itCrc_Crc32Jamcrc );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_Polynomial( IT_CRC_ID, 0x8005u, CRC_DATA_SIZE_16BITS ) );
}


/**
 * \brief   Configuration and intermediate value are kept while the clock is disabled.
 *
 * \details Initializes CRC-32/CKSUM, feeds "1234", disables and enables the
 *          peripheral, feeds "56789".
 *
 * \par Expected results
 * - Calculation fails while inactive.
 * - After activation the result XOR 0xFFFFFFFF is 0x765E7680.
 */
void It_Crc_Set_PeriphInactive_ConfigurationKept( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Cksum, CRC_FUNCTION_INACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 0u ], 4u ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphInactive( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_CNT, &value ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_PeriphActive( IT_CRC_ID ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Data( IT_CRC_ID, &itCrc_CheckData[ 4u ], IT_CRC_CHECK_CNT - 4u ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( itCrc_Crc32Cksum.CheckValue, value ^ itCrc_Crc32Cksum.FinalXor );
}


/**
 * \brief   Crc_Deinit() releases the peripheral.
 *
 * \details Initializes the module with DMA, deinitializes it and calculates CRC.
 *
 * \par Expected results
 * - Crc_Deinit() returns CRC_REQUEST_OK, calculation returns error.
 */
void It_Crc_Deinit_ServicesReturnError( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2Words, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Deinit() );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_WORD_CNT, &value ) );
}

/*------------------------- Non-blocking calculation -------------------------*/

/**
 * \brief   Non-blocking calculation of the check string (32-bit items).
 *
 * \details CRC-32/MPEG-2 of "12345678" (flash) by DMA2, transfer complete
 *          callback configured, then the same by CPU.
 *
 * \par Expected results
 * - Transfer ends (state CRC_TRANSFER_IDLE), callback called once.
 * - Result is 0xFEFC54F9 (reference model) for DMA and CPU.
 */
void It_Crc_Set_TransferStart_CheckValue( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2Words, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_WORD_CNT ) );

    It_Crc_Wait_TransferEnd( CRC_TRANSFER_IDLE );

    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( IT_CRC_MPEG2_WORDS_CHECK, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_WORD_CNT, &value ) );
    TEST_ASSERT_EQUAL_HEX32( IT_CRC_MPEG2_WORDS_CHECK, value );
}


/**
 * \brief   Non-blocking calculation of a large block equals blocking calculation.
 *
 * \details CRC-32/MPEG-2 of 1024 bytes as 32-bit items - blocking reference,
 *          then DMA with transfer complete and half transfer callbacks.
 *
 * \par Expected results
 * - Transfer complete and half transfer callbacks called once, no transfer error.
 * - Both results are 0xA6EA9642 (reference model).
 */
void It_Crc_Set_TransferStart_LargeBlockEqualsBlocking( void )
{
    crc_Data_t blockingValue = 0u;
    crc_Data_t dmaValue      = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2Words, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Calc_Block( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT, &blockingValue ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT ) );

    It_Crc_Wait_TransferEnd( CRC_TRANSFER_IDLE );

    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_HalfTransferCnt );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &dmaValue ) );
    TEST_ASSERT_EQUAL_HEX32( IT_CRC_MPEG2_BLOCK_CHECK, blockingValue );
    TEST_ASSERT_EQUAL_HEX32( blockingValue, dmaValue );
}


/**
 * \brief   Non-blocking calculation starts from the emulated initial value and
 *          supports output reversal.
 *
 * \details Initial value 0 (pre-load word written by Crc_Set_Reset() before the
 *          DMA transfer) of 1024 bytes as 32-bit items, result read normal and
 *          bit-reversed.
 *
 * \par Expected results
 * - Result is 0x2DE0C44A (reference model), reversed result is its bit reversal.
 */
void It_Crc_Set_TransferStart_InitValueAndOutputOrder( void )
{
    crc_Data_t value         = 0u;
    crc_Data_t reversedValue = 0u;
    crc_Data_t expectedRev   = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2Words, CRC_FUNCTION_ACTIVE );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InitValue( IT_CRC_ID, 0u ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT ) );

    It_Crc_Wait_TransferEnd( CRC_TRANSFER_IDLE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( IT_CRC_INIT0_BLOCK_CHECK, value );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_OutputDataOrder( IT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &reversedValue ) );

    for( uint32_t bitIdx = 0u; 32u > bitIdx; bitIdx++ )
    {
        expectedRev |= ( ( IT_CRC_INIT0_BLOCK_CHECK >> bitIdx ) & 1u ) << ( 31u - bitIdx );
    }

    TEST_ASSERT_EQUAL_HEX32( expectedRev, reversedValue );
}


/**
 * \brief   Consecutive non-blocking calculations reuse the DMA stream.
 *
 * \details Calculates CRC-32/MPEG-2 of "12345678" twice by DMA.
 *
 * \par Expected results
 * - Both results are 0xFEFC54F9, transfer complete callback called twice, no
 *   transfer error.
 */
void It_Crc_Set_TransferStart_Repeated( void )
{
    crc_Data_t value = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2Words, CRC_FUNCTION_ACTIVE );

    for( uint32_t runIdx = 0u; 2u > runIdx; runIdx++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_WORD_CNT ) );

        It_Crc_Wait_TransferEnd( CRC_TRANSFER_IDLE );

        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
        TEST_ASSERT_EQUAL_HEX32( IT_CRC_MPEG2_WORDS_CHECK, value );
    }

    TEST_ASSERT_EQUAL_UINT32( 2u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCrc_ErrorCnt );
}


/**
 * \brief   Non-blocking calculation supports only 32-bit items without input reversal.
 *
 * \details Default configuration (8-bit items, input reversed) with DMA, start
 *          request, then 32-bit items with input reversed by word.
 *
 * \par Expected results
 * - Both starts return CRC_REQUEST_ERROR, no callback, state CRC_TRANSFER_IDLE.
 */
void It_Crc_Set_TransferStart_UnsupportedInput_ReturnsError( void )
{
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;

    It_Crc_Init_Algorithm( &itCrc_Crc32IsoHdlc, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataSize( IT_CRC_ID, CRC_INPUT_SIZE_32BITS ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_InputDataOrder( IT_CRC_ID, CRC_DATA_ORDER_REV_WORD ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Set_TransferStart( IT_CRC_ID, itCrc_DmaData, IT_CRC_DMA_DATA_CNT ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( IT_CRC_ID, &xferState ) );
    TEST_ASSERT_EQUAL( CRC_TRANSFER_IDLE, xferState );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCrc_TransferCpltCnt );
}

/**
 * \brief   DMA transfer error is reported for data not accessible by DMA.
 *
 * \details Starts non-blocking calculation from CCM data RAM (not connected to
 *          DMA2), then calculates the check string by DMA again after reset.
 *
 * \par Expected results
 * - State CRC_TRANSFER_ERROR, transfer error callback called once with
 *   CRC_XFER_ERROR_DMA_TRANSFER, no transfer complete callback.
 * - Result read returns error until Crc_Set_Reset(), next transfer succeeds.
 * - Ignored on MCUs without CCM data RAM (test function exists always - the
 *   runner collects test functions without preprocessor).
 */
void It_Crc_Set_TransferStart_CcmData_ReportsTransferError( void )
{
#if defined(CCMDATARAM_BASE)
    const crc_DataByte_t * const ccmData = (const crc_DataByte_t *)CCMDATARAM_BASE;
    crc_Data_t                   value   = 0u;

    It_Crc_Init_Algorithm( &itCrc_Crc32Mpeg2Words, CRC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    (void)Crc_Set_TransferStart( IT_CRC_ID, ccmData, IT_CRC_DMA_DATA_CNT );

    It_Crc_Wait_TransferEnd( CRC_TRANSFER_ERROR );

    TEST_ASSERT_EQUAL_UINT32( 1u, itCrc_ErrorCnt );
    TEST_ASSERT_EQUAL( CRC_XFER_ERROR_DMA_TRANSFER, itCrc_ErrorId );
    TEST_ASSERT_EQUAL_UINT32( 0u, itCrc_TransferCpltCnt );
    TEST_ASSERT_EQUAL( CRC_REQUEST_ERROR, Crc_Get_Data( IT_CRC_ID, &value ) );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_Reset( IT_CRC_ID ) );
    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Set_TransferStart( IT_CRC_ID, itCrc_CheckData, IT_CRC_CHECK_WORD_CNT ) );

    It_Crc_Wait_TransferEnd( CRC_TRANSFER_IDLE );

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_Data( IT_CRC_ID, &value ) );
    TEST_ASSERT_EQUAL_HEX32( IT_CRC_MPEG2_WORDS_CHECK, value );
#else
    TEST_IGNORE_MESSAGE( "MCU has no CCM data RAM" );
#endif /* CCMDATARAM_BASE */
}

/* ========================== HELPER FUNCTIONS ============================== */

/**
 * \brief Initializes the module with the algorithm parameters.
 *
 * \param algorithm [in]: Algorithm parameters
 * \param dmaState  [in]: Non-blocking calculation state (DMA2 stream 0, callbacks)
 */
static void It_Crc_Init_Algorithm( const it_CrcAlgorithm_t * const algorithm, crc_FunctionState_t dmaState )
{
    crc_Config_t config;

    TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_DefaultConfig( &config ) );

    config.PeriphId         = IT_CRC_ID;
    config.InitValue        = algorithm->InitValue;
    config.InputDataOrder   = algorithm->InputOrder;
    config.OutputDataOrder  = algorithm->OutputOrder;
    config.InputDataSize    = algorithm->InputSize;
    config.DmaState         = dmaState;
    config.DmaPeriphId      = CRC_DMA_PERIPH_2;
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
 *
 * \param expectedState [in]: Expected state at the end of the transfer
 */
static void It_Crc_Wait_TransferEnd( crc_TransferState_t expectedState )
{
    crc_TransferState_t xferState = CRC_TRANSFER_BUSY;

    for( uint32_t loopIdx = 0u; ( IT_CRC_WAIT_LOOPS > loopIdx ) && ( CRC_TRANSFER_BUSY == xferState ); loopIdx++ )
    {
        TEST_ASSERT_EQUAL( CRC_REQUEST_OK, Crc_Get_TransferState( IT_CRC_ID, &xferState ) );
    }

    TEST_ASSERT_EQUAL( expectedState, xferState );
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
 * \brief Transfer error callback - counts calls and stores reported error.
 *
 * \param errorId [in]: Reported error
 */
static void It_Crc_TransferErrorCallback( crc_XferErrorId_t errorId )
{
    itCrc_ErrorId = errorId;
    itCrc_ErrorCnt++;
}
