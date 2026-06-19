#pragma once

// XploderMemoryCryptEngine.hpp
// Standalone C++17 XplorerPro / Xploder PSX decrypt/encrypt engine.
//
// Supports:
//   - Normal 6-byte XplorerPro / FX code-line decrypt/encrypt, keys 4/5/6/7.
//   - Xploder memory-dump route behavior for decrypting normal lines.
//   - Loader-side Type 5 / Type 6 mass-write header conversion.
//   - Forced Type 5 / Type 6 payload chunk decrypt/encrypt for payload keys 6 and 7.
//
// No file I/O in this engine header.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xploder_psx
{
    inline constexpr std::size_t CodeLength = 6;

    enum class Key : std::uint8_t
    {
        Key4 = 4,
        Key5 = 5,
        Key6 = 6,
        Key7 = 7
    };

    enum class Route : std::uint8_t
    {
        Copy,
        Key4,
        Key5,
        Key6,
        Key7,
        Special32,
        Special33
    };

    struct Code
    {
        std::array<std::uint8_t, CodeLength> b{};

        constexpr std::uint8_t& operator[](std::size_t index) noexcept { return b[index]; }
        constexpr const std::uint8_t& operator[](std::size_t index) const noexcept { return b[index]; }
    };

    struct MassWriteInfo
    {
        int payloadKey = 0;
        int payloadSize = 0;        // value written to the active header
        int sourcePayloadSize = 0;  // bytes supplied by following source rows
        int payloadLineCount = 0;   // ceil(sourcePayloadSize / 6)
        bool isType6 = false;
    };

    inline constexpr std::uint8_t byte(int value) noexcept
    {
        return static_cast<std::uint8_t>(value & 0xFF);
    }

    inline constexpr bool isExplicitKey(Key key) noexcept
    {
        return key == Key::Key4 || key == Key::Key5 || key == Key::Key6 || key == Key::Key7;
    }

    inline constexpr Route routeForMaskedFirstByte(int maskedFirst) noexcept
    {
        switch (maskedFirst)
        {
            case 0x04: case 0x14: case 0x34: case 0x44: case 0x54: case 0x64:
            case 0x74: case 0x84: case 0x94: case 0xB4: case 0xD4: case 0xF4:
                return Route::Key4;

            case 0x05: case 0x15: case 0x35: case 0x45: case 0x55: case 0x65:
            case 0x75: case 0x85: case 0x95: case 0xB5: case 0xD5: case 0xF5:
                return Route::Key5;

            case 0x06: case 0x16: case 0x36: case 0x46: case 0x56: case 0x66:
            case 0x76: case 0x86: case 0x96: case 0xB6: case 0xD6: case 0xF6:
                return Route::Key6;

            case 0x07: case 0x17: case 0x37: case 0x47: case 0x57: case 0x67:
            case 0x77: case 0x87: case 0x97: case 0xB7: case 0xD7: case 0xF7:
                return Route::Key7;

            case 0x32: case 0x82:
                return Route::Special32;

            case 0x33: case 0x83:
                return Route::Special33;

            default:
                return Route::Copy;
        }
    }

    inline constexpr Route routeForCode(const Code& code) noexcept
    {
        return routeForMaskedFirstByte(code[0] & 0xF7);
    }

    // Decrypt one 6-byte XplorerPro / FX line using Xploder memory-dump routing.
    // Returns true when a crypto transform was applied; false means copy/pass-through.
    inline bool decryptCode(Code& code) noexcept
    {
        const Code input = code;
        code = {};

        switch (routeForCode(input))
        {
            case Route::Key4:
                code[0] = byte(input[0] & 0xF0);
                code[1] = byte(input[1] ^ 0x25);
                code[2] = byte(input[2] ^ (((code[1] & 0x11) + 0xFA) & 0xFF));
                code[3] = byte(input[3] ^ (((code[2] & 0x11) + (code[1] ^ 0x12) - 0x40) & 0xFF));
                code[4] = byte(input[4] ^ (((code[3] & 0x11) + (code[2] ^ 0x12) - 0x82 + code[1]) & 0xFF));
                code[5] = byte(input[5] ^ (((code[4] & 0x11) + (code[3] ^ 0x12) - 0xDA + code[2] + code[1]) & 0xFF));
                return true;

            case Route::Key5:
                code[0] = byte(input[0] & 0xF0);
                code[1] = byte(input[1] + 0x57); // W
                code[2] = byte(input[2] + 0x42); // B
                code[3] = byte(input[3] + 0x31); // 1
                code[4] = byte(input[4] + 0x32); // 2
                code[5] = byte(input[5] + 0x33); // 3
                return true;

            case Route::Key6:
                code[0] = byte(input[0] & 0xF0);
                code[1] = byte((input[1] + 0xAB) ^ 0x01);
                code[2] = byte((input[2] + 0xAB) ^ 0x02);
                code[3] = byte((input[3] + 0xAB) ^ 0x03);
                code[4] = byte((input[4] + 0xAB) ^ 0x04);
                code[5] = byte((input[5] + 0xAB) ^ 0x05);
                return true;

            case Route::Key7:
                code[0] = byte(input[0] & 0xF0);
                code[5] = byte(input[5] - 0x35);
                code[4] = byte(input[4] + (code[5] & 0x73) - 0x35);
                code[3] = byte(input[3] + (code[4] & 0x73) - (code[5] ^ 0x90) + 0x5A);
                code[2] = byte(input[2] + (code[3] & 0x73) - (code[4] ^ 0x90) + 0x16 + code[5]);
                code[1] = byte(input[1] + (code[2] & 0x73) - (code[3] ^ 0x90) + 0xF5 + code[4] + code[5]);
                return true;

            case Route::Special32:
                code[0] = 0x00;
                code[1] = byte(input[1] + 0x43); // C
                code[2] = byte(input[2] + 0x44); // D
                code[3] = input[3];
                code[4] = input[4];
                code[5] = input[5];
                return true;

            case Route::Special33:
                code[0] = 0x00;
                code[1] = byte(~input[1]);
                code[2] = byte(~input[2]);
                code[3] = byte(~input[3]);
                code[4] = input[4];
                code[5] = input[5];
                return true;

            case Route::Copy:
            default:
                code = input;
                return false;
        }
    }

    // Encrypt one normal RAW/CMP 6-byte line into XplorerPro / FX form.
    // This writes the encrypted first byte as raw top nibble OR selected key.
    inline bool encryptCode(Code& code, Key key) noexcept
    {
        if (!isExplicitKey(key))
            return false;

        const Code input = code;
        code = {};
        code[0] = byte((input[0] & 0xF0) | static_cast<std::uint8_t>(key));

        switch (key)
        {
            case Key::Key4:
                code[1] = byte(input[1] ^ 0x25);
                code[2] = byte(input[2] ^ (((input[1] & 0x11) + 0xFA) & 0xFF));
                code[3] = byte(input[3] ^ (((input[2] & 0x11) + (input[1] ^ 0x12) - 0x40) & 0xFF));
                code[4] = byte(input[4] ^ (((input[3] & 0x11) + (input[2] ^ 0x12) - 0x82 + input[1]) & 0xFF));
                code[5] = byte(input[5] ^ (((input[4] & 0x11) + (input[3] ^ 0x12) - 0xDA + input[2] + input[1]) & 0xFF));
                return true;

            case Key::Key5:
                code[1] = byte(input[1] - 0x57);
                code[2] = byte(input[2] - 0x42);
                code[3] = byte(input[3] - 0x31);
                code[4] = byte(input[4] - 0x32);
                code[5] = byte(input[5] - 0x33);
                return true;

            case Key::Key6:
                code[1] = byte((input[1] ^ 0x01) - 0xAB);
                code[2] = byte((input[2] ^ 0x02) - 0xAB);
                code[3] = byte((input[3] ^ 0x03) - 0xAB);
                code[4] = byte((input[4] ^ 0x04) - 0xAB);
                code[5] = byte((input[5] ^ 0x05) - 0xAB);
                return true;

            case Key::Key7:
                code[1] = byte(input[1] - ((input[2] & 0x73) - (input[3] ^ 0x90) + 0xF5 + input[4] + input[5]));
                code[2] = byte(input[2] - ((input[3] & 0x73) - (input[4] ^ 0x90) + 0x16 + input[5]));
                code[3] = byte(input[3] - ((input[4] & 0x73) - (input[5] ^ 0x90) + 0x5A));
                code[4] = byte(input[4] - ((input[5] & 0x73) - 0x35));
                code[5] = byte(input[5] + 0x35);
                return true;
        }

        return false;
    }

    // Decrypt one forced Type 5/6 payload chunk. These are not normal line keys.
    inline bool decryptPayloadChunk(Code& code, int payloadKey) noexcept
    {
        const Code input = code;
        code = {};

        switch (payloadKey)
        {
            case 6:
                // Forced payload-key 6 route, output in active-slot memory order.
                code[0] = byte(input[3] ^ input[1]);
                code[1] = byte(input[4] - 0x1B);
                code[2] = byte(input[0] - 0x34);
                code[3] = byte(~input[1]);
                code[4] = byte(input[2] - input[0]);
                code[5] = byte(input[5] - 0x55);
                return true;

            case 7:
                // Forced payload-key 7 route: reverse byte order and subtract 0xAB.
                code[5] = byte(input[0] - 0xAB);
                code[4] = byte(input[1] - 0xAB);
                code[3] = byte(input[2] - 0xAB);
                code[2] = byte(input[3] - 0xAB);
                code[1] = byte(input[4] - 0xAB);
                code[0] = byte(input[5] - 0xAB);
                return true;

            default:
                code = input;
                return false;
        }
    }

    // Encrypt one active-slot Type 5/6 payload chunk back to encrypted payload form.
    inline bool encryptPayloadChunk(Code& code, int payloadKey) noexcept
    {
        const Code input = code;
        code = {};

        switch (payloadKey)
        {
            case 6:
                code[0] = byte(input[2] + 0x34);
                code[1] = byte(~input[3]);
                code[2] = byte(input[4] + code[0]);
                code[3] = byte(input[0] ^ code[1]);
                code[4] = byte(input[1] + 0x1B);
                code[5] = byte(input[5] + 0x55);
                return true;

            case 7:
                code[0] = byte(input[5] + 0xAB);
                code[1] = byte(input[4] + 0xAB);
                code[2] = byte(input[3] + 0xAB);
                code[3] = byte(input[2] + 0xAB);
                code[4] = byte(input[1] + 0xAB);
                code[5] = byte(input[0] + 0xAB);
                return true;

            default:
                code = input;
                return false;
        }
    }

    inline bool tryGetMassWriteInfoFromPublicHeader(const Code& publicHeader, MassWriteInfo& info) noexcept
    {
        info = {};

        const int codeType = publicHeader[0] & 0xF0;
        if (codeType != 0x50 && codeType != 0x60)
            return false;

        info.isType6 = (codeType == 0x60);

        if (info.isType6)
        {
            info.payloadKey = publicHeader[3] & 0x0F;
            const int baseSize = (static_cast<int>(publicHeader[4]) << 8) | publicHeader[5];
            info.payloadSize = baseSize + 0x12;

            // Type 6 source rows include the fixed 0x12-byte descriptor plus
            // the public base-size bytes, so the full active size is consumed.
            info.sourcePayloadSize = info.payloadSize;
        }
        else
        {
            info.payloadKey = publicHeader[4] >> 4;
            const int baseSize = ((publicHeader[4] & 0x0F) << 8) | publicHeader[5];
            info.payloadSize = baseSize + 0x06;

            // Type 5 adds 0x06 to the active header value, but those six bytes
            // are not another following payload row. Only the public base-size
            // bytes are supplied after the header.
            info.sourcePayloadSize = baseSize;
        }

        if (info.payloadSize <= 0 || info.sourcePayloadSize <= 0)
            return false;

        info.payloadLineCount = (info.sourcePayloadSize + static_cast<int>(CodeLength) - 1) / static_cast<int>(CodeLength);
        return true;
    }

    inline bool decryptMassWriteHeaderToActive(Code& header, MassWriteInfo& info) noexcept
    {
        const Code original = header;
        const bool decryptApplied = decryptCode(header);

        // Copy/pass-through lines like $52...... should not be treated as mass-write headers.
        // Raw public headers with low nibble 0 are allowed.
        if (!decryptApplied && (original[0] & 0x0F) != 0)
            return false;

        if (!tryGetMassWriteInfoFromPublicHeader(header, info))
            return false;

        header[4] = byte(info.payloadSize >> 8);
        header[5] = byte(info.payloadSize);
        return true;
    }

    inline bool encryptMassWriteHeaderFromActive(Code& activeHeader, Key normalLineKey, int payloadKey, MassWriteInfo* infoOut = nullptr) noexcept
    {
        if (payloadKey < 0 || payloadKey > 0x0F)
            return false;

        const int codeType = activeHeader[0] & 0xF0;
        if (codeType != 0x50 && codeType != 0x60)
            return false;

        const bool isType6 = (codeType == 0x60);
        const int payloadSize = (static_cast<int>(activeHeader[4]) << 8) | activeHeader[5];
        const int adjust = isType6 ? 0x12 : 0x06;
        const int baseSize = payloadSize - adjust;

        if (payloadSize <= 0 || baseSize < 0 || baseSize > 0x0FFF)
            return false;

        Code publicHeader = activeHeader;
        if (isType6)
        {
            publicHeader[3] = byte((publicHeader[3] & 0xF0) | (payloadKey & 0x0F));
            publicHeader[4] = byte(baseSize >> 8);
            publicHeader[5] = byte(baseSize);
        }
        else
        {
            publicHeader[4] = byte(((payloadKey & 0x0F) << 4) | ((baseSize >> 8) & 0x0F));
            publicHeader[5] = byte(baseSize);
        }

        if (infoOut != nullptr)
        {
            infoOut->payloadKey = payloadKey;
            infoOut->payloadSize = payloadSize;
            infoOut->sourcePayloadSize = isType6 ? payloadSize : baseSize;
            infoOut->payloadLineCount =
                (infoOut->sourcePayloadSize + static_cast<int>(CodeLength) - 1) /
                static_cast<int>(CodeLength);
            infoOut->isType6 = isType6;
        }

        activeHeader = publicHeader;
        return encryptCode(activeHeader, normalLineKey);
    }

    inline bool decryptMassWriteBlock(const std::vector<Code>& input, std::vector<Code>& output, MassWriteInfo& info)
    {
        output.clear();
        info = {};

        if (input.empty())
            return false;

        Code header = input[0];
        if (!decryptMassWriteHeaderToActive(header, info))
            return false;

        if (input.size() < static_cast<std::size_t>(1 + info.payloadLineCount))
            return false;

        output.reserve(1 + static_cast<std::size_t>(info.payloadLineCount));
        output.push_back(header);

        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            Code payload = input[static_cast<std::size_t>(1 + i)];
            decryptPayloadChunk(payload, info.payloadKey);
            output.push_back(payload);
        }

        return true;
    }

    inline bool encryptMassWriteBlockFromActive(const std::vector<Code>& input, Key normalLineKey, int payloadKey, std::vector<Code>& output, MassWriteInfo& info)
    {
        output.clear();
        info = {};

        if (input.empty())
            return false;

        Code header = input[0];
        const int codeType = header[0] & 0xF0;
        if (codeType != 0x50 && codeType != 0x60)
            return false;

        const bool isType6 = codeType == 0x60;
        const int payloadSize = (static_cast<int>(header[4]) << 8) | header[5];
        const int sourcePayloadSize = isType6 ? payloadSize : payloadSize - 0x06;
        const int payloadLineCount =
            (sourcePayloadSize + static_cast<int>(CodeLength) - 1) /
            static_cast<int>(CodeLength);

        if (payloadSize <= 0 || sourcePayloadSize <= 0 ||
            input.size() < static_cast<std::size_t>(1 + payloadLineCount))
        {
            return false;
        }

        if (!encryptMassWriteHeaderFromActive(header, normalLineKey, payloadKey, &info))
            return false;

        output.reserve(1 + static_cast<std::size_t>(payloadLineCount));
        output.push_back(header);

        for (int i = 0; i < payloadLineCount; ++i)
        {
            Code payload = input[static_cast<std::size_t>(1 + i)];
            encryptPayloadChunk(payload, payloadKey);
            output.push_back(payload);
        }

        return true;
    }

    inline int hexValue(char c) noexcept
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    inline bool codeFromHex(std::string_view text, Code& out) noexcept
    {
        out = {};
        char hex[CodeLength * 2]{};
        std::size_t count = 0;

        for (char c : text)
        {
            const int v = hexValue(c);
            if (v >= 0)
            {
                if (count >= CodeLength * 2)
                    break;
                hex[count++] = c;
            }
        }

        if (count != CodeLength * 2)
            return false;

        for (std::size_t i = 0; i < CodeLength; ++i)
        {
            const int hi = hexValue(hex[i * 2]);
            const int lo = hexValue(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = byte((hi << 4) | lo);
        }

        return true;
    }

    inline std::string toCompactHex(const Code& code)
    {
        static constexpr char lut[] = "0123456789ABCDEF";
        std::string s;
        s.resize(CodeLength * 2);
        for (std::size_t i = 0; i < CodeLength; ++i)
        {
            s[i * 2] = lut[code[i] >> 4];
            s[i * 2 + 1] = lut[code[i] & 0x0F];
        }
        return s;
    }

    inline std::string toRawCmpText(const Code& code)
    {
        const std::string compact = toCompactHex(code);
        return compact.substr(0, 8) + " " + compact.substr(8, 4);
    }

    inline std::string toGroupedEncryptedText(const Code& code)
    {
        const std::string compact = toCompactHex(code);
        return compact.substr(0, 4) + " " + compact.substr(4, 4) + " " + compact.substr(8, 4);
    }
}
