#pragma once

#include <cstddef>
#include <cstdint>

#include "common.h"
#include "isotp/iso_tp.h"
#include "message_translator.h"
#include "uds/uds_mapper.h"

class TwaiWrapper;

/// One of the three control units in scope (ToyotaLens `ModuleDefinition` / `Modules`, requirements R3).
struct ModuleDefinition
{
    const char *key;   // engine | ect | abs
    const char *name;
    uint32_t requestId;

    /// §1.1: the module answers on request id + 8 (7E0 -> 7E8).
    uint32_t responseId() const
    {
        return requestId + Services::ResponseIdOffset;
    }
};

namespace Modules
{
constexpr ModuleDefinition Engine = {"engine", "Engine", 0x7E0};
constexpr ModuleDefinition Ect = {"ect", "ECT", 0x7E1};
constexpr ModuleDefinition Abs = {"abs", "ABS/VSC/TRAC", 0x7B0};
constexpr ModuleDefinition All[] = {Engine, Ect, Abs};
constexpr int Count = 3;
} // namespace Modules

/// What one request got back, as the session interprets it (ToyotaLens `ReplyType`).
enum class ReplyType : uint8_t
{
    Silent,     // no reply before the timeout
    Negative,   // 7F xx NRC: the module refused the request
    Map,        // E8 11: a response data map (§3.3), stored on the module
    Bitmap,     // 61 x0: a support bitmap (§3.2), merged into the supported set
    Block,      // 61 xx: a data block, decoded
    Other       // anything else
};

/// Where translated frames go (main.cpp queues them for the GT86 task).
typedef void (*FrameSink)(const CANMessage &frame);

/**
 * @brief The one place that speaks Toyota Mode 21 to the ISF modules: the port of ToyotaLens `EcuSession`
 * (specification §3, §5) plus the polling loop of its `LiveStream`.
 *
 * Every reply goes through one `interpret`: it learns support bitmaps and data maps and decodes data blocks.
 * `discover` probes a module (21 00; any reply means present), walks the bitmaps and fetches the data maps of
 * the blocks the gateway polls. `listen` is the task loop body: discovery while a module is silent (retried
 * every RediscoverMs, as the ToyotaLens watch does while WAITING FOR CAR), then one polling step. A module's
 * silence is never a bus fault (§8.5).
 */
class IsfService
{
public:
    /// §3.7 / §8.1 P2: a present module answers 21 00 within 100 ms; 700 ms bounds the cost of an absent one.
    static constexpr uint32_t ProbeTimeoutMs = 700;
    /// §8.1 P2 for data: multi-frame blocks and slow modules finish well within 2 s.
    static constexpr uint32_t DataTimeoutMs = 2000;
    /// An absent module is probed again this often.
    static constexpr uint32_t RediscoverMs = 3000;
    /// Blocks polled every cycle; every other polled block takes its turn, one per cycle.
    static constexpr int MaxFastBlocks = 4;
    static constexpr int MaxPolledBlocks = 24;
    static constexpr int MaxSupported = 255;
    static constexpr int MaxValuesPerBlock = 64;

    IsfService();
    ~IsfService();

    /// Opens the ISF bus and the ISO-TP channel.
    bool initialize();
    /// One step of the session (call repeatedly from the task loop).
    void listen();

    void setFrameSink(FrameSink sink)
    {
        sink_ = sink;
    }

    /// Set the timers short for the self-test.
    uint32_t probeTimeoutMs = ProbeTimeoutMs;
    uint32_t dataTimeoutMs = DataTimeoutMs;
    uint32_t rediscoverMs = RediscoverMs;

    /// Learnt per module from the 21 x0 bitmaps (R4.1).
    struct ModuleState
    {
        const ModuleDefinition *module = nullptr;
        bool present = false;
        uint32_t lastProbeMs = 0;
        bool supported[MaxSupported + 1] = {false};
        int supportedCount = 0;
        /// The blocks with definitions that the module supports, with their maps (R4.2) once learnt.
        uint8_t polled[MaxPolledBlocks];
        int polledCount = 0;
        ResponseDataMap maps[MaxPolledBlocks];
        bool hasMap[MaxPolledBlocks] = {false};
    };

    /// Classifies one reply, learns from it (bitmap, map) and decodes it (block). Public for the self-test.
    ReplyType interpret(ModuleState &m, const uint8_t *request, size_t requestLength, const uint8_t *reply,
                        size_t replyLength, uint8_t &nrc);

    /// R3.3 / R4: probe the module (21 00; any reply means present), walk the support bitmaps, fetch the data
    /// maps of the polled blocks not yet known. Returns false when the module is absent.
    bool discover(ModuleState &m);

    /// R5: read one block (21 <id>) and decode it through `interpret`; true when a block came back.
    bool read(ModuleState &m, uint8_t parameterAddress);

    ModuleState &state(int index)
    {
        return modules_[index];
    }
    MessageTranslator &translator()
    {
        return translator_;
    }

    /// For the self-test: run the session over a given channel instead of the real bus.
    void attach(TwaiWrapper *twai, IsoTp *isotp);

private:
    size_t request(const ModuleDefinition &m, const uint8_t *payload, size_t length, uint32_t timeoutMs);
    void decodeBlock(ModuleState &m, uint8_t parameterAddress, const uint8_t *raw, int rawLength);
    int polledIndex(const ModuleState &m, uint8_t parameterAddress) const;
    static bool isFastBlock(uint32_t requestId, uint8_t parameterAddress);
    void fastStep(ModuleState &m);
    bool slowStep();

    int slowRotation_ = 0;
    TwaiWrapper *twai_ = nullptr;
    IsoTp *isotp_ = nullptr;
    bool ownsChannel_ = false;
    FrameSink sink_ = nullptr;
    MessageTranslator translator_;
    ModuleState modules_[Modules::Count];
    uint8_t reply_[MAX_UDS_PAYLOAD_LEN];
};
