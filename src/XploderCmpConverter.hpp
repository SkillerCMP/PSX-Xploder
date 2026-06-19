#pragma once

// XploderCmpConverter.hpp
// Full CMP-style text converter using XploderMemoryCryptEngine.hpp.
//
// Decrypt mode:
//   XplorerPro / FX encrypted text -> active/runtime CMP-style RAW text.
//   Type 5/6 mass-write blocks are converted to the active header + payload bytes.
//   Type 6 copy-route blocks can carry an embedded Type 5 continuation.
//
// Encrypt mode:
//   active/runtime CMP-style RAW text -> XplorerPro / FX encrypted text.
//   Type 5/6 mass-write blocks are rebuilt to the public header and encrypted payload lines.

#include "XploderMemoryCryptEngine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace xploder_converter
{
    enum class Mode
    {
        Decrypt,
        Encrypt
    };

    struct Options
    {
        Mode mode = Mode::Decrypt;
        xploder_psx::Key encryptionKey = xploder_psx::Key::Key5;
        int massWritePayloadKey = 6;
        bool prefixPlainNames = true;
        bool groupEncryptedOutput = false;
        bool annotateCodeTypes = false;
    };

    inline bool isHex(char c) noexcept
    {
        return xploder_psx::hexValue(c) >= 0;
    }

    inline std::string trim(std::string_view text)
    {
        std::size_t first = 0;
        while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
            ++first;

        std::size_t last = text.size();
        while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
            --last;

        return std::string(text.substr(first, last - first));
    }

    inline bool startsWith(std::string_view s, std::string_view prefix) noexcept
    {
        return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
    }

    inline std::vector<std::string> splitLines(const std::string& input)
    {
        std::vector<std::string> lines;
        std::string current;
        for (char c : input)
        {
            if (c == '\r')
                continue;
            if (c == '\n')
            {
                lines.push_back(current);
                current.clear();
            }
            else
            {
                current.push_back(c);
            }
        }
        lines.push_back(current);
        return lines;
    }

    inline std::string formatEncrypted(const xploder_psx::Code& code, bool grouped)
    {
        return grouped ? xploder_psx::toGroupedEncryptedText(code) : xploder_psx::toCompactHex(code);
    }

    inline std::string hex4(int value)
    {
        static constexpr char lut[] = "0123456789ABCDEF";
        std::string s(4, '0');
        value &= 0xFFFF;
        s[0] = lut[(value >> 12) & 0xF];
        s[1] = lut[(value >> 8) & 0xF];
        s[2] = lut[(value >> 4) & 0xF];
        s[3] = lut[value & 0xF];
        return s;
    }

    inline std::string hex7(std::uint32_t value)
    {
        static constexpr char lut[] = "0123456789ABCDEF";
        value &= 0x0FFFFFFF;
        std::string s(7, '0');
        for (int i = 6; i >= 0; --i)
        {
            s[static_cast<std::size_t>(i)] = lut[value & 0xF];
            value >>= 4;
        }
        return s;
    }

    inline bool isConfirmedMassWritePayloadKey(int payloadKey) noexcept
    {
        return payloadKey == 6 || payloadKey == 7;
    }

    inline std::string buildCodeTypeComment(const xploder_psx::Code& raw)
    {
        const std::uint32_t codeWord =
            (static_cast<std::uint32_t>(raw[0]) << 24) |
            (static_cast<std::uint32_t>(raw[1]) << 16) |
            (static_cast<std::uint32_t>(raw[2]) << 8) |
            static_cast<std::uint32_t>(raw[3]);
        const int type = raw[0] >> 4;
        const std::uint32_t address = codeWord & 0x0FFFFFFF;
        const int value = (static_cast<int>(raw[4]) << 8) | raw[5];

        std::ostringstream ss;
        ss << "// Type " << std::uppercase << std::hex << type << ": ";

        switch (type)
        {
            case 0x0: ss << "32-bit write | 0AAAAAAA VVVVVVVV | addr=0x" << hex7(address); break;
            case 0x3: ss << "8-bit write | 3AAAAAAA 00VV | addr=0x" << hex7(address) << " value=0x" << hex4(value & 0xFF); break;
            case 0x5: ss << "Type 5 mass-write / block copy | addr=0x" << hex7(address) << " payloadBytes=0x" << hex4(value); break;
            case 0x6: ss << "Type 6 special descriptor/bootstrap | addr=0x" << hex7(address) << " payloadBytes/value=0x" << hex4(value); break;
            case 0x7: ss << "16-bit equal conditional | if [0x" << hex7(address) << "] == 0x" << hex4(value); break;
            case 0x8: ss << "16-bit write | 8AAAAAAA VVVV | addr=0x" << hex7(address) << " value=0x" << hex4(value); break;
            case 0x9: ss << "16-bit not-equal conditional | if [0x" << hex7(address) << "] != 0x" << hex4(value); break;
            case 0xB: ss << "serial/repeater | BNNNIIII DDDD"; break;
            case 0xE: ss << "8-bit write to address+1 | EAAAAAAA 00VV | addr=0x" << hex7(address + 1) << " value=0x" << hex4(value & 0xFF); break;
            case 0xF: ss << "global equal gate | if [0x" << hex7(address) << "] == 0x" << hex4(value); break;
            default: ss << "not directly handled by confirmed Xploder runtime | addr/param=0x" << hex7(address) << " value=0x" << hex4(value); break;
        }
        return ss.str();
    }

    inline bool tryGetCodeBody(const std::string& line, std::string& body)
    {
        body.clear();
        const std::string t = trim(line);
        if (t.empty())
            return false;

        if (startsWith(t, "!!") || t[0] == '!' || t[0] == '%' || t[0] == ';' || startsWith(t, "//") || t[0] == '+')
            return false;

        if (t[0] == '$')
            body = trim(std::string_view(t).substr(1));
        else
            body = t;

        return !body.empty();
    }

    inline std::vector<std::string> getLeadingHexTokens(std::string_view text)
    {
        std::vector<std::string> tokens;
        std::string current;

        for (char c : text)
        {
            if (isHex(c))
            {
                current.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(c)))
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }
                continue;
            }

            if (!current.empty())
                tokens.push_back(current);
            break;
        }

        if (!current.empty())
            tokens.push_back(current);

        return tokens;
    }

    inline bool hexToCode(std::string_view hex, xploder_psx::Code& code)
    {
        if (hex.size() != xploder_psx::CodeLength * 2)
            return false;

        code = {};
        for (std::size_t i = 0; i < xploder_psx::CodeLength; ++i)
        {
            const int hi = xploder_psx::hexValue(hex[i * 2]);
            const int lo = xploder_psx::hexValue(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            code[i] = xploder_psx::byte((hi << 4) | lo);
        }
        return true;
    }

    inline int collectFirstHexPayload(std::string_view text, std::string& hex, Mode mode)
    {
        hex.clear();
        const std::size_t maxHex = xploder_psx::CodeLength * 2;

        for (char c : text)
        {
            if (isHex(c))
            {
                if (hex.size() < maxHex)
                    hex.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(c)))
                continue;

            if (hex.size() == maxHex || (mode == Mode::Encrypt && hex.size() == 10))
                break;

            return -1;
        }

        return static_cast<int>(hex.size());
    }

    inline bool parseRawCodeWithSeparateValue(std::string_view text, xploder_psx::Code& code)
    {
        const std::vector<std::string> tokens = getLeadingHexTokens(text);
        if (tokens.size() < 2 || tokens[0].size() != 8 || tokens[1].empty() || tokens[1].size() > 4)
            return false;

        int value = 0;
        for (char c : tokens[1])
        {
            const int hv = xploder_psx::hexValue(c);
            if (hv < 0)
                return false;
            value = (value << 4) | hv;
        }

        std::string full = tokens[0] + hex4(value);
        return hexToCode(full, code);
    }

    inline bool parseCodeBody(std::string_view text, Mode mode, xploder_psx::Code& code)
    {
        code = {};

        if (mode == Mode::Encrypt && parseRawCodeWithSeparateValue(text, code))
            return true;

        std::string hex;
        const int hexCount = collectFirstHexPayload(text, hex, mode);

        if (hexCount == static_cast<int>(xploder_psx::CodeLength * 2))
            return hexToCode(hex, code);

        // RAW byte-write shorthand in encrypt mode, e.g. 3007EB2001 -> 3007EB20 0001.
        if (mode == Mode::Encrypt && hexCount == 10)
        {
            std::string expanded = hex.substr(0, 8) + "00" + hex.substr(8, 2);
            return hexToCode(expanded, code);
        }

        return false;
    }

    inline bool parsePayloadLineBytes(const std::string& line, xploder_psx::Code& bytes, int& byteCount)
    {
        bytes = {};
        byteCount = 0;

        std::string body;
        if (!tryGetCodeBody(line, body))
            return false;

        std::string hex;
        for (char c : body)
        {
            if (isHex(c))
            {
                if (hex.size() >= xploder_psx::CodeLength * 2)
                    break;
                hex.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(c)))
                continue;

            break;
        }

        if (hex.empty() || (hex.size() & 1U) != 0)
            return false;

        byteCount = static_cast<int>(hex.size() / 2);
        for (int i = 0; i < byteCount; ++i)
        {
            const int hi = xploder_psx::hexValue(hex[static_cast<std::size_t>(i * 2)]);
            const int lo = xploder_psx::hexValue(hex[static_cast<std::size_t>(i * 2 + 1)]);
            if (hi < 0 || lo < 0)
                return false;
            bytes[static_cast<std::size_t>(i)] = xploder_psx::byte((hi << 4) | lo);
        }

        return true;
    }

    inline bool convertCodeBody(std::string_view text, const Options& options, std::string& converted)
    {
        converted.clear();
        xploder_psx::Code code;
        if (!parseCodeBody(text, options.mode, code))
            return false;

        bool preservedPassThrough = false;
        bool ok = false;
        if (options.mode == Mode::Decrypt)
        {
            ok = xploder_psx::decryptCode(code);
        }
        else
        {
            // Some Xploder database/trailer lines are already in an encrypted/pass-through
            // route, such as $52FF30F3 1B15 after a Type 5 block. They are not active
            // runtime RAW lines and should not be re-encrypted with Key 5. Preserve them.
            if (xploder_psx::routeForCode(code) == xploder_psx::Route::Copy && (code[0] & 0x0F) != 0)
            {
                preservedPassThrough = true;
                ok = true;
            }
            else
            {
                ok = xploder_psx::encryptCode(code, options.encryptionKey);
            }
        }

        if (!ok && options.mode == Mode::Encrypt)
            return false;

        converted = (options.mode == Mode::Encrypt)
            ? formatEncrypted(code, options.groupEncryptedOutput)
            : xploder_psx::toRawCmpText(code);

        if (options.annotateCodeTypes)
        {
            if (preservedPassThrough)
            {
                converted += "\t// preserved pass-through/trailer line";
            }
            else
            {
                xploder_psx::Code rawForComment = code;
                if (options.mode == Mode::Encrypt)
                    (void)xploder_psx::decryptCode(rawForComment);
                converted += "\t" + buildCodeTypeComment(rawForComment);
            }
        }

        return true;
    }

    inline bool tryPreserveActiveMassWriteBlock(const std::vector<std::string>& lines, std::size_t startIndex, const Options& options, std::string& preservedBlock, std::size_t& lastConsumedIndex)
    {
        preservedBlock.clear();
        lastConsumedIndex = startIndex;

        std::string headerBody;
        if (!tryGetCodeBody(lines[startIndex], headerBody))
            return false;

        xploder_psx::Code header;
        if (!parseCodeBody(headerBody, Mode::Decrypt, header))
            return false;

        // Active/runtime headers are already raw and use a normal Type 5/6
        // first byte. Encrypted headers use one of the key routes instead.
        const int codeType = header[0] & 0xF0;

        // This preservation rule is intentionally Type 5 only.
        //
        // A public Type 6 descriptor commonly decrypts to a raw-looking 60
        // header with payload key 0. Treating every such header as already
        // active breaks valid database codes such as:
        //   $65A58ECFCED9 -> $60FCD000 000C
        //
        // The Xploder loader expands that Type 6 value by 0x12 and copies the
        // following descriptor rows. Type 6 therefore needs to continue into
        // tryConvertMassWriteBlock instead of being protected here.
        if (codeType != 0x50 ||
            (header[0] & 0x0F) != 0 ||
            xploder_psx::routeForCode(header) != xploder_psx::Route::Copy)
        {
            return false;
        }

        // A raw public Type 5 header carrying a confirmed payload key must be
        // converted. A raw Type 5 header with no confirmed key is treated as
        // the already-active form and preserved byte-for-byte.
        const int possiblePublicKey = header[4] >> 4;
        if (isConfirmedMassWritePayloadKey(possiblePublicKey))
            return false;

        const int payloadSize = (static_cast<int>(header[4]) << 8) | header[5];
        if (payloadSize <= 0 || payloadSize > 0x10000)
            return false;

        const int payloadLineCount =
            (payloadSize + static_cast<int>(xploder_psx::CodeLength) - 1) /
            static_cast<int>(xploder_psx::CodeLength);

        if (payloadLineCount <= 0 ||
            startIndex + static_cast<std::size_t>(payloadLineCount) >= lines.size())
        {
            return false;
        }

        std::ostringstream out;
        out << "$" << xploder_psx::toRawCmpText(header);
        if (options.annotateCodeTypes)
        {
            out << "\t// already-active Type " << (codeType == 0x60 ? '6' : '5')
                << " mass-write header | payloadBytes=0x" << hex4(payloadSize)
                << " payloadLines=" << payloadLineCount;
        }

        for (int i = 0; i < payloadLineCount; ++i)
        {
            const std::size_t lineIndex = startIndex + 1U + static_cast<std::size_t>(i);
            std::string payloadBody;
            if (!tryGetCodeBody(lines[lineIndex], payloadBody))
                return false;

            xploder_psx::Code payload;
            if (!parseCodeBody(payloadBody, Mode::Decrypt, payload))
                return false;

            // Keep active payload bytes exactly as supplied. They are binary
            // data and must never be passed through normal line decryption.
            out << "\n$" << xploder_psx::toRawCmpText(payload);
            if (options.annotateCodeTypes)
            {
                const int firstByteOffset = i * static_cast<int>(xploder_psx::CodeLength);
                const int bytesUsed = std::min(
                    static_cast<int>(xploder_psx::CodeLength),
                    std::max(0, payloadSize - firstByteOffset));
                out << "\t// preserved active payload | bytes "
                    << hex4(firstByteOffset) << "-"
                    << hex4(firstByteOffset + bytesUsed - 1);
                if (bytesUsed != static_cast<int>(xploder_psx::CodeLength))
                {
                    out << "; last "
                        << (static_cast<int>(xploder_psx::CodeLength) - bytesUsed)
                        << " byte(s) padding/ignored";
                }
            }
        }

        preservedBlock = out.str();
        lastConsumedIndex = startIndex + static_cast<std::size_t>(payloadLineCount);
        return true;
    }


    struct NestedType5ContinuationInfo
    {
        bool found = false;
        bool inputHeaderWasActive = false;
        int payloadKey = 0;
        int basePayloadSize = 0;       // bytes supplied after the Type 6 block
        int activePayloadSize = 0;     // basePayloadSize + 0x06
        int payloadLineCount = 0;      // ceil(basePayloadSize / 6)
        std::size_t embeddedHeaderLineIndex = 0;
        xploder_psx::Code publicHeader{};
        xploder_psx::Code activeHeader{};
    };

    inline bool tryFindNestedType5Continuation(
        const std::vector<std::string>& lines,
        std::size_t type6StartIndex,
        const xploder_psx::MassWriteInfo& type6Info,
        int selectedPayloadKey,
        NestedType5ContinuationInfo& nestedInfo)
    {
        nestedInfo = {};

        // Confirmed nested layout:
        //
        //   Type 6 active size = 0x12-byte descriptor + public Type 6 base size.
        //
        // The final complete six-byte row inside that Type 6 payload may be an
        // encrypted Type 5 header. When this happens, the Type 5 header itself
        // supplies the extra +0x06 bytes represented by its active size. Only
        // the Type 5 base-size bytes follow outside the Type 6 block.
        //
        // Example:
        //   $65A58ECFCED9 -> public Type 6 $60FCD000 000C
        //                    active Type 6 $60FCD000 001E
        //
        //   0x12 descriptor bytes
        //   $90007612 AC08
        //   $55A934DF 2E7D  (embedded encrypted Type 5 header)
        //   followed by 0xB0 bytes / 30 encrypted Type 5 payload rows.
        //
        // This behavior is only confirmed for the Type 6 copy route (key 0).
        if (!type6Info.isType6 || type6Info.payloadKey != 0)
            return false;

        constexpr int Type6DescriptorSize = 0x12;
        if (type6Info.payloadSize <= Type6DescriptorSize)
            return false;

        const int publicType6BaseSize = type6Info.payloadSize - Type6DescriptorSize;
        if (publicType6BaseSize < static_cast<int>(xploder_psx::CodeLength))
            return false;

        // The embedded Type 5 header must be the final complete six-byte row of
        // the Type 6 payload. If the Type 6 base data ends in a partial row, do
        // not guess that padding is another header.
        if ((publicType6BaseSize % static_cast<int>(xploder_psx::CodeLength)) != 0)
            return false;

        const int embeddedRowCount =
            publicType6BaseSize / static_cast<int>(xploder_psx::CodeLength);
        const int embeddedHeaderPayloadRow =
            (Type6DescriptorSize / static_cast<int>(xploder_psx::CodeLength)) +
            embeddedRowCount - 1;

        if (embeddedHeaderPayloadRow < 0 ||
            embeddedHeaderPayloadRow >= type6Info.payloadLineCount)
        {
            return false;
        }

        const std::size_t embeddedHeaderLineIndex =
            type6StartIndex + 1U +
            static_cast<std::size_t>(embeddedHeaderPayloadRow);

        if (embeddedHeaderLineIndex >= lines.size())
            return false;

        std::string embeddedBody;
        if (!tryGetCodeBody(lines[embeddedHeaderLineIndex], embeddedBody))
            return false;

        xploder_psx::Code embeddedHeader;
        if (!parseCodeBody(embeddedBody, Mode::Decrypt, embeddedHeader))
            return false;

        xploder_psx::Code publicNestedHeader{};
        xploder_psx::Code activeNestedHeader{};
        int payloadKey = 0;
        int basePayloadSize = 0;
        bool inputHeaderWasActive = false;

        // First try the real encrypted nested-header form. This is the form
        // stored inside the original Type 6 payload, for example:
        //
        //   $55A934DF 2E7D -> $50007610 60B0
        //
        // If that does not apply, accept the normalized active form emitted by
        // this converter:
        //
        //   $50007610 00B6
        //
        // The active form no longer stores the payload key, so encryption uses
        // the current Type 5 Payload Key selection from the GUI.
        xploder_psx::Code decryptedCandidate = embeddedHeader;
        const bool decryptedAsNormalLine =
            xploder_psx::decryptCode(decryptedCandidate);

        if (decryptedAsNormalLine &&
            (decryptedCandidate[0] & 0xF0) == 0x50)
        {
            const int detectedPayloadKey =
                decryptedCandidate[4] >> 4;

            if (!isConfirmedMassWritePayloadKey(
                    detectedPayloadKey))
            {
                return false;
            }

            payloadKey = detectedPayloadKey;
            basePayloadSize =
                ((static_cast<int>(decryptedCandidate[4]) & 0x0F) << 8) |
                decryptedCandidate[5];
            publicNestedHeader = decryptedCandidate;
        }
        else if ((embeddedHeader[0] & 0xF0) == 0x50)
        {
            if (!isConfirmedMassWritePayloadKey(
                    selectedPayloadKey))
            {
                return false;
            }

            const int encodedValue =
                (static_cast<int>(embeddedHeader[4]) << 8) |
                embeddedHeader[5];

            // Also accept a raw public Type 5 header for compatibility. A raw
            // public header keeps its payload key in the high nibble.
            const int possiblePublicKey =
                embeddedHeader[4] >> 4;
            const int possiblePublicSize =
                ((static_cast<int>(embeddedHeader[4]) & 0x0F) << 8) |
                embeddedHeader[5];

            if (isConfirmedMassWritePayloadKey(
                    possiblePublicKey) &&
                possiblePublicSize > 0 &&
                possiblePublicSize <= 0x0FFF)
            {
                payloadKey = possiblePublicKey;
                basePayloadSize = possiblePublicSize;
                publicNestedHeader = embeddedHeader;
            }
            else
            {
                if (encodedValue <= 0x06)
                    return false;

                payloadKey = selectedPayloadKey;
                basePayloadSize = encodedValue - 0x06;
                publicNestedHeader = embeddedHeader;
                publicNestedHeader[4] = xploder_psx::byte(
                    ((payloadKey & 0x0F) << 4) |
                    ((basePayloadSize >> 8) & 0x0F));
                publicNestedHeader[5] = xploder_psx::byte(
                    basePayloadSize);
                inputHeaderWasActive = true;
            }
        }
        else
        {
            return false;
        }

        if (basePayloadSize <= 0 || basePayloadSize > 0x0FFF)
            return false;

        const int activePayloadSize = basePayloadSize + 0x06;
        activeNestedHeader = publicNestedHeader;
        activeNestedHeader[4] = xploder_psx::byte(
            activePayloadSize >> 8);
        activeNestedHeader[5] = xploder_psx::byte(
            activePayloadSize);

        const int payloadLineCount =
            (basePayloadSize + static_cast<int>(xploder_psx::CodeLength) - 1) /
            static_cast<int>(xploder_psx::CodeLength);

        const std::size_t firstContinuationLine =
            type6StartIndex + 1U +
            static_cast<std::size_t>(type6Info.payloadLineCount);

        if (payloadLineCount <= 0 ||
            firstContinuationLine + static_cast<std::size_t>(payloadLineCount) >
                lines.size())
        {
            return false;
        }

        // Require every expected continuation row to be a complete code line.
        // This prevents coincidental Type 5-looking bytes inside descriptor
        // data from swallowing following names, comments, or unrelated codes.
        for (int i = 0; i < payloadLineCount; ++i)
        {
            std::string payloadBody;
            if (!tryGetCodeBody(
                    lines[firstContinuationLine + static_cast<std::size_t>(i)],
                    payloadBody))
            {
                return false;
            }

            xploder_psx::Code payload;
            if (!parseCodeBody(payloadBody, Mode::Decrypt, payload))
                return false;
        }

        nestedInfo.found = true;
        nestedInfo.inputHeaderWasActive = inputHeaderWasActive;
        nestedInfo.payloadKey = payloadKey;
        nestedInfo.basePayloadSize = basePayloadSize;
        nestedInfo.activePayloadSize = activePayloadSize;
        nestedInfo.payloadLineCount = payloadLineCount;
        nestedInfo.embeddedHeaderLineIndex = embeddedHeaderLineIndex;
        nestedInfo.publicHeader = publicNestedHeader;
        nestedInfo.activeHeader = activeNestedHeader;
        return true;
    }

    inline bool tryConvertMassWriteBlock(const std::vector<std::string>& lines, std::size_t startIndex, const Options& options, std::string& convertedBlock, std::size_t& lastConsumedIndex)
    {
        convertedBlock.clear();
        lastConsumedIndex = startIndex;

        std::string headerBody;
        if (!tryGetCodeBody(lines[startIndex], headerBody))
            return false;

        xploder_psx::Code header;
        if (!parseCodeBody(headerBody, Mode::Decrypt, header))
            return false;

        xploder_psx::MassWriteInfo info;
        if (!xploder_psx::decryptMassWriteHeaderToActive(header, info))
            return false;

        // Type 5 payload encryption is confirmed for keys 6 and 7 only.
        // A raw active Type 5 header such as $50007800 0050 has no public
        // payload key encoded in its value and must not be reinterpreted.
        //
        // Type 6 is different. The loader accepts the low nibble of byte 3 as
        // its descriptor payload route. Keys 6 and 7 use the forced payload
        // transforms; every other Type 6 key follows the loader copy route.
        // Key 0 is common and is valid pass-through descriptor data.
        if (!info.isType6 && !isConfirmedMassWritePayloadKey(info.payloadKey))
            return false;

        // Keep sane bounds so normal accidental $50/$60 lines do not swallow
        // the whole file.
        if (info.payloadSize <= 0 ||
            info.payloadSize > 0x10000 ||
            info.payloadLineCount <= 0)
        {
            return false;
        }

        if (startIndex + static_cast<std::size_t>(info.payloadLineCount) >=
            lines.size())
        {
            return false;
        }

        // Check the original Type 6 payload before converting it. The final
        // embedded row may be an encrypted Type 5 header whose payload follows
        // immediately after the Type 6 block.
        NestedType5ContinuationInfo nestedType5;
        if (info.isType6)
        {
            (void)tryFindNestedType5Continuation(
                lines,
                startIndex,
                info,
                options.massWritePayloadKey,
                nestedType5);
        }

        std::ostringstream out;
        out << "$" << xploder_psx::toRawCmpText(header);
        if (options.annotateCodeTypes)
        {
            out << "\t// Type " << (info.isType6 ? '6' : '5')
                << " mass-write header | payloadKey=" << info.payloadKey
                << " activeBytes=0x" << hex4(info.payloadSize)
                << " sourceBytes=0x" << hex4(info.sourcePayloadSize)
                << " payloadLines=" << info.payloadLineCount;

            if (nestedType5.found)
            {
                out << " | nested Type 5 continuation: key="
                    << nestedType5.payloadKey
                    << " baseBytes=0x"
                    << hex4(nestedType5.basePayloadSize)
                    << " rows="
                    << nestedType5.payloadLineCount;
            }
        }

        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            const std::size_t lineIndex =
                startIndex + 1U + static_cast<std::size_t>(i);

            std::string payloadBody;
            if (!tryGetCodeBody(lines[lineIndex], payloadBody))
                return false;

            xploder_psx::Code payload;
            if (!parseCodeBody(payloadBody, Mode::Decrypt, payload))
                return false;

            if (isConfirmedMassWritePayloadKey(info.payloadKey))
            {
                if (!xploder_psx::decryptPayloadChunk(
                        payload,
                        info.payloadKey))
                {
                    return false;
                }
            }
            else if (!info.isType6)
            {
                return false;
            }
            // For Type 6 keys other than 6/7, the real loader uses the copy
            // route. Keep descriptor rows byte-for-byte. When the final row is
            // a confirmed nested Type 5 header, normalize it into active form.
            const bool isNestedHeaderRow =
                nestedType5.found &&
                lineIndex == nestedType5.embeddedHeaderLineIndex;

            if (isNestedHeaderRow)
            {
                payload = nestedType5.activeHeader;
            }

            out << "\n$" << xploder_psx::toRawCmpText(payload);
            if (options.annotateCodeTypes)
            {
                const int firstByteOffset =
                    i * static_cast<int>(xploder_psx::CodeLength);
                const int bytesUsed = std::min(
                    static_cast<int>(xploder_psx::CodeLength),
                    std::max(0, info.sourcePayloadSize - firstByteOffset));

                out << "\t// Type "
                    << (info.isType6 ? '6' : '5')
                    << " payload data | bytes "
                    << hex4(firstByteOffset)
                    << "-"
                    << hex4(firstByteOffset + bytesUsed - 1);

                if (bytesUsed !=
                    static_cast<int>(xploder_psx::CodeLength))
                {
                    out << "; last "
                        << (static_cast<int>(xploder_psx::CodeLength) -
                            bytesUsed)
                        << " byte(s) padding/ignored";
                }

                if (isNestedHeaderRow)
                {
                    out << " | normalized nested Type 5 active header"
                        << " | public="
                        << xploder_psx::toRawCmpText(
                               nestedType5.publicHeader)
                        << " | key="
                        << nestedType5.payloadKey;
                }
            }
        }

        std::size_t consumedIndex =
            startIndex + static_cast<std::size_t>(info.payloadLineCount);

        if (nestedType5.found)
        {
            const std::size_t firstContinuationLine =
                consumedIndex + 1U;

            for (int i = 0;
                 i < nestedType5.payloadLineCount;
                 ++i)
            {
                const std::size_t lineIndex =
                    firstContinuationLine +
                    static_cast<std::size_t>(i);

                std::string payloadBody;
                if (!tryGetCodeBody(lines[lineIndex], payloadBody))
                    return false;

                xploder_psx::Code payload;
                if (!parseCodeBody(
                        payloadBody,
                        Mode::Decrypt,
                        payload))
                {
                    return false;
                }

                if (!xploder_psx::decryptPayloadChunk(
                        payload,
                        nestedType5.payloadKey))
                {
                    return false;
                }

                out << "\n$"
                    << xploder_psx::toRawCmpText(payload);

                if (options.annotateCodeTypes)
                {
                    const int firstByteOffset =
                        i * static_cast<int>(
                                xploder_psx::CodeLength);
                    const int bytesUsed = std::min(
                        static_cast<int>(
                            xploder_psx::CodeLength),
                        std::max(
                            0,
                            nestedType5.basePayloadSize -
                                firstByteOffset));

                    out << "\t// nested Type 5 payload data | bytes "
                        << hex4(firstByteOffset)
                        << "-"
                        << hex4(firstByteOffset + bytesUsed - 1);

                    if (bytesUsed !=
                        static_cast<int>(
                            xploder_psx::CodeLength))
                    {
                        out << "; last "
                            << (static_cast<int>(
                                    xploder_psx::CodeLength) -
                                bytesUsed)
                            << " byte(s) padding/ignored";
                    }
                }
            }

            consumedIndex +=
                static_cast<std::size_t>(
                    nestedType5.payloadLineCount);
        }

        convertedBlock = out.str();
        lastConsumedIndex = consumedIndex;
        return true;
    }

    inline bool tryEncryptMassWriteBlock(const std::vector<std::string>& lines, std::size_t startIndex, const Options& options, std::string& convertedBlock, std::size_t& lastConsumedIndex)
    {
        convertedBlock.clear();
        lastConsumedIndex = startIndex;

        std::string headerBody;
        if (!tryGetCodeBody(lines[startIndex], headerBody))
            return false;

        xploder_psx::Code activeHeader;
        if (!parseCodeBody(headerBody, Mode::Encrypt, activeHeader))
            return false;

        const int codeType = activeHeader[0] & 0xF0;
        if (codeType != 0x50 && codeType != 0x60)
            return false;

        const bool isType6 = codeType == 0x60;
        const int payloadSize =
            (static_cast<int>(activeHeader[4]) << 8) |
            activeHeader[5];

        if (payloadSize <= 0 || payloadSize > 0x10000)
            return false;

        const int sourcePayloadSize =
            isType6 ? payloadSize : payloadSize - 0x06;
        const int payloadLineCount =
            (sourcePayloadSize + static_cast<int>(xploder_psx::CodeLength) - 1) /
            static_cast<int>(xploder_psx::CodeLength);

        if (sourcePayloadSize <= 0 || payloadLineCount <= 0 ||
            startIndex + static_cast<std::size_t>(payloadLineCount) >=
                lines.size())
        {
            return false;
        }

        // Type 5 active headers no longer contain the original payload key, so
        // use the selected key. Type 6 keeps its payload/copy route in the low
        // nibble of byte 3; preserve that route during round-trip encryption.
        const int effectivePayloadKey =
            isType6
                ? (activeHeader[3] & 0x0F)
                : options.massWritePayloadKey;

        if (!isType6 &&
            !isConfirmedMassWritePayloadKey(effectivePayloadKey))
        {
            return false;
        }

        std::vector<xploder_psx::Code> payloadRows;
        payloadRows.reserve(
            static_cast<std::size_t>(payloadLineCount));

        for (int i = 0; i < payloadLineCount; ++i)
        {
            const std::size_t lineIndex =
                startIndex + 1U + static_cast<std::size_t>(i);

            xploder_psx::Code payload;
            int byteCount = 0;
            if (!parsePayloadLineBytes(
                    lines[lineIndex],
                    payload,
                    byteCount))
            {
                return false;
            }

            if (byteCount == 0)
                return false;

            payloadRows.push_back(payload);
        }

        xploder_psx::MassWriteInfo activeInfo;
        activeInfo.payloadKey = effectivePayloadKey;
        activeInfo.payloadSize = payloadSize;
        activeInfo.sourcePayloadSize = sourcePayloadSize;
        activeInfo.payloadLineCount = payloadLineCount;
        activeInfo.isType6 = isType6;

        NestedType5ContinuationInfo nestedType5;
        if (isType6)
        {
            (void)tryFindNestedType5Continuation(
                lines,
                startIndex,
                activeInfo,
                options.massWritePayloadKey,
                nestedType5);
        }

        xploder_psx::Code encryptedHeader = activeHeader;
        xploder_psx::MassWriteInfo publicInfo;
        if (!xploder_psx::encryptMassWriteHeaderFromActive(
                encryptedHeader,
                options.encryptionKey,
                effectivePayloadKey,
                &publicInfo))
        {
            return false;
        }

        std::ostringstream out;
        out << "$"
            << formatEncrypted(
                   encryptedHeader,
                   options.groupEncryptedOutput);

        if (options.annotateCodeTypes)
        {
            xploder_psx::Code publicHeader = activeHeader;
            const int adjust = isType6 ? 0x12 : 0x06;
            const int baseSize = payloadSize - adjust;

            if (isType6)
            {
                publicHeader[3] = xploder_psx::byte(
                    (publicHeader[3] & 0xF0) |
                    (effectivePayloadKey & 0x0F));
                publicHeader[4] = xploder_psx::byte(
                    baseSize >> 8);
                publicHeader[5] = xploder_psx::byte(
                    baseSize);
            }
            else
            {
                publicHeader[4] = xploder_psx::byte(
                    ((effectivePayloadKey & 0x0F) << 4) |
                    ((baseSize >> 8) & 0x0F));
                publicHeader[5] = xploder_psx::byte(
                    baseSize);
            }

            out << "\t// Type "
                << (isType6 ? '6' : '5')
                << " mass-write encrypted header | payloadKey="
                << effectivePayloadKey
                << " payloadBytes=0x"
                << hex4(payloadSize)
                << " publicValue="
                << xploder_psx::toCompactHex(publicHeader)
                       .substr(8, 4);

            if (nestedType5.found)
            {
                out << " | nested Type 5 continuation: key="
                    << nestedType5.payloadKey
                    << " baseBytes=0x"
                    << hex4(nestedType5.basePayloadSize)
                    << " rows="
                    << nestedType5.payloadLineCount;
            }
        }

        for (int i = 0; i < payloadLineCount; ++i)
        {
            xploder_psx::Code payload =
                payloadRows[static_cast<std::size_t>(i)];

            const std::size_t lineIndex =
                startIndex + 1U +
                static_cast<std::size_t>(i);
            const bool isNestedHeaderRow =
                nestedType5.found &&
                lineIndex == nestedType5.embeddedHeaderLineIndex;

            if (isNestedHeaderRow)
            {
                // The normalized input contains an active nested Type 5 header
                // such as $50007610 00B6. Rebuild its public value using the
                // current Type 5 Payload Key selection, then encrypt the header
                // with the normal line key.
                payload = nestedType5.publicHeader;
                if (!xploder_psx::encryptCode(
                        payload,
                        options.encryptionKey))
                {
                    return false;
                }
            }
            else if (isConfirmedMassWritePayloadKey(
                         effectivePayloadKey))
            {
                if (!xploder_psx::encryptPayloadChunk(
                        payload,
                        effectivePayloadKey))
                {
                    return false;
                }
            }
            else if (!isType6)
            {
                return false;
            }
            // Type 6 routes other than 6/7 are loader copy routes. Descriptor
            // rows are preserved, while a normalized nested Type 5 header is
            // rebuilt above.

            out << "\n$"
                << formatEncrypted(
                       payload,
                       options.groupEncryptedOutput);

            if (options.annotateCodeTypes)
            {
                const int firstByteOffset =
                    i * static_cast<int>(
                            xploder_psx::CodeLength);
                const int bytesUsed = std::min(
                    static_cast<int>(
                        xploder_psx::CodeLength),
                    std::max(
                        0,
                        sourcePayloadSize - firstByteOffset));

                out << "\t// Type "
                    << (isType6 ? '6' : '5')
                    << " encrypted payload | bytes "
                    << hex4(firstByteOffset)
                    << "-"
                    << hex4(firstByteOffset + bytesUsed - 1);

                if (bytesUsed !=
                    static_cast<int>(
                        xploder_psx::CodeLength))
                {
                    out << "; last "
                        << (static_cast<int>(
                                xploder_psx::CodeLength) -
                            bytesUsed)
                        << " byte(s) padding/ignored";
                }

                if (isNestedHeaderRow)
                {
                    out << " | rebuilt nested Type 5 encrypted header"
                        << " | key="
                        << nestedType5.payloadKey
                        << " | public="
                        << xploder_psx::toRawCmpText(
                               nestedType5.publicHeader);
                }
            }
        }

        std::size_t consumedIndex =
            startIndex +
            static_cast<std::size_t>(payloadLineCount);

        if (nestedType5.found)
        {
            const std::size_t firstContinuationLine =
                consumedIndex + 1U;

            for (int i = 0;
                 i < nestedType5.payloadLineCount;
                 ++i)
            {
                const std::size_t lineIndex =
                    firstContinuationLine +
                    static_cast<std::size_t>(i);

                xploder_psx::Code payload;
                int byteCount = 0;
                if (!parsePayloadLineBytes(
                        lines[lineIndex],
                        payload,
                        byteCount))
                {
                    return false;
                }

                if (byteCount == 0)
                    return false;

                if (!xploder_psx::encryptPayloadChunk(
                        payload,
                        nestedType5.payloadKey))
                {
                    return false;
                }

                out << "\n$"
                    << formatEncrypted(
                           payload,
                           options.groupEncryptedOutput);

                if (options.annotateCodeTypes)
                {
                    const int firstByteOffset =
                        i * static_cast<int>(
                                xploder_psx::CodeLength);
                    const int bytesUsed = std::min(
                        static_cast<int>(
                            xploder_psx::CodeLength),
                        std::max(
                            0,
                            nestedType5.basePayloadSize -
                                firstByteOffset));

                    out << "\t// nested Type 5 encrypted payload | bytes "
                        << hex4(firstByteOffset)
                        << "-"
                        << hex4(firstByteOffset + bytesUsed - 1);

                    if (bytesUsed !=
                        static_cast<int>(
                            xploder_psx::CodeLength))
                    {
                        out << "; last "
                            << (static_cast<int>(
                                    xploder_psx::CodeLength) -
                                bytesUsed)
                            << " byte(s) padding/ignored";
                    }
                }
            }

            consumedIndex +=
                static_cast<std::size_t>(
                    nestedType5.payloadLineCount);
        }

        convertedBlock = out.str();
        lastConsumedIndex = consumedIndex;
        return true;
    }

    inline std::string convertLine(const std::string& line, const Options& options)
    {
        if (trim(line).empty())
            return std::string{};

        const std::string t = trim(line);

        if (startsWith(t, "!!") || t[0] == '!' || t[0] == '%' || t[0] == ';' || startsWith(t, "//"))
            return t;

        if (t[0] == '+')
        {
            const std::string name = trim(std::string_view(t).substr(1));
            return name.empty() ? "+" : "+" + name;
        }

        if (t[0] == '$')
        {
            const std::string body = trim(std::string_view(t).substr(1));
            std::string converted;
            return convertCodeBody(body, options, converted) ? "$" + converted : t;
        }

        std::string converted;
        if (convertCodeBody(t, options, converted))
            return "$" + converted;

        return options.prefixPlainNames ? "+" + t : t;
    }

    inline std::string convertText(const std::string& input, const Options& options)
    {
        if (input.empty())
            return {};

        const std::vector<std::string> lines = splitLines(input);
        std::ostringstream out;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
                out << '\n';

            std::string block;
            std::size_t lastConsumed = i;

            if (options.mode == Mode::Decrypt && tryPreserveActiveMassWriteBlock(lines, i, options, block, lastConsumed))
            {
                out << block;
                i = lastConsumed;
                continue;
            }

            if (options.mode == Mode::Decrypt && tryConvertMassWriteBlock(lines, i, options, block, lastConsumed))
            {
                out << block;
                i = lastConsumed;
                continue;
            }

            if (options.mode == Mode::Encrypt && tryEncryptMassWriteBlock(lines, i, options, block, lastConsumed))
            {
                out << block;
                i = lastConsumed;
                continue;
            }

            out << convertLine(lines[i], options);
        }

        return out.str();
    }
}
