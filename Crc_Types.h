/**
 * \defgroup Crc Crc
 * \brief Crc module
 */

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
#include "Gpdma_Types.h"                    /* DMA types definitions          */
#include "Stm32_crc.h"                      /* CRC RAL functionality          */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define CRC_NULL_PTR                        ( ( void* ) 0u )

/** Maximal count of bytes of one non-blocking (GPDMA) calculation, see \ref Crc_Set_TransferStart */
#define CRC_TRANSFER_DATA_CNT_MAX           ( 0xFFFFu )

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


/** Enumeration used to signal request processing state */
typedef enum
{
    CRC_REQUEST_ERROR = 0u, /**< Processing request failed  */
    CRC_REQUEST_OK          /**< Processing request succeed */
}   crc_RequestState_t;


/** Flag states enumeration */
typedef enum
{
    CRC_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    CRC_FLAG_ACTIVE         /**< Active flag state   */
}   crc_FlagState_t;


/** CRC peripherals enumeration list */
typedef enum
{
#if defined(CRC)
    CRC_PERIPH_1 = 0u, /**< CRC peripheral 1 ID */
#endif /* CRC */
    CRC_PERIPH_CNT     /**< Count of CRC peripherals */
}   crc_PeriphId_t;


/** \brief Type used to store CRC values (polynomial, initial value, result) */
typedef uint32_t crc_Data_t;


/** \brief Type used to store count of data bytes */
typedef uint32_t crc_DataCnt_t;


/** \brief Type of one byte of input data buffer */
typedef uint8_t crc_DataByte_t;


/** \brief Type used to store value of independent data register */
typedef uint32_t crc_IndepData_t;


/** Input data item size enumeration.
 *
 * Data buffer is processed as an array of items of this size in native (little
 * endian) memory order, every item is written to the data register by one access.
 * Byte stream algorithms (e.g. CRC-32 Ethernet) shall use \ref CRC_INPUT_SIZE_8BITS.
 */
typedef enum
{
    CRC_INPUT_SIZE_8BITS = 0u, /**< Data items of 8 bits  */
    CRC_INPUT_SIZE_16BITS,     /**< Data items of 16 bits */
    CRC_INPUT_SIZE_32BITS,     /**< Data items of 32 bits */
    CRC_INPUT_SIZE_CNT         /**< Count of input data sizes */
}   crc_InputSize_t;


/** Polynomial (and result) size enumeration */
typedef enum
{
    CRC_DATA_SIZE_7BITS = 0u, /**< 7 bits polynomial  */
    CRC_DATA_SIZE_8BITS,      /**< 8 bits polynomial  */
    CRC_DATA_SIZE_16BITS,     /**< 16 bits polynomial */
    CRC_DATA_SIZE_32BITS,     /**< 32 bits polynomial */
    CRC_DATA_SIZE_CNT         /**< Count of polynomial sizes */
}   crc_DataSize_t;


/** Data bit order enumeration.
 *
 * Input data supports every option. Output data (result) supports only
 * \ref CRC_DATA_ORDER_NORMAL and \ref CRC_DATA_ORDER_REV_WORD (bit-reversed result).
 */
typedef enum
{
    CRC_DATA_ORDER_NORMAL = 0u,  /**< Bit order not affected        */
    CRC_DATA_ORDER_REV_BYTE,     /**< Bit reversal done by byte      */
    CRC_DATA_ORDER_REV_HALFWORD, /**< Bit reversal done by half-word */
    CRC_DATA_ORDER_REV_WORD,     /**< Bit reversal done by word      */
    CRC_DATA_ORDER_CNT           /**< Count of data order options    */
}   crc_DataOrder_t;


/** Non-blocking (GPDMA) calculation state enumeration */
typedef enum
{
    CRC_TRANSFER_IDLE = 0u, /**< No transfer is running, result can be read */
    CRC_TRANSFER_BUSY,      /**< Data are fed to the CRC unit by GPDMA      */
    CRC_TRANSFER_ERROR      /**< Last transfer was stopped by GPDMA error   */
}   crc_TransferState_t;


/** DMA peripherals enumeration list */
typedef enum
{
#if defined(GPDMA1)
    CRC_DMA_PERIPH_1 = GPDMA_PERIPH_1, /**< DMA peripheral 1 identification */
#endif
#if defined(GPDMA2)
    CRC_DMA_PERIPH_2 = GPDMA_PERIPH_2, /**< DMA peripheral 2 identification */
#endif
    CRC_DMA_PERIPH_CNT                 /**< Count of DMA peripherals        */
}   crc_DmaPeriphId_t;


/** Enumeration of available channels for all DMA peripherals */
typedef enum
{
    CRC_DMA_CHANNEL_0  = GPDMA_CHANNEL_0,  /**< DMA transfer channel 0 (FIFO size 8 bytes) linear addressing mode        */
    CRC_DMA_CHANNEL_1  = GPDMA_CHANNEL_1,  /**< DMA transfer channel 1 (FIFO size 8 bytes) linear addressing mode        */
    CRC_DMA_CHANNEL_2  = GPDMA_CHANNEL_2,  /**< DMA transfer channel 2 (FIFO size 8 bytes) linear addressing mode        */
    CRC_DMA_CHANNEL_3  = GPDMA_CHANNEL_3,  /**< DMA transfer channel 3 (FIFO size 8 bytes) linear addressing mode        */
    CRC_DMA_CHANNEL_4  = GPDMA_CHANNEL_4,  /**< DMA transfer channel 4 (FIFO size 32 bytes) linear addressing mode       */
    CRC_DMA_CHANNEL_5  = GPDMA_CHANNEL_5,  /**< DMA transfer channel 5 (FIFO size 32 bytes) linear addressing mode       */
    CRC_DMA_CHANNEL_6  = GPDMA_CHANNEL_6,  /**< DMA transfer channel 6 (FIFO size 32 bytes) linear or 2D addressing mode */
    CRC_DMA_CHANNEL_7  = GPDMA_CHANNEL_7,  /**< DMA transfer channel 7 (FIFO size 32 bytes) linear or 2D addressing mode */
    CRC_DMA_CHANNEL_CNT                    /**< Count of available DMA channels                                          */
}   crc_DmaChannelId_t;


/** Channel priority options enumeration */
typedef enum
{
    CRC_DMA_PRIORITY_LOW      = GPDMA_PRIORITY_LOW     , /**< Priority level : Low       */
    CRC_DMA_PRIORITY_MEDIUM   = GPDMA_PRIORITY_MEDIUM  , /**< Priority level : Medium    */
    CRC_DMA_PRIORITY_HIGH     = GPDMA_PRIORITY_HIGH    , /**< Priority level : High      */
    CRC_DMA_PRIORITY_VERYHIGH = GPDMA_PRIORITY_VERYHIGH, /**< Priority level : Very_High */
}   crc_DmaPriority_t;


/** Non-blocking calculation error identification enumeration */
typedef enum
{
    CRC_XFER_ERROR_DMA_TRANSFER = 0u,       /**< GPDMA data transfer error (bus error)   */
    CRC_XFER_ERROR_DMA_CONFIG,              /**< GPDMA channel configuration error       */
    CRC_XFER_ERROR_DMA_CONFIG_UPDATE,       /**< GPDMA linked-list update error          */
    CRC_XFER_ERROR_DMA_TRIGGER_OVERRUN,     /**< GPDMA trigger overrun                   */
    CRC_XFER_ERROR_CNT                      /**< Count of error identifications          */
}   crc_XferErrorId_t;


/** \brief Non-blocking calculation user callback (transfer complete, half transfer).
 *
 * Called from GPDMA interrupt context.
 */
typedef void ( crc_CalcCompleteIsr_t )( void );


/** \brief Non-blocking calculation error user callback.
 *
 * Called from GPDMA interrupt context once for every reported error, after the
 * transfer was stopped (state \ref CRC_TRANSFER_ERROR).
 */
typedef void ( crc_TransferErrorIsr_t )( crc_XferErrorId_t errorId );


/** \brief Type used to store CRC configuration */
typedef struct
{
    /** CRC peripheral identification */
    crc_PeriphId_t          PeriphId;

    /** Polynomial coefficients without the highest bit (e.g. 0x04C11DB7 for CRC-32).
     *  Must be odd (even polynomials are not supported by HW) and fit into \ref PolySize. */
    crc_Data_t              Polynomial;

    /** Polynomial size, the result has the same size */
    crc_DataSize_t          PolySize;

    /** Initial value of the calculation. Only bits within \ref PolySize are used. */
    crc_Data_t              InitValue;

    /** Input data bit order */
    crc_DataOrder_t         InputDataOrder;

    /** Output data (result) bit order, \ref CRC_DATA_ORDER_NORMAL or \ref CRC_DATA_ORDER_REV_WORD */
    crc_DataOrder_t         OutputDataOrder;

    /** Input data item size (data register access width) */
    crc_InputSize_t         InputDataSize;

    /** Non-blocking calculation (data fed by GPDMA channel). \ref CRC_FUNCTION_INACTIVE
     *  disables \ref Crc_Set_TransferStart and following DMA items are ignored. */
    crc_FunctionState_t     DmaState;

    /** GPDMA peripheral used for non-blocking calculation */
    crc_DmaPeriphId_t       DmaPeriphId;

    /** GPDMA channel used for non-blocking calculation */
    crc_DmaChannelId_t      DmaChannelId;

    /** GPDMA channel priority */
    crc_DmaPriority_t       DmaPriority;

    /** Transfer complete user callback, can be NULL. Activated by \ref Crc_Init if set. */
    crc_CalcCompleteIsr_t  *TransferCpltIsr;

    /** Half transfer user callback, can be NULL. Activated by \ref Crc_Init if set. */
    crc_CalcCompleteIsr_t  *HalfTransferIsr;

    /** Transfer error user callback, can be NULL. Activated by \ref Crc_Init if set. */
    crc_TransferErrorIsr_t *TransferErrorIsr;
}   crc_Config_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* CRC_CRC_TYPES_H */
