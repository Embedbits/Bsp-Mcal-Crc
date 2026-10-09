/**
 * \author Mr.Nobody
 * \file Crc_Port.h
 * \ingroup Crc
 * \brief Crc module public functionality
 *
 * This file contains all available public functionality, any other files shall
 * not used outside of the module.
 *
 */

#ifndef CRC_CRC_PORT_H
#define CRC_CRC_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Crc_Types.h"                      /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS =============================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */

crc_ModuleVersion_t     Crc_Get_ModuleVersion           ( void );

crc_RequestState_t      Crc_Init                        ( crc_Config_t * const crcConfig );
crc_RequestState_t      Crc_Deinit                      ( void );
void                    Crc_Task                        ( void );

crc_RequestState_t      Crc_Get_DefaultConfig           ( crc_Config_t * const crcConfig );

/* ------------------------------ Peripheral control ------------------------ */

crc_RequestState_t      Crc_Set_PeriphActive            ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Set_PeriphInactive          ( crc_PeriphId_t crcId );

/* --------------------------- Blocking calculation ------------------------- */

crc_RequestState_t      Crc_Set_Reset                   ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Set_Data                    ( crc_PeriphId_t crcId, const crc_DataByte_t * const data, crc_DataCnt_t dataCnt );
crc_RequestState_t      Crc_Get_Data                    ( crc_PeriphId_t crcId, crc_Data_t * const crcValue );
crc_RequestState_t      Crc_Calc_Block                  ( crc_PeriphId_t crcId, const crc_DataByte_t * const data, crc_DataCnt_t dataCnt, crc_Data_t * const crcValue );

/* ------------------------------- Configuration ---------------------------- */

crc_RequestState_t      Crc_Set_OutputDataOrder         ( crc_PeriphId_t crcId, crc_DataOrder_t dataOrder );
crc_RequestState_t      Crc_Get_OutputDataOrder         ( crc_PeriphId_t crcId, crc_DataOrder_t * const dataOrder );

crc_RequestState_t      Crc_Set_InputDataOrder          ( crc_PeriphId_t crcId, crc_DataOrder_t dataOrder );
crc_RequestState_t      Crc_Get_InputDataOrder          ( crc_PeriphId_t crcId, crc_DataOrder_t * const dataOrder );

crc_RequestState_t      Crc_Set_Polynomial              ( crc_PeriphId_t crcId, crc_Data_t polynomial, crc_DataSize_t polySize );
crc_RequestState_t      Crc_Get_Polynomial              ( crc_PeriphId_t crcId, crc_Data_t * const polynomial, crc_DataSize_t * const polySize );

crc_RequestState_t      Crc_Set_InitValue               ( crc_PeriphId_t crcId, crc_Data_t initValue );
crc_RequestState_t      Crc_Get_InitValue               ( crc_PeriphId_t crcId, crc_Data_t * const initValue );

crc_RequestState_t      Crc_Set_InputDataSize           ( crc_PeriphId_t crcId, crc_InputSize_t inputSize );
crc_RequestState_t      Crc_Get_InputDataSize           ( crc_PeriphId_t crcId, crc_InputSize_t * const inputSize );

/* -------------------------- Independent data register --------------------- */

crc_RequestState_t      Crc_Set_IndependentData         ( crc_PeriphId_t crcId, crc_IndepData_t indepData );
crc_RequestState_t      Crc_Get_IndependentData         ( crc_PeriphId_t crcId, crc_IndepData_t * const indepData );

/* ------------------------ Non-blocking (DMA) calculation ------------------ */

crc_RequestState_t      Crc_Set_TransferStart           ( crc_PeriphId_t crcId, const crc_DataByte_t * const data, crc_DataCnt_t dataCnt );
crc_RequestState_t      Crc_Set_TransferStop            ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Get_TransferState           ( crc_PeriphId_t crcId, crc_TransferState_t * const xferState );

crc_RequestState_t      Crc_Set_TransferCpltActive      ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Set_TransferCpltInactive    ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Get_TransferCpltState       ( crc_PeriphId_t crcId, crc_FunctionState_t * const funcState );

crc_RequestState_t      Crc_Set_TransferCpltIsr         ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t * const callback );
crc_RequestState_t      Crc_Get_TransferCpltIsr         ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t ** const callback );

crc_RequestState_t      Crc_Set_HalfTransferActive      ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Set_HalfTransferInactive    ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Get_HalfTransferState       ( crc_PeriphId_t crcId, crc_FunctionState_t * const funcState );

crc_RequestState_t      Crc_Set_HalfTransferIsr         ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t * const callback );
crc_RequestState_t      Crc_Get_HalfTransferIsr         ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t ** const callback );

crc_RequestState_t      Crc_Set_TransferErrorActive     ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Set_TransferErrorInactive   ( crc_PeriphId_t crcId );
crc_RequestState_t      Crc_Get_TransferErrorState      ( crc_PeriphId_t crcId, crc_FunctionState_t * const funcState );

crc_RequestState_t      Crc_Set_TransferErrorIsr        ( crc_PeriphId_t crcId, crc_TransferErrorIsr_t * const callback );
crc_RequestState_t      Crc_Get_TransferErrorIsr        ( crc_PeriphId_t crcId, crc_TransferErrorIsr_t ** const callback );

#ifdef __cplusplus
}
#endif

#endif /* CRC_CRC_PORT_H */
