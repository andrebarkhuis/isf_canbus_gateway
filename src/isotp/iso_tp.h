#pragma once

#include <cstddef>
#include <cstdint>

class TwaiWrapper;

// ---------------------------------------------------------------------------------------------------------
// ISO 15765-2 (ISO-TP) over classic CAN, ported from ToyotaLens (Protocol/IsoTp.cs, IsoTpReassembler.cs,
// FlowControl.cs, IsoTpTimers.cs, IsoTpChannel.cs; specification §1.6 and §8). Every frame carries 8 data
// bytes; the first byte (or two) is the protocol control information (PCI), whose high nibble says which frame
// it is:
//
//   0L        single frame       L payload bytes follow in D1..D7
//   1L LL     first frame        total length 0x0LLL; 6 payload bytes follow in D2..D7
//   2S        consecutive frame  7 payload bytes in D1..D7; S = sequence 1..F, wraps 15 -> 0
//   3F BS ST  flow control       F = continue / wait / overflow, BS block size, ST separation time
// ---------------------------------------------------------------------------------------------------------

/// The framing rules and constants (ToyotaLens `IsoTp`): the names of the bytes on the wire.
namespace IsoTpFraming
{
/// Classic CAN data length; every ISO-TP frame is padded to it.
constexpr int FrameLength = 8;
/// Padding byte Techstream and the MVCI driver use (some clones pad with 0xAA or 0x55; ECUs ignore it).
constexpr uint8_t Padding = 0x00;

// --- PCI: the high nibble of D0 is the frame type, the low nibble its argument
constexpr int PciTypeShift = 4;
constexpr uint8_t PciArgumentMask = 0x0F;
constexpr uint8_t SingleFrame = 0x0;
constexpr uint8_t FirstFrame = 0x1;
constexpr uint8_t ConsecutiveFrame = 0x2;
constexpr uint8_t FlowControl = 0x3;

/// A single frame carries at most 7 payload bytes (D1..D7).
constexpr int SingleFramePayload = 7;
/// A first frame carries 6 payload bytes (D2..D7) after the two-byte PCI.
constexpr int FirstFramePayload = 6;
/// A consecutive frame carries 7 payload bytes (D1..D7).
constexpr int ConsecutiveFramePayload = 7;
/// A message that would fit a single frame is never sent as a first frame.
constexpr int FirstFrameMinLength = SingleFramePayload + 1;
/// The 12-bit length field of a first frame.
constexpr int MaxMessageLength = 0xFFF;
/// The sequence number of the consecutive frame that follows a first frame.
constexpr int FirstSequence = 1;
/// Sequence numbers are 4 bits: 1, 2, … F, 0, 1, …
constexpr uint8_t SequenceMask = 0x0F;

// --- flow control (the receiver of a first frame sends it)
constexpr uint8_t FlowStatusContinueToSend = 0x0;
constexpr uint8_t FlowStatusWait = 0x1;
constexpr uint8_t FlowStatusOverflow = 0x2;
/// Block size 0: send every consecutive frame without waiting for another flow control.
constexpr uint8_t BlockSizeAll = 0x00;
/// Separation time 0 ms; Toyota ECUs ask for 0 or 1 ms.
constexpr uint8_t SeparationTimeNone = 0x00;
/// STmin 0x00..0x7F are whole milliseconds.
constexpr uint8_t SeparationTimeMaxMs = 0x7F;
/// STmin 0xF1..0xF9 are 100..900 microseconds.
constexpr uint8_t SeparationTimeMicroFirst = 0xF1;
constexpr uint8_t SeparationTimeMicroLast = 0xF9;
constexpr int SeparationTimeMicroStep = 100;
constexpr int MicrosecondsPerMillisecond = 1000;

// --- reading a frame
inline int pciType(uint8_t d0)
{
    return d0 >> PciTypeShift;
}
inline int singleFrameLength(uint8_t d0)
{
    return d0 & PciArgumentMask;
}
inline int firstFrameLength(uint8_t d0, uint8_t d1)
{
    return ((d0 & PciArgumentMask) << 8) | d1;
}
inline int sequenceNumber(uint8_t d0)
{
    return d0 & SequenceMask;
}
inline int nextSequence(int sequence)
{
    return (sequence + 1) & SequenceMask;
}
/// STmin (ISO 15765-2 §9.6.5.4) in microseconds: 0x00..0x7F = that many ms; 0xF1..0xF9 = 100..900 µs; the
/// reserved values (0x80..0xF0, 0xFA..0xFF) count as the maximum, 127 ms, as the standard says.
int separationTimeMicroseconds(uint8_t st);

// --- writing a frame
inline uint8_t singleFramePci(int payloadLength)
{
    return static_cast<uint8_t>((SingleFrame << PciTypeShift) | payloadLength);
}
inline uint8_t firstFramePciHigh(int messageLength)
{
    return static_cast<uint8_t>((FirstFrame << PciTypeShift) | (messageLength >> 8));
}
inline uint8_t firstFramePciLow(int messageLength)
{
    return static_cast<uint8_t>(messageLength);
}
inline uint8_t consecutiveFramePci(int sequence)
{
    return static_cast<uint8_t>((ConsecutiveFrame << PciTypeShift) | (sequence & SequenceMask));
}
inline uint8_t flowControlPci(uint8_t status)
{
    return static_cast<uint8_t>((FlowControl << PciTypeShift) | (status & PciArgumentMask));
}

/// Send side (§8.2), ToyotaLens `IsoTp.Segment`: how many frames a payload occupies (0 when it is too long).
size_t segmentCount(size_t payloadLength);
/// The frame at `index` of the segmented payload, padded to FrameLength: up to 7 bytes go in one single frame;
/// longer payloads go as a first frame (6 bytes) followed by consecutive frames (7 bytes each) with sequence
/// numbers 1, 2, … F, 0, 1, …  Returns false when `index` is past the last frame.
bool segment(const uint8_t *payload, size_t payloadLength, size_t index, uint8_t frame[FrameLength]);
} // namespace IsoTpFraming

/// Largest reassembled message the gateway keeps. Specification §8.3: a 256-byte buffer covers every Toyota
/// answer in the document (the longest seen are 35 and 52 bytes).
constexpr size_t MAX_UDS_PAYLOAD_LEN = 256;

/// Receive-side reassembly for one CAN id (ToyotaLens `IsoTpReassembler`, §1.6, §8.3), under this repository's
/// historical name. Padding bytes beyond the declared length are dropped. A consecutive frame with the wrong
/// sequence number abandons the message. Flow-control frames are the receiver's own and are ignored. Timing
/// (N_Cr) is not enforced here; IsoTp enforces it live (§8.1).
struct Message_t
{
    uint32_t id = 0;
    uint8_t buffer[MAX_UDS_PAYLOAD_LEN] = {0};
    /// Declared length of the message in progress, or the length of the last completed message.
    uint16_t expected = 0;
    uint16_t received = 0;
    uint8_t nextSequence = 0;
    /// True between an accepted first frame and the consecutive frame that completes (or abandons) the
    /// message; IsoTp sends flow control when this turns true.
    bool inProgress = false;
    /// Messages abandoned on a sequence error.
    int abandoned = 0;

    /// Feeds one frame's data bytes; returns true when the frame completes a message. The payload is then in
    /// `buffer` with its length in `expected`, valid until the next feed.
    bool feed(const uint8_t *data, uint8_t length);
    void reset();
};

/// An ISO-TP flow control frame, decoded (ToyotaLens `FlowControl`, ISO 15765-2 §9.6):
///   D0 = 3F   F = 0 continue to send, 1 wait, 2 overflow (anything else: treated as overflow)
///   D1 = BS   block size: 0 = send every consecutive frame; n = send n, then wait for the next flow control
///   D2 = ST   separation time between consecutive frames (IsoTpFraming::separationTimeMicroseconds)
struct FlowControl
{
    static constexpr int Length = 3;

    uint8_t status = 0;
    int blockSize = 0;
    int separationTimeUs = 0;

    bool continueToSend() const
    {
        return status == IsoTpFraming::FlowStatusContinueToSend;
    }
    bool wait() const
    {
        return status == IsoTpFraming::FlowStatusWait;
    }
    /// Overflow, or a status the standard does not define: abort the send.
    bool abort() const
    {
        return status != IsoTpFraming::FlowStatusContinueToSend && status != IsoTpFraming::FlowStatusWait;
    }

    /// Decodes the data bytes of a frame whose PCI type is FlowControl (at least `Length` bytes).
    static FlowControl parse(const uint8_t *data);
    /// The one flow control this gateway sends: continue, every frame at once, no gap: 30 00 00 plus padding.
    static void continueAll(uint8_t frame[IsoTpFraming::FrameLength]);
};

/// ISO 15765-2 network-layer timers (specification §8.1) as IsoTp enforces them. P2 (the ECU's first response
/// frame) is the caller's: IsfService::ProbeTimeoutMs and DataTimeoutMs.
namespace IsoTpTimers
{
/// N_As: the driver accepts a frame we send.
constexpr uint32_t N_As_Ms = 1000;
/// N_Bs: the ECU's flow control after our first frame.
constexpr uint32_t N_Bs_Ms = 1000;
/// N_Cr: the ECU's next consecutive frame.
constexpr uint32_t N_Cr_Ms = 1000;
/// N_Br: our flow control after the ECU's first frame. We send it at once.
constexpr uint32_t N_Br_Ms = 0;
/// How many "wait" flow controls in a row we tolerate before abandoning a send.
constexpr int WaitFramesMax = 10;
/// P2*: after 7F xx 78 (response pending) the real reply may take this long.
constexpr uint32_t P2StarMs = 5000;
} // namespace IsoTpTimers

/**
 * @brief ISO 15765-2 live on the ISF bus, over a TwaiWrapper: the port of ToyotaLens `IsoTpChannel`
 * (docs/direct-can-over-serial.md §5).
 *
 * Send side (§8.2): single frame, or first frame + consecutive frames paced by the ECU's flow control. Receive
 * side (§8.3): filter the target's response id out of the bus traffic, answer its first frame with 30 00 00 at
 * once, reassemble, enforce N_Bs / N_Cr, honour 7F xx 78 response pending. Single-threaded on the caller:
 * IsfService never interleaves requests. Every frame sent or received is traced by the TwaiWrapper.
 */
class IsoTp
{
public:
    explicit IsoTp(TwaiWrapper *can);

    /// The module to talk to; it answers on `requestId` + Services::ResponseIdOffset.
    void target(uint32_t requestId);

    /// One request to the current target. Returns the reply length copied into `reply`, or 0 when the module
    /// stayed silent for `timeoutMs`, the reply was abandoned, or the request could not be sent. Silence is
    /// not an error (§8.5).
    size_t request(const uint8_t *payload, size_t payloadLength, uint32_t timeoutMs, uint8_t *reply,
                   size_t replyCapacity);

    /// The §8.1 timers, settable so the self-test can run them short.
    uint32_t bsTimeoutMs = IsoTpTimers::N_Bs_Ms;
    uint32_t crTimeoutMs = IsoTpTimers::N_Cr_Ms;
    uint32_t responsePendingMs = IsoTpTimers::P2StarMs;
    int waitFramesMax = IsoTpTimers::WaitFramesMax;
    /// A module may answer 7F xx 78 (busy) a few times before the real reply; a flood past this many is
    /// treated as no reply, so one id that streams "pending" cannot hang the session.
    int maxResponsePending = 6;

    /// Frames seen on other ids (other modules' traffic, our own echo) and dropped by the software filter.
    uint32_t otherFrames = 0;
    /// Replies abandoned on a sequence error or a missing consecutive frame.
    int abandoned = 0;

private:
    bool sendMessage(const uint8_t *payload, size_t payloadLength);
    bool waitForFlowControl(FlowControl &fc);
    bool send(const uint8_t frame[IsoTpFraming::FrameLength]);
    size_t receiveReply(uint32_t timeoutMs, uint8_t *reply, size_t replyCapacity);
    static bool isResponsePending(const uint8_t *payload, size_t length);
    static void pauseSince(uint64_t sinceUs, int microseconds);

    TwaiWrapper *can_;
    uint32_t txId_ = 0;
    uint32_t rxId_ = 0;
    Message_t reassembler_;
};
