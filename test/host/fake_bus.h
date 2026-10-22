#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// A CAN bus with a scripted ECU on it, for the self-test: the port of ToyotaLens SelfTest/FakeCanTransport.cs.
// Records every frame the channel sends, with the time it was sent, and answers each one with whatever the
// `ecu` script returns, each frame after its own delay. Frames can also be queued unsolicited. The TwaiWrapper
// of the firmware is implemented on top of it in fake_twai_wrapper.cpp.

struct Frame
{
    uint32_t id = 0;
    uint8_t data[8] = {0};
    uint8_t len = 8;

    std::string hex() const;
    std::string text() const; // "7E8#0661008400000100"
};

Frame frameOf(uint32_t id, const std::string &hex);
std::string hexOf(const uint8_t *data, size_t len);
std::vector<uint8_t> bytesOf(const std::string &hex);

struct FakeBus
{
    using Reply = std::vector<std::pair<int, Frame>>; // (delay ms, frame)

    std::function<Reply(const Frame &)> ecu;
    std::vector<std::pair<Frame, double>> sent;
    std::vector<std::pair<double, Frame>> pending;
    bool open = false;

    double nowMs() const;
    void send(const Frame &frame);
    void queue(int delayMs, const Frame &frame);
    bool tryReceive(Frame &frame, int timeoutMs);
    /// Like a real receive buffer: drops what has already arrived, not what is still on its way.
    void drain();
    void reset();
};

FakeBus &fakeBus();
