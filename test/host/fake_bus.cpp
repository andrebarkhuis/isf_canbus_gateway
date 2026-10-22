#include "fake_bus.h"

#include <chrono>
#include <cstdio>
#include <thread>

#include "common.h"

std::string hexOf(const uint8_t *data, size_t len)
{
    std::string s;
    char b[3];
    for (size_t i = 0; i < len; i++)
    {
        snprintf(b, sizeof(b), "%02X", data[i]);
        s += b;
    }
    return s;
}

std::vector<uint8_t> bytesOf(const std::string &hex)
{
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::string Frame::hex() const
{
    return hexOf(data, len);
}

std::string Frame::text() const
{
    char b[8];
    snprintf(b, sizeof(b), "%03X#", static_cast<unsigned>(id));
    return std::string(b) + hex();
}

Frame frameOf(uint32_t id, const std::string &hex)
{
    Frame f;
    f.id = id;
    std::vector<uint8_t> d = bytesOf(hex);
    f.len = static_cast<uint8_t>(d.size() > 8 ? 8 : d.size());
    for (uint8_t i = 0; i < f.len; i++)
    {
        f.data[i] = d[i];
    }
    return f;
}

double FakeBus::nowMs() const
{
    return static_cast<double>(nowUs()) / 1000.0;
}

void FakeBus::send(const Frame &frame)
{
    sent.push_back({frame, nowMs()});
    if (!ecu)
    {
        return;
    }
    for (const auto &r : ecu(frame))
    {
        queue(r.first, r.second);
    }
}

void FakeBus::queue(int delayMs, const Frame &frame)
{
    pending.push_back({nowMs() + delayMs, frame});
}

bool FakeBus::tryReceive(Frame &frame, int timeoutMs)
{
    double deadline = nowMs() + timeoutMs;
    while (true)
    {
        int next = -1;
        for (size_t i = 0; i < pending.size(); i++)
        {
            if (next < 0 || pending[i].first < pending[next].first)
            {
                next = static_cast<int>(i);
            }
        }
        if (next >= 0 && pending[next].first <= nowMs())
        {
            frame = pending[next].second;
            pending.erase(pending.begin() + next);
            return true;
        }
        if (nowMs() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void FakeBus::drain()
{
    double now = nowMs();
    for (size_t i = 0; i < pending.size();)
    {
        if (pending[i].first <= now)
        {
            pending.erase(pending.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

void FakeBus::reset()
{
    ecu = nullptr;
    sent.clear();
    pending.clear();
    open = true;
}

FakeBus &fakeBus()
{
    static FakeBus bus;
    return bus;
}
