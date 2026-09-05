/**
 * \author Mr.Nobody
 * \file Crc_Types.h
 * \ingroup Crc
 * \brief Crc module global types definition
 *
 * This file contains the types definitions used across the module and are 
 * available for other modules through Port file.
 *
 */

#ifndef CRC_CRC_TYPES_H
#define CRC_CRC_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
#include "Stm32_crc.h"                      /* CRC peripheral functionality   */
#include "Gpdma_Port.h"                     /* GPDMA module types definition  */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define CRC_NULL_PTR                        ( ( void* ) 0u )

/** Default polynomial value (0x04C11DB7) */
#define CRC_DEFAULT_POLY                    ( LL_CRC_DEFAULT_CRC32_POLY )

/** Default initial value for CRC calculation (0xFFFFFFFF) */
#define CRC_DEFAULT_INIT_VAL                ( LL_CRC_DEFAULT_CRC_INITVALUE )

/* ========================== EXPORTED MACROS =============================== */

/* ============================== TYPEDEFS ================================== */

/** \brief Type signaling major version of SW module */
typedef uint8_t crc_MajorVersion_t;


/** \brief Type signaling minor version of SW module */
typedef uint8_t crc_MinorVersion_t;


/** \brief Type signaling patch version of SW module */
typedef uint8_t crc_PatchVersion_t;


/** \brief Type signaling actual version of SW module */
typedef struct
{
    crc_MajorVersion_t Major; /**< Major version */
    crc_MinorVersion_t Minor; /**< Minor version */
    crc_PatchVersion_t Patch; /**< Patch version */
}   crc_ModuleVersion_t;


/** Function status enumeration */
typedef enum
{
    CRC_FUNCTION_INACTIVE = 0u, /**< Function status is inactive */
    CRC_FUNCTION_ACTIVE         /**< Function status is active   */
}   crc_FunctionState_t;


/** Flag states enumeration */
typedef enum
{
    CRC_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    CRC_FLAG_ACTIVE         /**< Active flag state   */
}   crc_FlagState_t;


/** \brief CRC data type definition */
typedef uint32_t crc_Data_t;


/** \brief DMA interrupt priority type definition */
typedef uint32_t crc_DmaIrqPrio_t;


/** \brief Type representing count of data to be used in CRC calculation.
 */
typedef uint32_t crc_BlockSize_t;


/** \brief Type representing count of repetitions in single transfer (only for 2D transfers).
 *
 * This type specifies count of repetitions of single transfer (specified by
 * \ref crc_BlockSize_t) from source to destination. It is used only for
 * 2D transfers. This value can represent number of lines (rows) in 2D transfer.
 */
typedef uint16_t crc_BlockRep_t;


/** \brief List of possible errors */
typedef enum
{
    CRC_ERROR_DMA = 0u, /**< DMA transfer error */
    CRC_ERROR_CNT
}   crc_ErrorId_t;


/** \brief CRC input data size type definition */
typedef enum
{
    CRC_DATA_8B = 0u, /**< 8bits data size            */
    CRC_DATA_16B,     /**< 16bits data size           */
    CRC_DATA_32B,     /**< 32bits data size           */
    CRC_DATA_CNT      /**< Count of available options */
}   crc_DataSize_t;


/** Enumeration used to signal request processing state */
typedef enum
{
    CRC_REQUEST_ERROR = 0u, /**< Processing request failed  */
    CRC_REQUEST_OK          /**< Processing request succeed */
}   crc_RequestState_t;


/** Peripheral identification */
typedef enum
{
    CRC_PERIPH_1 = 0u, /**< CRC peripheral 1 */
    CRC_PERIPH_CNT
}   crc_PeriphId_t;


/** Data transfer handling style enumeration */
typedef enum
{
    CRC_TRANSFER_NONE = 0u, /**< No data transfer handling style is used. User must handle data transfer by himself. */
    CRC_TRANSFER_BLOCKING,  /**< Blocking style is used and is handled by module task function.                      */
    CRC_TRANSFER_DMA,       /**< DMA is used for data transfer                                                       */
    CRC_TRANSFER_CNT        /**< Count of available options                                                          */
}   crc_TransferStyle_t;


/** \brief Input data format configuration */
typedef enum
{
    CRC_IN_DATA_NORMAL       = LL_CRC_INDATA_REVERSE_NONE,     /**< Input data are not inverted       */
    CRC_IN_DATA_8B_INVERTED  = LL_CRC_INDATA_REVERSE_BYTE,     /**< Input data are inverted by 8bits  */
    CRC_IN_DATA_16B_INVERTED = LL_CRC_INDATA_REVERSE_HALFWORD, /**< Input data are inverted by 16bits */
    CRC_IN_DATA_32B_INVERTED = LL_CRC_INDATA_REVERSE_WORD      /**< Input data are inverted by 32bits */
}   crc_InDataOrder_t;


/** \brief Output data format */
typedef enum
{
    CRC_DATA_NORMAL = 0u, /**< Data bit order is standard */
    CRC_DATA_INVERTED     /**< Data bit order is inverted */
}   crc_OutDataOrder_t;


/** \brief CRC polynomial size configuration. */
typedef enum
{
    CRC_POLY_SIZE_32 = 0u, /**< CRC polynomial size is 32bits */
    CRC_POLY_SIZE_16,      /**< CRC polynomial size is 16bits */
    CRC_POLY_SIZE_8,       /**< CRC polynomial size is 8bits  */
    CRC_POLY_SIZE_7        /**< CRC polynomial size is 7bits  */
}   crc_PolySize_t;


/**
 * \brief Type definition for calculation finished callback from ISR in DMA mode
 *
 * \return Value of calculated CRC
 */
typedef void ( crc_CalcCompleteIsr_t )( crc_Data_t );


/**
 * \brief Type definition for calculation finished callback from ISR in DMA mode
 *
 * \return Value of calculated CRC
 */
typedef void ( crc_ErrorIsr_t )( crc_ErrorId_t );


typedef struct
{
    /** DMA peripheral identification used for CRC bus.
     * User can configure which DMA bus will be used for data transfer. */
    gpdma_PeriphId_t         DmaPeriphId;

    /** DMA peripheral channel used for CRC bus.
     * CRC bus use only one channel of bus BUT it is memory-to-memory mode.
     * (Really "awesome" job ST) And because there is only one memory-to-memory
     * trigger, user shall avoid other memory-to-memory transfers. */
    gpdma_ChannelId_t        DmaChannelId;

    /** DMA peripheral channel priority used for CRC bus.
     * User can configure priority for DMA channel. This can prioritize current
     * channel in front of other transfers in same DMA bus. */
    gpdma_Priority_t         DmaChannelPrio;

    /**< DMA interrupt priority */
    crc_DmaIrqPrio_t         IrqPrio;

    /** Trigger type configuration.
     * If not used set to \ref GPDMA_TRG_NOT_USED and other trigger
     * configurations will be ignored. */
    gpdma_TrgType_t          TriggerType;
    /** Trigger source identification. */
    gpdma_TrgSrcId_t         TriggerSource;
    /** Trigger mode configuration. */
    gpdma_TriggerMode_t      TriggerMode;

    /** Transfer lock mode.
     * User can select if another X-fers can be added to current channel or not.
     */
    gpdma_XferListLock_t     DmaTransferLockState;

    /** Execution style of transfer list.
     * User can select if transfer list will be executed once or continuously. */
    gpdma_XferListExecMode_t XferListExecMode;

    /** Event generation mode.
     * User can select if events will be generated by half or full transfer. */
    gpdma_TransferEvent_t    EventMode;

}   crc_DmaConfig_t;


/** \brief CRC peripheral configuration structure */
typedef struct
{
    /** CRC peripheral identification.
     * Currently only one peripheral is available. */
    crc_PeriphId_t        PeriphId;

    /** CRC input data order (normal/inverted) */
    crc_InDataOrder_t     InputDataOrder;

    /** CRC output data order (normal/inverted) */
    crc_OutDataOrder_t    OutputDataOrder;

    /** CRC polynomial value.
     * User can configure polynomial for CRC calculation. Default value is
     * \ref CRC_DEFAULT_POLY */
    uint32_t              PolynomialValue;

    /** CRC calculation polynomial data size.
     * This value only describes the usage of polynomial. The calculation itself
     * depends on data size written into data register. */
    crc_PolySize_t        PolynomialSize;

    /** CRC calculation initial value.
     * User can configure CRC initial value for calculation. Default value is
     * \ref LL_CRC_DEFAULT_CRC_INITVALUE */
    uint32_t              InitialValue;

    /** Data transfer style.
     * For current MCU, no interrupts are supported by HW. DMA data transfer is
     * possible only in memory-to-memory mode (really "great" job ST (sarcasm)).
     * User must avoid memory-to-memory elsewhere. */
    crc_TransferStyle_t   TransferStyle;

    /** Bit size of data written into CRC unit. */
    crc_DataSize_t        InDataSize;

    /** Size of input data block used for CRC calculation.
     * In DMA mode, the value is limited by \ref gpdma_BlockSize_t.
     * In blocking mode, is the value limited by size of data type. */
    crc_BlockSize_t       InBlockSize;
    /** Count of block repetitions.
     * In DMA mode, is this option available only for 2D transfers. The size is
     * limited by \ref gpdma_BlockRep_t. For linear transfers this value must
     * be 0.
     * In Blocking mode, is the value limited by size of data type. */
    crc_BlockRep_t        InBlockRepetitionCount;

    /** Size of single block processed during one access.
     * In DMA mode, the value is limited by \ref gpdma_BurstLength_t.
     * In Blocking mode is the value limited by size of data type. */
    crc_BlockSize_t       BurstProcessSize;

    /** Address of variable holding input data array.
     * In blocking mode, user must write data into CRC data register by himself.
     * In non-blocking mode, this pointer is used by DMA for data transfer. */
    uint32_t              InDataAddr;

    /** Address of variable holding output data.
     * If not configured, user must read data from CRC data register by himself.
     * Otherwise this pointer is used by callback to return calculated CRC value. */
    uint32_t              OutDataAddr;

    /** Error callback pointer */
    crc_ErrorIsr_t        *ErrorIsr;

    /** Calculation complete ISR callback.
     * Because ST did not connect this peripheral with NVIC and did not provide
     * any flags or interrupts, only DMA mode is possible for non-blocking transfer.
     * Thus is this callback executed, when DMA finish transfer of specified
     * transfer count. */
    crc_CalcCompleteIsr_t *CalcCompleteIsr;

    /** DMA configuration structure.
     * This structure contains all necessary information for DMA configuration
     * and data transfer handling. */
    crc_DmaConfig_t       DmaConfig;

}   crc_Config_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* CRC_CRC_TYPES_H */
