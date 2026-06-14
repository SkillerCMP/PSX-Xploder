#pragma once

// XploderCmpConverter.hpp
// Full CMP-style text converter using XploderMemoryCryptEngine.hpp.
//
// Decrypt mode:
//   XplorerPro / FX encrypted text -> active/runtime CMP-style RAW text.
//   Type 5/6 mass-write blocks are converted to the active header + payload bytes.
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

        // Keep sane bounds so normal accidental $50 lines do not swallow the whole file.
        if (info.payloadSize <= 0 || info.payloadSize > 0x10000 || info.payloadLineCount <= 0)
            return false;

        if (startIndex + static_cast<std::size_t>(info.payloadLineCount) >= lines.size())
            return false;

        std::ostringstream out;
        out << "$" << xploder_psx::toRawCmpText(header);
        if (options.annotateCodeTypes)
        {
            out << "\t// Type " << (info.isType6 ? '6' : '5')
                << " mass-write header | payloadKey=" << info.payloadKey
                << " payloadBytes=0x" << hex4(info.payloadSize)
                << " payloadLines=" << info.payloadLineCount;
        }

        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            const std::size_t lineIndex = startIndex + 1U + static_cast<std::size_t>(i);
            std::string payloadBody;
            if (!tryGetCodeBody(lines[lineIndex], payloadBody))
                return false;

            xploder_psx::Code payload;
            if (!parseCodeBody(payloadBody, Mode::Decrypt, payload))
                return false;

            (void)xploder_psx::decryptPayloadChunk(payload, info.payloadKey);

            out << "\n$" << xploder_psx::toRawCmpText(payload);
            if (options.annotateCodeTypes)
            {
                const int firstByteOffset = i * static_cast<int>(xploder_psx::CodeLength);
                const int bytesUsed = std::min(static_cast<int>(xploder_psx::CodeLength), std::max(0, info.payloadSize - firstByteOffset));
                out << "\t// Type " << (info.isType6 ? '6' : '5') << " payload data | bytes "
                    << hex4(firstByteOffset) << "-" << hex4(firstByteOffset + bytesUsed - 1);
                if (bytesUsed != static_cast<int>(xploder_psx::CodeLength))
                    out << "; last " << (static_cast<int>(xploder_psx::CodeLength) - bytesUsed) << " byte(s) padding/ignored";
            }
        }

        convertedBlock = out.str();
        lastConsumedIndex = startIndex + static_cast<std::size_t>(info.payloadLineCount);
        return true;
    }

    inline bool tryEncryptMassWriteBlock(const std::vector<std::string>& lines, std::size_t startIndex, const Options& options, std::string& convertedBlock, std::size_t& lastConsumedIndex)
    {
        convertedBlock.clear();
        lastConsumedIndex = startIndex;

        if (options.massWritePayloadKey != 6 && options.massWritePayloadKey != 7)
            return false;

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
        const int payloadSize = (static_cast<int>(activeHeader[4]) << 8) | activeHeader[5];
        if (payloadSize <= 0 || payloadSize > 0x10000)
            return false;

        const int payloadLineCount = (payloadSize + static_cast<int>(xploder_psx::CodeLength) - 1) / static_cast<int>(xploder_psx::CodeLength);
        if (startIndex + static_cast<std::size_t>(payloadLineCount) >= lines.size())
            return false;

        std::vector<xploder_psx::Code> payloadRows;
        payloadRows.reserve(static_cast<std::size_t>(payloadLineCount));

        for (int i = 0; i < payloadLineCount; ++i)
        {
            const std::size_t lineIndex = startIndex + 1U + static_cast<std::size_t>(i);
            xploder_psx::Code payload;
            int byteCount = 0;
            if (!parsePayloadLineBytes(lines[lineIndex], payload, byteCount))
                return false;
            if (byteCount == 0)
                return false;
            payloadRows.push_back(payload);
        }

        xploder_psx::MassWriteInfo info;
        xploder_psx::Code encryptedHeader = activeHeader;
        if (!xploder_psx::encryptMassWriteHeaderFromActive(encryptedHeader, options.encryptionKey, options.massWritePayloadKey, &info))
            return false;

        std::ostringstream out;
        out << "$" << formatEncrypted(encryptedHeader, options.groupEncryptedOutput);
        if (options.annotateCodeTypes)
        {
            xploder_psx::Code publicHeader = activeHeader;
            const int adjust = isType6 ? 0x12 : 0x06;
            const int baseSize = payloadSize - adjust;
            if (isType6)
            {
                publicHeader[3] = xploder_psx::byte((publicHeader[3] & 0xF0) | (options.massWritePayloadKey & 0x0F));
                publicHeader[4] = xploder_psx::byte(baseSize >> 8);
                publicHeader[5] = xploder_psx::byte(baseSize);
            }
            else
            {
                publicHeader[4] = xploder_psx::byte(((options.massWritePayloadKey & 0x0F) << 4) | ((baseSize >> 8) & 0x0F));
                publicHeader[5] = xploder_psx::byte(baseSize);
            }
            out << "\t// Type " << (isType6 ? '6' : '5')
                << " mass-write encrypted header | payloadKey=" << options.massWritePayloadKey
                << " payloadBytes=0x" << hex4(payloadSize)
                << " publicValue=" << xploder_psx::toCompactHex(publicHeader).substr(8, 4);
        }

        for (int i = 0; i < payloadLineCount; ++i)
        {
            xploder_psx::Code payload = payloadRows[static_cast<std::size_t>(i)];
            if (!xploder_psx::encryptPayloadChunk(payload, options.massWritePayloadKey))
                return false;

            out << "\n$" << formatEncrypted(payload, options.groupEncryptedOutput);
            if (options.annotateCodeTypes)
            {
                const int firstByteOffset = i * static_cast<int>(xploder_psx::CodeLength);
                const int bytesUsed = std::min(static_cast<int>(xploder_psx::CodeLength), std::max(0, payloadSize - firstByteOffset));
                out << "\t// Type " << (isType6 ? '6' : '5') << " encrypted payload | bytes "
                    << hex4(firstByteOffset) << "-" << hex4(firstByteOffset + bytesUsed - 1);
                if (bytesUsed != static_cast<int>(xploder_psx::CodeLength))
                    out << "; last " << (static_cast<int>(xploder_psx::CodeLength) - bytesUsed) << " byte(s) padding/ignored";
            }
        }

        convertedBlock = out.str();
        lastConsumedIndex = startIndex + static_cast<std::size_t>(payloadLineCount);
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
