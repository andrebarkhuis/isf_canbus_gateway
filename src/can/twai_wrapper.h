#pragma once

#include <cstdint>

/**
 * @brief The ESP32's built-in CAN controller (TWAI) on the ISF bus, 500 kbit/s, pins TX 7 / RX 6.
 *
 * This is the ICanTransport role of ToyotaLens (docs/direct-can-over-serial.md §4.2): one frame at a time and
 * nothing more. It puts an 8-byte frame on the bus and hands back every frame that arrives; IsoTp does the
 * segmentation, flow control, timers and id filtering on top of it. The header has no ESP-IDF types so the
 * host self-test can substitute its own scripted implementation (test/host/fake_twai_wrapper.cpp).
 */
class TwaiWrapper
{
public:
    /// The interface column of the candump trace lines.
    static constexpr const char *INTERFACE = "isf";

    TwaiWrapper();
    ~TwaiWrapper();

    /// Installs and starts the TWAI driver at 500 kbit/s with an accept-all filter.
    bool initialize();

    bool isOpen() const;

    /// Puts one frame on the bus; false when the driver did not accept it within N_As. Every frame is traced.
    bool send(uint32_t id, const uint8_t *data, uint8_t len);

    /// The next received frame, any id; false when none arrived within `timeoutMs`. Every frame is traced.
    bool tryReceive(uint32_t &id, uint8_t *data, uint8_t &len, uint32_t timeoutMs);

    /// Empties the receive queue, like IOCTL_CLEAR_RX_BUFFER before a request.
    void drain();

    /// Reads the driver's alerts: logs bus errors and starts recovery from bus-off, the one condition that is a
    /// bus fault (specification §8.5); a silent ECU never is.
    void checkAlerts();

private:
    static constexpr int TWAI_TX = 7;
    static constexpr int TWAI_RX = 6;
    bool open_ = false;
};
