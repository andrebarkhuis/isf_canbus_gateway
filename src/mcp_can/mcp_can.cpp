#include "mcp_can/mcp_can.h"
#include "mcp_can/mcp_can_dfs.h"

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "logger/logger.h"

// ---------------------------------------------------------------------------------------------------------
// SPI primitives on the ESP-IDF SPI master driver. Each MCP2515 command is one full-duplex transaction with the
// chip select held low for its whole length, exactly as the Arduino version did with SELECT / transfer x n /
// UNSELECT. The register-level logic below the primitives is unchanged from the version verified on the bench.
// ---------------------------------------------------------------------------------------------------------

namespace
{
constexpr int PIN_SCK = 12;   // ESP32-CAN-X2: MCP2515 on SPI2 (FSPI) pins, CS on GPIO 10
constexpr int PIN_MISO = 13;
constexpr int PIN_MOSI = 11;
constexpr int SPI_CLOCK_HZ = 10 * 1000 * 1000;
constexpr spi_host_device_t SPI_HOST_ID = SPI2_HOST;
/// The longest command: WRITE + address + up to 14 register bytes.
constexpr size_t MAX_TRANSFER = 2 + 14;

void delayMs(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
} // namespace

void MCP_CAN::transfer(const byte *tx, byte *rx, size_t length)
{
    if (spi == nullptr || length == 0 || length > MAX_TRANSFER)
    {
        return;
    }
    spi_transaction_t t = {};
    t.length = length * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t err = spi_device_polling_transmit(spi, &t);
    if (err != ESP_OK)
    {
        LOG_ERROR("MCP2515 SPI transfer failed: %d", err);
    }
}

/*********************************************************************************************************
** Function name:           mcp2515_reset
** Descriptions:            reset the device
*********************************************************************************************************/
void MCP_CAN::mcp2515_reset(void)
{
    byte tx[1] = {MCP_RESET};
    byte rx[1] = {0};
    transfer(tx, rx, 1);
    delayMs(10);
}

/*********************************************************************************************************
** Function name:           mcp2515_readRegister
** Descriptions:            read register
*********************************************************************************************************/
byte MCP_CAN::mcp2515_readRegister(const byte address)
{
    byte tx[3] = {MCP_READ, address, 0x00};
    byte rx[3] = {0, 0, 0};
    transfer(tx, rx, 3);
    return rx[2];
}

/*********************************************************************************************************
** Function name:           mcp2515_readRegisterS
** Descriptions:            read registerS
*********************************************************************************************************/
void MCP_CAN::mcp2515_readRegisterS(const byte address, byte values[], const byte n)
{
    byte count = n < CAN_MAX_CHAR_IN_MESSAGE ? n : CAN_MAX_CHAR_IN_MESSAGE;
    byte tx[MAX_TRANSFER] = {MCP_READ, address};
    byte rx[MAX_TRANSFER] = {0};
    // mcp2515 has auto-increment of address-pointer
    transfer(tx, rx, 2 + count);
    for (byte i = 0; i < count; i++)
    {
        values[i] = rx[2 + i];
    }
}

/*********************************************************************************************************
** Function name:           mcp2515_setRegister
** Descriptions:            set register
*********************************************************************************************************/
void MCP_CAN::mcp2515_setRegister(const byte address, const byte value)
{
    byte tx[3] = {MCP_WRITE, address, value};
    byte rx[3] = {0, 0, 0};
    transfer(tx, rx, 3);
}

/*********************************************************************************************************
** Function name:           mcp2515_setRegisterS
** Descriptions:            set registerS
*********************************************************************************************************/
void MCP_CAN::mcp2515_setRegisterS(const byte address, const byte values[], const byte n)
{
    byte count = n < (MAX_TRANSFER - 2) ? n : (MAX_TRANSFER - 2);
    byte tx[MAX_TRANSFER] = {MCP_WRITE, address};
    byte rx[MAX_TRANSFER] = {0};
    for (byte i = 0; i < count; i++)
    {
        tx[2 + i] = values[i];
    }
    transfer(tx, rx, 2 + count);
}

/*********************************************************************************************************
** Function name:           mcp2515_modifyRegister
** Descriptions:            set bit of one register
*********************************************************************************************************/
void MCP_CAN::mcp2515_modifyRegister(const byte address, const byte mask, const byte data)
{
    byte tx[4] = {MCP_BITMOD, address, mask, data};
    byte rx[4] = {0, 0, 0, 0};
    transfer(tx, rx, 4);
}

/*********************************************************************************************************
** Function name:           mcp2515_readStatus
** Descriptions:            read mcp2515's Status
*********************************************************************************************************/
byte MCP_CAN::mcp2515_readStatus(void)
{
    byte tx[2] = {MCP_READ_STATUS, 0x00};
    byte rx[2] = {0, 0};
    transfer(tx, rx, 2);
    return rx[1];
}

/*********************************************************************************************************
** Function name:           mcp2515_setCANCTRL_Mode
** Descriptions:            set control mode
*********************************************************************************************************/
byte MCP_CAN::mcp2515_setCANCTRL_Mode(const byte newmode)
{
    byte i;

    mcp2515_modifyRegister(MCP_CANCTRL, MODE_MASK, newmode);

    i = mcp2515_readRegister(MCP_CANCTRL);
    i &= MODE_MASK;

    if (i == newmode)
    {
        return MCP2515_OK;
    }

    return MCP2515_FAIL;
}

/*********************************************************************************************************
** Function name:           mcp2515_configRate
** Descriptions:            set boadrate
*********************************************************************************************************/
byte MCP_CAN::mcp2515_configRate(const byte canSpeed)
{
    byte set, cfg1, cfg2, cfg3;
    set = 1;
    switch (canSpeed)
    {
    case (CAN_5KBPS):
        cfg1 = MCP_16MHz_5kBPS_CFG1;
        cfg2 = MCP_16MHz_5kBPS_CFG2;
        cfg3 = MCP_16MHz_5kBPS_CFG3;
        break;

    case (CAN_10KBPS):
        cfg1 = MCP_16MHz_10kBPS_CFG1;
        cfg2 = MCP_16MHz_10kBPS_CFG2;
        cfg3 = MCP_16MHz_10kBPS_CFG3;
        break;

    case (CAN_20KBPS):
        cfg1 = MCP_16MHz_20kBPS_CFG1;
        cfg2 = MCP_16MHz_20kBPS_CFG2;
        cfg3 = MCP_16MHz_20kBPS_CFG3;
        break;

    case (CAN_25KBPS):
        cfg1 = MCP_16MHz_25kBPS_CFG1;
        cfg2 = MCP_16MHz_25kBPS_CFG2;
        cfg3 = MCP_16MHz_25kBPS_CFG3;
        break;

    case (CAN_31K25BPS):
        cfg1 = MCP_16MHz_31k25BPS_CFG1;
        cfg2 = MCP_16MHz_31k25BPS_CFG2;
        cfg3 = MCP_16MHz_31k25BPS_CFG3;
        break;

    case (CAN_33KBPS):
        cfg1 = MCP_16MHz_33kBPS_CFG1;
        cfg2 = MCP_16MHz_33kBPS_CFG2;
        cfg3 = MCP_16MHz_33kBPS_CFG3;
        break;

    case (CAN_40KBPS):
        cfg1 = MCP_16MHz_40kBPS_CFG1;
        cfg2 = MCP_16MHz_40kBPS_CFG2;
        cfg3 = MCP_16MHz_40kBPS_CFG3;
        break;

    case (CAN_50KBPS):
        cfg1 = MCP_16MHz_50kBPS_CFG1;
        cfg2 = MCP_16MHz_50kBPS_CFG2;
        cfg3 = MCP_16MHz_50kBPS_CFG3;
        break;

    case (CAN_80KBPS):
        cfg1 = MCP_16MHz_80kBPS_CFG1;
        cfg2 = MCP_16MHz_80kBPS_CFG2;
        cfg3 = MCP_16MHz_80kBPS_CFG3;
        break;

    case (CAN_83K3BPS):
        cfg1 = MCP_16MHz_83k3BPS_CFG1;
        cfg2 = MCP_16MHz_83k3BPS_CFG2;
        cfg3 = MCP_16MHz_83k3BPS_CFG3;
        break;

    case (CAN_95KBPS):
        cfg1 = MCP_16MHz_95kBPS_CFG1;
        cfg2 = MCP_16MHz_95kBPS_CFG2;
        cfg3 = MCP_16MHz_95kBPS_CFG3;
        break;

    case (CAN_100KBPS):
        cfg1 = MCP_16MHz_100kBPS_CFG1;
        cfg2 = MCP_16MHz_100kBPS_CFG2;
        cfg3 = MCP_16MHz_100kBPS_CFG3;
        break;

    case (CAN_125KBPS):
        cfg1 = MCP_16MHz_125kBPS_CFG1;
        cfg2 = MCP_16MHz_125kBPS_CFG2;
        cfg3 = MCP_16MHz_125kBPS_CFG3;
        break;

    case (CAN_200KBPS):
        cfg1 = MCP_16MHz_200kBPS_CFG1;
        cfg2 = MCP_16MHz_200kBPS_CFG2;
        cfg3 = MCP_16MHz_200kBPS_CFG3;
        break;

    case (CAN_250KBPS):
        cfg1 = MCP_16MHz_250kBPS_CFG1;
        cfg2 = MCP_16MHz_250kBPS_CFG2;
        cfg3 = MCP_16MHz_250kBPS_CFG3;
        break;

    case (CAN_500KBPS):
        cfg1 = MCP_16MHz_500kBPS_CFG1;
        cfg2 = MCP_16MHz_500kBPS_CFG2;
        cfg3 = MCP_16MHz_500kBPS_CFG3;
        break;

    case (CAN_666KBPS):
        cfg1 = MCP_16MHz_666kBPS_CFG1;
        cfg2 = MCP_16MHz_666kBPS_CFG2;
        cfg3 = MCP_16MHz_666kBPS_CFG3;
        break;

    case (CAN_1000KBPS):
        cfg1 = MCP_16MHz_1000kBPS_CFG1;
        cfg2 = MCP_16MHz_1000kBPS_CFG2;
        cfg3 = MCP_16MHz_1000kBPS_CFG3;
        break;

    default:
        set = 0;
        break;
    }

    if (set)
    {
        mcp2515_setRegister(MCP_CNF1, cfg1);
        mcp2515_setRegister(MCP_CNF2, cfg2);
        mcp2515_setRegister(MCP_CNF3, cfg3);
        return MCP2515_OK;
    }
    else
    {
        return MCP2515_FAIL;
    }
}

/*********************************************************************************************************
** Function name:           mcp2515_initCANBuffers
** Descriptions:            init canbuffers
*********************************************************************************************************/
void MCP_CAN::mcp2515_initCANBuffers(void)
{
    byte i, a1, a2, a3;

    a1 = MCP_TXB0CTRL;
    a2 = MCP_TXB1CTRL;
    a3 = MCP_TXB2CTRL;
    for (i = 0; i < 14; i++) // in-buffer loop
    {
        mcp2515_setRegister(a1, 0);
        mcp2515_setRegister(a2, 0);
        mcp2515_setRegister(a3, 0);
        a1++;
        a2++;
        a3++;
    }
    mcp2515_setRegister(MCP_RXB0CTRL, 0);
    mcp2515_setRegister(MCP_RXB1CTRL, 0);
}

/*********************************************************************************************************
** Function name:           mcp2515_init
** Descriptions:            init the device
*********************************************************************************************************/
byte MCP_CAN::mcp2515_init(const byte canSpeed)
{
    mcp2515_reset();

    delayMs(10);

    byte res = mcp2515_configRate(canSpeed);

    if (res == MCP2515_OK)
    {
        // init canbuffers
        mcp2515_initCANBuffers();

        // interrupt mode
        mcp2515_setRegister(MCP_CANINTE, MCP_RX0IF | MCP_RX1IF);

#if (DEBUG_RXANY == 1)
        // enable both receive-buffers to receive any message and enable rollover
        mcp2515_modifyRegister(MCP_RXB0CTRL,
                               MCP_RXB_RX_MASK | MCP_RXB_BUKT_MASK,
                               MCP_RXB_RX_ANY | MCP_RXB_BUKT_MASK);
        mcp2515_modifyRegister(MCP_RXB1CTRL, MCP_RXB_RX_MASK,
                               MCP_RXB_RX_ANY);
#else
        // enable both receive-buffers to receive messages with std. and ext. identifiers and enable rollover
        mcp2515_modifyRegister(MCP_RXB0CTRL,
                               MCP_RXB_RX_MASK | MCP_RXB_BUKT_MASK,
                               MCP_RXB_RX_STDEXT | MCP_RXB_BUKT_MASK);
        mcp2515_modifyRegister(MCP_RXB1CTRL, MCP_RXB_RX_MASK,
                               MCP_RXB_RX_STDEXT);
#endif
    }

    return res;
}

/*********************************************************************************************************
** Function name:           mcp2515_write_id
** Descriptions:            write can id
*********************************************************************************************************/
void MCP_CAN::mcp2515_write_id(const byte mcp_addr, const byte ext, const unsigned long id)
{
    uint16_t canid;
    byte tbufdata[4];

    canid = (uint16_t)(id & 0x0FFFF);

    if (ext == 1)
    {
        tbufdata[MCP_EID0] = (byte)(canid & 0xFF);
        tbufdata[MCP_EID8] = (byte)(canid >> 8);
        canid = (uint16_t)(id >> 16);
        tbufdata[MCP_SIDL] = (byte)(canid & 0x03);
        tbufdata[MCP_SIDL] += (byte)((canid & 0x1C) << 3);
        tbufdata[MCP_SIDL] |= MCP_TXB_EXIDE_M;
        tbufdata[MCP_SIDH] = (byte)(canid >> 5);
    }
    else
    {
        tbufdata[MCP_SIDH] = (byte)(canid >> 3);
        tbufdata[MCP_SIDL] = (byte)((canid & 0x07) << 5);
        tbufdata[MCP_EID0] = 0;
        tbufdata[MCP_EID8] = 0;
    }
    mcp2515_setRegisterS(mcp_addr, tbufdata, 4);
}

/*********************************************************************************************************
** Function name:           mcp2515_read_id
** Descriptions:            read can id
*********************************************************************************************************/
void MCP_CAN::mcp2515_read_id(const byte mcp_addr, byte *ext, unsigned long *id)
{
    byte tbufdata[4];

    *ext = 0;
    *id = 0;

    mcp2515_readRegisterS(mcp_addr, tbufdata, 4);

    *id = (tbufdata[MCP_SIDH] << 3) + (tbufdata[MCP_SIDL] >> 5);

    if ((tbufdata[MCP_SIDL] & MCP_TXB_EXIDE_M) == MCP_TXB_EXIDE_M)
    {
        // extended id
        *id = (*id << 2) + (tbufdata[MCP_SIDL] & 0x03);
        *id = (*id << 8) + tbufdata[MCP_EID8];
        *id = (*id << 8) + tbufdata[MCP_EID0];
        *ext = 1;
    }
}

/*********************************************************************************************************
** Function name:           mcp2515_write_canMsg
** Descriptions:            write msg
*********************************************************************************************************/
void MCP_CAN::mcp2515_write_canMsg(const byte buffer_sidh_addr, int rtrBit)
{
    byte mcp_addr;
    mcp_addr = buffer_sidh_addr;
    mcp2515_setRegisterS(mcp_addr + 5, dta, dta_len); // write data bytes
    // Serial.print("RTR: ");
    // Serial.println(rtrBit);
    if (rtrBit == 1) // if RTR set bit in byte
    {
        dta_len |= MCP_RTR_MASK;
    }
    mcp2515_setRegister((mcp_addr + 4), dta_len); // write the RTR and DLC
    mcp2515_write_id(mcp_addr, ext_flg, can_id);  // write CAN id
}

/*********************************************************************************************************
** Function name:           mcp2515_read_canMsg
** Descriptions:            read message
*********************************************************************************************************/
void MCP_CAN::mcp2515_read_canMsg(const byte buffer_sidh_addr) // read can msg
{
    byte mcp_addr, ctrl;

    mcp_addr = buffer_sidh_addr;
    mcp2515_read_id(mcp_addr, &ext_flg, &can_id);
    ctrl = mcp2515_readRegister(mcp_addr - 1);
    dta_len = mcp2515_readRegister(mcp_addr + 4);

    rtr = (ctrl & 0x08) ? 1 : 0;

    dta_len &= MCP_DLC_MASK;
    mcp2515_readRegisterS(mcp_addr + 5, &(dta[0]), dta_len);
}

/*********************************************************************************************************
** Function name:           mcp2515_start_transmit
** Descriptions:            start transmit
*********************************************************************************************************/
void MCP_CAN::mcp2515_start_transmit(const byte mcp_addr) // start transmit
{
    mcp2515_modifyRegister(mcp_addr - 1, MCP_TXB_TXREQ_M, MCP_TXB_TXREQ_M);
}

/*********************************************************************************************************
** Function name:           mcp2515_getNextFreeTXBuf
** Descriptions:            get Next free txbuf
*********************************************************************************************************/
byte MCP_CAN::mcp2515_getNextFreeTXBuf(byte *txbuf_n) // get Next free txbuf
{
    byte res, i, ctrlval;
    byte ctrlregs[MCP_N_TXBUFFERS] = {MCP_TXB0CTRL, MCP_TXB1CTRL, MCP_TXB2CTRL};

    res = MCP_ALLTXBUSY;
    *txbuf_n = 0x00;

    // check all 3 TX-Buffers
    for (i = 0; i < MCP_N_TXBUFFERS; i++)
    {
        ctrlval = mcp2515_readRegister(ctrlregs[i]);
        if ((ctrlval & MCP_TXB_TXREQ_M) == 0)
        {
            *txbuf_n = ctrlregs[i] + 1; // return SIDH-address of Buffer
            res = MCP2515_OK;
            return res; // ! function exit
        }
    }
    return res;
}

/*********************************************************************************************************
** Function name:           constructor
** Descriptions:            Default constructor for singleton pattern
*********************************************************************************************************/
MCP_CAN::MCP_CAN()
    : SPICS(10), spi(nullptr)
{
}

MCP_CAN::MCP_CAN(byte _CS)
    : SPICS(_CS), spi(nullptr)
{
}

/*********************************************************************************************************
** Function name:           init
** Descriptions:            init can and set speed
*********************************************************************************************************/
byte MCP_CAN::begin()
{
    if (spi == nullptr)
    {
        spi_bus_config_t bus = {};
        bus.sclk_io_num = PIN_SCK;
        bus.miso_io_num = PIN_MISO;
        bus.mosi_io_num = PIN_MOSI;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = MAX_TRANSFER;
        esp_err_t err = spi_bus_initialize(SPI_HOST_ID, &bus, SPI_DMA_DISABLED);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) // INVALID_STATE: the bus is already initialised
        {
            LOG_ERROR("spi_bus_initialize failed: %d", err);
            return CAN_FAILINIT;
        }
        spi_device_interface_config_t dev = {};
        dev.clock_speed_hz = SPI_CLOCK_HZ;
        dev.mode = 0;
        dev.spics_io_num = SPICS;
        dev.queue_size = 1;
        err = spi_bus_add_device(SPI_HOST_ID, &dev, &spi);
        if (err != ESP_OK)
        {
            LOG_ERROR("spi_bus_add_device failed: %d", err);
            spi = nullptr;
            return CAN_FAILINIT;
        }
    }

    mcp2515_setCANCTRL_Mode(MODE_CONFIG);

    byte res = mcp2515_init(CAN_500KBPS);
    if (res != MCP2515_OK)
    {
        LOG_ERROR("MCP2515 init failed (no answer on SPI?)");
        return CAN_FAILINIT;
    }

    res = mcp2515_setCANCTRL_Mode(MODE_NORMAL);

    return ((res == MCP2515_OK) ? CAN_OK : CAN_FAILINIT);
}

/*********************************************************************************************************
** Function name:           init_Mask
** Descriptions:            init canid Masks
*********************************************************************************************************/
byte MCP_CAN::init_Mask(byte num, byte ext, unsigned long ulData)
{
    byte res = MCP2515_OK;
#if DEBUG_MCP_CAN
    LOG_DEBUG("Begin to set Mask!!");
#else
    delayMs(5);
#endif
    res = mcp2515_setCANCTRL_Mode(MODE_CONFIG);
    if (res > 0)
    {
#if DEBUG_MCP_CAN
        LOG_DEBUG("Enter setting mode fall");
#else
        delayMs(5);
#endif
        return res;
    }

    if (num == 0)
    {
        mcp2515_write_id(MCP_RXM0SIDH, ext, ulData);
    }
    else if (num == 1)
    {
        mcp2515_write_id(MCP_RXM1SIDH, ext, ulData);
    }
    else
        res = MCP2515_FAIL;

    res = mcp2515_setCANCTRL_Mode(MODE_NORMAL);
    if (res > 0)
    {
#if DEBUG_MCP_CAN
        LOG_DEBUG("Enter normal mode fall");
#else
        delayMs(5);
#endif
        return res;
    }
#if DEBUG_MCP_CAN
    LOG_DEBUG("set Mask success!!");
#else
    delayMs(5);
#endif
    return res;
}

/*********************************************************************************************************
** Function name:           init_Filt
** Descriptions:            init canid filters
*********************************************************************************************************/
byte MCP_CAN::init_Filt(byte num, byte ext, unsigned long ulData)
{
    byte res = MCP2515_OK;
#if DEBUG_MCP_CAN
    LOG_DEBUG("Begin to set Filter!!");
#else
    delayMs(5);
#endif
    res = mcp2515_setCANCTRL_Mode(MODE_CONFIG);
    if (res > 0)
    {
#if DEBUG_MCP_CAN
        LOG_DEBUG("Enter setting mode fall");
#else
        delayMs(5);
#endif
        return res;
    }

    switch (num)
    {
    case 0:
        mcp2515_write_id(MCP_RXF0SIDH, ext, ulData);
        break;

    case 1:
        mcp2515_write_id(MCP_RXF1SIDH, ext, ulData);
        break;

    case 2:
        mcp2515_write_id(MCP_RXF2SIDH, ext, ulData);
        break;

    case 3:
        mcp2515_write_id(MCP_RXF3SIDH, ext, ulData);
        break;

    case 4:
        mcp2515_write_id(MCP_RXF4SIDH, ext, ulData);
        break;

    case 5:
        mcp2515_write_id(MCP_RXF5SIDH, ext, ulData);
        break;

    default:
        res = MCP2515_FAIL;
    }

    res = mcp2515_setCANCTRL_Mode(MODE_NORMAL);
    if (res > 0)
    {
#if DEBUG_MCP_CAN
        LOG_DEBUG("Enter normal mode fallSet filter fail!!");
#else
        delayMs(5);
#endif
        return res;
    }
#if DEBUG_MCP_CAN
    LOG_DEBUG("set Filter success!!");
#else   
    delayMs(5);
#endif

    return res;
}

/*********************************************************************************************************
** Function name:           write_Mask
** Descriptions:            Write a CAN mask (assumes already in Config mode)
*********************************************************************************************************/
byte MCP_CAN::write_Mask(byte num, byte ext, unsigned long ulData)
{
    // No mode switching here. Assume caller puts us in CONFIG mode already.
    byte res = MCP2515_OK;

    if (num == 0)
    {
        mcp2515_write_id(MCP_RXM0SIDH, ext, ulData);
    }
    else if (num == 1)
    {
        mcp2515_write_id(MCP_RXM1SIDH, ext, ulData);
    }
    else
    {
        res = MCP2515_FAIL;
    }

    return res;
}

/*********************************************************************************************************
** Function name:           write_Filt
** Descriptions:            Write a CAN filter (assumes already in Config mode)
*********************************************************************************************************/
byte MCP_CAN::write_Filt(byte num, byte ext, unsigned long ulData)
{
    // No mode switching here. Assume caller puts us in CONFIG mode already.
    byte res = MCP2515_OK;

    switch (num)
    {
    case 0:
        mcp2515_write_id(MCP_RXF0SIDH, ext, ulData);
        break;
    case 1:
        mcp2515_write_id(MCP_RXF1SIDH, ext, ulData);
        break;
    case 2:
        mcp2515_write_id(MCP_RXF2SIDH, ext, ulData);
        break;
    case 3:
        mcp2515_write_id(MCP_RXF3SIDH, ext, ulData);
        break;
    case 4:
        mcp2515_write_id(MCP_RXF4SIDH, ext, ulData);
        break;
    case 5:
        mcp2515_write_id(MCP_RXF5SIDH, ext, ulData);
        break;
    default:
        res = MCP2515_FAIL;
        break;
    }

    return res;
}

/*********************************************************************************************************
** Function name:           setMsg
** Descriptions:            set can message, such as dlc, id, dta[] and so on
*********************************************************************************************************/
byte MCP_CAN::setMsg(unsigned long id, byte ext, byte len, byte rtr, byte *pData)
{
    ext_flg = ext;
    can_id = id;
    // dta_len     = min(len, MAX_CHAR_IN_MESSAGE);
    dta_len = len > MAX_CHAR_IN_MESSAGE ? MAX_CHAR_IN_MESSAGE : len;
    this->rtr = rtr;
    for (int i = 0; i < dta_len; i++)
    {
        dta[i] = *(pData + i);
    }
    return MCP2515_OK;
}

/*********************************************************************************************************
** Function name:           setMsg
** Descriptions:            set can message, such as dlc, id, dta[] and so on
*********************************************************************************************************/
byte MCP_CAN::setMsg(unsigned long id, byte ext, byte len, byte *pData)
{
    return setMsg(id, ext, len, 0, pData);
}

/*********************************************************************************************************
** Function name:           clearMsg
** Descriptions:            set all message to zero
*********************************************************************************************************/
byte MCP_CAN::clearMsg()
{
    can_id = 0;
    dta_len = 0;
    ext_flg = 0;
    rtr = 0;
    filhit = 0;

    for (int i = 0; i < dta_len; i++)
    {
        dta[i] = 0x00;
    }

    return MCP2515_OK;
}

/*********************************************************************************************************
** Function name:           sendMsg
** Descriptions:            send message
*********************************************************************************************************/
byte MCP_CAN::sendMsg(int rtrBit)
{
    byte res, res1, txbuf_n;
    uint16_t uiTimeOut = 0;

    do
    {
        res = mcp2515_getNextFreeTXBuf(&txbuf_n);
        uiTimeOut++;
    } while (res == MCP_ALLTXBUSY && (uiTimeOut < TIMEOUTVALUE));

    if (uiTimeOut == TIMEOUTVALUE)
    {
        return CAN_GETTXBFTIMEOUT;
    }

    uiTimeOut = 0;
    mcp2515_write_canMsg(txbuf_n, rtrBit);
    mcp2515_start_transmit(txbuf_n);

    do
    {
        uiTimeOut++;
        res1 = mcp2515_readRegister(txbuf_n - 1 /* the ctrl reg is located at txbuf_n-1 */); // read send buff ctrl reg
        res1 = res1 & 0x08;
    } while (res1 && (uiTimeOut < TIMEOUTVALUE));

    if (uiTimeOut == TIMEOUTVALUE) // send msg timeout
    {
        return CAN_SENDMSGTIMEOUT;
    }
    return CAN_OK;
}

/*********************************************************************************************************
** Function name:           sendMsgBuf
** Descriptions:            send buf
*********************************************************************************************************/
byte MCP_CAN::sendMsgBuf(unsigned long id, byte ext, byte rtr, byte len, byte *buf)
{
    setMsg(id, ext, len, rtr, buf);
    return sendMsg(rtr);
}

/*********************************************************************************************************
** Function name:           sendMsgBuf
** Descriptions:            send buf
*********************************************************************************************************/
byte MCP_CAN::sendMsgBuf(unsigned long id, byte ext, byte len, byte *buf)
{
    setMsg(id, ext, len, buf); // Pass the buffer as a pointer
    return sendMsg(0);
}

/*********************************************************************************************************
** Function name:           readMsg
** Descriptions:            read message
*********************************************************************************************************/
byte MCP_CAN::readMsg()
{
    byte stat, res;

    stat = mcp2515_readStatus();

    if (stat & MCP_STAT_RX0IF)
    {
        mcp2515_read_canMsg(MCP_RXBUF_0);
        mcp2515_modifyRegister(MCP_CANINTF, MCP_RX0IF, 0);
        res = CAN_OK;
    }
    else if (stat & MCP_STAT_RX1IF)
    {
        mcp2515_read_canMsg(MCP_RXBUF_1);
        mcp2515_modifyRegister(MCP_CANINTF, MCP_RX1IF, 0);
        res = CAN_OK;
    }
    else
    {
        res = CAN_NOMSG;
    }
    return res;
}

/*********************************************************************************************************
** Function name:           readMsgBuf
** Descriptions:            read message buf
*********************************************************************************************************/
byte MCP_CAN::readMsgBuf(byte *len, byte buf[])
{
    byte rc;

    rc = readMsg();

    if (rc == CAN_OK)
    {
        *len = dta_len;
        for (int i = 0; i < dta_len; i++)
        {
            buf[i] = dta[i];
        }
    }
    else
    {
        *len = 0;
    }
    return rc;
}

/*********************************************************************************************************
** Function name:           readMsgBufID
** Descriptions:            read message buf and can bus source ID
*********************************************************************************************************/
byte MCP_CAN::readMsgBufID(unsigned long *ID, byte *len, byte buf[])
{
    byte rc;
    rc = readMsg();

    if (rc == CAN_OK)
    {
        *len = dta_len;
        *ID = can_id;
        for (int i = 0; i < dta_len && i < MAX_CHAR_IN_MESSAGE; i++)
        {
            buf[i] = dta[i];
        }
    }
    else
    {
        *len = 0;
    }
    return rc;
}

/*********************************************************************************************************
** Function name:           checkReceive
** Descriptions:            check if got something
*********************************************************************************************************/
byte MCP_CAN::checkReceive(void)
{
    byte res;
    res = mcp2515_readStatus(); // RXnIF in Bit 1 and 0
    return ((res & MCP_STAT_RXIF_MASK) ? CAN_MSGAVAIL : CAN_NOMSG);
}

/*********************************************************************************************************
** Function name:           checkError
** Descriptions:            if something error
*********************************************************************************************************/
byte MCP_CAN::checkError(void)
{
    byte eflg = mcp2515_readRegister(MCP_EFLG);
    return ((eflg & MCP_EFLG_ERRORMASK) ? CAN_CTRLERROR : CAN_OK);
}

/*********************************************************************************************************
** Function name:           getCanId
** Descriptions:            when receive something you can get the can id!!
*********************************************************************************************************/
unsigned long MCP_CAN::getCanId(void)
{
    return can_id;
}

/*********************************************************************************************************
** Function name:           isRemoteRequest
** Descriptions:            when receive something you can check if it was a request
*********************************************************************************************************/
byte MCP_CAN::isRemoteRequest(void)
{
    return rtr;
}

/*********************************************************************************************************
** Function name:           isExtendedFrame
** Descriptions:            did we just receive standard 11bit frame or extended 29bit? 0 = std, 1 = ext
*********************************************************************************************************/
byte MCP_CAN::isExtendedFrame(void)
{
    return ext_flg;
}

/*********************************************************************************************************
** Function name:           peekMsgId
** Descriptions:            peek at the ID of a message in the buffer without removing it
*********************************************************************************************************/
byte MCP_CAN::peekMsgId(unsigned long *ID, byte *ext)
{
    byte status = mcp2515_readStatus();

    // Check if any messages are available
    if (!(status & MCP_STAT_RXIF_MASK))
    {
        return CAN_NOMSG;
    }

    // Determine which buffer has a message
    byte buffer_sidh_addr;
    if (status & MCP_STAT_RX0IF)
    {
        buffer_sidh_addr = MCP_RXBUF_0;
    }
    else if (status & MCP_STAT_RX1IF)
    {
        buffer_sidh_addr = MCP_RXBUF_1;
    }
    else
    {
        return CAN_NOMSG; // Shouldn't happen, but just in case
    }

    // Read ID without reading message data or clearing interrupt
    mcp2515_read_id(buffer_sidh_addr, ext, ID);

    return CAN_OK;
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
