/**
 * \author Mr.Nobody
 * \file Crc_Port.h
 * \ingroup Crc
 * \brief Crc module public functionality
 *
 * This file contains all available public functionality, any other files shall 
 * be used outside of the module.
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

crc_ModuleVersion_t         Crc_Get_ModuleVersion       ( void );

crc_RequestState_t          Crc_Init                    ( crc_Config_t * const crcConfig );
crc_RequestState_t          Crc_Deinit                  ( crc_Config_t * const crcConfig );
void                        Crc_Task                    ( void );

crc_RequestState_t          Crc_Get_DefaultConfig       ( crc_Config_t * const crcConfig );

crc_RequestState_t          Crc_Set_ExecActive          ( crc_PeriphId_t crcId );
crc_RequestState_t          Crc_Set_ExecInactive        ( crc_PeriphId_t crcId );
crc_RequestState_t          Crc_Get_ExecState           ( crc_PeriphId_t crcId, crc_FunctionState_t * const state );

crc_RequestState_t          Crc_Set_ExecPauseActive     ( crc_PeriphId_t crcId );
crc_RequestState_t          Crc_Set_ExecPauseInactive   ( crc_PeriphId_t crcId );
crc_RequestState_t          Crc_Get_ExecPauseState      ( crc_PeriphId_t crcId, crc_FunctionState_t * const state );

crc_Data_t                  Crc_Get_Data                ( crc_PeriphId_t crcId );

/*-------------------------- Primitive functionality -------------------------*/

crc_RequestState_t          Crc_Set_OutputDataOrder     ( crc_PeriphId_t crcId, crc_OutDataOrder_t dataOrder );
crc_RequestState_t          Crc_Get_OutputDataOrder     ( crc_PeriphId_t crcId, crc_OutDataOrder_t * const dataOrder );

crc_RequestState_t          Crc_Set_InputDataOrder      ( crc_PeriphId_t crcId, crc_InDataOrder_t dataOrder );
crc_RequestState_t          Crc_Get_InputDataOrder      ( crc_PeriphId_t crcId, crc_InDataOrder_t * const dataOrder );

crc_RequestState_t          Crc_Set_Polynomial          ( crc_PeriphId_t crcId, crc_Data_t polynomial, crc_PolySize_t polySize );
crc_RequestState_t          Crc_Get_Polynomial          ( crc_PeriphId_t crcId, crc_Data_t * const polynomial, crc_PolySize_t * const polySize );

crc_RequestState_t          Crc_Set_InitValue           ( crc_PeriphId_t crcId, crc_Data_t initValue );
crc_RequestState_t          Crc_Get_InitValue           ( crc_PeriphId_t crcId, crc_Data_t * const initValue );

/*-------------------- Non-blocking transfer functionality -------------------*/

crc_RequestState_t          Crc_Set_TransferCpltActive  ( crc_PeriphId_t crcId );
crc_RequestState_t          Crc_Set_TransferCpltInactive( crc_PeriphId_t crcId );

crc_RequestState_t          Crc_Set_TransferCpltIsr     ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback );
crc_RequestState_t          Crc_Get_TransferCpltIsr     ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t ** const callback );

crc_RequestState_t          Crc_Set_ErrorIrqActive      ( crc_PeriphId_t crcId );
crc_RequestState_t          Crc_Set_ErrorIrqInactive    ( crc_PeriphId_t crcId );

crc_RequestState_t          Crc_Set_ErrorIsr            ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback );
crc_RequestState_t          Crc_Get_ErrorIsr            ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t ** const callback );

/*------------------------- Interrupts functionality -------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* CRC_CRC_PORT_H */

