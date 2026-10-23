// The acceptance vectors, ported from ToyotaLens SelfTest (IsoTpVectors, IsoTpChannelVectors, DecoderVectors,
// SessionVectors) and extended with the translator and an end-to-end run of the session over the fake bus.
// Builds on a host compiler against the firmware's own sources; see test/host/run.sh. Prints one line per
// check and RESULT: PASS or RESULT: FAIL.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "can/twai_wrapper.h"
#include "fake_bus.h"
#include "isotp/iso_tp.h"
#include "logger/logger.h"
#include "message_translator.h"
#include "services/isf_service.h"
#include "uds/uds_mapper.h"

namespace
{
int passed = 0, failed = 0;

void check(const std::string &what, bool ok, const std::string &detail = "")
{
    printf("  [%s] %s%s\n", ok ? "PASS" : "FAIL", what.c_str(), ok || detail.empty() ? "" : ("  " + detail).c_str());
    if (ok)
    {
        passed++;
    }
    else
    {
        failed++;
    }
}

constexpr uint32_t Engine = 0x7E0, EngineReply = 0x7E8, Ect = 0x7E1, EctReply = 0x7E9;
constexpr int ShortBsMs = 200, ShortCrMs = 200, ShortPendingMs = 1000, ShortP2Ms = 100;
const std::string ExampleBPayload = std::string("610100000000") + "4C426537000000" + "0000002D7F2951" + "2D000000032F34" + "91";

/// Feeds candump-style frames of one id through a Message_t; returns the completed payloads.
std::vector<std::string> reassemble(uint32_t id, const std::vector<std::string> &frames)
{
    Message_t r;
    r.id = id;
    std::vector<std::string> out;
    for (const std::string &text : frames)
    {
        size_t hash = text.find('#');
        uint32_t fid = static_cast<uint32_t>(std::stoul(text.substr(0, hash), nullptr, 16));
        if (fid != id)
        {
            continue;
        }
        Frame f = frameOf(fid, text.substr(hash + 1));
        if (r.feed(f.data, f.len))
        {
            out.push_back(hexOf(r.buffer, r.expected));
        }
    }
    return out;
}

// ------------------------------------------------------------------ isotp (framing and reassembly, §1.6, §8)

void isotpVectors()
{
    printf("isotp\n");
    std::vector<std::string> a = reassemble(EctReply, {"7E1#0221000000000000", "7E9#0661008400000100"});
    check("Example A reassembles the single-frame reply", a.size() == 1 && a[0] == "610084000001", a.empty() ? "none" : a[0]);
    if (!a.empty())
    {
        std::vector<uint8_t> p = bytesOf(a[0]);
        uint8_t ids[SupportBitmap::MaxIds];
        bool next = false;
        int n = SupportBitmap::parse(0x00, p.data() + 2, p.size() - 2, ids, next);
        check("ECT 21 00 bitmap -> ids 01 06, more", n == 2 && ids[0] == 0x01 && ids[1] == 0x06 && next);
    }

    std::vector<std::string> b = reassemble(EngineReply, {"7E0#0221010000000000", "7E8#101C610100000000", "7E0#3000010000000000",
                                                        "7E8#214C426537000000", "7E8#220000002D7F2951", "7E8#232D000000032F34", "7E8#2491000000000000"});
    check("Example B reassembles 28 bytes", b.size() == 1 && b[0] == ExampleBPayload, b.empty() ? "none" : b[0]);

    std::vector<std::string> d = reassemble(EctReply, {"7E1#03A8110100000000", "7E9#1021E811011DFF00", "7E9#2100000000000000",
                                                     "7E9#22FFFFFFFFFFFF00", "7E9#23FF00000000FFFF", "7E9#24FFFFFFFFFF0000"});
    ResponseDataMap map;
    bool mapOk = d.size() == 1 && ResponseDataMap::fromReply(bytesOf(d[0]).data(), bytesOf(d[0]).size(), map);
    check("Example D map 21 01 is 15/29 present", mapOk && map.fullLength == 29 && map.presentCount() == 15,
          mapOk ? std::to_string(map.presentCount()) + "/" + std::to_string(map.fullLength) : "no map");

    std::vector<std::string> e = reassemble(0x7B8, {"7B0#0221050000000000", "7B8#100961057F800000", "7B8#2100000000000000"});
    check("Example E reassembles 61 05 + 7 bytes", e.size() == 1 && e[0] == "61057F800000000000", e.empty() ? "none" : e[0]);

    std::vector<uint8_t> payload = bytesOf(ExampleBPayload);
    std::string segmented;
    uint8_t frame[8];
    for (size_t i = 0; IsoTpFraming::segment(payload.data(), payload.size(), i, frame); i++)
    {
        segmented += (i ? " " : "") + hexOf(frame, 8);
    }
    check("Example B segments into FF + 4 CFs with sequence 1..4",
          segmented == "101C610100000000 214C426537000000 220000002D7F2951 232D000000032F34 2491000000000000", segmented);
    uint8_t req[] = {Services::ReadDataByLocalId, 0x01};
    IsoTpFraming::segment(req, 2, 0, frame);
    check("2-byte request segments into one single frame", IsoTpFraming::segmentCount(2) == 1 && hexOf(frame, 8) == "0221010000000000", hexOf(frame, 8));

    using IsoTpFraming::separationTimeMicroseconds;
    check("STmin: 00 -> 0 us, 7F -> 127 ms, F1 -> 100 us, F9 -> 900 us, 80 and FA (reserved) -> 127 ms",
          separationTimeMicroseconds(0x00) == 0 && separationTimeMicroseconds(0x7F) == 127000 && separationTimeMicroseconds(0xF1) == 100 &&
              separationTimeMicroseconds(0xF9) == 900 && separationTimeMicroseconds(0x80) == 127000 && separationTimeMicroseconds(0xFA) == 127000);
    uint8_t fc1[] = {0x30, 0x02, 0x05}, fc2[] = {0x31, 0, 0}, fc3[] = {0x32, 0, 0}, fc4[] = {0x35, 0, 0};
    FlowControl fc = FlowControl::parse(fc1);
    check("flow control 30 02 05 -> continue, BS 2, STmin 5 ms; 31 -> wait; 32 and 35 -> abort",
          fc.continueToSend() && fc.blockSize == 2 && fc.separationTimeUs == 5000 && FlowControl::parse(fc2).wait() &&
              FlowControl::parse(fc3).abort() && FlowControl::parse(fc4).abort());
    uint8_t ours[8];
    FlowControl::continueAll(ours);
    check("our flow control is 30 00 00 + padding", hexOf(ours, 8) == "3000000000000000", hexOf(ours, 8));
}

// ------------------------------------------------------------------ channel (IsoTp over the fake bus, §5)

struct Channel
{
    TwaiWrapper twai;
    IsoTp isotp;
    Channel() : isotp(&twai)
    {
        fakeBus().reset();
        twai.initialize();
        isotp.bsTimeoutMs = ShortBsMs;
        isotp.crTimeoutMs = ShortCrMs;
        isotp.responsePendingMs = ShortPendingMs;
    }
    std::string request(const std::vector<uint8_t> &payload, int timeoutMs, size_t *length = nullptr)
    {
        uint8_t reply[MAX_UDS_PAYLOAD_LEN];
        size_t n = isotp.request(payload.data(), payload.size(), timeoutMs, reply, sizeof(reply));
        if (length)
        {
            *length = n;
        }
        return n == 0 ? "" : hexOf(reply, n);
    }
};

bool isFlowControl(const Frame &f)
{
    return IsoTpFraming::pciType(f.data[0]) == IsoTpFraming::FlowControl;
}
bool isFirstFrame(const Frame &f)
{
    return IsoTpFraming::pciType(f.data[0]) == IsoTpFraming::FirstFrame;
}
bool isConsecutive(const Frame &f)
{
    return IsoTpFraming::pciType(f.data[0]) == IsoTpFraming::ConsecutiveFrame;
}
FakeBus::Reply none()
{
    return {};
}
std::string sentText()
{
    std::string s;
    for (auto &p : fakeBus().sent)
    {
        s += (s.empty() ? "" : " ") + p.first.text();
    }
    return s;
}
std::vector<uint8_t> payloadOf(int length)
{
    std::vector<uint8_t> p;
    for (int i = 0; i < length; i++)
    {
        p.push_back(static_cast<uint8_t>(i));
    }
    return p;
}
const std::vector<uint8_t> Probe = {Services::ReadDataByLocalId, Services::ProbeParameterAddress};
const std::vector<uint8_t> Block01 = {Services::ReadDataByLocalId, 0x01};

void channelVectors()
{
    printf("channel\n");
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            return sent.id == Ect && sent.hex() == "0221000000000000" ? FakeBus::Reply{{0, frameOf(EctReply, "0661008400000100")}} : none();
        };
        ch.isotp.target(Ect);
        std::string a = ch.request(Probe, ShortP2Ms);
        check("Example A: SF request, SF reply 61 00 84 00 00 01, no flow control sent", a == "610084000001" && fakeBus().sent.size() == 1, a);
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            if (sent.hex() == "0221010000000000")
                return FakeBus::Reply{{0, frameOf(EngineReply, "101C610100000000")}};
            if (isFlowControl(sent))
                return FakeBus::Reply{{0, frameOf(EngineReply, "214C426537000000")}, {1, frameOf(EngineReply, "220000002D7F2951")},
                                      {2, frameOf(EngineReply, "232D000000032F34")}, {3, frameOf(EngineReply, "2491000000000000")}};
            return none();
        };
        ch.isotp.target(Engine);
        std::string b = ch.request(Block01, ShortP2Ms);
        check("Example B: FF + 4 CFs reassemble to 28 bytes; our 30 00 00 went out right after the FF on the request id",
              b == ExampleBPayload && fakeBus().sent.size() == 2 && fakeBus().sent[1].first.hex() == "3000000000000000" && fakeBus().sent[1].first.id == Engine,
              b + " sent " + sentText());
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            if (sent.hex() == "03A8110100000000")
                return FakeBus::Reply{{0, frameOf(EctReply, "1021E811011DFF00")}};
            if (isFlowControl(sent))
                return FakeBus::Reply{{0, frameOf(EctReply, "2100000000000000")}, {0, frameOf(EctReply, "22FFFFFFFFFFFF00")},
                                      {0, frameOf(EctReply, "23FF00000000FFFF")}, {0, frameOf(EctReply, "24FFFFFFFFFF0000")}};
            return none();
        };
        ch.isotp.target(Ect);
        size_t n = 0;
        std::string d = ch.request({Services::ReadDataMap, Services::ReadDataMapSubFunction, 0x01}, ShortP2Ms, &n);
        ResponseDataMap map;
        bool ok = n == 33 && ResponseDataMap::fromReply(bytesOf(d).data(), n, map) && map.fullLength == 29 && map.presentCount() == 15;
        check("Example D: A8 11 01 -> 33 bytes in 5 frames, map 21 01 is 15/29 present", ok, std::to_string(n) + " bytes");
    }
    {
        Channel ch;
        bool broken = true;
        fakeBus().ecu = [&broken](const Frame &sent) {
            if (sent.hex() == "0221010000000000")
                return FakeBus::Reply{{0, frameOf(EngineReply, "101C610100000000")}};
            if (isFlowControl(sent))
                return broken ? FakeBus::Reply{{0, frameOf(EngineReply, "214C426537000000")}, {1, frameOf(EngineReply, "232D000000032F34")}}
                              : FakeBus::Reply{{0, frameOf(EngineReply, "214C426537000000")}, {1, frameOf(EngineReply, "220000002D7F2951")},
                                               {2, frameOf(EngineReply, "232D000000032F34")}, {3, frameOf(EngineReply, "2491000000000000")}};
            return none();
        };
        ch.isotp.target(Engine);
        std::string bad = ch.request(Block01, ShortP2Ms);
        broken = false;
        std::string good = ch.request(Block01, ShortP2Ms);
        check("sequence error (CF 1 then 3): no reply, abandoned 1, next request fine", bad.empty() && ch.isotp.abandoned == 1 && good.size() == 56,
              "bad=" + bad + " abandoned=" + std::to_string(ch.isotp.abandoned) + " good=" + std::to_string(good.size() / 2));
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            return sent.hex() == "0221010000000000" ? FakeBus::Reply{{0, frameOf(EngineReply, "101C610100000000")}} : none();
        };
        ch.isotp.target(Engine);
        uint32_t t0 = nowMs();
        std::string r = ch.request(Block01, ShortP2Ms);
        long ms = static_cast<long>(nowMs() - t0);
        check("N_Cr: FF then silence -> no reply within N_Cr + 100", r.empty() && ms >= ShortCrMs - 20 && ms < ShortCrMs + 100 && ch.isotp.abandoned == 1,
              std::to_string(ms) + " ms");
    }
    {
        Channel ch;
        ch.isotp.target(Engine);
        uint32_t t0 = nowMs();
        std::string r = ch.request(Probe, ShortP2Ms);
        long ms = static_cast<long>(nowMs() - t0);
        check("P2: silence -> no reply at the timeout", r.empty() && ms >= ShortP2Ms - 20 && ms < ShortP2Ms + 100, std::to_string(ms) + " ms");
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            return sent.hex() == "0221000000000000"
                       ? FakeBus::Reply{{0, frameOf(EngineReply, "037F217800000000")}, {300, frameOf(EngineReply, "0661008400000100")}}
                       : none();
        };
        ch.isotp.target(Engine);
        uint32_t t0 = nowMs();
        std::string r = ch.request(Probe, ShortP2Ms);
        long ms = static_cast<long>(nowMs() - t0);
        check("response pending: 7F 21 78 then the reply 300 ms later -> the reply, within P2*", r == "610084000001" && ms >= 280 && ms < ShortPendingMs,
              r + " after " + std::to_string(ms) + " ms");
    }
    // send side
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            if (isFirstFrame(sent))
                return FakeBus::Reply{{0, frameOf(EngineReply, "3000000000000000")}};
            if (isConsecutive(sent) && IsoTpFraming::sequenceNumber(sent.data[0]) == 2)
                return FakeBus::Reply{{0, frameOf(EngineReply, "0661008400000100")}};
            return none();
        };
        ch.isotp.target(Engine);
        std::string r = ch.request(payloadOf(20), ShortP2Ms);
        auto &s = fakeBus().sent;
        check("send 20 bytes, FC 30 00 00: FF (10 14) + CF seq 1, 2, no gap; reply received",
              !r.empty() && s.size() == 3 && s[0].first.hex() == "1014000102030405" && s[1].first.hex() == "21060708090A0B0C" && s[2].first.hex() == "220D0E0F10111213",
              sentText());
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            return (isFirstFrame(sent) || (isConsecutive(sent) && IsoTpFraming::sequenceNumber(sent.data[0]) % 2 == 0))
                       ? FakeBus::Reply{{0, frameOf(EngineReply, "3002050000000000")}}
                       : none();
        };
        ch.isotp.target(Engine);
        ch.request(payloadOf(40), ShortP2Ms);
        auto &s = fakeBus().sent;
        std::string order;
        for (auto &p : s)
        {
            char b[4];
            snprintf(b, sizeof(b), "%02X", p.first.data[0]);
            order += (order.empty() ? "" : " ") + std::string(b);
        }
        auto gap = [&s](size_t i) { return s.size() > i ? s[i].second - s[i - 1].second : -1.0; };
        check("send 40 bytes, FC 30 02 05: FF, CF1, CF2, (second FC), CF3, CF4, CF5 with >= 5 ms between CFs",
              order == "10 21 22 23 24 25" && gap(2) >= 4.9 && gap(3) >= 4.9 && gap(4) >= 4.9 && gap(5) >= 4.9, order);
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            return isFirstFrame(sent) ? FakeBus::Reply{{0, frameOf(EngineReply, "3100000000000000")}, {50, frameOf(EngineReply, "3000000000000000")}} : none();
        };
        ch.isotp.target(Engine);
        ch.request(payloadOf(20), ShortP2Ms);
        auto &s = fakeBus().sent;
        double firstCf = s.size() > 1 ? s[1].second - s[0].second : -1;
        check("FC 31 (wait) then 30 after 50 ms: nothing sent during the wait, then the CFs", s.size() == 3 && firstCf >= 45,
              std::to_string(s.size()) + " frames, first CF after " + std::to_string(firstCf) + " ms");
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) { return isFirstFrame(sent) ? FakeBus::Reply{{0, frameOf(EngineReply, "3200000000000000")}} : none(); };
        ch.isotp.target(Engine);
        std::string r = ch.request(payloadOf(20), ShortP2Ms);
        check("FC 32 (overflow): request abandoned, only the FF was sent", r.empty() && fakeBus().sent.size() == 1, std::to_string(fakeBus().sent.size()) + " frames");
    }
    {
        Channel ch;
        ch.isotp.target(Engine);
        uint32_t t0 = nowMs();
        std::string r = ch.request(payloadOf(20), ShortP2Ms);
        long ms = static_cast<long>(nowMs() - t0);
        check("no FC: request abandoned after N_Bs", r.empty() && fakeBus().sent.size() == 1 && ms >= ShortBsMs - 20 && ms < ShortBsMs + 100,
              std::to_string(ms) + " ms, " + std::to_string(fakeBus().sent.size()) + " frames");
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) { return isFirstFrame(sent) ? FakeBus::Reply{{0, frameOf(EngineReply, "3000000000000000")}} : none(); };
        ch.isotp.target(Engine);
        ch.request(payloadOf(120), ShortP2Ms);
        std::string sequence;
        for (size_t i = 1; i < fakeBus().sent.size(); i++)
        {
            char b[3];
            snprintf(b, sizeof(b), "%X", IsoTpFraming::sequenceNumber(fakeBus().sent[i].first.data[0]));
            sequence += (sequence.empty() ? "" : " ") + std::string(b);
        }
        check("120 bytes: 17 CFs with sequence 1..F, 0, 1", fakeBus().sent.size() == 18 && sequence == "1 2 3 4 5 6 7 8 9 A B C D E F 0 1", sequence);
    }
    {
        Channel ch;
        fakeBus().ecu = [](const Frame &sent) {
            return sent.hex() == "0221000000000000"
                       ? FakeBus::Reply{{0, frameOf(EctReply, "0661008400000100")}, {0, frameOf(Engine, "0221000000000000")}, {5, frameOf(EngineReply, "0661008400000100")}}
                       : none();
        };
        ch.isotp.target(Engine);
        fakeBus().queue(0, frameOf(EctReply, "0661000000000000"));
        std::string r = ch.request(Probe, ShortP2Ms);
        check("filtering: 7E9 frames and our echoed 7E0 frame are dropped while targeting 7E8", !r.empty() && ch.isotp.otherFrames == 2,
              r + " other=" + std::to_string(ch.isotp.otherFrames));
    }
}

// ------------------------------------------------------------------ decoder (§4.3, §6.8)

const UdsDefinition *find(uint32_t requestId, uint8_t address, const char *name)
{
    for (size_t i = 0; i < UDS_DEFINITIONS_COUNT; i++)
    {
        const UdsDefinition &d = UDS_DEFINITIONS[i];
        if (d.requestId == requestId && d.parameterAddress == address && strcmp(d.name, name) == 0)
        {
            return &d;
        }
    }
    return nullptr;
}

void decoderVectors()
{
    printf("decoder\n");
    ResponseDataMap map;
    map.parameterAddress = 0x05;
    const char *flags = "111100000111000";
    map.fullLength = static_cast<uint8_t>(strlen(flags));
    for (int i = 0; i < map.fullLength; i++)
    {
        map.present[i] = flags[i] == '1';
    }
    std::vector<uint8_t> raw = bytesOf("7F800000000000");
    uint8_t full[ResponseDataMap::MaxLayoutLength];
    int n = map.expand(raw.data(), static_cast<int>(raw.size()), full);
    check("ABS 21 05 expands 7 -> 15 bytes", n == 15 && full[0] == 0x7F && full[1] == 0x80, std::to_string(n));
    const UdsDefinition *dec = find(0x7B0, 0x05, "Deceleration Sensor");
    ParameterValue v = dec ? ParameterDecoder::decode(*dec, full, n, &map) : ParameterValue{};
    check("Deceleration Sensor = -0.14 m/s2", dec && v.isAvailable && std::fabs(v.value - (127 * 0.14318 - 18.327)) < 1e-9 && strcmp(v.unit, "m/s2") == 0,
          dec ? std::to_string(v.value) : "definition missing");
    const UdsDefinition *rev = find(0x7B0, 0x05, "Engine Revolutions");
    v = rev ? ParameterDecoder::decode(*rev, full, n, &map) : ParameterValue{};
    check("Engine Revolutions = 0 rpm", rev && v.isAvailable && v.value == 0 && strcmp(v.unit, "rpm") == 0, rev ? std::to_string(v.value) : "definition missing");
    UdsDefinition vacuum = {"test", "Vacuum Sensor", 0x7B0, 0x05, 4, 0, {0xFF, 0, 0, 0}, 1, false, 0.0196078431372549, 0, 48, DataType::Numeric};
    v = ParameterDecoder::decode(vacuum, full, n, &map);
    check("Vacuum Sensor (byte 4, absent in map) reported not supported", !v.isSupported && !v.isAvailable);

    const UdsDefinition *rpm = find(0x7E0, 0x01, "Engine Speed");
    const UdsDefinition *ect = find(0x7E0, 0x01, "Coolant Temp");
    const UdsDefinition *mil = find(0x7E0, 0x06, "MIL");
    uint8_t blk01[29] = {0};
    blk01[9] = 0x5A;
    blk01[10] = 0x1A;
    blk01[11] = 0xF8;
    check("Engine Speed 1A F8 -> 1726 rpm", rpm && std::fabs(ParameterDecoder::decode(*rpm, blk01, 29, nullptr).value - 1726) < 1e-9,
          rpm ? std::to_string(ParameterDecoder::decode(*rpm, blk01, 29, nullptr).value) : "definition missing");
    check("Coolant Temp 5A -> 50 degC", ect && std::fabs(ParameterDecoder::decode(*ect, blk01, 29, nullptr).value - 50) < 1e-9,
          ect ? std::to_string(ParameterDecoder::decode(*ect, blk01, 29, nullptr).value) : "definition missing");
    uint8_t blk06[18] = {0};
    blk06[0] = 0x80;
    check("MIL 80 -> raw 1 (ON)", mil && ParameterDecoder::decode(*mil, blk06, 18, nullptr).raw == 1, mil ? "" : "definition missing");
    uint8_t torqueBlock[15] = {0};
    torqueBlock[10] = 0xFF;
    torqueBlock[11] = 0xE0; // -32 * 0.03125 = -1 Nm
    const UdsDefinition *torque = find(0x7B0, 0x05, "Real Engine Torque");
    check("Real Engine Torque FF E0 (signed) -> -1 Nm", torque && std::fabs(ParameterDecoder::decode(*torque, torqueBlock, 15, nullptr).value + 1.0) < 1e-9,
          torque ? std::to_string(ParameterDecoder::decode(*torque, torqueBlock, 15, nullptr).value) : "definition missing");
}

// ------------------------------------------------------------------ session (EcuSession.Interpret)

void sessionVectors()
{
    printf("session\n");
    IsfService service;
    IsfService::ModuleState &ect = service.state(1);
    uint8_t nrc = 0;
    uint8_t req[] = {Services::ReadDataByLocalId, Services::ProbeParameterAddress};
    std::vector<uint8_t> reply = bytesOf("610084000001");
    ReplyType kind = service.interpret(ect, req, 2, reply.data(), reply.size(), nrc);
    check("session: bitmap reply learnt as supported ids 01 06", kind == ReplyType::Bitmap && ect.supported[0x01] && ect.supported[0x06] && ect.supportedCount == 2,
          std::to_string(ect.supportedCount) + " ids");
    uint8_t req07[] = {Services::ReadDataByLocalId, 0x07};
    uint8_t refusal[] = {Services::NegativeResponse, Services::ReadDataByLocalId, Nrc::SubFunctionNotSupported};
    kind = service.interpret(ect, req07, 2, refusal, 3, nrc);
    check("session: NRC 12 reported as Negative with its code", kind == ReplyType::Negative && nrc == Nrc::SubFunctionNotSupported);
}

// ------------------------------------------------------------------ translator

void translatorVectors()
{
    printf("translator\n");
    uint8_t d[8] = {0};
    MessageTranslator::packSignal(d, 32, 14, 1726);
    check("pack 1726 into bits 32-45 -> bytes 4-5 = BE 06", d[4] == 0xBE && d[5] == 0x06 && MessageTranslator::unpackSignal(d, 32, 14) == 1726, hexOf(d, 8));
    uint8_t keep[8] = {0, 0, 0, 0, 0, 0x83, 0xA7, 0};
    MessageTranslator::packSignal(keep, 32, 14, 1726);
    check("bits outside the field are kept (0x141 default byte 5 = 83 -> 86)", keep[5] == 0x86 && keep[6] == 0xA7, hexOf(keep, 8));

    const UdsDefinition *rpm = find(0x7E0, 0x01, "Engine Speed");
    const UdsDefinition *ect = find(0x7E0, 0x01, "Coolant Temp");
    const UdsDefinition *speed = find(0x7E0, 0x01, "Vehicle Speed");
    if (!rpm || !ect || !speed)
    {
        check("translator: definitions present", false, "missing");
        return;
    }
    ParameterValue values[3];
    values[0].parameter = rpm;
    values[0].isAvailable = true;
    values[0].raw = 1726 * 4;
    values[0].value = 1726;
    values[1].parameter = ect;
    values[1].isAvailable = true;
    values[1].raw = 90;
    values[1].value = 50;
    values[2].parameter = speed;
    values[2].isAvailable = true;
    values[2].raw = 60;
    values[2].value = 60;
    MessageTranslator t;
    CANMessage changed[MessageTranslator::MaxFrames];
    int n = t.apply(values, 3, changed, MessageTranslator::MaxFrames);
    std::string got;
    bool ok141 = false, ok360 = false, okD1 = false, okSpeed = false, okTemps = false;
    for (int i = 0; i < n; i++)
    {
        got += (got.empty() ? "" : " ") + std::string(1, ' ') + std::to_string(changed[i].id) + ":" + hexOf(changed[i].data, 8);
        if (changed[i].id == 0x141)
            ok141 = changed[i].data[4] == 0xBE && changed[i].data[5] == 0x86 && changed[i].data[6] == 0xA7;
        if (changed[i].id == 0x360)
            ok360 = changed[i].data[2] == 0x84 && changed[i].data[3] == 90;
        if (changed[i].id == 0xD1)
            okD1 = (changed[i].data[0] | changed[i].data[1] << 8) == 1062 && changed[i].data[2] == 0x13 && changed[i].data[4] == 0x00;
        if (changed[i].id == 0x1F0005E0) // ENG_Speed_Load: rpm bytes 0-1, speed byte 2
            okSpeed = changed[i].data[0] == 0xF8 && changed[i].data[1] == 0x1A && changed[i].data[2] == 60;
        if (changed[i].id == 0x1F0005E1) // ENG_Temperatures: coolant byte 0
            okTemps = changed[i].data[0] == 90;
    }
    check("GT86 0x141: rpm 1726 in bits 32-45, default bits kept", ok141, got);
    check("GT86 0x360: coolant 50 degC -> byte 3 = 90, oil byte untouched", ok360, got);
    check("GT86 0xD1: 60 km/h -> round(60/0.0565) = 1062 in bytes 0-1, rest default", okD1, got);
    check("ADU ENG_Speed_Load: raw rpm 6904 (1A F8) little-endian in bytes 0-1, speed raw 60 in byte 2", okSpeed, got);
    check("ADU ENG_Temperatures: coolant raw 90 in byte 0", okTemps, got);
}

// ------------------------------------------------------------------ end to end: the session over a scripted engine ECU

std::vector<CANMessage> sunk;
void sink(const CANMessage &m)
{
    sunk.push_back(m);
}

/// A scripted ECU that answers whole payloads: segments its reply, waits for our flow control before the CFs.
struct ScriptedEcu
{
    uint32_t requestId, responseId;
    std::function<std::vector<uint8_t>(const std::vector<uint8_t> &)> answer;
    std::vector<std::vector<uint8_t>> pendingCfs;

    FakeBus::Reply operator()(const Frame &sent)
    {
        if (sent.id != requestId)
        {
            return {};
        }
        if (isFlowControl(sent))
        {
            FakeBus::Reply r;
            int delay = 0;
            for (auto &cf : pendingCfs)
            {
                r.push_back({delay++, frameOf(responseId, hexOf(cf.data(), cf.size()))});
            }
            pendingCfs.clear();
            return r;
        }
        int len = IsoTpFraming::singleFrameLength(sent.data[0]);
        std::vector<uint8_t> request(sent.data + 1, sent.data + 1 + len);
        std::vector<uint8_t> reply = answer(request);
        if (reply.empty())
        {
            return {};
        }
        uint8_t frame[8];
        FakeBus::Reply r;
        for (size_t i = 0; IsoTpFraming::segment(reply.data(), reply.size(), i, frame); i++)
        {
            if (i == 0)
            {
                r.push_back({5, frameOf(responseId, hexOf(frame, 8))});
            }
            else
            {
                pendingCfs.push_back(std::vector<uint8_t>(frame, frame + 8));
            }
        }
        return r;
    }
};

void endToEnd()
{
    printf("end-to-end\n");
    fakeBus().reset();
    TwaiWrapper twai;
    IsoTp isotp(&twai);
    twai.initialize();
    IsfService service;
    service.attach(&twai, &isotp);
    service.setFrameSink(sink);
    service.probeTimeoutMs = 100;
    service.dataTimeoutMs = 100;
    sunk.clear();

    ScriptedEcu ecu;
    ecu.requestId = Engine;
    ecu.responseId = EngineReply;
    ecu.answer = [](const std::vector<uint8_t> &q) -> std::vector<uint8_t> {
        if (q.size() == 2 && q[0] == 0x21 && q[1] == 0x00)
            return bytesOf("6100AC000001"); // ids 01 03 05 06, next bitmap exists
        if (q.size() == 2 && q[0] == 0x21 && q[1] == 0x20)
            return bytesOf("612008000000"); // id 25, no more
        if (q.size() == 3 && q[0] == 0xA8 && q[2] == 0x01)
        {
            std::vector<uint8_t> r = bytesOf("E811011D");
            for (char c : std::string("11111010111111111111111111110"))
                r.push_back(c == '1' ? 0xFF : 0x00);
            return r;
        }
        if (q.size() == 3 && q[0] == 0xA8)
            return {0x7F, 0xA8, 0x12};
        if (q.size() == 2 && q[0] == 0x21 && q[1] == 0x01)
        {
            // The 28-byte 21 01 answer recorded in ToyotaLens samples/live-isf.log (FF + 4 CFs), reassembled.
            return bytesOf("610100000000443F653C00000000000030842951" "2C01000102A332BC");
        }
        return {};
    };
    fakeBus().ecu = [&ecu](const Frame &f) { return ecu(f); };

    IsfService::ModuleState &engine = service.state(0);
    bool present = service.discover(engine);
    check("discover: engine present, bitmaps 00 and 20 walked -> ids 01 03 05 06 25", present && engine.supportedCount == 5 && engine.supported[0x25],
          std::to_string(engine.supportedCount) + " ids");
    check("discover: the polled blocks are the five with definitions; map 21 01 learnt (26/29)",
          engine.polledCount == 5 && engine.hasMap[0] && engine.maps[0].presentCount() == 26 && engine.maps[0].fullLength == 29,
          std::to_string(engine.polledCount) + " polled");

    sunk.clear();
    bool got = service.read(engine, 0x01);
    std::string ids;
    bool okGt = false, okRpm = false, okAdu = false, okTemps = false, okExtras = false;
    for (auto &m : sunk)
    {
        char b[8];
        snprintf(b, sizeof(b), "%03X ", static_cast<unsigned>(m.id));
        ids += b;
        if (m.id == 0x360)
            okGt = m.data[3] == 0x3C; // coolant 20 degC -> 60
        if (m.id == 0x141)
            okRpm = m.data[4] == 0x00 && m.data[5] == 0x80 && m.data[6] == 0xA7; // rpm 0 written, bits 46-47 kept
        if (m.id == 0x1F0005E0) // ENG_Speed_Load: rpm 0
            okAdu = m.data[0] == 0 && m.data[1] == 0;
        if (m.id == 0x1F0005E1) // ENG_Temperatures: coolant raw 60, intake raw 63
            okTemps = m.data[0] == 0x3C && m.data[1] == 0x3F;
        if (m.id == 0x1F0005EB) // ENG_Electrical: battery raw 0x32BC = 12.988 V in bytes 0-1
            okExtras = m.data[0] == 0xBC && m.data[1] == 0x32;
    }
    check("read 21 01: the 26-byte block (live-isf.log) expands through the map and decodes", got, "");
    check("read 21 01: GT86 0x360 byte 3 = 3C (coolant 20 degC + 40)", okGt, ids);
    check("read 21 01: GT86 0x141 refreshed with rpm 0 (constant values are still forwarded)", okRpm, ids);
    check("read 21 01: ADU ENG_Speed_Load rpm 0", okAdu, ids);
    check("read 21 01: ADU ENG_Temperatures coolant raw 60, intake raw 63", okTemps, ids);
    check("read 21 01: ADU ENG_Electrical battery raw 32BC little-endian", okExtras, ids);
    bool fwdAdu = false, fwdGt86 = false;
    for (auto &p : fakeBus().sent)
    {
        if (p.first.id == 0x1F0005E0)
            fwdAdu = true;
        if (p.first.id == 0x141 || p.first.id == 0x360)
            fwdGt86 = true;
    }
    check("read 21 01: ADU ENG_Speed_Load is sent on the ISF bus as well; the GT86 packets are not", fwdAdu && !fwdGt86, sentText());
}
} // namespace

int main(int argc, char **argv)
{
    bool verbose = argc > 1 && std::string(argv[1]) == "-v";
    Logger::begin(verbose ? LOG_LEVEL_DEBUG : LOG_LEVEL_ERROR, verbose);
    isotpVectors();
    channelVectors();
    decoderVectors();
    sessionVectors();
    translatorVectors();
    endToEnd();
    if (failed == 0)
    {
        printf("RESULT: PASS (%d checks)\n", passed);
        return 0;
    }
    printf("RESULT: FAIL (%d of %d checks)\n", failed, passed + failed);
    return 1;
}
