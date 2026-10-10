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
#include "Stm32.h"                          /* MCU device header (family macro STM32H7RS) */

#if defined(STM32H7RS)
#include "Gpdma_Types.h"                    /* DMA types definitions          */
#else
#include "Dma_Types.h"                      /* DMA types definitions          */
#endif /* STM32H7RS */
#include "Stm32_crc.h"                      /* CRC RAL functionality          */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define CRC_NULL_PTR                        ( ( void* ) 0u )

#if defined(STM32H7RS)
/** Maximal count of bytes of one non-blocking (GPDMA) calculation, see \ref Crc_Set_TransferStart */
#define CRC_TRANSFER_DATA_CNT_MAX           ( 0xFFFFu )
#else
/** Maximal count of bytes of one non-blocking (DMA) calculation (DMA data counter counts
 *  items, the limit is valid for every input data size), see \ref Crc_Set_TransferStart */
#define CRC_TRANSFER_DATA_CNT_MAX           ( DMA_DATA_COUNT_MAX )
#endif /* STM32H7RS */

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


/** Non-blocking (DMA) calculation state enumeration */
typedef enum
{
    CRC_TRANSFER_IDLE = 0u, /**< No transfer is running, result can be read */
    CRC_TRANSFER_BUSY,      /**< Data are fed to the CRC unit by DMA        */
    CRC_TRANSFER_ERROR      /**< Last transfer was stopped by DMA error     */
}   crc_TransferState_t;


#if defined(STM32H7RS)
/** DMA peripherals enumeration list */
typedef enum
{
#if defined(GPDMA1)
    CRC_DMA_PERIPH_1 = GPDMA_PERIPH_1, /**< DMA peripheral 1 identification */
#endif
#if defined(HPDMA1)
    CRC_DMA_PERIPH_2 = GPDMA_PERIPH_2, /**< DMA peripheral 2 identification */
#endif
    CRC_DMA_PERIPH_CNT                 /**< Count of DMA peripherals        */
}   crc_DmaPeriphId_t;
#else
/** DMA peripherals enumeration list (memory to memory transfer is supported by DMA1 and DMA2) */
typedef enum
{
#if defined(DMA1)
    CRC_DMA_PERIPH_1 = DMA_PERIPH_1, /**< DMA peripheral 1 identification */
#endif
#if defined(DMA2)
    CRC_DMA_PERIPH_2 = DMA_PERIPH_2, /**< DMA peripheral 2 identification */
#endif
    CRC_DMA_PERIPH_CNT               /**< Count of DMA peripherals        */
}   crc_DmaPeriphId_t;
#endif /* STM32H7RS */


/** Enumeration of available streams of DMA peripheral (memory to memory transfer is supported by every stream) */
typedef enum
{
#if defined(STM32H7RS)
    CRC_DMA_CHANNEL_0   = GPDMA_CHANNEL_0, /**< DMA transfer channel 0  (linear addressing) */
    CRC_DMA_CHANNEL_1   = GPDMA_CHANNEL_1, /**< DMA transfer channel 1  (linear addressing) */
    CRC_DMA_CHANNEL_2   = GPDMA_CHANNEL_2, /**< DMA transfer channel 2  (linear addressing) */
    CRC_DMA_CHANNEL_3   = GPDMA_CHANNEL_3, /**< DMA transfer channel 3  (linear addressing) */
    CRC_DMA_CHANNEL_4   = GPDMA_CHANNEL_4, /**< DMA transfer channel 4  (linear addressing) */
    CRC_DMA_CHANNEL_5   = GPDMA_CHANNEL_5, /**< DMA transfer channel 5  (linear addressing) */
    CRC_DMA_CHANNEL_6   = GPDMA_CHANNEL_6, /**< DMA transfer channel 6  (linear addressing) */
    CRC_DMA_CHANNEL_7   = GPDMA_CHANNEL_7, /**< DMA transfer channel 7  (linear addressing) */
    CRC_DMA_CHANNEL_8   = GPDMA_CHANNEL_8, /**< DMA transfer channel 8  (linear addressing) */
    CRC_DMA_CHANNEL_9   = GPDMA_CHANNEL_9, /**< DMA transfer channel 9  (linear addressing) */
    CRC_DMA_CHANNEL_10  = GPDMA_CHANNEL_10, /**< DMA transfer channel 10 (linear addressing) */
    CRC_DMA_CHANNEL_11  = GPDMA_CHANNEL_11, /**< DMA transfer channel 11 (linear addressing) */
    CRC_DMA_CHANNEL_12  = GPDMA_CHANNEL_12, /**< DMA transfer channel 12 (linear or 2D addressing) */
    CRC_DMA_CHANNEL_13  = GPDMA_CHANNEL_13, /**< DMA transfer channel 13 (linear or 2D addressing) */
    CRC_DMA_CHANNEL_14  = GPDMA_CHANNEL_14, /**< DMA transfer channel 14 (linear or 2D addressing) */
    CRC_DMA_CHANNEL_15  = GPDMA_CHANNEL_15, /**< DMA transfer channel 15 (linear or 2D addressing) */
    CRC_DMA_CHANNEL_CNT                    /**< Count of available DMA channels */
#else
    CRC_DMA_CHANNEL_0 = DMA_STREAM_0, /**< DMA stream 0                    */
    CRC_DMA_CHANNEL_1 = DMA_STREAM_1, /**< DMA stream 1                    */
    CRC_DMA_CHANNEL_2 = DMA_STREAM_2, /**< DMA stream 2                    */
    CRC_DMA_CHANNEL_3 = DMA_STREAM_3, /**< DMA stream 3                    */
    CRC_DMA_CHANNEL_4 = DMA_STREAM_4, /**< DMA stream 4                    */
    CRC_DMA_CHANNEL_5 = DMA_STREAM_5, /**< DMA stream 5                    */
    CRC_DMA_CHANNEL_6 = DMA_STREAM_6, /**< DMA stream 6                    */
    CRC_DMA_CHANNEL_7 = DMA_STREAM_7, /**< DMA stream 7                    */
    CRC_DMA_CHANNEL_CNT                /**< Count of available DMA streams  */
#endif /* STM32H7RS */
}   crc_DmaChannelId_t;


/** Channel priority options enumeration */
typedef enum
{
#if defined(STM32H7RS)
    CRC_DMA_PRIORITY_LOW      = GPDMA_PRIORITY_LOW     , /**< Priority level : Low       */
    CRC_DMA_PRIORITY_MEDIUM   = GPDMA_PRIORITY_MEDIUM  , /**< Priority level : Medium    */
    CRC_DMA_PRIORITY_HIGH     = GPDMA_PRIORITY_HIGH    , /**< Priority level : High      */
    CRC_DMA_PRIORITY_VERYHIGH = GPDMA_PRIORITY_VERYHIGH, /**< Priority level : Very_High */
#else
    CRC_DMA_PRIORITY_LOW      = DMA_PRIORITY_LOW       , /**< Priority level : Low       */
    CRC_DMA_PRIORITY_MEDIUM   = DMA_PRIORITY_MEDIUM    , /**< Priority level : Medium    */
    CRC_DMA_PRIORITY_HIGH     = DMA_PRIORITY_HIGH      , /**< Priority level : High      */
    CRC_DMA_PRIORITY_VERYHIGH = DMA_PRIORITY_VERYHIGH  , /**< Priority level : Very_High */
#endif /* STM32H7RS */
}   crc_DmaPriority_t;


/** Non-blocking calculation error identification enumeration */
typedef enum
{
#if defined(STM32H7RS)
    CRC_XFER_ERROR_DMA_TRANSFER = 0u,       /**< GPDMA data transfer error (bus error)   */
    CRC_XFER_ERROR_DMA_CONFIG,              /**< GPDMA channel configuration error       */
    CRC_XFER_ERROR_DMA_CONFIG_UPDATE,       /**< GPDMA linked-list update error          */
    CRC_XFER_ERROR_DMA_TRIGGER_OVERRUN,     /**< GPDMA trigger overrun                   */
#else
    CRC_XFER_ERROR_DMA_TRANSFER = 0u,       /**< DMA data transfer error (bus error)     */
#endif /* STM32H7RS */
    CRC_XFER_ERROR_CNT                      /**< Count of error identifications          */
}   crc_XferErrorId_t;


/** \brief Non-blocking calculation user callback (transfer complete, half transfer).
 *
 * Called from DMA interrupt context.
 */
typedef void ( crc_CalcCompleteIsr_t )( void );


/** \brief Non-blocking calculation error user callback.
 *
 * Called from DMA interrupt context after the transfer was stopped (state
 * \ref CRC_TRANSFER_ERROR).
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

    /** Non-blocking calculation (data fed by DMA channel). \ref CRC_FUNCTION_INACTIVE
     *  disables \ref Crc_Set_TransferStart and following DMA items are ignored. */
    crc_FunctionState_t     DmaState;

    /** DMA peripheral used for non-blocking calculation */
    crc_DmaPeriphId_t       DmaPeriphId;

    /** DMA channel used for non-blocking calculation */
    crc_DmaChannelId_t      DmaChannelId;

    /** DMA channel priority */
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
