<!--
Xploder PSX Converter README
-->
<div align="center">

# 🔴 Xploder PSX Converter

### A clean Windows GUI for Xploder / Xplorer PSX cheat-code conversion

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Build](https://img.shields.io/badge/build-MSVC-green)
![License](https://img.shields.io/badge/license-GPLv3-red)

</div>
<a href="https://github.com/SkillerCMP/PSX-Xploder/releases" target="_blank">
    <img alt="GitHub Downloads (all assets, all releases)" src="https://img.shields.io/github/downloads/SkillerCMP/PSX-Xploder/total?style=social">
  </a>
<img alt="GitHub Downloads (all assets, latest release)" src="https://img.shields.io/github/downloads/SkillerCMP/PSX-Xploder/latest/total?style=social">

---

## 📌 Overview

**Xploder PSX Converter** is a native Windows utility for converting PlayStation 1 Xploder / Xplorer-style cheat codes between encrypted and decrypted formats.

This project focuses on practical PlayStation cheat-code research, especially:

- Xploder PSX code encryption and decryption
- CMP-style text handling
- RAW/encrypted auto-detection
- Xploder Type 5 mass-write / payload behavior
- Active-memory behavior observed through memory-dump testing

---

<details open>
<summary><strong>✨ Features</strong></summary>

<br>

- Convert encrypted Xploder PSX codes to decrypted RAW-style lines.
- Encrypt RAW-style Xploder lines back to Xploder format.
- Auto-detect RAW vs encrypted input where possible.
- Export Type 5 and Type 6 blocks in canonical external RAW form without leaking loader-only expanded sizes.
- Rebuild standalone Type 5 blocks and Type 5 blocks that immediately follow Type 6 using the selected Type 5 payload key.
- Preserve CMP-style metadata and directives.
- Drop one text file directly onto the Input pane to load it.
- Drop a folder onto the Input pane to batch-clean and decrypt every `.txt` file.
- Dropped files support UTF-8, UTF-8 BOM, UTF-16 LE/BE BOM, and legacy ANSI text.
- Folder batches export into a `Decrypted` folder using the same filenames and mirrored subfolders.
- Normalize pasted or dropped line endings.
- Native Windows desktop GUI.
- Draggable divider for resizing the Input and Output panes. The divider previews the new position while dragging, then resizes both panes cleanly on release.
- Simple MSVC build script.
- Optional Win32 / Win64 build output.
- Custom glowing neon red **X** icon.

</details>

---

<details>
<summary><strong>🔑 Supported Xploder Key Styles</strong></summary>

<br>

The converter supports the common Xploder / Xplorer key styles used for normal code encryption and decryption:

| Key | Style Name | Notes |
|---:|---|---|
| Key 4 | `WHBX` style | Rolling XOR-style behavior seen from the Xploder dump constants. |
| Key 5 | `WB123` style | Common Xploder / XplorerPro style key. |
| Key 6 | `AB + XOR` style | Behavior-based label; does not have the same readable text marker as the others. |
| Key 7 | `FCD!` style | Rolling ADD-style behavior seen from the Xploder dump constants. |

</details>

---

<details>
<summary><strong>🧩 CMP / Text Format Support</strong></summary>

<br>

The converter is designed to work cleanly with CMP-style cheat text.

It preserves common metadata and structure lines such as:

```text
^1 = Hash:
^2 = GameID:
^3 = NAME:
+Code Name
%Credits
!Group:
```

This allows the tool to be used for practical code-list conversion without destroying the surrounding file structure.

Encrypted output uses standard CMP `8 + 4` spacing by default:

```text
$65A58ECF CED9
```

The optional **Group encrypted 4-4-4** setting changes the same line to:

```text
$65A5 8ECF CED9
```

Compact 12-hex input remains accepted.

</details>

---

<details>
<summary><strong>🧠 Type 6 Structured Lengths</strong></summary>

<br>

Type 6 stores two payload bytes directly beside the breakpoint mask. The header size field counts the payload bytes that follow those two inline bytes:

```text
totalPayloadBytes = sizeField + 2
sourceRows = ceil((0x0A + totalPayloadBytes) / 6)
```

Examples:

- `000C` means 14 total payload bytes and four source rows.
- `003F` means 65 total stored payload bytes and thirteen source rows.
- `0000` still carries the two inline payload bytes.

The original Type 6 header value is preserved in canonical RAW output.

</details>

---

<details>
<summary><strong>📁 Project Layout</strong></summary>

<br>

Expected folder layout:

```text
build.cmd
src/
  XploderConverterGui.cpp
  XploderCmpConverter.hpp
  XploderMemoryCryptEngine.hpp
  XploderConverterGui.rc
  resource.h
  XploderNeonX.ico
```

The build script outputs the EXE beside the `.cmd` file.

Example outputs:

```text
XploderConverterGui-Win32.exe
XploderConverterGui-Win64.exe
```

</details>

---

<details>
<summary><strong>🛠️ Building</strong></summary>

<br>

This project is intended to build with **Microsoft Visual C++ / MSVC**.

From a normal Command Prompt, run:

```bat
build.cmd
```

The script can prompt for the target build:

```text
1. Win32 / x86
2. Win64 / x64
```

You can also pass the target directly:

```bat
build.cmd win32
```

```bat
build.cmd win64
```

The script attempts to locate and initialize the Visual Studio Developer Command Prompt environment automatically.

If you are already inside a Visual Studio Developer Command Prompt, the script should build directly.

</details>

---

<details>
<summary><strong>▶️ Usage</strong></summary>

<br>

1. Open `XploderConverterGui.exe`.
2. Paste code text into the Input pane, or drop one text file directly onto it.
3. Choose decrypt or encrypt mode.
4. Select the desired key style when encrypting.
5. Copy the converted output from the output box.

When **Auto Convert** is enabled, a dropped file is converted immediately after it is loaded. Only one file or one folder is accepted per drop, with a 64 MB safety limit for each text file.

### Folder batch decrypt

Drop a folder onto the Input pane to process all `.txt` files in that folder and its subfolders. The batch always uses decrypt mode and performs this cleanup before decryption:

```text
Infinite Health
by Code Master,
35b4-0db4-cece
```

becomes:

```text
+Infinite Health
%Credits: Code Master
$300B4FE5 0001
```

The cleanup stage changes hyphen-grouped `4-4-4`, space-grouped `4 4 4`, compact 12-hex, and existing `8 + 4` code lines into uppercase `$XXXXXXXX XXXX` form. RAW byte-write shorthand with a two-character value is also recognized in `8 + 2`, compact 10-hex, `4-4-2`, and `4 4 2` layouts; for example, `3007DE60 00` becomes `$3007DE60 00`. Wildcard template values containing `?` are recognized in two- or four-character form. For example, `700FB57E??` becomes `$700FB57E ??`, `700FB57E????` becomes `$700FB57E ????`, and `8007 8448 ????` becomes `$80078448 ????`.

DuckStation extended codes using an 8-hex address plus an 8-character value are detected separately and preserved instead of being sent through Xploder decryption. Examples include `$9006D9D8 EF6FF7C8`, `$A701E7F0 00882000`, and `$D7200000 00000004`. The value field may contain wildcards, such as `$9000C03C 240B????` or `$9000C020 240A00??`. These rows are uppercased and given a leading `$`, but their 8 + 8 layout and value remain unchanged. Compact 16-character DuckStation rows are also accepted.

These rows are recognized before plain-name prefixing, so they do not receive an incorrect leading `+`. Wildcard Xploder template rows are formatted but are not decrypted because their value is intentionally unknown. CMP metadata directives such as `^3 = NAME:` and `^2 = GameID:` remain unprefixed. The inline `by Author` portion is moved to `%Credits:` between the code name and first hex line, while the original `Crypt:` section remains attached to the code name. Results are written automatically to a `Decrypted` folder using the same filenames. Subfolder layouts are mirrored, and an existing `Decrypted` folder is skipped during scanning.

Example encrypted input:

```text
$35B40DB4CECE
```

Example decrypted output:

```text
$300B4FE5 0001
```

</details>

---

<details>
<summary><strong>🧠 Xploder Type 5 and Type 6 Notes</strong></summary>

<br>

Version 1.02 separates the external RAW format from the loader's internal runtime representation.

### Type 5 external RAW size

An encrypted/public Type 5 header can contain the payload key in the high nibble of its size word. After decryption, the converter exports a keyless RAW header whose value is the exact number of following payload bytes:

```text
Public/decrypted header:  $50007800 607C
Canonical external RAW:   $50007800 007C
Loader-internal value:    $50007800 0082
```

Only `007C` is written to decrypted RAW output. The loader-only `+0x06` value is kept internal.

A standalone RAW Type 5 example:

```text
$501E4FDC 0018
$2F667574 7572
$652F636F 6E73
$6F6C652F 6465
$7369676E 2F00
```

`0018` means exactly 24 following payload bytes, which is four six-byte rows.

After payload decryption, Type 5 rows are converted from loader byte order to conventional RAW code order by reversing the 32-bit address word and 16-bit value word independently:

```text
AC080004 0008  ->  040008AC 0800
AABBCCDD EEFF  ->  DDCCBBAA FFEE
```

The same self-inverse operation is applied before payload encryption, preserving exact round trips.

### Type 6 external RAW size

Type 6 is a breakpoint/bootstrap structure. Its external `nnnn` field counts the payload bytes that come **after** the first two payload bytes stored beside the breakpoint mask:

```text
actual payload bytes = nnnn + 2
```

The following rows also contain ten descriptor bytes: a six-byte break-address/type record and a four-byte break mask. The first two payload bytes share the second descriptor row.

The converter therefore consumes:

```text
payload bytes = size field + 2
source bytes  = payload bytes + 0x0A
source rows   = ceil(source bytes / 6)
```

Examples:

```text
$60FCD000 000C  -> 0x0E payload bytes, 0x18 source bytes, 4 rows
$60000000 003F  -> 0x41 payload bytes, 0x4B source bytes, 13 rows
$60000000 0000  -> 0x02 payload bytes, 0x0C source bytes, 2 rows
```

For the `000C` example, the complete final six-byte payload record `$90007612 AC08` remains inside Type 6. For the Rayman `003F` example, the 64-byte MIPS routine is followed by one stored zero byte before the final three row-padding bytes. The loader may internally expand the Type 6 value by `0x12`, but v1.02 keeps that runtime value internal. Decrypted RAW output retains the original field (`000C`, `003F`, and so on) exactly.

### Type 6 followed by Type 5

A Type 5 header that follows a Type 6 block is parsed as the next complete block, not as an embedded Type 6 payload row:

```text
$60FCD000 000C
... four Type 6 source rows ...
$50007610 00B0
... 0xB0 bytes of Type 5 payload ...
```

This distinction fixes RAW → encrypted → RAW round trips and prevents the converter from changing `000C` to `001E`, changing `00B0` to `00B6`, or processing already-RAW payload bytes a second time.

### Context-aware Type 6 annotations

When **Annotate code types** is enabled, the rows following a Type 6 header are interpreted using the Type 6 block layout rather than by their first hexadecimal digit. The annotation identifies the breakpoint descriptor, mask, payload ranges, and final padding.

For example:

```text
$90007612 AC08	// Type 6 payload bytes 0008-000C | paddingBytes=1 | leading nibble 9 is structured Type 6 data, not a standalone code type
```

Although this row starts with `9`, it remains part of the Type 6 byte stream. It is not a standalone Type 9 conditional.

</details>

---

<details>
<summary><strong>⚠️ Important Format Notes</strong></summary>

<br>

This project is focused on **Xploder / Xplorer-style PSX codes**.

GameShark and CodeBreaker may share some runtime concepts, but their public code formats are not always the same.

In particular:

```text
GameShark public Type 5 / 50-style serial code ≠ Xploder Type 5
```

GameShark public `50`-style codes are generally serial / slider / repeater-style codes.

Xploder Type 5 is handled as a mass-write / payload-style structure.

</details>

---

<details>
<summary><strong>🙏 Credits and Thanks</strong></summary>

<br>

Special thanks to:

### misfire  
GitHub: https://github.com/mlafeldt

Thank you for the original `xpcrypt` work and for the long-standing contributions to the PlayStation cheat and hacking scene.

That research helped preserve important knowledge about Xploder / Xplorer-style code encryption and made later verification, documentation, and tooling work much easier to build upon.

### Parasyte

Thank you for your contributions to the PlayStation hacking and cheat-code scene.

The tools, notes, discoveries, and community work from early scene researchers continue to help people understand how these devices, code formats, and cheat engines work.

### Scene and Preservation Community

This project builds on the broader work of the PlayStation cheat-code community.

The goal is to preserve, document, and improve tooling for research, restoration, conversion, and compatibility.

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

See the repository `LICENSE` file for the full license text.

```text
GNU General Public License v3.0
```

</details>

---

<div align="center">

**Xploder PSX Converter v1.02**

</div>


### Folder batch progress

- A progress bar appears while a dropped folder is processed.
- The status line shows the current file, completed-file count, total count, and percentage.
- Conversion controls are temporarily disabled until the batch finishes.
