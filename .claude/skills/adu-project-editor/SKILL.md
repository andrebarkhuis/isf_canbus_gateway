---
name: adu-project-editor
description: Edit an ECUmaster ADU project (.adu) directly - add CAN message objects and inputs from a CANX stream without using the client's import, point page items at channels, replace channels, remove old objects. Use when the user wants the ADU dash wired to known CAN frames (e.g. the GT86-ISF gateway's isf-bus.canx / gt86-bus.canx) or wants a page's values changed, and the ADU client is not the tool to do it with.
---

# ADU project editor

Edits `.adu` files of the ECUmaster ADU client (tested with client/firmware 0.120). The file is a zip:
`project.adu` (the project XML) plus the page background images. `adu_project.py` next to this file does the
work; it edits the XML as text so the client's formatting survives, and refuses to write anything that is
not well-formed XML. Always write to a **new file** (`--out`) and keep the user's original.

```
python adu_project.py info        isf-old.adu
python adu_project.py import-canx isf-old.adu isf-bus.canx --bus CAN2 --group CAN2_IN_ISF \
                                  --channels ENG_Coolant,ENG_KnockFeedback --out isf-new.adu
python adu_project.py set-channel isf-new.adu --page PAGE_HOME --label "TIMING ADV" \
                                  --channel ecu.ignAngle --decimals 1 --out isf-new.adu
python adu_project.py replace-channel in.adu --from c_speed --to ecu.speed --out out.adu
python adu_project.py remove-mob   in.adu --name m_rpm --out out.adu     # also drops its inputs, renumbers the rest
python adu_project.py extract in.adu dir / pack dir out.adu              # for hand edits
```

Workflow: `info` first, decide, edit into a new file, run `info` on the result, then the user opens it in the
client, checks the project tree and pages, and sends it to the device.

## project.adu structure

```
<project version=".." clientVersion=".." firmwareVersion=".." platform="ADU" serialNumber="..">
  <variables>
    <scope id="adu">       device settings (buttons, shift light, ...)
    <scope id="general">   logging (logToMemoryChannel, logCondition, logCond2..4Channel), CAN2 terminator/speed, GPS
    <scope id="functions"> arrays of everything in the project tree:
       canbusMobs, canbusInputs, analogInputs, digitalInputs, numbers, functions, enumerations, timers, tables,
       canbusExports, dashboardPages, dashboardAlarms, groups, canbusKeyboards...
  <tables>                 calibration tables
  <loggedChannels>         <channel id="NAME" base="" cond2="" cond3="" cond4=""/> per channel: log rate per profile
  <loggedGroups>
```

Every setting is `<variable id="..." value="..." type="..."/>`. An array holds `<item>` elements; an empty array
is self-closing (`<array id="digitalInputs" type="eDigitalInput"/>`). Items of `numbers` / `functions` carry a
`<program>` of operation items; a page item carries a `<layout>` of UI items (`eUIText`, `eUIRect`, `eUIGroup`,
`eUIImage`, `eUIRPMBar`, `eUIGrid`), which can nest inside `eUIGroup`.

Channels are referenced **by name** everywhere (`type="channel"` variables): `ecu.rpm`, `adu.battery`, a CAN
input's name, a function's name, the constants `zero` / `one`. Message objects are referenced **by array
index** (`mobIndex`): deleting one shifts every later index, which `remove-mob` handles. Groups are referenced
by **1-based position** in the `groups` array (`groupId`; 0 = none).

## CAN message object (`canbusMobs` item, struct eCanbusMob)

| variable | meaning |
|---|---|
| canbusID | decimal; a 29-bit id carries flag 0x20000000 (seen on one object; inferred, verify on first use) |
| canifChannel | 0 = CAN1, 1 = CAN2 |
| type | 0 Normal, 0xD0 Compound/multiplexed, 34..40 PMU streams |
| width | 0/1/2/3 = 1/2/4/8 consecutive frames from the base id |
| timeout | hundredths of a second (100 = 1 s) |
| mux* | multiplexer settings, used by Compound only |

## CAN input (`canbusInputs` item, struct eCanbusInput)

| variable | meaning |
|---|---|
| name | the channel name; an input that feeds a built-in channel is conventionally named `.` |
| mobIndex, mobOffset | message object index, frame offset within it |
| byteOffset, bitCount, bitPosition | position in the frame |
| dataFormat | 0 U8, 1 S8, 2 U16, 3 S16, 4 U64, 5 S64 |
| dataEndian | 0 little, 1 big |
| multiplier, divider, offset, decimalPlaces | shown = (raw * multiplier / divider + offset) / 10^decimalPlaces; the three are s16 (-32768..32767): the client clamps anything larger ("Value -102400 for field 'offset' is out of range"), so a big offset needs fewer decimal places |
| overrideChannel | the built-in channel this input feeds (`ecu.clt`, `ecu.rpm`, `ecu.flags4.MIL`...), or empty |
| holdPrevious, defaultValue, timeoutValue | behaviour on timeout |
| quantity, unit | client-only unit codes, see below |

Unit codes (quantity, unit), harvested from ECUmaster's own template projects: rpm (3,1), degrees (9,0),
lambda (11,0), kPa (16,1), bar (16,3), % (17,0), °C (18,0), ms (19,3), km/h (20,0), V (21,0), L (22,0).
Any other unit gets (0,0) and a warning; the user sets it in the client. A named input also needs a
`loggedChannels` entry (the script adds it); `.` inputs do not.

Built-in channels and what they expect: `ecu.rpm` rpm, `ecu.speed` km/h 1 dp, `ecu.clt`/`iat`/`oilTemp`/
`gearboxOilTemp`/`ambientAirTemp` °C 1 dp, `ecu.tps` % 1 dp, `ecu.battery` V 2 dp, `ecu.ignAngle` °BTDC 1 dp,
`ecu.lambda1`/`lambda2`/`lambda1Trgt` lambda 3 dp, `ecu.fuelPress` bar 2 dp, `ecu.injPW` ms 2 dp, `ecu.gear`,
`ecu.flags4.MIL`, `ecu.flags5.ACClutch`. One input per built-in channel in a project.

The ADU takes at most **100 CAN inputs** over both buses, and the old inputs already in a project count.
Import selectively (`--channels`, `--channels-file`, `--only-overrides`) and remove what is obsolete.

## CANX in

`import-canx` reads an ECUmaster CANX stream (`<mob canbusID ext width><frame offset><channel id type
byteOffset bitCount bitPosition unit multiplier divider offset decimalPlaces override/>`), which is what the
GT86-ISF gateway generates (`tools/generate.py` -> `gt86-bus.canx`, `isf-bus.canx`). One message object per
CANX mob that has a selected channel, named `m_<hex id>` (`m_x...` for 29-bit), one input per channel; channels
whose names already exist in the project are skipped. Override channels are named `.` by default (`--names
keep` keeps the CANX id as the name as well; unverified in the client).

## Pages

A value on a page is an `eUIText` item with `variableChannel`; its label is usually the `eUIText` with `text`
just before it in the layout, and some items carry both (`text` + `variableChannel`). `set-channel` finds the
value item by that label on the given page and sets `variableChannel` and optionally `decimalPlaces`. Other
channel-typed fields on items: `visibleChannel` (0 hides), `colorChannel` (lamps), `activateChannel` on a page.
`replace-channel` rewrites every reference to a channel name in the whole project (pages, functions, numbers).

## Checks before handing over

- `info` on the result: no duplicate channel names, inputs <= 100, every page reference resolves to an
  existing input, built-in channel, function, number or analog input.
- Removing a message object must also drop its own logged channels (`<name>.timeout` in `loggedChannels`), or
  the client reports "Channel 'm_x.timeout' not found. Skipped." on load; `remove-mob` does this.
- The old project may read the same frames itself (e.g. raw IS F 0x2C4 into `ecu.rpm`); one source per
  built-in channel, so remove the old `.` input or message object first.
- Tell the user what was changed, what still points at a channel that has no source, and that the client is
  the place to confirm the unit codes and the 29-bit id flag the first time either is used.
