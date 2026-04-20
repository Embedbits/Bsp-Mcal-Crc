/*
 *    Mr.Nobody, COPYRIGHT (c) 2021
 *    ALL RIGHTS RESERVED
 *
 */

/**
 * \file Crc.h
 * \ingroup Crc
 * \brief Crc module common functionality
 *
 */
/* ============================== INCLUDES ================================== */
#include "Crc.h"                            /* Self include                   */
#include "Crc_Types.h"                      /* Module types definitions       */
#include "Crc_Port.h"                       /* Module public functionality    */
#include "Rcc_Port.h"                       /* RCC functionality              */
#include "Gpdma_Port.h"                     /* DMA functionality              */
#include "Stm32.h"                          /* MCU common functionality       */
#include "Stm32_crc.h"                      /* CRC peripheral functionality   */
/* ============================== TYPEDEFS ================================== */

/** Enumeration used to signal execution state of peripheral */
typedef enum
{
    CRC_EXEC_STATE_IDLE = 0u, /**< Peripheral is idle (not processing data)       */
    CRC_EXEC_STATE_BUSY,      /**< Peripheral is busy (processing data)           */
    CRC_EXEC_STATE_PAUSED,    /**< Peripheral is paused (processing is suspended) */
    CRC_EXEC_STATE_ERROR,     /**< Peripheral is in error state                   */
    CRC_EXEC_STATE_CNT
}   crc_ExecState_t;


typedef void crc_FeedData_t( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId );
typedef uint32_t crc_ReadData_t( CRC_TypeDef *CRCx );

typedef struct
{
    crc_FeedData_t *FeedData; /**< Function used to feed data into CRC peripheral */
    crc_ReadData_t *ReadData; /**< Function used to read data from CRC peripheral */
}   crc_DataFunc_t;


/** Structure type used for USART/UART configuration array */
typedef struct
{
    CRC_TypeDef*      CrcReg; /**< CRC configuration register */
    rcc_PeriphId_t    CrcRcc; /**< CRC RCC configuration ID   */
}   crc_ConfigStruct_t;


typedef struct
{
    crc_PeriphId_t         CrcPeriphId;            /**< Peripheral ID       */
    crc_TransferStyle_t    TransferStyle;          /**< Data transfer style */
    crc_ExecState_t        ExecState;              /**< Execution state     */

    crc_BlockSize_t        InBlockSize;            /**< Size of input data block used for CRC calculation. */
    crc_BlockRep_t         InBlockRepetitionCount; /**< Count of block repetitions.                        */
    crc_DataSize_t         InDataSize;             /**< Bit size of data written into CRC unit.            */
    uint32_t               InDataAddr;             /**< Pointer to input data array.                       */
    uint32_t               OutDataAddr;            /**< Pointer to output data variable.                   */
    crc_BlockSize_t        BurstProcessSize;       /**< Size of single block processed during one access.  */

    uint32_t               PolynomialValue;        /**< CRC polynomial value.                 */
    crc_PolySize_t         PolynomialSize;         /**< CRC calculation polynomial data size. */
    uint32_t               InitialValue;           /**< CRC calculation initial value.        */

    gpdma_PeriphId_t       DmaPeriphId;            /**< DMA peripheral identification */
    gpdma_ChannelId_t      DmaChannelId;           /**< DMA channel identification    */

    crc_CalcCompleteIsr_t *CalcCompleteIsr;        /**< Calculation complete callback */
    crc_ErrorIsr_t        *ErrorIsr;               /**< Error ISR callback            */

    crc_BlockSize_t        ProcessingId;           /**< Currently processing data index */
}   crc_RuntimeData_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static void Crc_TransferCompleteIsrHandler( void );
static void Crc_ErrorIsrHandler( gpdma_ErrorMaskId_t errorId );

static void Crc_FeedData_8b( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId );
static void Crc_FeedData_16b( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId );
static void Crc_FeedData_32b( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId );

static uint32_t Crc_ReadData_8b( CRC_TypeDef *CRCx );
static uint32_t Crc_ReadData_16b( CRC_TypeDef *CRCx );
static uint32_t Crc_ReadData_32b( CRC_TypeDef *CRCx );

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define CRC_MAJOR_VERSION           ( 1u )

/** Value of minor version of SW module */
#define CRC_MINOR_VERSION           ( 0u )

/** Value of patch version of SW module */
#define CRC_PATCH_VERSION           ( 0u )

/** Maximum wait time for configuration request confirmation */
#define CRC_TIMEOUT_RAW             ( 0x84FCB )

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Runtime data for each peripheral instance */
static volatile crc_RuntimeData_t       crc_RuntimeData[ CRC_PERIPH_CNT ] =
{
  { .CrcPeriphId            = CRC_PERIPH_1 ,
    .TransferStyle          = CRC_TRANSFER_BLOCKING ,
    .ExecState              = CRC_EXEC_STATE_IDLE,
    .InBlockSize            = 0u,
    .InBlockRepetitionCount = 0u,
    .InDataAddr             = 0u,
    .OutDataAddr            = 0u,
    .BurstProcessSize       = 0u,
    .PolynomialValue        = 0u,
    .PolynomialSize         = CRC_POLY_SIZE_32,
    .InitialValue           = 0u,
    .DmaPeriphId            = GPDMA_PERIPH_CNT,
    .DmaChannelId           = GPDMA_CHANNEL_CNT,
    .CalcCompleteIsr        = CRC_NULL_PTR,
    .ErrorIsr               = CRC_NULL_PTR,
    .ProcessingId           = 0u },
};


/** Configuration of each peripheral instance */
static crc_ConfigStruct_t const         crc_PeriphConf[ CRC_PERIPH_CNT ] =
{
  { .CrcReg = CRC, .CrcRcc = RCC_PERIPH_CRC },
};


static crc_DataFunc_t const             crc_DataFunc[ CRC_DATA_CNT ] =
{
  { .FeedData = Crc_FeedData_8b , .ReadData = Crc_ReadData_8b  },
  { .FeedData = Crc_FeedData_16b, .ReadData = Crc_ReadData_16b },
  { .FeedData = Crc_FeedData_32b, .ReadData = Crc_ReadData_32b },
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Returns module SW version
 *
 * \return Module SW version
 */
crc_ModuleVersion_t Crc_Get_ModuleVersion( void )
{
    crc_ModuleVersion_t retVersion;

    retVersion.Major = CRC_MAJOR_VERSION;
    retVersion.Minor = CRC_MINOR_VERSION;
    retVersion.Patch = CRC_PATCH_VERSION;

    return (retVersion);
}


/**
 * \brief Initialization of Random Number Generator (CRC) peripheral
 *
 * \note The clock for CRC shall be configured to 48MHz. Otherwise error will
 *       be raised.
 *
 * \param crcConfig [in]: Peripheral configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Init( crc_Config_t * const crcConfig )
{
    crc_RequestState_t retState = CRC_REQUEST_ERROR;

    if( CRC_NULL_PTR != crcConfig )
    {
        /* Activate peripheral clock */
        rcc_RequestState_t rccState = Rcc_Set_PeriphActive( crc_PeriphConf[ crcConfig->PeriphId ].CrcRcc );

        if( RCC_REQUEST_OK != rccState )
        {
            /* Clock was not successfully activated */
            retState = CRC_REQUEST_ERROR;
        }
        else
        {
            retState = CRC_REQUEST_OK;
        }

        /* Reset peripheral */
        LL_CRC_ResetCRCCalculationUnit( crc_PeriphConf[ crcConfig->PeriphId ].CrcReg );

        /*----------------------- Configure peripheral -----------------------*/

        if( CRC_REQUEST_OK == retState )
        {
            retState = Crc_Set_OutputDataOrder( crcConfig->PeriphId, crcConfig->OutputDataOrder );
        }

        if( CRC_REQUEST_OK == retState )
        {
            retState = Crc_Set_InputDataOrder( crcConfig->PeriphId, crcConfig->InputDataOrder );
        }

        if( CRC_REQUEST_OK == retState )
        {
            retState = Crc_Set_InitValue( crcConfig->PeriphId, crcConfig->InitialValue );
        }

        if( CRC_REQUEST_OK == retState )
        {
            retState = Crc_Set_Polynomial( crcConfig->PeriphId, crcConfig->PolynomialValue, crcConfig->PolynomialSize );
        }

        /*--------------------- Configure data transfer ----------------------*/

        crc_RuntimeData[ crcConfig->PeriphId ].TransferStyle = crcConfig->TransferStyle;



        /* Configure DMA (if used) */
        if( ( CRC_TRANSFER_DMA == crcConfig->TransferStyle ) &&
            ( CRC_REQUEST_OK   == retState                 )    )
        {
            static gpdma_XferList_t dmaXferList[ 2u ];
            gpdma_TransferConfig_t  dmaTransferConfig[ 2u ];
            gpdma_ConfigStruct_t    dmaConfig;

            dmaConfig.PeriphId              = crcConfig->DmaConfig.DmaPeriphId;
            dmaConfig.ChannelId             = crcConfig->DmaConfig.DmaChannelId;
            dmaConfig.ChannelPrio           = crcConfig->DmaConfig.DmaChannelPrio;

            dmaConfig.TransferExecMode      = GPDMA_XFER_EXEC_CONTINUOUS;
            dmaConfig.TransferConfig        = dmaTransferConfig;

            dmaConfig.XferListAccessMode    = GPDMA_TRANSFER_LIST_ACCESS_APPEND;
            dmaConfig.XferList              = dmaXferList;
            dmaConfig.TransferLockState     = crcConfig->DmaConfig.DmaTransferLockState;

            dmaConfig.TransferCompleteIsr   = Crc_TransferCompleteIsrHandler;
            dmaConfig.HalfTransferIsr       = GPDMA_NULL_PTR;
            dmaConfig.ErrorIsr              = GPDMA_NULL_PTR;
            dmaConfig.ErrorMask             = GPDMA_ERROR_ALL;


            gpdma_DataSize_t dataSize  = GPDMA_DATA_SIZE_8BITS;

            if( CRC_DATA_8B == crcConfig->InDataSize )
            {
                dataSize = GPDMA_DATA_SIZE_8BITS;
            }
            else if( CRC_DATA_16B == crcConfig->InDataSize )
            {
                dataSize = GPDMA_DATA_SIZE_16BITS;
            }
            else
            {
                dataSize = GPDMA_DATA_SIZE_32BITS;
            }

            if( 0u != crcConfig->InDataAddr )
            {
                /* If user provided output data pointer, configure transfer
                 * to write data CRC value into peripheral. */

                dmaTransferConfig[ dmaConfig.TransfersCount ].Direction                   = GPDMA_DIR_MEMORY_TO_MEMORY;
                dmaTransferConfig[ dmaConfig.TransfersCount ].EventMode                   = GPDMA_TRANSFER_EVENT_BLOCK;
                dmaTransferConfig[ dmaConfig.TransfersCount ].XferListExecMode            = GPDMA_XFER_LIST_EXEC_CYCLIC_ALL;

                dmaTransferConfig[ dmaConfig.TransfersCount ].TriggerType                 = crcConfig->DmaConfig.TriggerType;
                dmaTransferConfig[ dmaConfig.TransfersCount ].TriggerSource               = crcConfig->DmaConfig.TriggerSource;
                dmaTransferConfig[ dmaConfig.TransfersCount ].TriggerMode                 = crcConfig->DmaConfig.TriggerMode;

                dmaTransferConfig[ dmaConfig.TransfersCount ].RequestSource               = 0u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].RequestMode                 = GPDMA_PERIPH_REQ_BLOCK;

                dmaTransferConfig[ dmaConfig.TransfersCount ].BlockSize                   = crcConfig->InBlockSize;
                dmaTransferConfig[ dmaConfig.TransfersCount ].BlockRepetitionCount        = crcConfig->InBlockRepetitionCount;

                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceAddr                  = (gpdma_SrcAddr_t)( crcConfig->InDataAddr );
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceDataSize              = dataSize;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceBurstLength           = 1u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceAddrMode              = GPDMA_ADDR_INCREMENT;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourcePortId                = GPDMA_PORT_DEFAULT;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceDataOp                = GPDMA_SRC_DATA_PRESERVE;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceBlockOffset2D         = 0u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceRepBlockOffset2D      = 0u;

                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationAddr             = (gpdma_SrcAddr_t)( &crc_PeriphConf[ crcConfig->PeriphId ].CrcReg->DR );
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationDataSize         = dataSize;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationBurstLength      = 1u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationAddrMode         = GPDMA_ADDR_STATIC;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationPortId           = GPDMA_PORT_DEFAULT;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationDataOp           = GPDMA_DEST_DATA_PRESERVE;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationBlockOffset2D    = crcConfig->InBlockSize;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationRepBlockOffset2D = crcConfig->InBlockRepetitionCount;

                dmaConfig.TransfersCount ++;
            }
            else
            {
                /* User did not provide input data pointer, so no need to transfer
                 * data to peripheral. */
            }

            if ( 0u != crcConfig->OutDataAddr)
            {
                /* If user provided output data pointer, configure transfer
                 * to read calculated CRC value from peripheral. */

                dmaTransferConfig[ dmaConfig.TransfersCount ].Direction                   = GPDMA_DIR_MEMORY_TO_MEMORY;
                dmaTransferConfig[ dmaConfig.TransfersCount ].EventMode                   = GPDMA_TRANSFER_EVENT_BLOCK;
                dmaTransferConfig[ dmaConfig.TransfersCount ].XferListExecMode            = GPDMA_XFER_LIST_EXEC_CYCLIC_ALL;

                dmaTransferConfig[ dmaConfig.TransfersCount ].TriggerType                 = GPDMA_TRG_NOT_USED;
                dmaTransferConfig[ dmaConfig.TransfersCount ].TriggerSource               = 0u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].TriggerMode                 = GPDMA_TRIGGER_BLOCK;

                dmaTransferConfig[ dmaConfig.TransfersCount ].RequestSource               = 0u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].RequestMode                 = GPDMA_PERIPH_REQ_BLOCK;

                dmaTransferConfig[ dmaConfig.TransfersCount ].BlockSize                   = 1u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].BlockRepetitionCount        = 0u;

                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceAddr                  = (uint32_t)( &crc_PeriphConf[ crcConfig->PeriphId ].CrcReg->DR );
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceDataSize              = GPDMA_DATA_SIZE_32BITS;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceBurstLength           = 1u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceAddrMode              = GPDMA_ADDR_STATIC;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourcePortId                = GPDMA_PORT_DEFAULT;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceDataOp                = GPDMA_SRC_DATA_PRESERVE;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceBlockOffset2D         = 0u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].SourceRepBlockOffset2D      = 0u;

                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationAddr             = (uint32_t)( crcConfig->OutDataAddr );
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationDataSize         = GPDMA_DATA_SIZE_32BITS;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationBurstLength      = 1u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationAddrMode         = GPDMA_ADDR_STATIC;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationPortId           = GPDMA_PORT_DEFAULT;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationDataOp           = GPDMA_DEST_DATA_PRESERVE;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationBlockOffset2D    = 0u;
                dmaTransferConfig[ dmaConfig.TransfersCount ].DestinationRepBlockOffset2D = 0u;

                dmaConfig.TransfersCount ++;
            }
            else
            {
                /* User did not provide output data pointer, so no need to transfer
                 * calculated CRC value by DMA. */
            }

            if( 0u != dmaConfig.TransfersCount )
            {
                gpdma_RequestState_t gpdmaRetState = Gpdma_Init( &dmaConfig );

                if( GPDMA_REQUEST_OK == gpdmaRetState )
                {
                    crc_RuntimeData[ crcConfig->PeriphId ].CalcCompleteIsr = crcConfig->CalcCompleteIsr;
                }
                else
                {
                    retState = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                /* No DMA transfer configured. */
            }
        }
        else if( ( CRC_TRANSFER_BLOCKING == crcConfig->TransferStyle ) &&
                 ( CRC_REQUEST_OK        == retState                 )    )
        {

        }
        else
        {
            /* CRC is able to operate only in blocking and DMA mode */
        }
    }
    else
    {
        /* Null pointer assigned */
        retState = CRC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Deinitializes module CRC
 *
 * This function shall call every necessary sub-module deinitialization function
 * and free all the resources allocated by the module. In case of failure, the
 * function shall handle it by itself and shall not be transferred to AppMain
 * layer.
 *
 * \param crcConfig [in]: Peripheral configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Deinit( crc_Config_t * const crcConfig )
{
    crc_RequestState_t retState = CRC_REQUEST_ERROR;

    rcc_RequestState_t rccRequest = Rcc_Set_PeriphInactive( crc_PeriphConf[ crcConfig->PeriphId ].CrcRcc );

    crc_RuntimeData[ crcConfig->PeriphId ].ExecState = CRC_EXEC_STATE_IDLE;

    if( RCC_REQUEST_OK != rccRequest )
    {
        retState = CRC_REQUEST_ERROR;
    }
    else
    {
        retState = CRC_REQUEST_OK;
    }

    return ( retState );
}


/**
 * \brief Main task of module Crc
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's
 * requirements.
 */
void Crc_Task( void )
{
    for( uint32_t periphId = 0u; CRC_PERIPH_CNT > periphId; periphId++ )
    {
        if( CRC_TRANSFER_BLOCKING == crc_RuntimeData[ periphId ].TransferStyle )
        {
            if( CRC_EXEC_STATE_BUSY == crc_RuntimeData[ periphId ].ExecState )
            {
                if( 0u != crc_RuntimeData[ periphId ].InDataAddr )
                {
                    crc_BlockSize_t burstProcessingCount = 0u;

                    if( crc_RuntimeData[ periphId ].InBlockSize > crc_RuntimeData[ periphId ].ProcessingId )
                    {
                        burstProcessingCount = crc_RuntimeData[ periphId ].InBlockSize - crc_RuntimeData[ periphId ].ProcessingId;
                    }
                    else
                    {
                        /* All data processed. */
                        burstProcessingCount = 0u;
                    }

                    if( burstProcessingCount > crc_RuntimeData[ periphId ].BurstProcessSize )
                    {
                        burstProcessingCount = crc_RuntimeData[ periphId ].BurstProcessSize;
                    }
                    else
                    {
                        /* Burst processing size is larger than remaining data to
                         * be processed, so process all remaining data. */
                    }

                    for( crc_BlockSize_t processed = 0u; burstProcessingCount > processed; processed++ )
                    {
                        /* Process data */
                        crc_DataFunc[ crc_RuntimeData[ periphId ].InDataSize ].FeedData( crc_PeriphConf[ periphId ].CrcReg,
                                                                                         crc_RuntimeData[ periphId ].InDataAddr,
                                                                                         crc_RuntimeData[ periphId ].ProcessingId );

                        crc_RuntimeData[ periphId ].ProcessingId ++;
                    }

                    if( crc_RuntimeData[ periphId ].ProcessingId < crc_RuntimeData[ periphId ].InBlockSize )
                    {
                        /* More data to be processed, so process with other peripherals. */
                        break;
                    }
                    else
                    {
                        crc_RuntimeData[ periphId ].ExecState = CRC_EXEC_STATE_IDLE;

                        /* All data processed, so continue to finalize calculation. */
                        uint32_t crcValue = crc_DataFunc[ crc_RuntimeData[ periphId ].InDataSize ].ReadData( crc_PeriphConf[ periphId ].CrcReg );

                        if( 0u != crc_RuntimeData[ periphId ].OutDataAddr )
                        {
                            *( (uint32_t *) crc_RuntimeData[ periphId ].OutDataAddr ) = crcValue;
                        }
                        else
                        {
                            /* User did not provide output data pointer, so no need to
                             * store calculated CRC value. */
                        }

                        crc_RuntimeData[ periphId ].ExecState = CRC_EXEC_STATE_IDLE;

                        if( CRC_NULL_PTR != crc_RuntimeData[ periphId ].CalcCompleteIsr )
                        {
                            crc_RuntimeData[ periphId ].CalcCompleteIsr( crcValue );
                        }
                        else
                        {
                            /* User did not provide calculation complete callback,
                             * so no need to call it. */
                        }
                    }
                }
                else
                {
                    /* User did not provide input data pointer, so no need to
                     * process data. */
                }
            }
            else
            {
                /* Peripheral is not active, so nothing to be done in task. */
            }
        }
        else
        {
            /* Blocking mode not used for current peripheral. */
        }
    }
}


/**
 * \brief Loads default values for Random Number Generator (CRC) peripheral
 *
 * \param crcConfig [out]: Pointer to configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_DefaultConfig( crc_Config_t * const crcConfig )
{
    crc_RequestState_t retState = CRC_REQUEST_ERROR;

    if( CRC_NULL_PTR != crcConfig )
    {
        crcConfig->PeriphId               = CRC_PERIPH_1;
        crcConfig->InputDataOrder         = CRC_IN_DATA_NORMAL;
        crcConfig->OutputDataOrder        = CRC_DATA_NORMAL;
        crcConfig->PolynomialValue        = CRC_DEFAULT_POLY;
        crcConfig->PolynomialSize         = CRC_POLY_SIZE_32;
        crcConfig->InitialValue           = CRC_DEFAULT_INIT_VAL;
        crcConfig->TransferStyle          = CRC_TRANSFER_DMA;
        crcConfig->InDataSize             = CRC_DATA_32B;
        crcConfig->InBlockSize            = 1u;
        crcConfig->InBlockRepetitionCount = 0u;
        crcConfig->BurstProcessSize       = 1u;
        crcConfig->InDataAddr             = 0u;
        crcConfig->ErrorIsr               = CRC_NULL_PTR;
        crcConfig->CalcCompleteIsr        = CRC_NULL_PTR;

        crcConfig->DmaConfig.DmaPeriphId          = GPDMA_PERIPH_1;
        crcConfig->DmaConfig.DmaChannelId         = GPDMA_CHANNEL_1;
        crcConfig->DmaConfig.DmaChannelPrio       = GPDMA_PRIORITY_LOW;
        crcConfig->DmaConfig.IrqPrio              = 5u;
        crcConfig->DmaConfig.TriggerType          = GPDMA_TRG_NOT_USED;
        crcConfig->DmaConfig.TriggerSource        = 0u;
        crcConfig->DmaConfig.TriggerMode          = GPDMA_TRIGGER_BLOCK;
        crcConfig->DmaConfig.DmaTransferLockState = GPDMA_TRANSFER_LIST_UNLOCKED;
        crcConfig->DmaConfig.XferListExecMode     = GPDMA_XFER_LIST_EXEC_CYCLIC_ALL;
        crcConfig->DmaConfig.EventMode            = GPDMA_TRANSFER_EVENT_BLOCK;
    }
    else
    {
        retState = CRC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activates Cyclic Redundant Calculation (CRC) peripheral data processing.
 *
 * This function activates peripheral data processing.
 *  - If blocking mode is used, the function will start this processing and user
 *    must cyclically call \ref Crc_Task().
 *  - If DMA is used for data transfer, the function will start the transfer.
 *
 * \note If DMA is used, user must ensure that DMA is properly configured
 *       (channel, priority, etc.) before calling this function.
 *
 * \note If blocking mode is used, user must ensure that \ref Crc_Task() is
 *       called cyclically in the main loop of the application or the task
 *       scheduler.
 *
 * \note The polynomial value and size, initial value, input and output data
 *       order will be checked before starting the processing. If any of
 *       these parameters are not properly configured, the system will set
 *       the user configuration and then start the processing.
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ExecActive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t  dmaState     = GPDMA_REQUEST_ERROR;
            gpdma_FunctionState_t channelState = GPDMA_FUNCTION_INACTIVE;

            dmaState = Gpdma_Get_ChannelState( crc_RuntimeData[crcId].DmaPeriphId,
                                               crc_RuntimeData[crcId].DmaChannelId,
                                               &channelState );

            if( ( GPDMA_REQUEST_OK        == dmaState     ) &&
                ( GPDMA_FUNCTION_INACTIVE == channelState )    )
            {
                dmaState = Gpdma_Set_ChannelActive( crc_RuntimeData[crcId].DmaPeriphId,
                                                    crc_RuntimeData[crcId].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_BUSY;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_ERROR;
                }
            }
            else
            {
                /* DMA channel is not active, so channel cannot be stopped */
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else if( CRC_TRANSFER_BLOCKING == crc_RuntimeData[crcId].TransferStyle )
        {
            if( CRC_EXEC_STATE_IDLE == crc_RuntimeData[crcId].ExecState )
            {
                /* In blocking mode, the processing is done in task function. */
                crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_BUSY;

                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Unsupported transfer style */
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/**
 * \brief Deactivates Cyclic Redundant Calculation (CRC) peripheral data processing.
 *
 * This function deactivates peripheral data processing.
 * - If blocking mode is used, the function will stop this processing.
 * - If DMA is used for data transfer, the function will stop the transfer.
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ExecInactive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t  dmaState     = GPDMA_REQUEST_ERROR;
            gpdma_FunctionState_t channelState = GPDMA_FUNCTION_INACTIVE;

            dmaState = Gpdma_Get_ChannelState( crc_RuntimeData[crcId].DmaPeriphId,
                                               crc_RuntimeData[crcId].DmaChannelId,
                                               &channelState );

            if( ( GPDMA_REQUEST_OK      == dmaState     ) &&
                ( GPDMA_FUNCTION_ACTIVE == channelState )    )
            {
                dmaState = Gpdma_Set_ChannelInactive( crc_RuntimeData[crcId].DmaPeriphId,
                                                      crc_RuntimeData[crcId].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_IDLE;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_ERROR;
                }
            }
            else
            {
                /* DMA channel is not active, so channel cannot be stopped */
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else if( CRC_TRANSFER_BLOCKING == crc_RuntimeData[crcId].TransferStyle )
        {
            if( CRC_EXEC_STATE_BUSY == crc_RuntimeData[crcId].ExecState )
            {
                /* In blocking mode, the processing is done in task function. */
                crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_IDLE;

                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Unsupported transfer style */
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/**
 * \brief Returns current state of peripheral data processing.
 *
 * This function returns current state of peripheral data processing.
 *
 * \param crcId  [in]: Peripheral identification
 * \param state [out]: Pointer to variable where current state will be stored
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_ExecState( crc_PeriphId_t crcId, crc_FunctionState_t * const state )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId ) &&
        ( CRC_NULL_PTR  != state )    )
    {
        if( CRC_EXEC_STATE_BUSY == crc_RuntimeData[ crcId ].ExecState )
        {
            *state = CRC_FUNCTION_ACTIVE;
        }
        else
        {
            *state = CRC_FUNCTION_INACTIVE;
        }

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Pauses Cyclic Redundant Calculation (CRC) peripheral data processing.
 *
 * This function pauses peripheral data processing. If the processing is not
 * active, error will be raised.
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ExecPauseActive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t  dmaState     = GPDMA_REQUEST_ERROR;
            gpdma_FunctionState_t channelState = GPDMA_FUNCTION_INACTIVE;

            dmaState = Gpdma_Get_PauseState( crc_RuntimeData[crcId].DmaPeriphId,
                                             crc_RuntimeData[crcId].DmaChannelId,
                                             &channelState );

            if( ( GPDMA_REQUEST_OK        == dmaState     ) &&
                ( GPDMA_FUNCTION_INACTIVE == channelState )    )
            {
                dmaState = Gpdma_Set_PauseActive( crc_RuntimeData[crcId].DmaPeriphId,
                                                  crc_RuntimeData[crcId].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_PAUSED;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_ERROR;
                }
            }
            else
            {
                /* DMA channel is not active, so channel cannot be stopped */
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else if( CRC_TRANSFER_BLOCKING == crc_RuntimeData[crcId].TransferStyle )
        {
            if( CRC_EXEC_STATE_BUSY == crc_RuntimeData[crcId].ExecState )
            {
                /* In blocking mode, the processing is done in task function. */
                crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_PAUSED;

                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Unsupported transfer style */
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Resumes Cyclic Redundant Calculation (CRC) peripheral data processing.
 *
 * This function resumes peripheral data processing. If the processing is not
 * in pause state, error will be raised.
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ExecPauseInactive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t  dmaState     = GPDMA_REQUEST_ERROR;
            gpdma_FunctionState_t channelState = GPDMA_FUNCTION_INACTIVE;

            dmaState = Gpdma_Get_PauseState( crc_RuntimeData[crcId].DmaPeriphId,
                                             crc_RuntimeData[crcId].DmaChannelId,
                                             &channelState );

            if( ( GPDMA_REQUEST_OK      == dmaState     ) &&
                ( GPDMA_FUNCTION_ACTIVE == channelState )    )
            {
                dmaState = Gpdma_Set_PauseInactive( crc_RuntimeData[crcId].DmaPeriphId,
                                                    crc_RuntimeData[crcId].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_BUSY;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;

                    crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_ERROR;
                }
            }
            else
            {
                /* DMA channel is not active, so channel cannot be stopped */
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else if( CRC_TRANSFER_BLOCKING == crc_RuntimeData[crcId].TransferStyle )
        {
            if( CRC_EXEC_STATE_PAUSED == crc_RuntimeData[crcId].ExecState )
            {
                /* In blocking mode, the processing is done in task function. */
                crc_RuntimeData[crcId].ExecState = CRC_EXEC_STATE_BUSY;

                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Unsupported transfer style */
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Returns current pause state of peripheral data processing.
 *
 * This function returns current pause state of peripheral data processing.
 *
 * \param crcId  [in]: Peripheral identification
 * \param state [out]: Pointer to variable where current pause state will be stored
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_ExecPauseState( crc_PeriphId_t crcId, crc_FunctionState_t * const state )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId ) &&
        ( CRC_NULL_PTR  != state )    )
    {
        if( CRC_EXEC_STATE_PAUSED == crc_RuntimeData[ crcId ].ExecState )
        {
            *state = CRC_FUNCTION_ACTIVE;
        }
        else
        {
            *state = CRC_FUNCTION_INACTIVE;
        }

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return (retValue);
}


/**
 * \brief Returns actual data from output register
 *
 * \return Value of generated data
 */
crc_Data_t Crc_Get_Data( crc_PeriphId_t crcId )
{
    crc_Data_t retData = 0u;

    if( CRC_PERIPH_CNT > crcId )
    {
        retData = LL_CRC_ReadData32( crc_PeriphConf[ crcId ].CrcReg );
    }
    else
    {
        retData = 0u;
    }

    return ( retData );
}


/** \brief Sets output data order (normal/inverted)
 *
 * \param crcId     [in]: Peripheral identification
 * \param dataOrder [in]: Data order configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_OutputDataOrder( crc_PeriphId_t crcId, crc_OutDataOrder_t dataOrder )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_DATA_NORMAL == dataOrder )
        {
            LL_CRC_SetOutputDataReverseMode( crc_PeriphConf[crcId].CrcReg, LL_CRC_OUTDATA_REVERSE_NONE );
        }
        else
        {
            LL_CRC_SetOutputDataReverseMode( crc_PeriphConf[crcId].CrcReg, LL_CRC_OUTDATA_REVERSE_BIT );
        }

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Gets output data order (normal/inverted)
 *
 * \param crcId     [in]: Peripheral identification
 * \param dataOrder [out]: Data order configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_OutputDataOrder( crc_PeriphId_t crcId, crc_OutDataOrder_t * const dataOrder )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        uint32_t regValue = LL_CRC_GetOutputDataReverseMode( crc_PeriphConf[crcId].CrcReg );

        if( LL_CRC_OUTDATA_REVERSE_NONE == regValue )
        {
            *dataOrder = CRC_DATA_NORMAL;
        }
        else
        {
            *dataOrder = CRC_DATA_INVERTED;
        }

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets input data order (normal/inverted)
 *
 * \param crcId     [in]: Peripheral identification
 * \param dataOrder [in]: Data order configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_InputDataOrder( crc_PeriphId_t crcId, crc_InDataOrder_t dataOrder )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_IN_DATA_NORMAL == dataOrder )
        {
            LL_CRC_SetInputDataReverseMode( crc_PeriphConf[crcId].CrcReg, LL_CRC_OUTDATA_REVERSE_NONE );
        }
        else if( CRC_IN_DATA_8B_INVERTED == dataOrder )
        {
            LL_CRC_SetInputDataReverseMode( crc_PeriphConf[crcId].CrcReg, LL_CRC_INDATA_REVERSE_BYTE );
        }
        else if( CRC_IN_DATA_16B_INVERTED == dataOrder )
        {
            LL_CRC_SetInputDataReverseMode( crc_PeriphConf[crcId].CrcReg, LL_CRC_INDATA_REVERSE_HALFWORD );
        }
        else
        {
            LL_CRC_SetInputDataReverseMode( crc_PeriphConf[crcId].CrcReg, LL_CRC_INDATA_REVERSE_WORD );
        }

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Gets input data order (normal/inverted)
 *
 * \param crcId     [in]: Peripheral identification
 * \param dataOrder [out]: Data order configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_InputDataOrder( crc_PeriphId_t crcId, crc_InDataOrder_t * const dataOrder )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId     ) &&
        ( CRC_NULL_PTR  != dataOrder )    )
    {
        uint32_t regValue = LL_CRC_GetInputDataReverseMode( crc_PeriphConf[crcId].CrcReg );

        if( LL_CRC_INDATA_REVERSE_NONE == regValue )
        {
            *dataOrder = CRC_IN_DATA_NORMAL;
        }
        else if ( LL_CRC_INDATA_REVERSE_BYTE == regValue)
        {
            *dataOrder = CRC_IN_DATA_8B_INVERTED;
        }
        else if ( LL_CRC_INDATA_REVERSE_HALFWORD == regValue)
        {
            *dataOrder = CRC_IN_DATA_16B_INVERTED;
        }
        else
        {
            *dataOrder = CRC_IN_DATA_32B_INVERTED;
        }

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets polynomial value and size
 *
 * \param crcId      [in]: Peripheral identification
 * \param polynomial [in]: Polynomial value
 * \param polySize   [in]: Polynomial size configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_Polynomial( crc_PeriphId_t crcId, crc_Data_t polynomial, crc_PolySize_t polySize )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if (CRC_POLY_SIZE_32 == polySize)
        {
            /* 32bits polynomial size is set by default after reset */
            LL_CRC_SetPolynomialSize( crc_PeriphConf[crcId].CrcReg, LL_CRC_POLYLENGTH_32B );
        }
        else if (CRC_POLY_SIZE_16 == polySize)
        {
            /* 16bits polynomial size */
            LL_CRC_SetPolynomialSize( crc_PeriphConf[crcId].CrcReg, LL_CRC_POLYLENGTH_16B );
        }
        else if (CRC_POLY_SIZE_8 == polySize)
        {
            /* 8bits polynomial size */
            LL_CRC_SetPolynomialSize( crc_PeriphConf[crcId].CrcReg, LL_CRC_POLYLENGTH_8B );
        }
        else
        {
            /* 7bits polynomial size */
            LL_CRC_SetPolynomialSize( crc_PeriphConf[crcId].CrcReg, LL_CRC_POLYLENGTH_7B );
        }

        LL_CRC_SetPolynomialCoef( crc_PeriphConf[crcId].CrcReg, polynomial );

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Gets polynomial value and size
 *
 * \param crcId      [in]: Peripheral identification
 * \param polynomial [out]: Polynomial value
 * \param polySize   [out]: Polynomial size configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_Polynomial( crc_PeriphId_t crcId, crc_Data_t * const polynomial, crc_PolySize_t * const polySize )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_NULL_PTR != polynomial )
        {
            *polynomial = LL_CRC_GetPolynomialCoef( crc_PeriphConf[crcId].CrcReg );

            retValue = CRC_REQUEST_OK;
        }
        else
        {
            /* Null pointer assigned */
        }

        if( CRC_NULL_PTR != polySize )
        {
            uint32_t regValue = LL_CRC_GetPolynomialSize( crc_PeriphConf[crcId].CrcReg );

            if ( LL_CRC_POLYLENGTH_32B == regValue)
            {
                *polySize = CRC_POLY_SIZE_32;
            }
            else if ( LL_CRC_POLYLENGTH_16B == regValue)
            {
                *polySize = CRC_POLY_SIZE_16;
            }
            else if ( LL_CRC_POLYLENGTH_8B == regValue)
            {
                *polySize = CRC_POLY_SIZE_8;
            }
            else
            {
                *polySize = CRC_POLY_SIZE_7;
            }

            retValue = CRC_REQUEST_OK;
        }
        else
        {
            /* Null pointer assigned */
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets initial value for CRC calculation
 *
 * \param crcId     [in]: Peripheral identification
 * \param initValue [in]: Initial value for CRC calculation
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_InitValue( crc_PeriphId_t crcId, crc_Data_t initValue )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        LL_CRC_SetInitialData( crc_PeriphConf[crcId].CrcReg, initValue );

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Gets initial value for CRC calculation
 *
 * \param crcId     [in]: Peripheral identification
 * \param initValue [out]: Initial value for CRC calculation
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_InitValue( crc_PeriphId_t crcId, crc_Data_t * const initValue )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId     ) &&
        ( CRC_NULL_PTR  != initValue )    )
    {
        *initValue = LL_CRC_GetInitialData( crc_PeriphConf[crcId].CrcReg );

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/*-------------------- Non-blocking transfer functionality -------------------*/

/** \brief Sets transfer complete interrupt to active state
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_TransferCpltActive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t dmaState = GPDMA_REQUEST_ERROR;

            dmaState = Gpdma_Set_TransferCompleteIsrHandler( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                             crc_RuntimeData[ crcId ].DmaChannelId,
                                                             Crc_TransferCompleteIsrHandler );

            if( GPDMA_REQUEST_OK == dmaState )
            {
                dmaState = Gpdma_Set_TransferCompleteIrqActive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                                crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Transfer complete interrupt can be set only in DMA transfer style */
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets transfer complete interrupt to inactive state
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_TransferCpltInactive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t dmaState = GPDMA_REQUEST_ERROR;

            dmaState = Gpdma_Set_TransferCompleteIrqInactive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                              crc_RuntimeData[ crcId ].DmaChannelId );

            if( GPDMA_REQUEST_OK == dmaState )
            {
                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Transfer complete interrupt can be set only in DMA transfer style */
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets transfer complete ISR callback
 *
 * \param crcId    [in]: Peripheral identification
 * \param callback [in]: Pointer to ISR callback function
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_TransferCpltIsr( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId    ) &&
        ( CRC_NULL_PTR  != callback )    )
    {
        crc_RuntimeData[ crcId ].CalcCompleteIsr = callback;

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Gets transfer complete ISR callback
 *
 * \param crcId    [in]: Peripheral identification
 * \param callback [out]: Pointer to ISR callback function
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_TransferCpltIsr( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t ** const callback )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId   ) &&
        ( CRC_NULL_PTR != callback )    )
    {
        *callback = crc_RuntimeData[ crcId ].CalcCompleteIsr;

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets error interrupt to active state
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ErrorIrqActive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t dmaState = GPDMA_REQUEST_ERROR;

            dmaState = Gpdma_Set_ErrorIsrHandler( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                  crc_RuntimeData[ crcId ].DmaChannelId,
                                                  Crc_ErrorIsrHandler );

            if( GPDMA_REQUEST_OK == dmaState )
            {
                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }

            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_TransferErrorIrqActive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                             crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_ConfigErrorIrqActive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                           crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_ConfigUpdateErrorIrqActive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                                 crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_TriggerOverrunIrqActive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                              crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_SuspensionIrqActive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                          crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets error interrupt to inactive state
 *
 * \param crcId [in]: Peripheral identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ErrorIrqInactive( crc_PeriphId_t crcId )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( CRC_PERIPH_CNT > crcId )
    {
        if( CRC_TRANSFER_DMA == crc_RuntimeData[crcId].TransferStyle )
        {
            gpdma_RequestState_t dmaState = GPDMA_REQUEST_ERROR;

            dmaState = Gpdma_Set_TransferErrorIrqInactive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                           crc_RuntimeData[ crcId ].DmaChannelId );

            if( GPDMA_REQUEST_OK == dmaState )
            {
                retValue = CRC_REQUEST_OK;
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_ConfigErrorIrqInactive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                             crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_ConfigUpdateErrorIrqInactive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                                   crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_TriggerOverrunIrqInactive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                                crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }


            if( CRC_REQUEST_OK == retValue )
            {
                dmaState = Gpdma_Set_SuspensionIrqInactive( crc_RuntimeData[ crcId ].DmaPeriphId,
                                                            crc_RuntimeData[ crcId ].DmaChannelId );

                if( GPDMA_REQUEST_OK == dmaState )
                {
                    retValue = CRC_REQUEST_OK;
                }
                else
                {
                    retValue = CRC_REQUEST_ERROR;
                }
            }
            else
            {
                retValue = CRC_REQUEST_ERROR;
            }
        }
        else
        {
            retValue = CRC_REQUEST_ERROR;
        }
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Sets error ISR callback
 *
 * \param crcId    [in]: Peripheral identification
 * \param callback [in]: Pointer to ISR callback function
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Set_ErrorIsr( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId    ) &&
        ( CRC_NULL_PTR  != callback )    )
    {
        crc_RuntimeData[ crcId ].CalcCompleteIsr = callback;

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}


/** \brief Gets error ISR callback
 *
 * \param crcId    [in]: Peripheral identification
 * \param callback [out]: Pointer to ISR callback function
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
crc_RequestState_t Crc_Get_ErrorIsr( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t ** const callback )
{
    crc_RequestState_t retValue = CRC_REQUEST_ERROR;

    if( ( CRC_PERIPH_CNT > crcId   ) &&
        ( CRC_NULL_PTR != callback )    )
    {
        *callback = crc_RuntimeData[ crcId ].CalcCompleteIsr;

        retValue = CRC_REQUEST_OK;
    }
    else
    {
        retValue = CRC_REQUEST_ERROR;
    }

    return ( retValue );
}

/* =========================== LOCAL FUNCTIONS ============================== */


/* =========================== INTERRUPT HANDLERS =========================== */

/**
 * \brief DMA Transfer Complete Interrupt handler
 */
static void Crc_TransferCompleteIsrHandler( void )
{
    crc_Data_t crcData = Crc_Get_Data( CRC_PERIPH_1 );

    if( CRC_NULL_PTR != crc_RuntimeData[ CRC_PERIPH_1 ].CalcCompleteIsr )
    {
        crc_RuntimeData[ CRC_PERIPH_1 ].CalcCompleteIsr( crcData );
    }
    else
    {
        /* Interrupt callback was not configured */
    }
}


/**
 * \brief DMA Error Interrupt handler
 */
static void Crc_ErrorIsrHandler( gpdma_ErrorMaskId_t errorId )
{
    (void) errorId;

    if( CRC_NULL_PTR != crc_RuntimeData[ CRC_PERIPH_1 ].ErrorIsr )
    {
        crc_RuntimeData[ CRC_PERIPH_1 ].ErrorIsr( CRC_ERROR_DMA );
    }
    else
    {
        /* Interrupt callback was not configured */
    }
}


/** * \brief Feeds 8-bit data to CRC peripheral
 *
 * \param CRCx   [in]: Pointer to CRC peripheral register base
 * \param InData [in]: Address of input data buffer
 * \param DataId [in]: Index of data to be processed
 */
static void Crc_FeedData_8b( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId )
{
    LL_CRC_FeedData8( CRCx, ((uint8_t *)InData)[ DataId ] );
}


/** * \brief Feeds 16-bit data to CRC peripheral
 *
 * \param CRCx   [in]: Pointer to CRC peripheral register base
 * \param InData [in]: Address of input data buffer
 * \param DataId [in]: Index of data to be processed
 */
static void Crc_FeedData_16b( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId )
{
    LL_CRC_FeedData16( CRCx, ((uint16_t *)InData)[ DataId ] );
}


/** * \brief Feeds 32-bit data to CRC peripheral
 *
 * \param CRCx   [in]: Pointer to CRC peripheral register base
 * \param InData [in]: Address of input data buffer
 * \param DataId [in]: Index of data to be processed
 */
static void Crc_FeedData_32b( CRC_TypeDef *CRCx, uint32_t InData, uint32_t DataId )
{
    LL_CRC_FeedData32( CRCx, ((uint32_t *)InData)[ DataId ] );
}


/** * \brief Reads 8-bit data from CRC peripheral
 *
 * \param CRCx [in]: Pointer to CRC peripheral register base
 *
 * \return Value of generated data
 */
static uint32_t Crc_ReadData_8b( CRC_TypeDef *CRCx )
{
    return ( (uint32_t) ( LL_CRC_ReadData8( CRCx ) & 0xFF ) );
}


/** * \brief Reads 16-bit data from CRC peripheral
 *
 * \param CRCx [in]: Pointer to CRC peripheral register base
 *
 * \return Value of generated data
 */
static uint32_t Crc_ReadData_16b( CRC_TypeDef *CRCx )
{
    return ( (uint32_t) ( LL_CRC_ReadData16( CRCx ) & 0xFFFF ) );
}


/** * \brief Reads 32-bit data from CRC peripheral
 *
 * \param CRCx [in]: Pointer to CRC peripheral register base
 *
 * \return Value of generated data
 */
static uint32_t Crc_ReadData_32b( CRC_TypeDef *CRCx )
{
    return ( (uint32_t) ( LL_CRC_ReadData32( CRCx ) & 0xFFFFFFFF ) );
}

/* ================================ TASKS =================================== */
