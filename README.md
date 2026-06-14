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
- Preserve CMP-style metadata and directives.
- Support drag-and-drop text input.
- Normalize pasted or dropped line endings.
- Native Windows desktop GUI.
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

</details>

---

<details>
<summary><strong>📁 Project Layout</strong></summary>

<br>

Expected folder layout:

```text
build-gui-msvc.cmd
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
build-gui-msvc.cmd
```

The script can prompt for the target build:

```text
1. Win32 / x86
2. Win64 / x64
```

You can also pass the target directly:

```bat
build-gui-msvc.cmd win32
```

```bat
build-gui-msvc.cmd win64
```

The script attempts to locate and initialize the Visual Studio Developer Command Prompt environment automatically.

If you are already inside a Visual Studio Developer Command Prompt, the script should build directly.

</details>

---

<details>
<summary><strong>▶️ Usage</strong></summary>

<br>

1. Open `XploderConverterGui.exe`.
2. Paste or drag Xploder code text into the input box.
3. Choose decrypt or encrypt mode.
4. Select the desired key style when encrypting.
5. Copy the converted output from the output box.

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
<summary><strong>🧠 Xploder Type 5 Notes</strong></summary>

<br>

Xploder Type 5 handling is one of the main reasons this converter exists.

A normal decrypted Type 5-style header may look like:

```text
$50007800 607C
```

During activation, Xploder converts this into an active-memory form such as:

```text
$50007800 0082
```

The public line contains the payload key and base payload size.  
The active version contains the expanded payload byte count.

Payload records after the Type 5 header are treated as payload data, not as normal standalone cheat lines.

This converter includes support for that behavior based on memory-dump testing and active-list comparison.

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

**Xploder PSX Converter v1.00**

</div>
