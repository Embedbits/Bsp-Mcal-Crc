# 🧮 CRC module

This repository provides the **MCAL (Microcontroller Abstraction Layer)** driver for the **CRC calculation unit** (`CRC`) used in **STM32 microcontrollers**.  
It offers a simple interface to configure the CRC algorithm and calculate CRC of data blocks, either blocking (CPU) or non-blocking (DMA2 stream).

Each STM32 family is supported in a **dedicated branch** of this repository:
- `STM32G4`
- `STM32U5`
- `STM32L4`
- `STM32H5`
- `STM32F4`
- `STM32F7` (this branch - same calculation unit as STM32H5, DMA2 streams, 8-bit independent data register)
- and others as needed.

---

## 📘 Overview

The **CRC MCAL driver** abstracts the STM32 CRC calculation unit:
- Programmable **polynomial** (7, 8, 16 or 32 bits) and **initial value**  
- **Input / output bit order** (bit reversal by byte, half-word, word / bit-reversed result)  
- **Blocking calculation** - data fed by CPU, in one block or in more parts  
- **Non-blocking calculation** - data fed by a DMA2 stream (memory to memory), **transfer complete**, **half transfer** and **transfer error** user callbacks  
- Peripheral clock gating (configuration is kept)  

Data buffer is processed as an array of items of the configured **input data size** (8, 16 or 32 bits, native little endian order). Byte stream algorithms use 8-bit items; blocking calculation does not require aligned data, non-blocking calculation requires data aligned to the item size.
Every configuration write is verified by read-back.

> ✅ The user **does not need to include or use** any additional modules such as RCC or DMA drivers.  
> Peripheral clock is enabled through the RCC module and the DMA stream is configured internally.

> ⚠️ **Final XOR** of the result (e.g. standard CRC-32 uses `0xFFFFFFFF`) is not supported by HW and has to be applied by the application.

---

## 🧩 Architecture

The architecture follows the standard MCAL layering used across all STM32 MCAL repositories:

```
┌────────────────────────────┐
│        Application         │
└────────────┬───────────────┘
             │
┌────────────▼───────────────┐
│           HAL              │
│(Hardware Abstraction Layer)│
└────────────┬───────────────┘
┌────────────▼───────────────┐
│       MCAL - Crc           │
│       ├── Crc_Port.h       │  
│       ├── Crc_Types.h      │  
│       └── Crc.c/.h         │  
└────────────┬───────────────┘
             │
┌────────────▼───────────────┐
│           RAL              │
│(Register Abstraction Layer)│
└────────────────────────────┘
```

---

## 🧠 Usage Guidelines

The user shall **only** interact with the following two public headers:

| File | Purpose |
|------|----------|
| `Crc_Port.h` | Contains all public API functions (init, calculation, configuration, non-blocking calculation) |
| `Crc_Types.h` | Contains type definitions used in the API (configuration structure, data order, polynomial size, callback type, request states) |

Everything else — register LUTs, DMA handlers, module context — is **internal** and must not be accessed directly.

### Public API

#### Module

| Function | Description |
|----------|-------------|
| `Crc_Get_ModuleVersion()` | Returns module SW version |
| `Crc_Init( config )` | Enables and resets the peripheral, configures the algorithm and DMA stream |
| `Crc_Deinit()` | Stops calculation, releases DMA stream, resets the peripheral and disables its clock |
| `Crc_Task()` | No periodic processing needed (unified module interface) |
| `Crc_Get_DefaultConfig( config )` | Fills default configuration (CRC-32 Ethernet, 8-bit input, no DMA) |

#### Peripheral control

| Function | Description |
|----------|-------------|
| `Crc_Set_PeriphActive( crcId )` | Enables peripheral clock |
| `Crc_Set_PeriphInactive( crcId )` | Disables peripheral clock (configuration and intermediate CRC kept) |

#### Blocking calculation

| Function | Description |
|----------|-------------|
| `Crc_Set_Reset( crcId )` | Loads the initial value - start of a new calculation |
| `Crc_Set_Data( crcId, data, dataCnt )` | Feeds data (continues from the current CRC value) |
| `Crc_Get_Data( crcId, &crcValue )` | Reads the current CRC value (masked by polynomial size) |
| `Crc_Calc_Block( crcId, data, dataCnt, &crcValue )` | Reset + feed + read in one call |

#### Configuration

| Function | Description |
|----------|-------------|
| `Crc_Set_Polynomial( crcId, polynomial, polySize )` / `Crc_Get_Polynomial( crcId, &polynomial, &polySize )` | Polynomial and its size |
| `Crc_Set_InitValue( crcId, initValue )` / `Crc_Get_InitValue( crcId, &initValue )` | Initial value |
| `Crc_Set_InputDataOrder( crcId, order )` / `Crc_Get_InputDataOrder( crcId, &order )` | Input data bit order |
| `Crc_Set_OutputDataOrder( crcId, order )` / `Crc_Get_OutputDataOrder( crcId, &order )` | Output data (result) bit order |
| `Crc_Set_InputDataSize( crcId, size )` / `Crc_Get_InputDataSize( crcId, &size )` | Input data item size (8 / 16 / 32 bits) |

#### Independent data register

| Function | Description |
|----------|-------------|
| `Crc_Set_IndependentData( crcId, value )` / `Crc_Get_IndependentData( crcId, &value )` | General purpose 8-bit register (0 - `CRC_INDEP_DATA_MAX`), not affected by the calculation |

#### Non-blocking (DMA) calculation

| Function | Description |
|----------|-------------|
| `Crc_Set_TransferStart( crcId, data, dataCnt )` | Starts feeding data by DMA2 stream (1 - 65535 items of the input data size, multiple of item size, aligned data) |
| `Crc_Set_TransferStop( crcId )` | Stops running transfer |
| `Crc_Get_TransferState( crcId, &state )` | `CRC_TRANSFER_IDLE` / `CRC_TRANSFER_BUSY` / `CRC_TRANSFER_ERROR` |
| `Crc_Set_TransferCpltActive( crcId )` / `Crc_Set_TransferCpltInactive( crcId )` / `Crc_Get_TransferCpltState( crcId, &state )` | Transfer complete callback enable |
| `Crc_Set_TransferCpltIsr( crcId, callback )` / `Crc_Get_TransferCpltIsr( crcId, &callback )` | Transfer complete callback |
| `Crc_Set_HalfTransferActive( crcId )` / `Crc_Set_HalfTransferInactive( crcId )` / `Crc_Get_HalfTransferState( crcId, &state )` | Half transfer callback enable |
| `Crc_Set_HalfTransferIsr( crcId, callback )` / `Crc_Get_HalfTransferIsr( crcId, &callback )` | Half transfer callback |
| `Crc_Set_TransferErrorActive( crcId )` / `Crc_Set_TransferErrorInactive( crcId )` / `Crc_Get_TransferErrorState( crcId, &state )` | Transfer error callback enable |
| `Crc_Set_TransferErrorIsr( crcId, callback )` / `Crc_Get_TransferErrorIsr( crcId, &callback )` | Transfer error callback - called with `crc_XferErrorId_t` |

All functions return `crc_RequestState_t` (except version and task); results are returned through output parameters.

### Configuration

| Item | Description |
|------|-------------|
| `PeriphId` | CRC peripheral (`CRC_PERIPH_1`) |
| `Polynomial` | Polynomial coefficients without the highest bit (e.g. `0x04C11DB7`). Must be **odd** and fit into `PolySize`. |
| `PolySize` | `CRC_DATA_SIZE_7BITS` / `8BITS` / `16BITS` / `32BITS` - the result has the same size |
| `InitValue` | Initial value, only bits within `PolySize` are used |
| `InputDataOrder` | `CRC_DATA_ORDER_NORMAL` / `REV_BYTE` / `REV_HALFWORD` / `REV_WORD` |
| `OutputDataOrder` | `CRC_DATA_ORDER_NORMAL` / `CRC_DATA_ORDER_REV_WORD` (bit-reversed result) |
| `InputDataSize` | `CRC_INPUT_SIZE_8BITS` / `16BITS` / `32BITS` - data item size (data register access width) |
| `DmaState` | `CRC_FUNCTION_ACTIVE` enables non-blocking calculation |
| `DmaPeriphId`, `DmaChannelId`, `DmaPriority` | DMA stream used for non-blocking calculation (`CRC_DMA_PERIPH_2` only - memory to memory transfer) |
| `TransferCpltIsr`, `HalfTransferIsr`, `TransferErrorIsr` | User callbacks (can be `NULL`), activated by `Crc_Init()` if set |

Invalid configuration is rejected by `Crc_Init()` **before** any register is written.

### Common algorithms

| Algorithm | Polynomial | Size | Init | Input order | Output order | Final XOR (SW) | Check ("123456789") |
|-----------|-----------|------|------|-------------|--------------|----------------|---------------------|
| CRC-32/ISO-HDLC (Ethernet, zlib, default) | `0x04C11DB7` | 32 | `0xFFFFFFFF` | `REV_BYTE` | `REV_WORD` | `0xFFFFFFFF` | `0xCBF43926` |
| CRC-32/MPEG-2 | `0x04C11DB7` | 32 | `0xFFFFFFFF` | `NORMAL` | `NORMAL` | - | `0x0376E6E7` |
| CRC-16/MODBUS | `0x8005` | 16 | `0xFFFF` | `REV_BYTE` | `REV_WORD` | - | `0x4B37` |
| CRC-16/IBM-3740 (CCITT-FALSE) | `0x1021` | 16 | `0xFFFF` | `NORMAL` | `NORMAL` | - | `0x29B1` |
| CRC-8/SMBUS | `0x07` | 8 | `0x00` | `NORMAL` | `NORMAL` | - | `0xF4` |
| CRC-7/MMC | `0x09` | 7 | `0x00` | `NORMAL` | `NORMAL` | - | `0x75` |

### Notes

- **New calculation** - call `Crc_Set_Reset()` (or `Crc_Calc_Block()`) before every new calculation; `Crc_Set_Data()` and `Crc_Set_TransferStart()` continue from the current CRC value.
- **Configuration change** - `Crc_Set_Polynomial()` / `Crc_Set_InitValue()` do not change the current CRC value, call `Crc_Set_Reset()` afterwards.
- **Non-blocking calculation** - the data buffer must stay valid and unchanged until the end of the transfer and must be accessible by DMA2 (not held in the Cortex-M7 data cache). While the transfer is running, configuration, data feed and result read return `CRC_REQUEST_ERROR`.
- **DMA error** - the transfer is stopped, the state is `CRC_TRANSFER_ERROR` (no transfer complete callback) and the transfer error callback (if active) is called with `CRC_XFER_ERROR_DMA_TRANSFER`. The state is cleared by `Crc_Set_Reset()`.
- **DMA stream** - the stream is released by repeated `Crc_Init()` and by `Crc_Deinit()`. It must not be shared with other modules.

---

## ⚙️ Typical Usage Example

```c
#include "Crc_Port.h"

static volatile uint8_t crcDone = 0u;

static void App_CrcDone( void )
{
    crcDone = 1u;
}

int main(void)
{
    static const uint8_t data[ ] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    crc_Config_t         crcConfig;
    crc_Data_t           crcValue = 0u;
    crc_RequestState_t   crcState = CRC_REQUEST_ERROR;

    /* CRC-32/ISO-HDLC (Ethernet, zlib) - default configuration */
    crcState = Crc_Get_DefaultConfig( &crcConfig );

    crcConfig.DmaState        = CRC_FUNCTION_ACTIVE;     // Non-blocking calculation
    crcConfig.DmaPeriphId     = CRC_DMA_PERIPH_1;
    crcConfig.DmaChannelId    = CRC_DMA_CHANNEL_0;
    crcConfig.TransferCpltIsr = App_CrcDone;

    if( CRC_REQUEST_OK == crcState )
    {
        crcState = Crc_Init( &crcConfig );
    }

    /* Blocking calculation */
    if( CRC_REQUEST_OK == crcState )
    {
        crcState = Crc_Calc_Block( CRC_PERIPH_1, data, sizeof( data ), &crcValue );
        crcValue ^= 0xFFFFFFFFu;                         // Final XOR -> 0xCBF43926
    }

    /* Non-blocking calculation */
    if( CRC_REQUEST_OK == crcState )
    {
        crcState = Crc_Set_Reset( CRC_PERIPH_1 );
    }

    if( CRC_REQUEST_OK == crcState )
    {
        crcState = Crc_Set_TransferStart( CRC_PERIPH_1, data, sizeof( data ) );
    }

    while (1)
    {
        if( 1u == crcDone )
        {
            crcDone = 0u;
            (void)Crc_Get_Data( CRC_PERIPH_1, &crcValue );
            crcValue ^= 0xFFFFFFFFu;
        }
    }
}
```

---

## 🧾 Branching Strategy

Each STM32 family has its own branch:

| Branch | Description |
|--------|--------------|
| `STM32G4` | MCAL driver for STM32G4 family |
| `STM32U5` | MCAL driver for STM32U5 family |
| `STM32L4` | MCAL driver for STM32L4 family |
| `STM32H5` | MCAL driver for STM32H5 family |
| `STM32F4` | MCAL driver for STM32F4 family |
| `STM32F7` | MCAL driver for STM32F7 family |

These branches contain family-specific register definitions and RCC/DMA bindings while maintaining a common interface.

---

## 🧩 Dependencies

- **Rcc_Lib** – Peripheral clock and reset (`RCC_PERIPH_CRC`)
- **Dma_Lib** – Non-blocking calculation (DMA2 memory to memory transfer)
- **RAL (Register Abstraction Layer)** – Used internally to access low-level registers  

All mandatory RCC and DMA configurations are handled internally.

---

## 🧱 Example Directory Structure

```
Crc/
├── Crc_Port.h
├── Crc_Types.h
├── Crc.c
├── Crc.h
├── CMakeLists.txt
├── Tests/
│   ├── UnitTests/
│   └── IntegrationTests/
├── LICENSE.md
└── README.md
```

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
