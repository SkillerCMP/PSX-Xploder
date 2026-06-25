<!--
Xploder PSX Converter README
-->

<div align="center">

# 🔴 Xploder PSX Converter

### Native Windows conversion tools for PlayStation 1 cheat-code formats

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Build](https://img.shields.io/badge/build-MSVC-green)
![License](https://img.shields.io/badge/license-GPLv3-red)
![Version](https://img.shields.io/badge/version-v1.04-brightgreen)

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
- DuckStation `80 + 80 -> 90` write combining.
- DuckStation `D0 / 70 -> C0` activator-block condensation.
- Reverse expansion of DuckStation `C0` blocks to GameShark `D0` or Xploder Type `7`.
- CMP-compatible names, credits, and `$` code-line formatting.
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
- equal and not-equal comparisons
- increment and decrement operations
- copy-memory blocks
- GameShark serial repeaters
- DuckStation conditional writes
- DuckStation block activators
- Xploder Type 5 mass-write expansion
- Xploder Type 6 structured payloads
- Caetla-native arithmetic types
- wildcard-preserving structural conversions

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
10012345 0005              ->   20012345 0005
12012348 00001000               60012348 00001000
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

The special `: Hook` form writes the first `j`/`jal` at the declared hook address, then continues assembling at that numeric jump target. For explicit control, `.org` can be used for every region.

DuckStation output:

```text
900D0A3C 08003FC7
9000FF1C 12400002
9000FF20 00000000
9000FF24 AE630020
9000FF28 08034291
9000FF2C 00000000
```

GameShark, Caetla, and Xploder output use paired 16-bit writes where required.

When **PS1 MIPS** is selected as the Output Type, the converter reconstructs 32-bit opcodes from:

- DuckStation Type `90` writes
- consecutive GameShark/Caetla/Xploder Type `80` writes at `A` and `A+2`
- canonical Xploder Type 5 payload bytes
- Xploder Encrypted Type 5 blocks after decryption

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
- `Type = Gameshark` is always emitted.
- `Activation = EndFrame` is always emitted.
- Code-only input uses `[Unnamed Cheat]`.

DuckStation-to-DuckStation conversion also rebuilds nonstandard CMP-style headings into canonical DuckStation sections.

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

### DuckStation: combine adjacent Type 80 writes

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

When a GameShark, DuckStation, or Caetla Type 5 repeater is converted to Xploder, it is expanded into individual Type `3` or Type `8` writes first because GameShark Type 5 and Xploder Type 5 are different formats.

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

</details>

---

<details>
<summary><strong>🧠 Xploder Type 6</strong></summary>

<br>

Type 6 is a breakpoint/bootstrap structure.

The size field counts payload bytes after the first two payload bytes stored beside the breakpoint mask:

```text
totalPayloadBytes = sizeField + 2
sourceRows = ceil((0x0A + totalPayloadBytes) / 6)
```

Examples:

```text
000C -> 14 payload bytes, 4 source rows
003F -> 65 stored payload bytes, 13 source rows
0000 -> 2 inline payload bytes, 2 source rows
```

The original external size field is preserved in RAW output.

A Type 5 header following Type 6 is parsed as the next structured block, not as Type 6 payload.

Context-aware annotations identify:

- breakpoint descriptor
- breakpoint type
- breakpoint mask
- payload-byte ranges
- final-row padding
- embedded records inside Type 6 data

Rows inside Type 6 are not reclassified only because their first digit resembles another code type.

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
Group Encrypted 4-4-4

Encryption Key
  Key 4 / WHBX
  Key 5 / WB123
  Key 6 / AB+XOR
  Key 7 / FCD!

Type 5 Payload Key
  Key 6
  Key 7
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
  Condense Writes -> Type 5
```

#### Xploder RAW

No output-specific settings are required.

Settings are saved beside the executable:

```text
XploderConverter.ini
```

Persisted values include:

- Input Type
- Output Type
- Auto Convert
- annotation state
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

The project is intended to build with **Microsoft Visual C++ / MSVC**.

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

The script attempts to locate and initialize the Visual Studio Developer Command Prompt automatically.

</details>

---

<details>
<summary><strong>▶️ Basic Usage</strong></summary>

<br>

1. Open `XploderConverterGui.exe`.
2. Select the Input Type.
3. Select the Output Type.
4. Paste code text into Input or drop one text file.
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

Caetla supports many standard GameShark / Action Replay codes, but some Caetla-specific prefixes conflict with GameShark meanings.

The converter therefore keeps the selected Input Type explicit and uses family-specific parsers.

</details>

---

<details>
<summary><strong>🙏 Credits and Thanks</strong></summary>

<br>

### misfire

GitHub: https://github.com/mlafeldt

Thank you for the original `xpcrypt` work and for long-standing contributions to the PlayStation cheat and hacking scene.

### Parasyte

Thank you for contributions to the PlayStation hacking and cheat-code community.

### Scene and Preservation Community

This project builds on the broader work of researchers who documented, tested, preserved, and shared knowledge about PlayStation cheat devices and code formats.

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

**Xploder PSX Converter v1.04**

</div>
