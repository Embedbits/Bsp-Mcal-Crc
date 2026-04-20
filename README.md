# CRC MCAL Module

The **CRC (Cyclic Redundancy Check) MCAL module** provides an abstraction for configuring and using the CRC peripheral on STM32 microcontrollers.  
This module is part of the MCAL layer and ensures a unified API across STM32 families.  
Different STM32 families are maintained in separate branches; switch to the correct branch for your MCU.

---

## 🚀 Features

- CRC peripheral initialization and deinitialization
- Configurable polynomial and initial value
- Input/output data order control
- Peripheral activation/inactivation
- Support for non-blocking transfer with completion and half-transfer interrupts
- Default configuration retrieval
- Periodic task handler

---

## Public API

### Module Information

- `crc_ModuleVersion_t Crc_Get_ModuleVersion ( void );`  
  Returns the current version of the CRC module.

---

### Initialization

- `crc_RequestState_t Crc_Init ( crc_Config_t * const crcConfig );`  
  Initializes the CRC peripheral with the given configuration.

- `void Crc_Deinit ( void );`  
  Deinitializes the CRC peripheral and resets configuration.

- `void Crc_Task ( void );`  
  Handles background tasks (if needed by the implementation).

- `crc_RequestState_t Crc_Get_DefaultConfig ( crc_Config_t * const crcConfig );`  
  Retrieves a default configuration structure for safe initialization.

---

### Peripheral Control

- `crc_RequestState_t Crc_Set_PeriphActive ( crc_PeriphId_t crcId );`  
  Activates the CRC peripheral.

- `crc_RequestState_t Crc_Set_PeriphInactive ( crc_PeriphId_t crcId );`  
  Deactivates the CRC peripheral.

- `crc_Data_t Crc_Get_Data ( crc_PeriphId_t crcId );`  
  Reads the last calculated CRC result.

---

### Configuration

- `crc_RequestState_t Crc_Set_OutputDataOrder ( crc_PeriphId_t crcId, crc_DataOrder_t dataOrder );`  
- `crc_RequestState_t Crc_Get_OutputDataOrder ( crc_PeriphId_t crcId, crc_DataOrder_t * const dataOrder );`  
  Configure or retrieve the output data order.

- `crc_RequestState_t Crc_Set_InputDataOrder ( crc_PeriphId_t crcId, crc_DataOrder_t dataOrder );`  
- `crc_RequestState_t Crc_Get_InputDataOrder ( crc_PeriphId_t crcId, crc_DataOrder_t * const dataOrder );`  
  Configure or retrieve the input data order.

- `crc_RequestState_t Crc_Set_Polynomial ( crc_PeriphId_t crcId, crc_Data_t polynomial, crc_DataSize_t polySize );`  
- `crc_RequestState_t Crc_Get_Polynomial ( crc_PeriphId_t crcId, crc_Data_t * const polynomial, crc_DataSize_t * const polySize );`  
  Set or get the CRC polynomial.

- `crc_RequestState_t Crc_Set_InitValue ( crc_PeriphId_t crcId, crc_Data_t initValue );`  
- `crc_RequestState_t Crc_Get_InitValue ( crc_PeriphId_t crcId, crc_Data_t * const initValue );`  
  Configure or retrieve the CRC initial value.

---

### Non-Blocking Transfer Support

- `crc_RequestState_t Crc_Set_TransferCpltActive ( crc_PeriphId_t crcId );`  
- `crc_RequestState_t Crc_Set_TransferCpltInactive ( crc_PeriphId_t crcId );`  
- `crc_RequestState_t Crc_Get_TransferCpltState ( crc_PeriphId_t crcId );`  
  Control or check the transfer complete state.

- `crc_RequestState_t Crc_Set_TransferCpltIsr ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback );`  
- `crc_RequestState_t Crc_Get_TransferCpltIsr ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback );`  
  Configure or retrieve the transfer complete ISR callback.

- `crc_RequestState_t Crc_Set_HalfTransferActive ( crc_PeriphId_t crcId );`  
- `crc_RequestState_t Crc_Set_HalfTransferInactive ( crc_PeriphId_t crcId );`  
- `crc_RequestState_t Crc_Get_HalfTransferState ( crc_PeriphId_t crcId );`  
  Control or check the half-transfer state.

- `crc_RequestState_t Crc_Set_HalfTransferIsr ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback );`  
- `crc_RequestState_t Crc_Get_HalfTransferIsr ( crc_PeriphId_t crcId, crc_CalcCompleteIsr_t const * const callback );`  
  Configure or retrieve the half-transfer ISR callback.

---

## Usage Notes

- Always initialize the module using **`Crc_Init`** before calling other functions.  
- The **default configuration API** provides a safe starting point.  
- Non-blocking mode allows handling CRC operations asynchronously with ISR callbacks.  
- Polynomial and input/output order must be configured according to the data format expected in the application.

---

## 🛠 CMake Integration
Add this to your module's CMakeLists.txt:
```
target_link_libraries(User_Lib PRIVATE Crc_Lib)
```
---

## License

This project is licensed under the **Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0)**.

You are free to use, modify, and share this work for **non-commercial purposes**, provided appropriate credit is given.

See [LICENSE.md](LICENSE.md) for full terms or visit [creativecommons.org/licenses/by-nc/4.0](https://creativecommons.org/licenses/by-nc/4.0/).

---

## Authors

- **Mr.Nobody** — [embedbits.com](https://embedbits.com)

Contributions are welcome! Please open a pull request.

---

## 🌐 Useful Links

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [Azure DevOps](https://azure.microsoft.com/en-us/services/devops/)
- [Embedbits Github](https://github.com/Embedbits)
- [CC BY-NC 4.0 License](https://creativecommons.org/licenses/by-nc/4.0/)