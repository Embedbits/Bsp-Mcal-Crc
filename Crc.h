/**
 * \author Mr.Nobody
 * \file Crc.h
 * \ingroup Crc
 * \brief Crc module common functionality header file.
 *
 * This file contains the common functionality used internally by the module,
 * and shall provide interface between the module and the application.
 *
 */

#ifndef CRC_CRC_H
#define CRC_CRC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Crc_Types.h"                      /* Module types definitions       */
#include "Rcc_Types.h"                      /* RCC peripheral identification  */
/* ============================= TYPEDEFS =================================== */

/** \brief CRC peripheral configuration item */
typedef struct
{
    CRC_TypeDef    *PeriphReg; /**< Peripheral registers           */
    rcc_PeriphId_t  PeriphRcc; /**< RCC peripheral identification  */
}   crc_PeriphConfig_t;


/** \brief Polynomial size configuration item */
typedef struct
{
    uint32_t    RegVal;  /**< LL polynomial size register value         */
    crc_Data_t  Mask;    /**< Mask of polynomial / initial value / result */
}   crc_PolySizeConfig_t;


/** \brief Input data size configuration item */
typedef struct
{
    crc_DataCnt_t       ItemSize;   /**< Size of one data item in bytes */
    gpdma_DataSize_t    DmaSize;    /**< GPDMA data size of one item    */
}   crc_InputSizeConfig_t;


/** \brief GPDMA handlers of one CRC peripheral (registered in GPDMA module) */
typedef struct
{
    gpdma_IsrCallback    *TransferCpltIsr;  /**< Transfer complete handler */
    gpdma_IsrCallback    *HalfTransferIsr;  /**< Half transfer handler     */
    gpdma_IsrErrCallback *ErrorIsr;         /**< Transfer error handler    */
}   crc_DmaIsrConfig_t;


/** \brief Runtime context of one CRC peripheral */
typedef struct
{
    crc_FunctionState_t             InitState;          /**< Peripheral was initialized by \ref Crc_Init          */
    crc_FunctionState_t             ClockState;         /**< Peripheral clock is enabled                          */
    crc_FunctionState_t             DmaState;           /**< Non-blocking calculation is configured               */
    volatile crc_TransferState_t    XferState;          /**< Non-blocking calculation state                       */
    crc_InputSize_t                 InputSize;          /**< Input data item size                                 */
    crc_FunctionState_t             TransferCpltState;  /**< Transfer complete user callback is enabled           */
    crc_FunctionState_t             HalfTransferState;  /**< Half transfer user callback is enabled               */
    crc_CalcCompleteIsr_t          *TransferCpltIsr;    /**< Transfer complete user callback                      */
    crc_CalcCompleteIsr_t          *HalfTransferIsr;    /**< Half transfer user callback                          */
    crc_FunctionState_t             TransferErrorState; /**< Transfer error user callback is enabled              */
    crc_TransferErrorIsr_t         *TransferErrorIsr;   /**< Transfer error user callback                         */
}   crc_PeriphContext_t;


/** \brief GPDMA error bit reporting */
typedef struct
{
    gpdma_ErrorMaskId_t DmaError;   /**< GPDMA error bit            */
    crc_XferErrorId_t   ErrorId;    /**< Error reported to the user */
}   crc_DmaErrorConfig_t;


/** \brief GPDMA channel ownership (GPDMA channel can not be de-initialized separately) */
typedef struct
{
    crc_FunctionState_t Initialized; /**< GPDMA channel was initialized for the peripheral */
    crc_DmaPeriphId_t   PeriphId;    /**< Initialized GPDMA peripheral                     */
    crc_DmaChannelId_t  ChannelId;   /**< Initialized GPDMA channel                        */
}   crc_DmaChannelState_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

#ifdef __cplusplus
}
#endif

#endif /* CRC_CRC_H */
