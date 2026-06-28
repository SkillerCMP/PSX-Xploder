# PlayStation Cheat Code Types

## Xploder / Xplorer / FX, GameShark / Action Replay, Caetla, and DuckStation

**Definitive working reference - Version 1.1**  
**Prepared from firmware analysis, original period documentation, DuckStation source analysis, and Xploder PSX Converter v1.05 tests**  
**Date: June 27, 2026**

> **Scope:** Original PlayStation (PS1/PSX) cheat devices, code formats, and DuckStation emulator extensions only. This document deliberately separates the user-facing code syntax, loader/preprocessor behavior, and active runtime records. Those layers are often similar, but they are not interchangeable.

## Contents

1. Scope, evidence, and terminology
2. Cross-device quick comparison
3. Xploder / Xplorer / FX
4. GameShark / Action Replay
5. Caetla 0.34
6. Caetla 0.341 extended types
7. DuckStation
8. Safe conversion rules
9. Worked examples
10. Open and version-specific items
11. Evidence base

# 1. Scope, Evidence, and Terminology

## 1.1 What "definitive" means here

This is the strongest reference supported by the material currently available. Every item is classified so that confirmed behavior is not mixed with old assumptions.

| Status | Meaning |
|---|---|
| **Verified** | Traced in a supplied firmware/runtime binary, reproduced by a test, or both. |
| **Documented** | Described by original manufacturer or period technical documentation, but not fully traced in every examined firmware. |
| **Device-specific** | Behavior is known, but no safe one-to-one conversion exists for all target devices. |
| **Version-specific** | Valid for a particular firmware, mode, or loader path and must not be generalized without checking that context. |
| **Unresolved** | Evidence conflicts or a loader/runtime path is not fully mapped. The code must be preserved instead of guessed. |

## 1.2 The three layers that must remain separate

| Layer | What it contains | Why it matters |
|---|---|---|
| **Public or database format** | Code lines shown to the user or stored in the device database. | Prefixes are interpreted according to the device and current mode. |
| **Loader or preprocessor format** | Rows after decryption and special expansion, before the active engine runs. | Type 5/6 blocks, global controls, and other records may be transformed here. |
| **Active runtime format** | The compact records executed by the installed cheat engine. | A runtime type nibble does not always have the same public meaning. |

A converter must identify the source family and layer before interpreting a prefix. For example, GameShark public Type `50` is a serial repeater, while Xploder Type `5` is an embedded payload/mass-write block. They are unrelated despite the similar number.

## 1.3 Common notation

| Symbol | Meaning |
|---|---|
| `A`, `XXXXXX`, `AAAAAAA` | Address or packed address field. |
| `V`, `YY`, `YYYY`, `VVVVVVVV` | Value, compare value, or increment. |
| `C`, `CC`, `CCC` | Count. |
| `I`, `IIII` | Address increment or signed offset, depending on the type. |
| `S`, `SSSSSSSS` | Value increment or source field, depending on the type. |
| `Z` | Width selector. |
| **Next row** | The following logical code record. It may be a carrier or part of the current operation rather than a separate cheat. |

PS1 user codes commonly show a 24-bit address beneath a type prefix. The runtime normally reaches the corresponding `0x80000000` RAM mirror. This document writes addresses in the form used by each device rather than forcing every example into one memory notation.

## 1.4 Condition convention

Unless identified as a global gate, a condition controls the **next logical operation**. A two-row repeater, copy, pointer, Type 5 block, or Type 6 block must be treated as one operation by a context-aware tool.

# 2. Cross-Device Quick Comparison

## 2.1 Core operations

| Operation | Xploder / Xplorer | GameShark / Action Replay | Caetla native |
|---|---|---|---|
| 8-bit constant write | `3` | `30` | `30` |
| 16-bit constant write | `8` | `80` | `80` |
| 32-bit constant write | Active Type `0`; ordinary database value is normally zero-extended from 16 bits | No standard single-row public type in the examined family; use two `80` rows where suitable | `90` |
| 16-bit equal condition | Canonical Type `7` | `D0` | `D0` |
| 16-bit not-equal condition | Canonical Type `9` | `D1` | `D1` |
| 16-bit less-than-or-equal | No confirmed basic public equivalent | `D2` | `D2` |
| 16-bit greater-than | No confirmed basic public equivalent | `D3` | `D3` |
| 8-bit equal / not-equal / less-or-equal / greater | No confirmed basic public equivalent | `E0` / `E1` / `E2` / `E3` | `E0` / `E1` / `E2` / `E3` |
| Serial repeater | Type `B` | Type `50` | Native Type `B`, or Type `50` in GameShark mode |
| Embedded payload write | Type `5` | No equivalent | No equivalent |
| Live RAM block copy | No exact Type 5 equivalent | `C2` | Caetla .341 `C2` |
| Executable/breakpoint payload | Type `6` | No exact equivalent | No exact equivalent |
| Arithmetic | No confirmed direct basic type | `10`, `11`, `20`, `21` | .341 native `10`-`12`, `20`-`22`; 0.34 GameShark mode supports the GS meanings |
| Bit set/clear/test | No confirmed direct basic type | No standard equivalent in this family | Native `50`-`6A` families |
| Pointer/indirect write | No confirmed public equivalent | No confirmed public equivalent | Caetla .341 `C3` |

## 2.2 DuckStation at a glance

DuckStation has a GameShark-compatible interpreter plus emulator-only extensions. The same text can therefore mean something different on physical hardware and in DuckStation.

| Operation | DuckStation opcode / form | Compatibility note |
|---|---|---|
| 8-bit / 16-bit / 32-bit writes | `30` / `80` / `90` | `90` is a DuckStation extension. |
| 16-bit equal / not equal | `D0` / `D1` | Same broad next-operation model as AR/GS. |
| 16-bit less / greater | `D2` / `D3` | DuckStation uses strict unsigned `<` and `>`. |
| 8-bit less / greater | `E2` / `E3` | DuckStation uses strict unsigned `<` and `>`. |
| Classic slide | `50` plus a following `30`, `80`, or `90` row | A zero count is a safe no-op in DuckStation. |
| Improved signed slide | `53` | DuckStation extension; supports write and bit-operation followers. |
| Live RAM byte copy | `C2` plus a carrier row | Carrier opcode/value are ignored; only its low 24-bit address is used. |
| Block conditions | `A4`, `C0`, `C3-C6`, `D5-D7`, `52`, `F6` | Blocks end at `00000000 FFFF`. |
| Bit set / clear | `31/32`, `81/82`, `91/92` | DuckStation extensions. |
| Internal registers / pointers | `51`, with comparisons in `52` | DuckStation-specific mini virtual machine. |
| Native MIPS source | `Type = Assembly` | Separate from the GameShark instruction engine. |

## 2.3 Prefixes that are especially easy to misread

| Prefix | Device context | Actual meaning |
|---|---|---|
| `50` | GameShark / Action Replay | Two-row serial repeater header. |
| `50` | Caetla native | 8-bit set-bits operation. |
| `50` | Caetla GameShark mode | GameShark serial repeater header. |
| `5` | Xploder | Embedded mass-write payload block. |
| `10` | GameShark or Caetla GameShark mode | Increment 16-bit. |
| `10` | Caetla .341 native mode | Increment 8-bit. |
| `90` | Caetla native | 32-bit constant write. |
| `9` | Xploder public/runtime family | 16-bit not-equal conditional in the canonical converter model. |
| `9` | Caetla Type B seed | 32-bit write seed used by the slider. |
| `90` | DuckStation | 32-bit constant write extension. |
| `D2` / `E2` | GameShark Pro 3.1 hardware | Verified handler behaves as unsigned less-than-or-equal. |
| `D2` / `E2` | DuckStation | Source implements strict unsigned less-than. |
| `D5` / `D6` | GameShark hardware family | Device/menu controller controls; exact transformation is firmware-specific. |
| `D5` / `D6` | DuckStation | Exact-controller-state block conditions terminated by `00000000 FFFF`. |
| `C0` | GameShark documentation/hardware family | Global or loader-level control in the examined references. |
| `C0` | DuckStation | 16-bit equality block condition. |

# 3. Xploder / Xplorer / FX

## 3.1 Confirmed data path

The analyzed Xploder firmware stores six-byte encrypted database rows. Selected rows are decrypted, special Type 5/6/control records are preprocessed, ordinary rows are packed into eight-byte active records, and the installed runtime engine executes the resulting list.

```text
Encrypted .FCD or cartridge database
    -> six-byte encrypted code records
    -> per-row decryption
    -> Type 5 / Type 6 / control preprocessing
    -> eight-byte active runtime records
    -> installed runtime interpreter
```

One fully mapped XTGER firmware uses:

| Component | Address / location |
|---|---|
| Main database in cartridge ROM | `0x1F025000` |
| Active runtime buffer | `0x1F022600` |
| Installed engine | `0x80000A00` |
| Type 5 payload area in active buffer | `active + 0x400` |
| Type 6 fixed descriptor area | `active + 0x782` |

Earlier Xploder memory-dump work found a related interpreter at `0x80037B00` with an active list at `0x8000FF00`. The common handler families agree, but condition polarity differs in one firmware trace; see Section 3.8.

## 3.2 Canonical Xploder type map

| Type | Status | Canonical meaning | Important notes |
|---|---|---|---|
| `0` | Verified runtime | 32-bit write | Active engine writes a full word. Ordinary six-byte database rows provide only a 16-bit value, normally zero-extended. |
| `1` | Verified carrier use | Type B seed/data carrier | Not a normal direct runtime write in the mapped engine. |
| `2` | Unresolved | No confirmed direct handler | Preserve. |
| `3` | Verified | 8-bit constant write | Uses low 8 bits. |
| `4` | Version-specific | Calls a cartridge helper in XTGER | Helper parameters and public syntax are not fully mapped. Preserve unless the exact firmware is known. |
| `5` | Verified | Embedded mass-write payload | Header owns a precise number of following six-byte payload rows. |
| `6` | Verified | Breakpoint/executable payload | Loader builds a descriptor and installs executable bytes at `0x80000040`. |
| `7` | Verified canonical model | 16-bit equal condition for the next logical operation | Runtime-polarity caveat applies in XTGER 2.0081. |
| `8` | Verified | 16-bit constant write | Most common normal Xploder write. |
| `9` | Verified canonical model | 16-bit not-equal condition for the next logical operation | Runtime-polarity caveat applies in XTGER 2.0081. |
| `A` | Unresolved | No confirmed direct handler | Preserve. |
| `B` | Verified | 16-bit serial repeater / slider | Uses the following row as the seed address and starting value. |
| `C` | Unresolved / compatibility | No confirmed direct handler in mapped engines | Some databases contain pass-through or compatibility rows. Preserve. |
| `D` | Loader-specific | Control rows may be preprocessed before runtime | No direct ordinary runtime handler confirmed. |
| `E` | Verified | 8-bit write to encoded address plus one | Useful for the high byte of a halfword. |
| `F` | Verified canonical model | Global 16-bit equality gate | Runtime-polarity caveat applies in XTGER 2.0081. |

## 3.3 Direct writes

```text
3AAAAAAA 00YY       Write byte YY to A
8AAAAAAA YYYY       Write halfword YYYY to A
0AAAAAAA VVVVVVVV   Active/runtime word write
EAAAAAAA 00YY       Write byte YY to A + 1
```

### Type 0 value-width rule

The active Type `0` handler writes a complete 32-bit value. A normal Xploder database record is only six bytes: four bytes for the code/address and two bytes for the value. Therefore an ordinary database Type `0` row generally becomes `0x0000YYYY` at runtime unless a loader-generated structure supplies a full value.

## 3.4 Default-off/menu-state bit

The second hexadecimal digit of an external Xploder code carries the default-off/menu-state bit. In numeric terms, the loader sets bit `0x08000000` in the first word.

```text
80012345 0009   Normal Type 8 row
88012345 0009   Same operation marked default-off
```

This bit is menu metadata. It must not be mistaken for a different operation type or included in the physical RAM address.

## 3.5 Type B serial repeater

### Canonical external form

```text
B0CCIIII VVVV
10AAAAAA DDDD
```

| Field | Meaning |
|---|---|
| `CC` | Repeat count, `01` through `FF` in the portable external form. |
| `IIII` | Signed 16-bit address step. |
| `VVVV` | Signed 16-bit value step. |
| `AAAAAA` | Starting address from the following carrier row. |
| `DDDD` | Starting 16-bit value from the carrier row. |

Example:

```text
B0030002 0001
10010000 0005
```

Result:

```text
80010000 0005
80010002 0006
80010004 0007
```

The active engine can expose a wider 12-bit count field (`BCCCIIII`). The converter deliberately uses the low eight-bit count externally because the second nibble is also used for the default-off/key state. The seed row is a data carrier and must not be executed separately.

## 3.6 Type 5 embedded mass-write payload

### Canonical decrypted form

```text
5AAAAAAA NNNN
[payload row 1: six bytes]
[payload row 2: six bytes]
...
```

| Field | Meaning |
|---|---|
| `AAAAAAA` | Destination address. |
| `NNNN` | Exact logical payload byte count, normally `0001` through `0FFF`. |
| Following rows | Six stored bytes per row; row count is `ceil(NNNN / 6)`. |

Rules:

1. The header owns exactly the rows required by its byte count.
2. Payload rows are data, even when their first nibble resembles `3`, `5`, `6`, `9`, `E`, or another ordinary type.
3. Only bytes beyond `NNNN` in the final stored row are logical padding.
4. The first row after the calculated span returns to normal top-level parsing.
5. Canonical RAW shows the direct byte count. It is not a count-plus-six format.
6. Encrypted/public Type 5 records embed a payload decryption key in the size field. Keys `6` and `7` are confirmed.

### Payload byte order

The loader's six-byte memory order and the canonical display order differ. The converter applies a self-inverse byte-order transform to each payload row. A known pair is:

```text
Loader bytes:         AC 08 00 04 00 08
Canonical RAW display: 040008AC 0800
```

### Important distinction

Xploder Type `5` installs **embedded bytes contained in the code itself**. GameShark/Caetla `C2` copies bytes from a **live source address in RAM**. Those are not exact equivalents.

## 3.7 Type 6 breakpoint/executable payload

### Canonical structure

```text
6??????? NNNN          Type 6 header; NNNN is logical executable-byte count
AAAAAAAA TTTT          Breakpoint descriptor row
MMMMMMMM PPPP          Four-byte mask + first two executable bytes
[payload rows continue]
```

| Element | Meaning |
|---|---|
| `NNNN` | Direct logical executable payload size. Zero is valid. |
| Descriptor row | Breakpoint address/type information. |
| Mask row | First four bytes are the breakpoint mask; final two bytes start the executable payload. |
| Remaining rows | Continue the executable payload in six-byte chunks. |
| Install destination | Executable bytes are installed at `0x80000040`. |

The number of continuation rows is:

```text
ceil((0x0A + logical_payload_bytes) / 6)
```

The fixed `0x0A` bytes are the six-byte breakpoint row plus the four-byte mask. The first two payload bytes share the mask row. The runtime rounds the copied executable size up to a complete 32-bit word boundary.

A Type 6 block is one structured operation. Rows inside it must never be redispatched by their leading nibble. This rule is essential for nested Type 6/Type 5 data and for exact encrypted round trips.

## 3.8 Condition polarity and firmware-layer caveat

The canonical Xploder model used by the converter and the earlier runtime dump is:

```text
7AAAAAAA YYYY   Run the next logical operation when halfword[A] == YYYY
9AAAAAAA YYYY   Run the next logical operation when halfword[A] != YYYY
FAAAAAAA YYYY   Continue the remaining list when halfword[A] == YYYY
```

A static trace of the installed XTGER 2.0081 engine shows the opposite branch sense at the active-record level:

- Type `7` skips when equal.
- Type `9` skips when not equal.
- Type `F` exits when equal.

The XTGER loader also performs a dedicated preprocessing pass for `D/F` control rows before staging the active list. The most defensible conclusion is that public/database conditions may be inverted or rewritten before they reach this particular engine. Therefore:

- use canonical public semantics for normal Xploder RAW conversion;
- do not infer public behavior from an isolated XTGER active row;
- preserve unknown control rows when the exact firmware/loader transformation is not known.

This is a documented version/layer difference, not a reason to silently flip every Xploder condition.

# 4. GameShark / Action Replay

## 4.1 Family relationship

The examined PlayStation GameShark and Action Replay products share the Datel public code family and closely related database formats. Names, region branding, firmware version, and supported advanced types vary. This section therefore uses **AR/GS** for their common public code language and calls out version-specific items separately.

The GameShark Pro 3.1 dispatcher was verified directly from a decrypted firmware image. Original Action Replay/GameShark CDX technical documentation was used for public global-control types that are not all present in that dispatcher.

## 4.2 Public code type map

| Prefix | Status | Operation | Notes |
|---|---|---|---|
| `30` | Verified | 8-bit constant write | `30XXXXXX 00YY`. |
| `80` | Verified | 16-bit constant write | `80XXXXXX YYYY`. |
| `10` | Verified | Increment 16-bit | Adds `YYYY` to the halfword. |
| `11` | Verified | Decrement 16-bit | Subtracts `YYYY` from the halfword. |
| `20` | Verified | Increment 8-bit | Adds low byte `YY`. |
| `21` | Verified | Decrement 8-bit | Subtracts low byte `YY`. |
| `D0` | Verified | 16-bit equal condition | Continue with next logical operation when equal. |
| `D1` | Verified | 16-bit not-equal condition | Continue when not equal. |
| `D2` | Verified correction | 16-bit less-than-or-equal condition | Inclusive `<=`, not strict `<`, in GameShark Pro 3.1. |
| `D3` | Verified | 16-bit greater-than condition | Strict `>`. |
| `D4` | Verified | Controller-state condition | Encoded address is ignored by the verified handler. |
| `D5` | Documented / menu-specific | All-code or joypad control row | Preserve as a GameShark control, not a RAM compare. |
| `D6` | Documented / menu-specific | All-code deactivation control | Preserve when no exact destination equivalent exists. |
| `E0` | Verified | 8-bit equal condition | Byte equivalent of `D0`. |
| `E1` | Verified | 8-bit not-equal condition | Byte equivalent of `D1`. |
| `E2` | Verified correction | 8-bit less-than-or-equal condition | Inclusive `<=`. |
| `E3` | Verified | 8-bit greater-than condition | Strict `>`. |
| `50` | Verified | Serial repeater / slider | Consumes the following `30` or `80` seed row. |
| `C0` | Original documentation / converter semantic | Global 16-bit equality gate | Controls all following/enabled codes, not only one row. |
| `C1` | Original documentation | Delayed global activation | Timing is firmware/game dependent; direct Pro 3.1 execution path is not fully traced. |
| `C2` | Verified | Copy byte block from RAM source to RAM destination | Following row carries destination. |
| `1F800xxx` | Converter-recognized legacy extension | 8-bit scratchpad write | Preserve address in the PS1 scratchpad range. |

## 4.3 Direct writes and arithmetic

```text
30XXXXXX 00YY   Write byte YY
80XXXXXX YYYY   Write halfword YYYY
10XXXXXX YYYY   halfword[address] += YYYY
11XXXXXX YYYY   halfword[address] -= YYYY
20XXXXXX 00YY   byte[address] += YY
21XXXXXX 00YY   byte[address] -= YY
```

Arithmetic executes every pass while enabled. It is usually placed under a button or memory condition so that it does not change continuously.

## 4.4 Conditions

```text
D0XXXXXX YYYY   halfword[address] == YYYY
D1XXXXXX YYYY   halfword[address] != YYYY
D2XXXXXX YYYY   halfword[address] <= YYYY
D3XXXXXX YYYY   halfword[address] >  YYYY

E0XXXXXX 00YY   byte[address] == YY
E1XXXXXX 00YY   byte[address] != YY
E2XXXXXX 00YY   byte[address] <= YY
E3XXXXXX 00YY   byte[address] >  YY
```

The `D2/E2` inclusive result is firmware-verified. Older code lists often label these as strict less-than. A definitive converter should use `<=` for GameShark Pro 3.1 and retain a firmware-version note if supporting older unverified handlers.

## 4.5 D4 controller-state condition

```text
D4000000 YYYY
```

The verified handler compares `YYYY` against a controller/input state maintained by the GameShark firmware. The low 24 address bits of any `D4xxxxxx` source row are ignored, so canonical output uses `D4000000`.

The exact button-mask convention belongs to the firmware's controller state. Do not reinterpret `D4` as a normal read from RAM address zero.

## 4.6 D5 and D6 control rows

Period documentation describes `D5` as an all-code button activator and `D6` as a universal deactivator. In the GameShark Pro 3.1 built-in database, `D5000000 0000` appears under entries named `(M) Joypad toggle off`.

The safe rule is:

- recognize only the canonical `D5000000` / `D6000000` control forms;
- keep them in the AR/GS family;
- do not label them as ordinary RAM conditions;
- warn rather than invent an equivalent when converting to another device.

## 4.7 Type 50 serial repeater

```text
5000CCAA VVVV
TTXXXXXX DDDD
```

| Field | Meaning |
|---|---|
| `CC` | Repeat count, `01` through `FF`. |
| `AA` | Address increment after each write. |
| `VVVV` | Value increment after each write. |
| `TT` | `30` for byte writes or `80` for halfword writes. |
| `XXXXXX` | Starting address. |
| `DDDD` | Starting value; low byte used by a `30` seed. |

Example:

```text
50000302 0001
80010000 0005
```

Approximate expansion:

```text
80010000 0005
80010002 0006
80010004 0007
```

The handler writes before decrementing the count. A zero count therefore underflows and is unsafe; it does not mean zero writes.

## 4.8 C0 and C1 global controls

```text
C0XXXXXX YYYY   Enable/continue the broad code set when halfword[address] == YYYY
C1000000 YYYY   Delay broad code activation by a firmware-dependent count
```

`C0` is broader than `D0`: it gates the remaining or enabled code set rather than only the next row. `C1` is a delay counter, not a memory comparison. Delay values are approximate and can vary with the game and device timing.

## 4.9 C2 live RAM byte copy

```text
C2SSSSSS CCCC
80DDDDDD 0000
```

Copies `CCCC` bytes from RAM source `SSSSSS` to RAM destination `DDDDDD`.

The verified GameShark Pro 3.1 handler uses the following row only as the destination-address carrier. Its high nibble and value are ignored. A converter may accept a noncanonical carrier but should emit the standard `80DDDDDD 0000` form.

The copy loop writes once before decrementing, so `CCCC = 0000` is unsafe and must not be generated.

## 4.10 Database and encrypted-ROM notes

Two database layouts are confirmed in examined Action Replay/GameShark firmware:

- aligned v1-style records with eight-byte stored rows;
- compact v2-style records with six-byte stored rows.

Datel v2.x/v3.x encrypted firmware can be detected from its contents, decrypted in memory, and accepted only after the complete database structure validates. Filename extension is not a reliable format identifier.

# 5. Caetla 0.34

## 5.1 Persistent interpreter modes

Caetla changes the meaning of several prefixes according to a persistent mode row:

```text
FFFFFFFF 00000000   Native Caetla mode
FFFFFFFF 00000001   GameShark-compatible mode
FFFFFFFF 00000002   GameShark-compatible mode
FFFFFFFF 00000003   Native Caetla mode
```

This state remains active until another `FFFFFFFF` selector is encountered. A parser that ignores the mode can misread valid code completely.

## 5.2 Native 0.34 type map

| Prefix | Width | Verified operation |
|---|---:|---|
| `30` | 8-bit | Constant write. |
| `80` | 16-bit | Constant write. |
| `90` | 32-bit | Constant write. |
| `50` / `51` / `52` | 8 / 16 / 32 | Set bits: `memory |= mask`. |
| `58` / `59` / `5A` | 8 / 16 / 32 | Clear bits: `memory &= ~mask`. |
| `60` / `61` / `62` | 8 / 16 / 32 | Continue when any masked bit is set. |
| `68` / `69` / `6A` | 8 / 16 / 32 | Continue when all masked bits are clear. |
| `70` / `71` / `72` | 8 / 16 / 32 | Copy one byte / halfword / word from source to destination. |
| `D0`-`D3` | 16-bit | Equal, not equal, less-or-equal, greater conditions. |
| `E0`-`E3` | 8-bit | Equal, not equal, less-or-equal, greater conditions. |
| `B` | Seed-selected | Native slider/repeater. |
| `1F800xxx` | 8-bit | Scratchpad byte write. |

The earlier text note that called `1F800xxx` a 16-bit write is superseded by the verified parser/engine behavior: it is treated as an 8-bit scratchpad write.

## 5.3 Native direct writes

```text
30XXXXXX 00YY       Write byte
80XXXXXX YYYY       Write halfword
90XXXXXX VVVVVVVV   Write word
1F800xxx 00YY       Write byte in PS1 scratchpad
```

## 5.4 Native bit operations

```text
50XXXXXX 00YY       Set byte bits
51XXXXXX YYYY       Set halfword bits
52XXXXXX VVVVVVVV   Set word bits

58XXXXXX 00YY       Clear byte bits
59XXXXXX YYYY       Clear halfword bits
5AXXXXXX VVVVVVVV   Clear word bits
```

Bit tests control the following logical operation:

```text
60 / 61 / 62   Continue when (memory & mask) != 0
68 / 69 / 6A   Continue when (memory & mask) == 0
```

## 5.5 Native single-unit copies

```text
70XXXXXX YYYYYYYY   Copy one byte     from source X to destination Y
71XXXXXX YYYYYYYY   Copy one halfword from source X to destination Y
72XXXXXX YYYYYYYY   Copy one word     from source X to destination Y
```

These are one-unit copies. They are not the same as Xploder Type 5, and they only represent a GameShark/Caetla .341 block copy when the requested length is exactly 1, 2, or 4 bytes with the matching width.

## 5.6 Native Type B slider

```text
BCCCIIII SSSSSSSS
T0AAAAAA VVVVVVVV
```

| Field | Meaning |
|---|---|
| `CCC` | 12-bit repeat count, `001` through `FFF`. |
| `IIII` | 16-bit address increment. |
| `SSSSSSSS` | 32-bit value increment. |
| `T` | Seed operation selector. |
| `AAAAAA` | Starting address. |
| `VVVVVVVV` | Starting value. |

Confirmed write seeds:

| Seed `T` | Write width |
|---|---|
| `3` | 8-bit |
| `8` | 16-bit |
| `9` | 32-bit |

The firmware can also route Type B seeds through native `C`, `D`, and `E` handlers. Those combinations are device-specific and should be preserved because they do not map safely to a simple write repeater.

## 5.7 GameShark-compatible mode in 0.34

While selector `FFFFFFFF 00000001` or `00000002` is active, these verified meanings apply:

| Prefix | Meaning in GameShark mode |
|---|---|
| `30` | 8-bit write. |
| `80` | 16-bit write. |
| `10` | Increment 16-bit. |
| `11` | Decrement 16-bit. |
| `20` | Increment 8-bit. |
| `21` | Decrement 8-bit. |
| `50` | GameShark two-row serial repeater. |
| `D0`-`D3` | 16-bit conditions. |
| `E0`-`E3` | 8-bit conditions. |

Native `90`, native Type `B`, bit operations, and `70`-`72` copies require native mode.

## 5.8 Types not safely assigned in 0.34

The following must not be generated as confirmed native 0.34 operations:

- native `12` / `22` 32-bit arithmetic;
- GameShark `D4` / `D5` / `D6` control rows;
- Caetla .341 `C2` block copy;
- Caetla .341 `C3` pointer write;
- arbitrary native `C` rows whose exact subtype is not mapped.

# 6. Caetla 0.341 Extended Types

Caetla .341 adds native arithmetic and two advanced two-row formats. These additions are recognized independently from the older 0.34-safe output mode.

## 6.1 Native arithmetic

| Prefix | Width | Operation |
|---|---:|---|
| `10` | 8-bit | Increment. |
| `11` | 16-bit | Increment. |
| `12` | 32-bit | Increment. |
| `20` | 8-bit | Decrement. |
| `21` | 16-bit | Decrement. |
| `22` | 32-bit | Decrement. |

```text
10012345 0001       byte     += 1
11012346 0002       halfword += 2
12012348 00000004   word     += 4
20012345 0001       byte     -= 1
21012346 0002       halfword -= 2
22012348 00000004   word     -= 4
```

Do not put the GameShark-mode selector before these native .341 rows. The same prefixes have different meanings in GameShark-compatible mode.

## 6.2 C2 live RAM block copy

```text
C2SSSSSS CCCC
80DDDDDD 0000
```

| Field | Meaning |
|---|---|
| `SSSSSS` | RAM source address. |
| `CCCC` | Byte count, `0001` through `FFFF`. |
| `DDDDDD` | RAM destination address. |

This is semantically compatible with GameShark `C2`. A zero count has no safe empty-copy meaning in the verified binary and must be preserved rather than generated or expanded.

## 6.3 C3 indirect/pointer write

```text
C3XXXXXX 000Z
9100YYYY DDDDDDDD
```

| Field | Meaning |
|---|---|
| `XXXXXX` | Address containing the base pointer. |
| `Z = 0` | Write 8-bit. |
| `Z = 1` | Write 16-bit. |
| `Z = 3` | Write 32-bit. |
| `Z = 2` | Invalid width selector in the verified .341 binary. |
| `YYYY` | Signed 16-bit offset added to the loaded pointer. |
| `DDDDDDDD` | Data written at `pointer + offset`. |

Example:

```text
C308C6B8 0001
91000022 000003E8
```

This reads the pointer at `8008C6B8`, adds signed offset `0x0022`, and writes `0x03E8` as a 16-bit value.

The C3 pair is recognized as one operation regardless of the persistent native/GameShark mode selector. Direct binary verification corrected an older text-only note: selector `3`, not `2`, is the 32-bit write form.

# 7. DuckStation

DuckStation is an emulator implementation, not a physical cheat cartridge. Its `Gameshark` interpreter starts with familiar Action Replay/GameShark syntax, but it also defines many emulator-only opcodes and several semantics that differ from verified GameShark Pro 3.1 hardware. A converter must keep the DuckStation target context explicit.

## 7.1 File-level format and activation

DuckStation supports two entry types:

| `Type` value | Meaning |
|---|---|
| `Gameshark` | Two-word hexadecimal instructions interpreted by DuckStation's GameShark-compatible engine. |
| `Assembly` | Native PS1 MIPS source assembled and patched by DuckStation. |

Supported activation values are `Manual` and `EndFrame`. `Assembly` entries require `EndFrame`.

```ini
[Code Name]
Author = Optional Author
Description = Optional Description
Type = Gameshark
Activation = EndFrame
80012340 0063
```

The file parser also preserves group paths, comments, options, option ranges, achievement restrictions, ignore flags, and contiguous `?` option wildcards in values.

## 7.2 GameShark instruction layout

```text
TTAAAAAA VVVVVVVV
```

| Field | Meaning |
|---|---|
| `TT` | Opcode byte. |
| `AAAAAA` | 24-bit address or opcode-specific parameter field. |
| `VVVVVVVV` | Value or opcode-specific parameter field. |

A `00000000 FFFF` row is a separator for DuckStation's advanced block conditions. It is not an ordinary RAM write.

## 7.3 Core GameShark-compatible interpreter

| Opcode | DuckStation operation | Important note |
|---|---|---|
| `00` | No operation / block separator | `00000000 FFFF` closes advanced blocks. |
| `10` / `11` | Add / subtract 16-bit | Low 16-bit operand. |
| `1F` | Scratchpad 16-bit write | Not an 8-bit write. |
| `20` / `21` | Add / subtract 8-bit | Low 8-bit operand. |
| `30` | 8-bit constant write | Low 8-bit operand. |
| `50` | Classic two-row slide | Follower may be `30`, `80`, or `90`. |
| `80` | 16-bit constant write | Low 16-bit operand. |
| `C0` | 16-bit equality block | Executes until the separator when true. |
| `C1` | Delayed activation | Delays following execution using the emulator frame-derived counter. |
| `C2` | Live RAM byte copy | Following row carries the destination address. |
| `D0` / `D1` | 16-bit equal / not equal | Controls the next nonconditional operation; conditions can chain. |
| `D2` / `D3` | 16-bit strict unsigned less / greater | DuckStation-specific comparison semantics. |
| `D4` | Exact digital controller comparison | Next-operation condition. |
| `D5` / `D6` | Exact controller equal / not-equal block | DuckStation block meaning, not the hardware menu-control meaning. |
| `E0` / `E1` | 8-bit equal / not equal | Next-operation condition. |
| `E2` / `E3` | 8-bit strict unsigned less / greater | DuckStation-specific comparison semantics. |

## 7.4 Hardware-versus-emulator differences

The following differences are conversion-critical:

1. DuckStation implements `D2` and `E2` as strict unsigned `<`. The verified GameShark Pro 3.1 handler appears to use unsigned `<=`.
2. DuckStation uses `D5` and `D6` as separator-terminated controller-state blocks. Physical GameShark references use those rows as device/menu activation controls.
3. DuckStation uses `C0` as a 16-bit equality block. The examined physical-device documentation treats `C0/C1` as broader control/delay types.
4. DuckStation Type `50` and `C2` count zero as bounded no-ops. Verified physical handlers can use decrement loops where zero underflows.
5. DuckStation Type `C2` ignores the carrier row's opcode and value; it uses only the carrier's low 24-bit address.

These operations must carry a source/target implementation tag inside a converter. Prefix matching alone is not sufficient.

## 7.5 DuckStation direct-write, arithmetic, and bit extensions

| Opcode | Operation |
|---|---|
| `31` / `32` | Set / clear selected bits in an 8-bit value. |
| `60` / `61` | Add / subtract a full 32-bit value. |
| `81` / `82` | Set / clear selected bits in a 16-bit value. |
| `90` | 32-bit constant write. |
| `91` / `92` | Set / clear selected bits in a 32-bit value. |
| `A5` | Scratchpad 32-bit write. |

DuckStation also provides one-command 32-bit next-operation conditions:

| Opcode | Condition |
|---|---|
| `A0` | 32-bit equal. |
| `A1` | 32-bit not equal. |
| `A2` | 32-bit strict unsigned less than. |
| `A3` | 32-bit strict unsigned greater than. |

## 7.6 Block conditions and conditional writes

The following execute or skip rows until `00000000 FFFF`:

| Opcode | Condition required to execute the block |
|---|---|
| `A4` | 32-bit memory equals the full value. |
| `C3` / `C4` | 8-bit memory is strictly less / greater. |
| `C5` / `C6` | 16-bit memory is strictly less / greater. |
| `D5` / `D6` | Exact digital controller state equals / does not equal the value. |
| `D7` | Advanced digital/analog button test, optionally with a held-frame counter. |
| `52` | Cheat-register comparison family. |
| `F6` | Multi-condition AND/OR/ELSE IF/ELSE structure. |

Conditional write extensions are:

| Opcode | Operation |
|---|---|
| `A6` | If a 16-bit value matches the compare half, replace it with the replacement half. |
| `A7` | `A6` plus restore-on-disable behavior. |
| `A8` | 8-bit compare/replace plus restore-on-disable behavior. |

## 7.7 Slides and memory copy

### Type 50 classic slide

```text
5000CCAA 0000VVVV
WWAAAAAA 0000IIII
```

| Field | Meaning |
|---|---|
| `CC` | Repeat count. |
| `AA` | Address increment. |
| `VVVV` | Value increment. |
| Following row | Initial operation, address, and value. |

The follower may be `30`, `80`, or DuckStation `90`. DuckStation keeps the changing classic-slide value as 16 bits even when the follower is Type `90`.

### Type 53 improved slide

Type `53` is a DuckStation two-row extension with a 16-bit count, 16-bit address change, 16-bit value change, and separate negative-address and negative-value flags. Its follower may be:

- `30`, `80`, or `90` constant writes;
- `31`, `32`, `81`, `82`, `91`, or `92` bit operations.

### Type C2 memory copy

```text
C2SSSSSS 0000NNNN
XXDDDDDD YYYYYYYY
```

| Field | Meaning |
|---|---|
| `SSSSSS` | Source address. |
| `NNNN` | Byte count. |
| `DDDDDD` | Destination address from the following row. |
| `XX`, `YYYYYYYY` | Ignored by the copy operation. |

## 7.8 Controller masks

DuckStation combines controller input into the conventional PS1 mask:

| Button | Mask | Button | Mask |
|---|---:|---|---:|
| L2 | `0001` | R2 | `0002` |
| L1 | `0004` | R1 | `0008` |
| Triangle | `0010` | Circle | `0020` |
| Cross | `0040` | Square | `0080` |
| Select | `0100` | L3 | `0200` |
| R3 | `0400` | Start | `0800` |
| Up | `1000` | Right | `2000` |
| Down | `4000` | Left | `8000` |

`D4-D6` use exact digital-state comparisons. `D7` supports bit tests, analog-stick direction bits, and held-frame counting.

## 7.9 Cheat-register virtual machine: Types 51 and 52

DuckStation provides 256 internal 32-bit cheat registers.

Type `51` supports register, immediate, memory, pointer, indexed-array, arithmetic, bitwise, and shift operations across 8-, 16-, and 32-bit widths. Major subtype families include:

- `00-07`: 8-bit register/memory/pointer operations;
- `40-47`: 16-bit equivalents;
- `80-86`: 32-bit equivalents;
- `C0-CA`: add, subtract, multiply, divide, modulo, AND, OR, XOR, NOT, left shift, and right shift.

Type `52` is a separator-terminated comparison block with 125 implemented subtypes. It covers register/immediate/memory/pointer comparisons, all three widths, equality and ordered comparisons, bit tests, and range tests.

These are DuckStation-specific virtual-machine instructions. They should be preserved verbatim unless the exact subtype has an explicitly implemented destination mapping.

## 7.10 Range, search, and value-control extensions

| Opcode | Operation |
|---|---|
| `F0` | 8-bit custom range correction. |
| `F1` | Clamp a 16-bit value between a minimum and maximum. |
| `F2` | Wrap a 16-bit value between a minimum and maximum. |
| `F3` | Two-row 16-bit custom range correction. |
| `F4` | Five-row 16-byte pattern find-and-replace with wildcard support. |
| `F5` | Swap a 16-bit value between two supplied values. |

These are not physical GameShark Pro types and normally have no exact Xploder or Caetla representation.

## 7.11 Type F6 multi-condition blocks

Type `F6` supports AND groups, OR groups, ELSE IF branches, and ELSE branches. It can embed `D0-D3`, `E0-E3`, `A0-A3`, and `D7` conditions. Internal operands `E4` and `E5` test whether selected 8-bit bits are set or clear; they are not valid standalone public opcodes.

All `F6` paths use `00000000 FFFF` separators. A converter must preserve the complete block structure and must never emit its body as unconditional writes.

## 7.12 Assembly entries

```ini
[Assembly Patch]
Type = Assembly
Activation = EndFrame
80010000:
addiu v0, zero, 1
jr ra
nop
```

Rules verified in the source:

1. Each section begins with a hexadecimal address followed by `:`; `0x` is optional.
2. Addresses must be 4-byte aligned.
3. Instructions are assembled sequentially at four-byte intervals.
4. Multiple address sections are allowed.
5. `#` and `;` introduce comments.
6. One contiguous assembly operand may contain up to eight hexadecimal `?` option digits.
7. Duplicate instruction addresses are rejected.
8. Patches are reapplied at frame end and can restore the previously observed instruction when disabled.

`Type = Assembly` is a DuckStation-native format. It is not an Action Replay, GameShark, Xploder, or Caetla hardware code type.

## 7.13 Compatibility classification

| Classification | Opcodes / format |
|---|---|
| Broad GameShark/Action Replay family | `10`, `11`, `1F`, `20`, `21`, `30`, `50`, `80`, `C0`, `C1`, `C2`, `D0-D6`, `E0-E3`, with implementation-specific caveats. |
| DuckStation extensions | `31`, `32`, `51`, `52`, `53`, `60`, `61`, `81`, `82`, `90-92`, `A0-A8`, `C3-C6`, `D7`, `F0-F6`. |
| Internal-only F6 operands | `E4`, `E5`. |
| Separate native format | `Type = Assembly`. |

# 8. Safe Conversion Rules

## 8.1 Exact or normally safe mappings

| Source operation | Safe destination mapping |
|---|---|
| 8-bit write | Xploder `3`, AR/GS `30`, Caetla `30`, DuckStation `30`. |
| 16-bit write | Xploder `8`, AR/GS `80`, Caetla `80`, DuckStation `80`. |
| Equal/not-equal 16-bit next-operation condition | Xploder `7/9`, AR/GS `D0/D1`, Caetla `D0/D1`, DuckStation `D0/D1`, subject to the Xploder firmware-layer caveat. |
| AR/GS arithmetic | Caetla 0.34 GameShark mode, or matching Caetla .341 native arithmetic after width/operation translation. |
| AR/GS C2 live RAM copy | Caetla .341 `C2` or DuckStation `C2`, after applying each implementation's zero-count and carrier-row rules. |
| Xploder 16-bit Type B | Caetla native Type B 16-bit seed, if count and increments fit. |
| AR/GS Type 50 with `80` seed | Xploder Type B when count fits `01`-`FF`; DuckStation `50` when hardware comparison semantics are not involved; otherwise expand or preserve. |
| DuckStation `90` 32-bit write | Caetla native `90`, or two adjacent `80` rows for AR/GS/Xploder-compatible output. |

## 8.2 Safe only by expansion

| Source | Expansion approach | Loss / warning |
|---|---|---|
| 32-bit write to AR/GS | Two adjacent `80` halfword writes in PS1 little-endian order. | Not atomic; ordering matters during a frame. |
| AR/GS Type 50 byte repeater to Xploder | Expand to individual Type `3` writes. | Loses compact repeater form. |
| Xploder Type 5 embedded payload | Expand to byte/halfword/word writes. | Loses exact payload packaging, encryption key, and round-trip identity. |
| Caetla bit operations | Expand only when the current value is known and fixed, or map to matching DuckStation `31/32`, `81/82`, or `91/92`. | Usually not safe for dynamic memory on targets without bit operations. |
| DuckStation `53` improved slide | Expand into the exact repeated operations. | Loses signed-slide packaging and can greatly increase code size. |
| DuckStation range/value types `F0-F5` | Expand only when a finite exact sequence can reproduce the operation. | Dynamic clamp, wrap, search, and swap behavior usually cannot be represented safely. |

## 8.3 Operations that are not exact equivalents

| Source | Do not map blindly to | Reason |
|---|---|---|
| GameShark Type `50` | Xploder Type `5` | Repeater versus embedded payload. |
| Xploder Type `5` | GameShark/Caetla `C2` | Embedded data versus live RAM source copy. |
| Xploder Type `6` | Ordinary writes | Contains breakpoint metadata and executable payload semantics. |
| AR/GS `D4`, `D5`, `D6` | Normal RAM conditions | Controller/menu/global controls are firmware-specific. |
| DuckStation `D5/D6` | Hardware GameShark `D5/D6` | DuckStation block conditions versus physical-device control rows. |
| DuckStation `D2/E2` | GameShark Pro 3.1 `D2/E2` without adjustment | Strict `<` versus verified hardware `<=`. |
| DuckStation `51/52/F6` | Ordinary writes or simple one-line conditions | Structured virtual-machine and block semantics would be destroyed. |
| DuckStation `Type = Assembly` | A physical-device code type | Native MIPS source is an emulator feature. |
| AR/GS `C0` or Xploder `F` | Simple one-line condition | They gate a broad remainder of the list. |
| Caetla .341 `C3` | A direct write | It dereferences a live pointer and applies a signed offset. |
| Caetla native `50` | GameShark `50` | Bit-set operation versus serial repeater. |

## 8.4 Context ownership rules for tools

1. Parse mode selectors before parsing Caetla rows.
2. When a Type B/50/C2/C3 header is recognized, consume its carrier row as part of the same operation.
3. When a Xploder Type 5/6 header is recognized, calculate the complete owned span before examining any continuation-row prefix.
4. Reject or preserve zero-count decrement loops rather than treating them as empty operations.
5. Preserve unknown rows with their original text and a warning. Never guess a meaning solely from a familiar first nibble.
6. Keep menu/default-off state separate from the operation and physical address.
7. Preserve the DuckStation file-level `Type`, `Activation`, options, comments, and block separators.
8. Keep physical GameShark comparison/control semantics separate from DuckStation opcodes that reuse the same prefix.

# 9. Worked Examples

## 9.1 Same basic writes across devices

Write byte `7F` to address `80012345`:

```text
Xploder:              30012345 007F
GameShark/AR:         30012345 007F
Caetla native:        30012345 007F
DuckStation:          30012345 0000007F
```

Write halfword `03E8` to address `80012346`:

```text
Xploder:              80012346 03E8
GameShark/AR:         80012346 03E8
Caetla native:        80012346 03E8
DuckStation:          80012346 000003E8
```

The identical text in these two examples does not imply that all prefixes are cross-compatible.

## 9.2 GameShark controller condition

```text
D4000000 FFFB
80012346 0009
```

The first row compares the GameShark-maintained controller state with `FFFB`. It does not read RAM address zero. The second row runs only when the controller condition succeeds.

## 9.3 GameShark repeater versus Xploder repeater

GameShark:

```text
50000302 0001
80010000 0005
```

Xploder equivalent for this 16-bit case:

```text
B0030002 0001
10010000 0005
```

Both produce three halfword writes, but the headers and seed-row conventions are device-specific.

## 9.4 Xploder Type 5 payload ownership

```text
50007800 0008
11223344 5566
778899AA BBCC
80010000 0001
```

The Type 5 header declares eight payload bytes, so it owns two six-byte stored rows. Only the first eight logical payload bytes are installed at the destination; remaining bytes in the final stored row are padding. `80010000 0001` is the first independent code after the Type 5 span.

## 9.5 Caetla mode ambiguity

```text
FFFFFFFF 00000001
10012346 0001
FFFFFFFF 00000000
10012345 0001
```

- First `10` row: GameShark mode, increment 16-bit at `80012346`.
- Second `10` row: .341 native mode, increment 8-bit at `80012345`.

A parser that ignores the selectors assigns the wrong width to one of the rows.

## 9.6 Caetla .341 pointer write

```text
C308C6B8 0003
9100FFFC 12345678
```

Read the pointer at `8008C6B8`, add signed offset `-4`, and write the 32-bit value `12345678`.

## 9.7 DuckStation strict comparison difference

```text
D2001000 00000005
80020000 00000001
```

In DuckStation the write runs only when the unsigned 16-bit value at `80001000` is strictly below `0005`. On the verified GameShark Pro 3.1 handler, the corresponding `D2` path also accepts equality. A cross-target converter must either change the logic or warn about the boundary difference.

## 9.8 DuckStation block condition

```text
C5001000 00000064
80020000 00000001
80020002 00000002
00000000 0000FFFF
```

DuckStation executes both writes while the unsigned 16-bit value at `80001000` is below `0064`. The separator belongs to the block and must not be emitted as a write.

## 9.9 DuckStation Assembly entry

```ini
[Force Return Value]
Type = Assembly
Activation = EndFrame
80010000:
addiu v0, zero, 1
jr ra
nop
```

This is native MIPS source handled by DuckStation. It should round-trip as Assembly text or be assembled through an explicitly selected PS1 MIPS path; it must not be mislabeled as a physical GameShark code.

# 10. Open and Version-Specific Items

The following remain intentionally marked rather than guessed:

1. **Xploder condition polarity across loader/runtime layers.** Canonical public semantics and one older runtime agree, while the XTGER 2.0081 installed handler has inverse active branch sense after a dedicated control-row preprocessing pass.
2. **Xploder Type 4 helper semantics.** The XTGER engine calls a cartridge helper, but the complete public parameter layout is not yet established.
3. **Xploder Types 2, A, C, and ordinary D rows.** No general direct runtime meaning is confirmed across the analyzed firmware.
4. **GameShark C1 execution path in Pro 3.1.** The delay type is manufacturer-documented, but the exact handler path outside the verified primary dispatcher is not fully traced.
5. **GameShark D5/D6 transformation.** Their menu/global purpose is documented, but the exact runtime transformation for every firmware is not fully mapped.
6. **Caetla native C family.** Preserve unknown subtypes until a specific handler is verified.
7. **Caetla Type B conditional seeds.** Native C/D/E seed routing exists, but it is not reducible to a normal write slider without more subtype analysis.
8. **DuckStation version scope.** The opcode descriptions are definitive for the examined source revision. Future emulator revisions can add or alter extensions and must be rechecked before claiming universal DuckStation behavior.
9. **DuckStation complex subtype serialization.** Types `51`, `52`, `F4`, and `F6` are structurally understood, but destination conversion should remain subtype-by-subtype rather than broad guessed translation.

# 11. Evidence Base

This reference consolidates the following evidence:

- Xploder/Xplorer XTGER decompressed firmware, database parser, selected-code builder, installed engine, and active-buffer layout.
- Earlier Xploder active-runtime memory dump and Type 5 payload research.
- Xploder PSX Converter v1.05 source and regression tests, including context-aware Type 5/6 parsing.
- Decrypted GameShark Pro 3.1 firmware and its primary runtime dispatcher.
- Original 1999 Action Replay CDX / GameShark CDX / GameBuster CDX code-type technical manual.
- Confirmed aligned-v1 and compact-v2 Datel database layouts and content-based Datel v2/v3 firmware decryption.
- Caetla 0.34 and 0.341 firmware handler analysis.
- Caetla .341 period notes, corrected where direct binary verification proved a different selector or width.
- DuckStation `src/core/cheats.h` and `src/core/cheats.cpp`, including file parsing, opcode dispatch, block execution, controller translation, Assembly parsing, and disable/restore behavior.
- Xploder PSX Converter v1.05 DuckStation regression tests covering strict comparisons, Type `1F`, Types `50/53/C2`, block ownership, metadata preservation, and complex-structure round trips.

## Reference policy

When firmware behavior, original documentation, and old web lists disagree, this document uses the following priority:

1. Direct handler trace and repeatable test.
2. Original manufacturer/period technical documentation.
3. Multiple consistent real database examples.
4. Secondary code lists or labels.

Conflicts are documented rather than silently resolved by assumption.
