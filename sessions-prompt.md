# Prompt to start the next session

Paste the block below as the first message. Update the state lines when something changes (a flash, a wiring
change, a resistance reading), and fill in the task line.

```
Read CLAUDE.md and follow its reading list before doing anything.

State as of 2026-10-23 evening:
- The board on COM10 runs the firmware from commit 6bf6d1c (DTC counts), flashed 19:50.
  Nothing in src/ or tools/ has changed since.
- Open hardware issue: the IS F bus shows ~10 bus error alerts/s with tec 0 and rec near 0
  (receive-side errors). Suspect: the board's TERM1 pad adds a third terminator to the car
  bus. I will measure CAN H to CAN L resistance and report the reading before any change.
- The ADU project is mine; never edit, generate or discuss the .adu file beyond naming
  channels from docs/adu-frames.md.
- Readme.md "IS F modules" table lists far fewer polled blocks than the firmware actually
  polls; it needs regenerating from tools/gateway_frames.json.

Today's task: <fill in>
```

## Task lines ready to use

- `Today's task: the bus resistance reads <N> Ω. Tell me whether to open TERM1, then help me verify from the
  serial log that the bus error alerts stop.`
- `Today's task: bring the Readme "IS F modules" table in line with what the firmware polls, derived from the
  frame specification, and nothing else.`
- `Today's task: a channel on the ADU reads <value> but the car shows <value>. Compare the 29-bit frame bytes
  in the serial trace with the channel definition and tell me which side is wrong.`

## Notes

- Opening and closing the serial port resets the board (native USB Serial/JTAG reacts to DTR). If a log must
  survive the session, say so and the monitor stays open in the background.
- If the IS F wiring changes before the next session, replace the hardware line with the new reading so the
  diagnosis does not start from stale evidence.
