<!--
Xploder PSX Converter README
-->

<div align="center">

# 🔴 Xploder PSX Converter

### Native Windows conversion tools for PlayStation 1 cheat-code formats

![Platform](https://img.shields.io/badge/platform-Windows%207%2B-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Build](https://img.shields.io/badge/build-MSVC-green)
![License](https://img.shields.io/badge/license-GPLv3-red)
![Version](https://img.shields.io/badge/version-v1.05-brightgreen)

<a href="https://github.com/SkillerCMP/PSX-Xploder/releases">
  <img alt="GitHub Downloads (all assets, all releases)" src="https://img.shields.io/github/downloads/SkillerCMP/PSX-Xploder/total?style=social">
</a>
<img alt="GitHub Downloads (latest release)" src="https://img.shields.io/github/downloads/SkillerCMP/PSX-Xploder/latest/total?style=social">

</div>

---

## 📌 Overview

**Xploder PSX Converter** is a native Windows utility for PlayStation 1 cheat-code conversion and preservation.

It supports window-to-window conversion between:

```text
GameShark / Action Replay
Xploder Encrypted
Xploder RAW
DuckStation
Caetla
PS1 MIPS
```

The project includes Xploder encryption/decryption, structured Type 5 and Type 6 handling, DuckStation patch metadata, CMP database formatting, folder batch cleanup, wildcard preservation, activator conversion, and code-type condensation.

---

<details open>
<summary><strong>🦈 v1.05 — GameShark Pro 3.1, Datel ROM Import, and DuckStation Source Update</strong></summary>

<br>

Version 1.05 validates the classic GameShark / Action Replay parser against the decrypted GameShark Pro 3.1 runtime handler.

- `D2` and `E2` are now modeled correctly as **less than or equal**, not strict less-than.
- `D3` and `E3` remain strict **greater than** comparisons.
- `D4xxxxxx` is recognized as a GameShark controller-state comparison; the encoded address field is ignored and output is canonicalized to `D4000000`.
- `D5` and `D6` are retained as GameShark menu/control rows instead of being mislabeled as ordinary “codes on/off” operations.
- Type `50` repeaters now reject a zero count because the verified firmware loop would underflow.
- Type `C2` byte-copy blocks accept the firmware's actual destination-carrier behavior and canonicalize the second row to `80...... 0000`.
- Generated C2 blocks are limited to safe lengths from `0001` through `FFFF`.
- Datel v2.x and v3.x encrypted ROM images are now recognized and decrypted from their file contents before database extraction. The filename and `.enc` extension are not required.
- Successful Datel imports are accepted only when the decrypted image contains a complete, structurally valid AR/GS database, reducing false-positive binary detection.
- DuckStation input/output is now aligned with the current DuckStation source interpreter instead of assuming every familiar GameShark prefix has identical behavior.
- DuckStation `D2/E2` are modeled as strict unsigned **less than**, while physical GameShark Pro 3.1 `D2/E2` remain **less than or equal** in the GameShark parser.
- DuckStation Type `1F` is handled as a **16-bit scratchpad write**.
- Classic Type `50` accepts DuckStation Type `90` 32-bit followers, and Type `53` preserves `31/32`, `81/82`, and `91/92` bit-set/bit-clear followers.
- DuckStation zero-count Type `50` and `C2` rows are retained as safe no-ops; the physical GameShark parser still warns on the verified underflowing zero-count forms.
- DuckStation `C2` accepts any following row as the destination-address carrier and canonicalizes it safely on output.
- DuckStation `D5/D6`, `A4`, and `C3-C6` are recognized as bounded block conditions rather than GameShark menu controls or ordinary one-row comparisons.
- `Type = Assembly`, `Activation = Manual/EndFrame`, `Option`, `OptionRange`, `DisallowForAchievements`, and `Ignore` metadata are preserved during DuckStation round trips.
- Complex DuckStation-only structures such as `51`, `52`, `D7`, `F0-F6`, and Assembly bodies are preserved verbatim when no exact cross-device translation exists.

</details>

---

<details open>
<summary><strong>✨ Main Features</strong></summary>

<br>

- Independent **Input Type** and **Output Type** selectors.
- Window-to-window conversion through a shared semantic code model.
- Xploder encryption and decryption using supported key styles.
- Canonical Xploder Type 5 and Type 6 RAW output.
- GameShark, Action Replay, DuckStation, Caetla, and Xploder code-type translation.
- DuckStation patch-section generation and reverse conversion.
- PS1 MIPS I assembly, disassembly, and exact data-directive preservation.
- Nested CMP group and subgroup support.
- GameShark Type 5 serial-repeater condensation and expansion.
- Optional Xploder Type B slider generation from GameShark Type 5 or compatible write runs.
- DuckStation and Caetla `80 + 80 -> 90` write combining.
- X-Link output formatting for quoted names and `$`-prefixed Xploder code rows.
- DuckStation `D0 / 70 -> C0` activator-block condensation.
- Reverse expansion of DuckStation `C0` blocks to GameShark `D0` or Xploder Type `7`.
- CMP-compatible names, credits, and `$` code-line formatting.
- Legacy database import from encrypted/decrypted Xploder ROMs, full Action Replay/GameShark ROMs, Datel v2.x/v3.x encrypted firmware images, and standalone AR/GS code-database files.
- Automatic content- and structure-based import detection regardless of whether a Datel image is named `.ENC`, `.BIN`, `.ROM`, `.DAT`, or has no extension.
- Folder drag-and-drop batch cleanup and Xploder decryption.
- UTF-8, UTF-16, ANSI, LF, CRLF, and browser/chat line-ending support.
- Persistent program settings.
- Resizable Input and Output panes.
- Native Windows desktop interface.

</details>

---

<details open>
<summary><strong>🔄 Window-to-Window Conversion</strong></summary>

<br>

The Input and Output panes use separate format selectors:

```text
Input Type  -> parser used for the Input pane
Output Type -> emitter used for the Output pane
```

Available formats:

```text
GameShark / Action Replay
Xploder Encrypted
Xploder RAW
DuckStation
Caetla
PS1 MIPS
```

The converter uses semantic operations instead of changing only the first hexadecimal digit.

Supported operations include:

- 8-bit, 16-bit, and 32-bit writes
- equal, not-equal, less-than-or-equal, and greater-than comparisons
- increment and decrement operations
- copy-memory blocks
- GameShark serial repeaters
- Xploder Type B serial sliders
- DuckStation conditional writes
- DuckStation block activators
- Xploder Type 5 mass-write expansion
- Xploder Type 6 structured payloads
- Caetla 0.34 native writes plus optional Caetla `.341` arithmetic, C2 block copy, and C3 indirect-write output
- wildcard-preserving structural conversions
- Action Replay/GameShark `C0` global equal/on conditions are preserved as their own semantic operation.
- Scratchpad rows retain their source-family width: DuckStation Type `1F` is 16-bit, while verified 8-bit scratchpad forms remain 8-bit where the source format defines them that way.

Examples:

```text
GameShark / Action Replay       Xploder RAW
D006BD92 FBFF              ->   7006BD92 FBFF
8002BEBA 2400                   8002BEBA 2400
```

```text
DuckStation                     GameShark / Action Replay
90008000 3C148006          ->   80008000 8006
                                 80008002 3C14
```

```text
Caetla                          DuckStation
FFFFFFFF 00000001          ->   10012345 0005
10012345 0005                   20012348 0001
20012348 0001                   9001234C 12345678
FFFFFFFF 00000000
9001234C 12345678
```

When no exact destination equivalent exists, the converter preserves the original code with a clear warning instead of silently guessing or deleting it.

</details>

---

<details open>
<summary><strong>🧠 PS1 MIPS Input and Output</strong></summary>

<br>

The Input Type and Output Type lists include:

```text
PS1 MIPS
```

When **PS1 MIPS** is selected as the Input Type, assembly is converted into the selected cheat-code family.

Example input:

```asm
0x800D0A3C : Hook
j     0x8000FF1C
beq   $s2, $zero, 0x8000FF28
nop
sw    $v1, 0x0020($s3)
j     0x800D0A44
nop
```

The special `: Hook` form writes the first `j`/`jal` at the declared hook address, then continues assembling at that numeric jump target. For explicit control, `.org` can be used for every region. Address `0x00000000` is a valid `.org` value and is preserved when assembling code or data.

DuckStation output:

```text
900D0A3C 08003FC7
9000FF1C 12400002
9000FF20 00000000
9000FF24 AE630020
9000FF28 08034291
9000FF2C 00000000
```

GameShark and Xploder output use paired 16-bit writes where required. Caetla and DuckStation use native Type `90` 32-bit writes.

When **PS1 MIPS** is selected as the Output Type, the converter reconstructs 32-bit opcodes from:

- DuckStation Type `90` writes
- consecutive GameShark/Caetla/Xploder Type `80` writes at `A` and `A+2`
- canonical Xploder Type 5 payload bytes
- Xploder Encrypted Type 5 blocks after decryption
- structured Xploder Type 6 executable payloads after decryption

For Type 6, the breakpoint descriptor and mask remain metadata while the executable bytes are reconstructed from the inline payload and continuation rows. The Xploder engine installs the rounded executable at `0x80000040`, so MIPS output emits that region with `.org 0x80000040`. A preceding Type `7` or Type `9` installation guard is retained as a comment rather than misinterpreted as an instruction.

Example Type 6 MIPS output begins as:

```asm
# Xploder Type 9 installation guard: execute the following Type 6 block when [0x80000040] != 0x27BD
# Xploder Type 6 breakpoint address: 0x801FAB92 | control: 0xEA80
# Xploder Type 6 executable installed at 0x80000040 (64 bytes)

.org 0x80000040

addiu $sp, $sp, -12
sw    $a0, 0x0004($sp)
```

Output is organized into `.org` regions and includes calculated branch and jump targets.

For Xploder RAW or Xploder Encrypted output, the optional menu item:

```text
Options > Current Output > Auto CodeType Conversion > Pack PS1 MIPS -> Type 5
```

packs each contiguous MIPS region into a canonical Xploder Type 5 payload before optional encryption.

The PS1 MIPS converter supports common MIPS I integer, branch, jump, load/store, and basic COP0 instructions. Unknown, reserved, or noncanonical encodings are shown as exact `.word 0xXXXXXXXX` values so they cannot be silently normalized into different opcodes. Exact data directives are also supported:

```asm
.word 0x00FFFF00
.half 0x1234
.byte 0x01
```

`.half` accepts one or more comma-separated 16-bit values and stores them in PS1 little-endian byte order. Type 5 payload tails now use `.half` when two bytes remain and `.byte` when one byte remains. These directives are accepted again as MIPS input and can be packed back into an exact-length Xploder Type 5 payload. Full GTE/COP2 mnemonic support and multi-instruction pseudo-operations are not included yet.

</details>

---

<details>
<summary><strong>🧾 CMP Database Compatible Output</strong></summary>

<br>

The **CMP DB Compatible Output** option controls all CMP presentation rules together.

When enabled:

```text
+Code Name
%Credits: Author Name
$80012345 0063
```

When disabled:

```text
Code Name, by Author Name
80012345 0063
```

The option controls:

- `+` before code names
- `%Credits:` author lines
- `$` before hexadecimal code rows

It does not change code behavior.

DuckStation output still uses its required section metadata:

```text
[Infinite Health]
Type = Gameshark
Activation = EndFrame
Description = Keeps health full during battle.
Author = Code Master
80012345 0063
```

For DuckStation output, CMP compatibility controls only the leading `$` on code rows.

</details>

---

<details>
<summary><strong>🦆 DuckStation Patch Format</strong></summary>

<br>

CMP-style entries are converted into DuckStation patch sections.

Input:

```text
+Moon Jump {Allows jumping while airborne.} , Crypt: Pro Action Replay/GameShark
%Credits: Code Master
80012340 0001
```

DuckStation output:

```text
[Moon Jump]
Type = Gameshark
Activation = EndFrame
Description = Allows jumping while airborne.
Author = Code Master
80012340 0001
```

Metadata rules:

- `+Code Name` becomes `[Code Name]`.
- A trailing `Crypt:` section is removed.
- Text inside `{...}` becomes `Description`.
- `%Credits:` becomes `Author`.
- `%Credits:` takes priority over an inline `by Name`.
- Generated sections default to `Type = Gameshark` and `Activation = EndFrame`.
- Existing DuckStation sections preserve `Type = Gameshark` or `Type = Assembly`.
- Existing DuckStation sections preserve `Activation = Manual` or `Activation = EndFrame`.
- `Option =`, `OptionRange =`, `DisallowForAchievements =`, and `Ignore =` properties are retained.
- Assembly bodies, labels, instructions, wildcards, and comments are preserved during DuckStation-to-DuckStation normalization.
- Code-only input uses `[Unnamed Cheat]`.

DuckStation-to-DuckStation conversion also rebuilds nonstandard CMP-style headings into canonical DuckStation sections.

DuckStation-specific rows that have no exact physical-device equivalent are kept as DuckStation rows for same-format output. When converting elsewhere, the converter emits a warning and preserves the original rows as comments instead of allowing a conditional body to become unconditional.

</details>

---

<details>
<summary><strong>📂 DuckStation Groups and Subgroups</strong></summary>

<br>

CMP group markers are converted into DuckStation backslash-delimited section paths.

```text
!Group Name:   opens one group level
!!             closes the most recently opened level
```

Example:

```text
!Player Info:
!Player Codes:
+Infinite Health
80012340 0063
!!
!Movement:
+Moon Jump
80012344 0001
!!
!!
```

DuckStation output:

```text
[Player Info\Player Codes\Infinite Health]
Type = Gameshark
Activation = EndFrame
80012340 0063

[Player Info\Movement\Moon Jump]
Type = Gameshark
Activation = EndFrame
80012344 0001
```

When DuckStation is converted to another output family, section paths are rebuilt into CMP-style group stacks.

Input:

```text
[Unlimited Nitrous\P1]
[Unlimited Nitrous\P2]
[Unlimited Nitrous\P3]
```

Output:

```text
!Unlimited Nitrous:
+P1
...
+P2
...
+P3
...
!!
```

Consecutive patches sharing the same path stay inside one open group.

</details>

---

<details>
<summary><strong>⚙️ Automatic Code-Type Conversion</strong></summary>

<br>

### DuckStation and Caetla: combine adjacent Type 80 writes

When enabled:

```text
80008000 8006
80008002 3C14
```

becomes:

```text
90008000 3C148006
```

The lower-address halfword becomes the low 16 bits of the 32-bit value.

Wildcard halves are supported:

```text
8000C03C ????
8000C03E 240B
```

becomes:

```text
9000C03C 240B????
```


### Caetla 0.34 native output

Caetla output now follows the handlers mapped directly from `CAETLA.BIN` rather than treating every familiar prefix as an ordinary GameShark row.

Native output includes:

```text
30                  8-bit write
80                  16-bit write
90                  32-bit write
B                   native serial slider
50 / 51 / 52        set bits (8 / 16 / 32-bit)
58 / 59 / 5A        clear bits (8 / 16 / 32-bit)
70 / 71 / 72        copy byte / halfword / word
D0-D3 / E0-E3       16-bit / 8-bit conditions
1F800xxx             scratchpad 8-bit write
```

The **Condense Writes / GS Type 5 -> Type B Slider** option emits native Caetla Type B rows. It supports byte, halfword, and word seeds, up to `0xFFF` writes, a `0xFFFF` address step, and a 32-bit value step.

Caetla 0.34 arithmetic rows use the firmware's persistent GameShark-compatible interpreter. The converter now adds the required mode rows automatically:

```text
FFFFFFFF 00000001   ; GameShark-compatible mode
10012345 0001       ; increment 16-bit
FFFFFFFF 00000000   ; return to native Caetla mode
```

Unsupported assumptions from the earlier output path are no longer generated as if they were confirmed Caetla 0.34 types. In particular, 32-bit increment/decrement, `D4-D6` control rows, and `.341`-only `C2/C3` output are not generated while 0.34-safe output is selected.

### Caetla .341 extended output

Under **Options > Current Output**, Caetla output now has a separate:

```text
Caetla .341 Extended Types
```

The option is off by default so existing Caetla 0.34 output remains unchanged. Input parsing recognizes `.341` rows automatically.

When enabled, arithmetic uses native `.341` rows without a GameShark-mode selector:

```text
10  8-bit increment
11  16-bit increment
12  32-bit increment
20  8-bit decrement
21  16-bit decrement
22  32-bit decrement
```

The option also enables the two-line `.341` formats:

```text
C2XXXXXX ZZZZ        Copy 0001-FFFF bytes from XXXXXX
80YYYYYY 0000        Destination YYYYYY

C3XXXXXX 000Z        Load a pointer from XXXXXX
9100YYYY DDDDDDDD    Add signed offset YYYY and store DDDDDDDD

C3 width selectors: 0000 = 8-bit, 0001 = 16-bit, 0003 = 32-bit
```

For example:

```text
C308C6B8 0001
91000022 000003E8
```

is preserved as one Caetla indirect-write operation and round-trips without the `91` row being mistaken for an ordinary Type `9` write. C3 pair recognition also remains active after a persistent GameShark-mode selector. The verified binary uses selector `0003` for 32-bit C3 writes; `0002` is not emitted as a valid width. Zero-count C2 blocks are preserved when encountered but are not generated. 

### DuckStation: condense equal activators

Repeated GameShark `D0` or Xploder Type `7` activators can be condensed into one bounded DuckStation `C0` block.

Input:

```text
D00D0A3A 0062
800D0A3C 3FC7
D00D0A3A 0062
800D0A3E 0800
```

DuckStation output:

```text
C00D0A3A 0062
800D0A3C 3FC7
800D0A3E 0800
00000000 FFFF
```

Reverse conversion expands the block safely:

```text
GameShark / Action Replay -> repeated D0
Xploder RAW / Encrypted   -> repeated Type 7
```

DuckStation `D1` not-equal activators continue to convert to Xploder Type `9`.

### GameShark Type 5 serial repeater

Compatible Type `30` or Type `80` write sequences can be condensed.

Input:

```text
300D0A3A 0062
300D0A3B 0062
300D0A3C 0062
300D0A3D 0062
300D0A3E 0062
300D0A3F 0062
300D0A40 0062
300D0A41 0062
```

Condensed output:

```text
50000801 0000
300D0A3A 0062
```

The repeater header stores:

```text
repeat count
address step
value step
```

At least three compatible writes are required.

By default, a GameShark, DuckStation, or Caetla GameShark-mode Type 5 repeater converted to Xploder is expanded into individual Type `3` or Type `8` writes because GameShark Type 5 and Xploder Type 5 are different formats.

For **Xploder RAW** or **Xploder Encrypted** output, enable:

```text
Options > Current Output > Auto CodeType Conversion
  Condense Writes / GS Type 5 -> Type B Slider
```

This maps compatible 16-bit repeaters to the native Xploder Type B slider:

```text
GameShark:
50000302 0001
80010000 0005

Xploder RAW:
B0030002 0001
10010000 0005
```

Type B stores an 8-bit repeat count, a signed 16-bit address step, and a signed 16-bit value step. The generated base row uses Type `1`, which supplies the starting address/value to the Type B handler without becoming a second normal write. Only aligned 16-bit runs are condensed; byte writes, odd addresses/steps, wildcard values, and fields outside the Type B limits remain expanded.

</details>

---

<details>
<summary><strong>🔑 Xploder Encryption Keys</strong></summary>

<br>

| Key | Style | Notes |
|---:|---|---|
| 4 | `WHBX` | Rolling XOR-style behavior |
| 5 | `WB123` | Common Xploder / XplorerPro style |
| 6 | `AB + XOR` | Behavior-based label |
| 7 | `FCD!` | Rolling ADD-style behavior |

Xploder Type 5 payload encryption supports payload keys:

```text
Key 6
Key 7
```

Encrypted output uses `8 + 4` spacing by default:

```text
$65A58ECF CED9
```

Optional grouped output:

```text
$65A5 8ECF CED9
```

Compact 12-character input remains accepted.

</details>

---

<details>
<summary><strong>🧠 Xploder Type 5</strong></summary>

<br>

Xploder Type 5 is a mass-write payload format and is not the same as the GameShark Type 5 serial repeater.

A canonical RAW Type 5 block:

```text
$501E4FDC 0018
$2F667574 7572
$652F636F 6E73
$6F6C652F 6465
$7369676E 2F00
```

`0018` means exactly 24 payload bytes, or four six-byte rows.

After payload decryption, each row is converted from loader byte order to conventional RAW code order:

```text
AC080004 0008  ->  040008AC 0800
AABBCCDD EEFF  ->  DDCCBBAA FFEE
```

The same self-inverse transformation is applied before encryption.

The converter keeps loader-only expanded sizes internal and exports the canonical external RAW size.

The decryptor is **Type 5 block-aware**. After a valid Type 5 header is read, the declared payload length owns the required following rows. A payload row is never sent through normal top-level code-type detection merely because its first nibble resembles an encrypted write, conditional, Type 5, or Type 6 header. The full final six-byte row is preserved for exact round trips; only bytes beyond the declared payload length are treated as logical padding when extracting the payload.

</details>

---

<details>
<summary><strong>🧠 Xploder Type 6</strong></summary>

<br>

Type 6 is a breakpoint/bootstrap structure.

The size field is the direct logical payload-byte count. The first two
payload bytes share the row containing the breakpoint mask:

```text
logicalPayloadBytes = sizeField
sourceRows = ceil((0x0A + logicalPayloadBytes) / 6)
runtimeCopiedBytes = align_up(logicalPayloadBytes, 4)
```

Examples:

```text
000C -> 12 logical payload bytes, 4 source rows, 12 runtime bytes
003F -> 63 logical payload bytes, 13 source rows, 64 runtime bytes
0000 ->  0 logical payload bytes, 2 fixed descriptor rows
```

The original external size field is preserved in RAW output. The runtime
engine copies Type 6 payloads as 32-bit words, so bytes beyond the logical
length may still complete the final rounded word. Stored six-byte rows are
therefore always preserved exactly.

A Type 5 header following Type 6 is parsed as the next structured block, not as Type 6 payload.

Context-aware annotations identify:

- breakpoint descriptor
- breakpoint type
- breakpoint mask
- payload-byte ranges
- final-row padding
- embedded records inside Type 6 data

Rows inside Type 6 are not reclassified only because their first digit resembles another code type.

Keyless Type 6 blocks also support the two structures verified in the German
Xploder ROM database:

- the breakpoint descriptor may be stored as one ordinary encrypted Xploder line;
- an escaped payload row such as `0E569755 5D5A` decodes to
  `08004003 0C00`, retaining the literal `08` payload byte.

The inverse rules are applied during encryption, allowing the original block
to round-trip exactly when the matching normal encryption key is selected.

The decryptor now uses one shared row-context model for the complete Type 6 block:

```text
header
breakpoint descriptor
breakpoint mask + first inline payload bytes
remaining length-controlled payload rows
```

This means payload rows beginning with values such as `9`, `E`, `5`, `6`, or `B` remain Type 6 data. The declared block length determines ownership and the exact next top-level code boundary. Stored rows always remain six bytes so decrypt/encrypt round trips retain final-row padding exactly.

</details>

---

<details>
<summary><strong>📥 Legacy Cartridge Database Import</strong></summary>

<br>

The **File** menu includes:

```text
File
  Import
    Xploder ROM
    AR/GS / Datel ROM / Code Database
```

**Xploder ROM** accepts encrypted `.FCD` images and already-decrypted `.ROM` images. The importer decrypts the ROM when required, locates its internal game/code database, and loads the extracted encrypted Xploder code list into the Input pane. The Input Type is switched automatically to **Xploder Encrypted**.

**AR/GS / Datel ROM / Code Database** accepts full cartridge ROM dumps, standalone code-list files, and Datel v2.x/v3.x encrypted firmware images. The importer examines the file contents and scans for a structurally valid database rather than depending on the filename, extension, or one hard-coded offset.

For an encrypted Datel image, the importer first checks the original bytes, then tries the appropriate v2.x/v3.x in-memory decryption paths. A decrypted result is accepted only when its database records, names, counts, byte order, and boundaries validate successfully. A v3 content marker is used only to choose the first decryption attempt; it is never treated as sufficient proof by itself.

The importer extracts game names, code names, master/auto-activation entries where present, and stored code rows, then switches the Input Type to **GameShark / Action Replay**. Control-only legacy names are accepted safely instead of causing the complete database to be rejected.

Imported text uses the converter's normal structured format:

```text
^3 = NAME: Game Name
+Code Name
$80012345 0063
```

The Input pane auto-detects supported binary formats during drag-and-drop. Extensions are only conveniences in the file-selection dialog; they do not control recognition. For example, the same Datel image can be imported as any of these names:

Detection validates the decrypted database structure, including record counts, names, alignment, line lengths, byte order, boundaries, and the padding area following the database. If the file is not a recognized binary database, normal text-file loading is attempted instead. The status line reports whether Datel v2.x/v3.x decryption was used, the detected layout, number of games, code entries, code rows, and source filename.

</details>

---

<details>
<summary><strong>📁 Folder Drag-and-Drop Batch Processing</strong></summary>

<br>

Drop a folder onto the Input pane to recursively process every `.txt` file.

The batch process:

- creates a `Decrypted` folder
- preserves filenames
- mirrors subfolders
- skips existing `Decrypted` folders
- normalizes supported code-line layouts
- decrypts supported Xploder codes
- preserves DuckStation 8+8 rows
- preserves wildcard templates
- cleans names, credits, and metadata
- displays current file, completed count, total count, and percentage
- temporarily disables conflicting controls

Folder processing uses the dedicated Xploder database cleanup/decrypt path.

It does **not** use the Input Type or Output Type selectors.

The **CMP DB Compatible Output** option is still honored.

Recognized layouts include:

```text
XXXXXXXX XXXX
XXXXXXXX XX
XXXXXXXX XXXXXXXX
XXXX-XXXX-XXXX
XXXX XXXX XXXX
compact 10, 12, or 16-character rows
```

DuckStation 8+8 rows are preserved:

```text
9006D9D8 EF6FF7C8
A701E7F0 00882000
D7200000 00000004
```

Wildcard values are supported:

```text
9000C03C 240B????
9000C020 240A00??
9000C03C ????????
```

</details>

---

<details>
<summary><strong>⌨️ Clipboard, Shortcuts, and Editors</strong></summary>

<br>

Supported shortcuts:

```text
Ctrl+A = Select all
Ctrl+C = Copy
Ctrl+V = Paste into Input
```

Input paste normalizes:

- Windows `CRLF`
- Unix `LF`
- old-style `CR`
- browser/chat separators
- Unicode line separators
- Unicode paragraph separators

The custom paste path prevents duplicate insertion and keeps pasted multiline text on separate lines.

The Output pane remains read-only.

The divider between Input and Output can be dragged. The divider position previews while dragging, then both panes resize cleanly when released.

</details>

---

<details>
<summary><strong>🔗 X-Link Format for Xploder Output</strong></summary>

<br>

Xploder RAW and Xploder Encrypted output now provide:

```text
Options > Current Output > X-Link Format
```

When enabled, one matching pair of double quotes is removed from each code name and every hexadecimal code row receives exactly one `$` prefix.

Input:

```text
"Infinite Health"
80123456 CDEF
80123458 89AB
```

X-Link output:

```text
Infinite Health
$80123456 CDEF
$80123458 89AB
```

A name is changed only when both its first and last characters are double quotes. Interior quotation marks are preserved. Existing `$` prefixes are not duplicated. Comments, credits, group directives, and other metadata lines remain unchanged.

X-Link Format is an alternative presentation style to CMP DB Compatible Output for Xploder output. While X-Link Format is enabled, code names remain plain rather than receiving the CMP `+` prefix. Code conversion and encryption are unchanged.

</details>

---

<details>
<summary><strong>⚙️ Persistent Options Menu</strong></summary>

<br>

The first menu-bar item is:

```text
Options
```

### Program Options

```text
Auto Convert
Annotate Code Types (Xploder)
CMP DB Compatible Output
```

### Hide Buttons

```text
Hide Convert
Hide Copy Output
Hide Output -> Input
Hide Clear
```

### Current Output

The submenu changes based on the selected Output Type.

#### Xploder Encrypted

```text
X-Link Format

Group Encrypted 4-4-4

Encryption Key
  Key 4 / WHBX
  Key 5 / WB123
  Key 6 / AB+XOR
  Key 7 / FCD!

Type 5 Payload Key
  Key 6
  Key 7

Auto CodeType Conversion
  Condense Writes / GS Type 5 -> Type B Slider
  Pack PS1 MIPS -> Type 5
```

#### GameShark / Action Replay

```text
Auto CodeType Conversion
  Condense Writes -> Type 5
```

#### DuckStation

```text
Auto CodeType Conversion
  80 + 80 -> 90
  D0 / 70 -> C0 Block
  Condense Writes -> Type 5
```

#### Caetla

```text
Auto CodeType Conversion
  80 + 80 -> 90
  Condense Writes / GS Type 5 -> Type B Slider
```

#### Xploder RAW

```text
X-Link Format

Auto CodeType Conversion
  Condense Writes / GS Type 5 -> Type B Slider
  Pack PS1 MIPS -> Type 5
```

Settings are saved beside the executable:

```text
XploderConverter.ini
```

Persisted values include:

- Input Type
- Output Type
- Auto Convert
- annotation state
- X-Link Format state
- CMP output state
- encryption key
- Type 5 payload key
- automatic code-type conversion options
- hidden-button selections

When no settings file exists, built-in defaults are used.

### In-place selection

Checkable and radio-style menu items update without closing the active menu.

The complete menu path remains open and stationary:

```text
Options > Current Output > Auto CodeType Conversion
```

Checkmarks refresh immediately on the same click.

Hide Buttons selections are applied after the menu is dismissed so layout changes do not interfere with native menu tracking.

</details>

---

<details>
<summary><strong>🧱 Modular Code-Type Files</strong></summary>

<br>

Each code family has its own parser/emitter module:

```text
src/CodeTypeCommon.hpp
src/GameSharkActionReplayCodeTypes.hpp
src/XploderCodeTypes.hpp
src/DuckStationCodeTypes.hpp
src/CaetlaCodeTypes.hpp
src/MultiFormatCodeConverter.hpp
src/Ps1MipsCodeTypes.hpp
```

This keeps device-specific meanings separate.

It is especially important for prefixes such as:

```text
10
11
20
21
```

because their meanings differ between Caetla and GameShark / Action Replay.

</details>

---

<details>
<summary><strong>🛠️ Building</strong></summary>

<br>

The project is intended to build with **Microsoft Visual C++ / MSVC** and now produces one executable path compatible with **Windows 7 and newer Windows versions**.

The source no longer uses `std::filesystem`. File and folder operations use Windows 7-compatible Win32 APIs, the C/C++ runtime is linked statically, and the linker subsystem is set to Windows 7 (`6.01`). The build also checks the finished executable for known Windows 8+ imports such as `CreateFile2`.

Use **Visual Studio 2026 Build Tools** with the **Desktop development with C++** workload. The full Visual Studio IDE is not required.

Run:

```bat
build.cmd
```

Choose:

```text
1. Win32 / x86
2. Win64 / x64
```

Or pass the target directly:

```bat
build.cmd win32
build.cmd win64
```

The script locates a Visual Studio 2026 C++ toolchain automatically. The generated filenames remain unchanged:

```text
XploderConverterGui-Win32.exe
XploderConverterGui-Win64.exe
```

A GitHub Actions workflow is also included at `.github/workflows/build-windows.yml`. It can build both executables on GitHub without installing Visual Studio locally.

</details>

<details>
<summary><strong>▶️ Basic Usage</strong></summary>

<br>

1. Open `XploderConverterGui.exe`.
2. Select the Input Type and Output Type for text conversion.
3. Paste code text, drop a supported file onto Input, or use `File > Import`.
4. For cartridge databases, choose **Xploder ROM** or **AR/GS v1.XX**; the Input Type is selected automatically.
5. Configure output-specific options under `Options > Current Output`.
6. Convert the code.
7. Copy the result from Output.

When **Auto Convert** is enabled, the output updates automatically after supported input changes.

</details>

---

<details>
<summary><strong>⚠️ Format Notes</strong></summary>

<br>

Matching hexadecimal prefixes do not always have matching meanings across devices.

For example:

```text
GameShark Type 5 serial repeater
≠
Xploder Type 5 mass-write payload
```

Caetla 0.34 has separate native and GameShark-compatible interpreters. Caetla `.341` additionally defines native arithmetic plus C2/C3 extended rows; the dedicated output option controls whether those newer rows may be generated. The rows `FFFFFFFF 00000001` and `FFFFFFFF 00000002` select GameShark-compatible mode; `FFFFFFFF 00000000` and `FFFFFFFF 00000003` return to native mode. The selection persists until another mode row is encountered.

Some prefixes therefore have different meanings depending on the active interpreter. For example, native `50` sets bits, while GameShark-mode `50` is a serial repeater header. The converter keeps the selected Input Type explicit and tracks Caetla interpreter mode while parsing and emitting.

</details>

---
<details>
<summary><strong>🙏 Credits and Thanks</strong></summary>

<br>

### misfire

GitHub: [mlafeldt](https://github.com/mlafeldt)

Thank you for the original `xpcrypt` work and for your long-standing contributions to the PlayStation cheat-code and hacking community.

### Parasyte

Thank you for your contributions to the PlayStation hacking and cheat-code community.

### Connor McLaughlin (stenzek)

GitHub: [stenzek/duckstation](https://github.com/stenzek/duckstation)

Thank you for the continued development of DuckStation and for making its source code available as a valuable technical reference for PlayStation emulation and cheat-code research.

### szalay

Thus...Thank you for the extensive testing, feedback, and help refining the program into a more reliable and practical tool.

### Lee4

Thank you for reminding me about the little details and features I had forgotten from the old PlayStation 1 days. 😄

### GameHacking.org

Thank you for the many years of contributions, documentation, code archives, and technical information provided to the game-hacking community.

### Scene and Preservation Community

This project builds upon the work of the many researchers, developers, testers, and preservationists who have documented, tested, preserved, and shared knowledge about PlayStation cheat devices, firmware, encryption methods, and code formats.

</details>

---

<details>
<summary><strong>🧾 Disclaimer</strong></summary>

<br>

This tool is intended for preservation, research, and personal code-conversion work.

No commercial cheat database, game content, BIOS, ROM, or copyrighted game material is included.

</details>

---

<details open>
<summary><strong>📜 License</strong></summary>

<br>

This project is licensed under the **GNU General Public License v3.0**.

See the repository `LICENSE` file for the complete license text.

</details>

---

<div align="center">

**Xploder PSX Converter v1.05**

</div>
