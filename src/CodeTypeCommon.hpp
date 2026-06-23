#pragma once

// Shared semantic model and text helpers for window-to-window PSX code conversion.
// Folder batch decryption deliberately continues to use XploderCmpConverter.hpp.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace psx_code_types
{
    enum class Family
    {
        GameSharkActionReplay,
        XploderEncrypted,
        XploderRaw,
        DuckStation,
        Caetla
    };

    enum class OperationKind
    {
        Text,
        Write8,
        Write16,
        Write32,
        BitSet8,
        BitSet16,
        BitSet32,
        BitClear8,
        BitClear16,
        BitClear32,
        CompareEqual8,
        CompareNotEqual8,
        CompareLess8,
        CompareGreater8,
        CompareEqual16,
        CompareNotEqual16,
        CompareLess16,
        CompareGreater16,
        CompareEqual32,
        CompareNotEqual32,
        CompareLess32,
        CompareGreater32,
        BlockCompareEqual16,
        BlockEnd,
        Increment8,
        Increment16,
        Increment32,
        Decrement8,
        Decrement16,
        Decrement32,
        SerialRepeater,
        CopyMemory,
        Joker16,
        CodesOn,
        CodesOff,
        ConditionalWrite16,
        ConditionalWrite16Restore,
        ConditionalWrite8Restore,
        Scratchpad16,
        Scratchpad32,
        XploderMassWrite,
        XploderMegaCode,
        CaetlaIndirectWrite,
        DeviceSpecific
    };

    struct Operation
    {
        OperationKind kind = OperationKind::Text;
        Family sourceFamily = Family::GameSharkActionReplay;
        std::uint32_t address = 0;
        std::uint32_t secondAddress = 0;
        std::uint32_t count = 0;
        std::uint32_t addressStep = 0;
        std::uint32_t valueStep = 0;
        int widthBits = 0;
        bool defaultOff = false;
        bool restoreOnDisable = false;
        bool valueDecreases = false;
        bool addressDecreases = false;
        std::string value;
        std::string compareValue;
        std::vector<std::uint8_t> payload;
        std::vector<std::string> sourceLines;
        std::string text;
        std::string suffix;
        std::string detail;
    };

    struct ParsedCodeLine
    {
        std::string addressText;
        std::string valueText;
        std::string suffix;
        std::uint32_t address = 0;
    };

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

    inline std::string upper(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return text;
    }

    inline bool isHex(char c) noexcept
    {
        return (c >= '0' && c <= '9') ||
               (c >= 'A' && c <= 'F') ||
               (c >= 'a' && c <= 'f');
    }

    inline bool isHexOrWildcard(char c) noexcept
    {
        return isHex(c) || c == '?';
    }

    inline int hexValue(char c) noexcept
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
    }

    inline bool hasWildcard(std::string_view value) noexcept
    {
        return value.find('?') != std::string_view::npos;
    }

    inline bool parseHex(std::string_view text, std::uint32_t& value) noexcept
    {
        if (text.empty() || text.size() > 8 || hasWildcard(text))
            return false;
        std::uint32_t result = 0;
        for (char c : text)
        {
            const int nibble = hexValue(c);
            if (nibble < 0)
                return false;
            result = (result << 4) | static_cast<std::uint32_t>(nibble);
        }
        value = result;
        return true;
    }

    inline std::string hex(std::uint32_t value, int digits)
    {
        std::ostringstream out;
        out << std::uppercase << std::hex << std::setw(digits) << std::setfill('0') << value;
        std::string result = out.str();
        if (static_cast<int>(result.size()) > digits)
            result = result.substr(result.size() - static_cast<std::size_t>(digits));
        return result;
    }

    inline std::vector<std::string> splitLines(const std::string& input)
    {
        std::vector<std::string> lines;
        std::string line;
        for (char c : input)
        {
            if (c == '\r')
                continue;
            if (c == '\n')
            {
                lines.push_back(line);
                line.clear();
            }
            else
            {
                line.push_back(c);
            }
        }
        lines.push_back(line);
        return lines;
    }

    inline std::string joinLines(const std::vector<std::string>& lines)
    {
        std::ostringstream out;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i != 0)
                out << '\n';
            out << lines[i];
        }
        return out.str();
    }

    inline bool startsWithIgnoreCase(std::string_view text, std::string_view prefix) noexcept
    {
        if (text.size() < prefix.size())
            return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            if (std::toupper(static_cast<unsigned char>(text[i])) !=
                std::toupper(static_cast<unsigned char>(prefix[i])))
                return false;
        }
        return true;
    }

    inline bool isTextLikeLine(std::string_view line)
    {
        const std::string t = trim(line);
        if (t.empty())
            return true;
        const char c = t[0];
        return c == '+' || c == '%' || c == '!' || c == '^' || c == ';' ||
               startsWithIgnoreCase(t, "//");
    }

    // Accept standard spaced rows, compact rows, and 4-4 grouped addresses.
    // Address must contain exactly 8 hexadecimal digits. Value may be 2, 4, or
    // 8 hexadecimal/wildcard characters.
    inline bool parseCodeLine(std::string_view input, ParsedCodeLine& parsed)
    {
        parsed = {};
        std::string text = trim(input);
        if (text.empty())
            return false;
        if (text[0] == '$')
            text = trim(std::string_view(text).substr(1));
        if (text.empty())
            return false;

        std::size_t pos = 0;
        std::string address;
        while (pos < text.size() && address.size() < 8)
        {
            const char c = text[pos];
            if (isHex(c))
            {
                address.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                ++pos;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c)) || c == '-')
            {
                ++pos;
                continue;
            }
            return false;
        }
        if (address.size() != 8)
            return false;

        while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == '-'))
            ++pos;

        std::string value;
        while (pos < text.size() && value.size() < 8 && isHexOrWildcard(text[pos]))
        {
            value.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(text[pos]))));
            ++pos;
        }
        if (value.size() != 2 && value.size() != 4 && value.size() != 8)
            return false;

        if (pos < text.size() && isHexOrWildcard(text[pos]))
            return false;

        parsed.addressText = address;
        parsed.valueText = value;
        parsed.suffix = trim(std::string_view(text).substr(pos));
        return parseHex(address, parsed.address);
    }

    inline std::string formatCode(std::string_view address, std::string_view value, std::string_view suffix = {})
    {
        std::string result = "$" + upper(std::string(address)) + " " + upper(std::string(value));
        if (!suffix.empty())
            result += "\t" + trim(suffix);
        return result;
    }

    inline std::string formatCode(std::uint32_t address, std::string_view value, std::string_view suffix = {})
    {
        return formatCode(hex(address, 8), value, suffix);
    }

    inline std::string normalizeValue(std::string value, int digits)
    {
        value = upper(trim(value));
        if (static_cast<int>(value.size()) < digits)
            value.insert(value.begin(), static_cast<std::size_t>(digits - static_cast<int>(value.size())), '0');
        if (static_cast<int>(value.size()) > digits)
            value = value.substr(value.size() - static_cast<std::size_t>(digits));
        return value;
    }

    inline std::string lowHalf32(std::string value)
    {
        value = normalizeValue(std::move(value), 8);
        return value.substr(4, 4);
    }

    inline std::string highHalf32(std::string value)
    {
        value = normalizeValue(std::move(value), 8);
        return value.substr(0, 4);
    }

    inline std::string byteValue(std::string value)
    {
        value = normalizeValue(std::move(value), 2);
        return value;
    }

    inline std::string wordValue(std::string value)
    {
        return normalizeValue(std::move(value), 4);
    }

    inline std::string dwordValue(std::string value)
    {
        return normalizeValue(std::move(value), 8);
    }


    inline bool isBasicRepeaterWrite(const Operation& operation) noexcept
    {
        return (operation.kind == OperationKind::Write8 || operation.kind == OperationKind::Write16) &&
               !operation.defaultOff && operation.suffix.empty();
    }

    inline std::vector<Operation> condenseWritesToBasicSerialRepeaters(
        const std::vector<Operation>& operations)
    {
        std::vector<Operation> output;
        output.reserve(operations.size());

        std::size_t index = 0;
        while (index < operations.size())
        {
            const Operation& first = operations[index];
            if (!isBasicRepeaterWrite(first) || index + 2U >= operations.size())
            {
                output.push_back(first);
                ++index;
                continue;
            }

            const int widthBits = first.kind == OperationKind::Write8 ? 8 : 16;
            const std::uint32_t valueMask = widthBits == 8 ? 0xFFU : 0xFFFFU;
            const Operation& second = operations[index + 1U];
            if (!isBasicRepeaterWrite(second) ||
                second.kind != first.kind ||
                second.address <= first.address)
            {
                output.push_back(first);
                ++index;
                continue;
            }

            const std::uint32_t addressStep = second.address - first.address;
            if (addressStep == 0U || addressStep > 0xFFU)
            {
                output.push_back(first);
                ++index;
                continue;
            }

            const bool wildcard = hasWildcard(first.value) || hasWildcard(second.value);
            std::uint32_t startValue = 0;
            std::uint32_t valueStep = 0;
            if (wildcard)
            {
                if (upper(first.value) != upper(second.value))
                {
                    output.push_back(first);
                    ++index;
                    continue;
                }
            }
            else
            {
                std::uint32_t secondValue = 0;
                if (!parseHex(first.value, startValue) || !parseHex(second.value, secondValue))
                {
                    output.push_back(first);
                    ++index;
                    continue;
                }
                valueStep = (secondValue - startValue) & valueMask;
            }

            std::size_t runLength = 2U;
            while (index + runLength < operations.size() && runLength < 0xFFU)
            {
                const Operation& candidate = operations[index + runLength];
                if (!isBasicRepeaterWrite(candidate) || candidate.kind != first.kind)
                    break;

                const std::uint32_t expectedAddress =
                    first.address + addressStep * static_cast<std::uint32_t>(runLength);
                if (candidate.address != expectedAddress)
                    break;

                if (wildcard)
                {
                    if (upper(candidate.value) != upper(first.value))
                        break;
                }
                else
                {
                    std::uint32_t candidateValue = 0;
                    if (!parseHex(candidate.value, candidateValue))
                        break;
                    const std::uint32_t expectedValue =
                        (startValue + valueStep * static_cast<std::uint32_t>(runLength)) & valueMask;
                    if (candidateValue != expectedValue)
                        break;
                }
                ++runLength;
            }

            // A two-write repeater is still two lines, so only condense runs of
            // three or more writes where the result is actually shorter.
            if (runLength < 3U)
            {
                output.push_back(first);
                ++index;
                continue;
            }

            Operation repeater;
            repeater.kind = OperationKind::SerialRepeater;
            repeater.sourceFamily = first.sourceFamily;
            repeater.address = first.address;
            repeater.count = static_cast<std::uint32_t>(runLength);
            repeater.addressStep = addressStep;
            repeater.valueStep = valueStep;
            repeater.widthBits = widthBits;
            repeater.value = first.value;
            for (std::size_t item = 0; item < runLength; ++item)
            {
                repeater.sourceLines.insert(
                    repeater.sourceLines.end(),
                    operations[index + item].sourceLines.begin(),
                    operations[index + item].sourceLines.end());
            }
            output.push_back(std::move(repeater));
            index += runLength;
        }

        return output;
    }

    inline Operation makeText(std::string text, Family sourceFamily = Family::GameSharkActionReplay)
    {
        Operation operation;
        operation.sourceFamily = sourceFamily;
        operation.kind = OperationKind::Text;
        operation.text = std::move(text);
        return operation;
    }

    inline Operation makeDeviceSpecific(
        std::vector<std::string> sourceLines,
        std::string detail,
        Family sourceFamily = Family::GameSharkActionReplay)
    {
        Operation operation;
        operation.sourceFamily = sourceFamily;
        operation.kind = OperationKind::DeviceSpecific;
        operation.sourceLines = std::move(sourceLines);
        operation.detail = std::move(detail);
        return operation;
    }

    inline void appendUnsupported(
        std::vector<std::string>& output,
        const Operation& operation,
        std::string_view destination,
        std::string_view reason = {})
    {
        std::string message = "// Unsupported exact conversion to ";
        message += destination;
        if (!reason.empty())
        {
            message += ": ";
            message += reason;
        }
        else if (!operation.detail.empty())
        {
            message += ": ";
            message += operation.detail;
        }
        output.push_back(message);
        for (const std::string& source : operation.sourceLines)
            output.push_back("// Source: " + trim(source));
    }

    inline std::uint32_t maskedPsxAddress(std::uint32_t codeAddress) noexcept
    {
        return codeAddress & 0x00FFFFFFU;
    }

    inline std::string familyName(Family family)
    {
        switch (family)
        {
            case Family::GameSharkActionReplay: return "GameShark / Action Replay";
            case Family::XploderEncrypted: return "Xploder Encrypted";
            case Family::XploderRaw: return "Xploder RAW";
            case Family::DuckStation: return "DuckStation";
            case Family::Caetla: return "Caetla";
        }
        return "Unknown";
    }
}
