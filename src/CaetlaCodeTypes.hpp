#pragma once

// Caetla 0.34 / .341 parser and emitter.
//
// The native interpreter was mapped directly from CAETLA.BIN. Native mode
// supports 8/16/32-bit writes, D/E conditions, Type B serial sliders,
// bit set/clear rows, bit-test conditions, 8/16/32-bit copy rows, and
// scratchpad halfword writes. Caetla .341 adds native 8/16/32-bit
// increment/decrement rows plus C2 block copy and C3 indirect writes.
// FFFFFFFF 00000001/00000002 selects the persistent GameShark-compatible
// interpreter; 00000000/00000003 returns to the native interpreter.

#include "GameSharkActionReplayCodeTypes.hpp"

namespace psx_code_types
{
    enum class CaetlaInterpreterMode
    {
        Native,
        GameShark
    };

    inline bool updateCaetlaInterpreterMode(
        const ParsedCodeLine& line,
        CaetlaInterpreterMode& mode)
    {
        if (line.addressText != "FFFFFFFF" || hasWildcard(line.valueText))
            return false;

        std::uint32_t selector = 0;
        if (!parseHex(dwordValue(line.valueText), selector) || selector > 3U)
            return false;

        mode = (selector == 1U || selector == 2U)
            ? CaetlaInterpreterMode::GameShark
            : CaetlaInterpreterMode::Native;
        return true;
    }


    inline bool tryParseCaetla341Copy(
        const std::vector<std::string>& lines,
        std::size_t index,
        const ParsedCodeLine& header,
        Operation& operation)
    {
        if (header.addressText.substr(0, 2) != "C2" ||
            header.valueText.size() != 4 || index + 1U >= lines.size() ||
            hasWildcard(header.valueText))
        {
            return false;
        }

        ParsedCodeLine destination;
        if (!parseCodeLine(lines[index + 1U], destination) ||
            destination.addressText.substr(0, 2) != "80" ||
            destination.valueText != "0000")
        {
            return false;
        }

        std::uint32_t byteCount = 0;
        if (!parseHex(header.valueText, byteCount))
            return false;

        if (byteCount == 0U)
        {
            operation = makeDeviceSpecific(
                {lines[index], lines[index + 1U]},
                "Caetla .341 C2 count 0000 is not treated as a zero-length copy and is preserved exactly",
                Family::Caetla);
            return true;
        }

        operation = {};
        operation.sourceFamily = Family::Caetla;
        operation.kind = OperationKind::CopyMemory;
        operation.address = maskedPsxAddress(header.address);
        operation.secondAddress = maskedPsxAddress(destination.address);
        operation.count = byteCount;
        operation.widthBits = 0; // .341 C2 count is already measured in bytes.
        operation.sourceLines = {lines[index], lines[index + 1U]};
        operation.suffix = !header.suffix.empty() ? header.suffix : destination.suffix;
        return true;
    }

    inline bool tryParseCaetla341IndirectWrite(
        const std::vector<std::string>& lines,
        std::size_t index,
        const ParsedCodeLine& header,
        Operation& operation)
    {
        if (header.addressText.substr(0, 2) != "C3" ||
            hasWildcard(header.valueText) || index + 1U >= lines.size())
        {
            return false;
        }

        std::uint32_t widthSelector = 0;
        if (!parseHex(dwordValue(header.valueText), widthSelector) ||
            (widthSelector != 0U && widthSelector != 1U && widthSelector != 3U))
        {
            return false;
        }

        ParsedCodeLine data;
        if (!parseCodeLine(lines[index + 1U], data) ||
            data.addressText.substr(0, 4) != "9100" ||
            data.valueText.size() != 8)
        {
            return false;
        }

        operation = {};
        operation.sourceFamily = Family::Caetla;
        operation.kind = OperationKind::CaetlaIndirectWrite;
        operation.address = maskedPsxAddress(header.address);
        operation.secondAddress = data.address & 0xFFFFU; // Signed/unsigned offset is preserved bit-for-bit.
        operation.widthBits = widthSelector == 0U ? 8 : (widthSelector == 1U ? 16 : 32);
        operation.value = dwordValue(data.valueText);
        operation.sourceLines = {lines[index], lines[index + 1U]};
        operation.suffix = !header.suffix.empty() ? header.suffix : data.suffix;
        return true;
    }

    inline bool tryParseCaetlaNativeSlider(
        const std::vector<std::string>& lines,
        std::size_t index,
        const ParsedCodeLine& header,
        Operation& operation)
    {
        if (header.addressText[0] != 'B' || index + 1U >= lines.size() ||
            hasWildcard(header.valueText))
        {
            return false;
        }

        ParsedCodeLine seed;
        if (!parseCodeLine(lines[index + 1U], seed) || hasWildcard(seed.addressText))
            return false;

        const char seedType = seed.addressText[0];
        int widthBits = 0;
        std::string seedValue;
        if (seedType == '3')
        {
            widthBits = 8;
            seedValue = byteValue(seed.valueText);
        }
        else if (seedType == '8')
        {
            widthBits = 16;
            seedValue = wordValue(seed.valueText);
        }
        else if (seedType == '9' && seed.valueText.size() == 8)
        {
            widthBits = 32;
            seedValue = dwordValue(seed.valueText);
        }
        else
        {
            // The firmware can also repeat C/D/E handlers, but those do not map
            // to the shared write-repeater operation safely.
            return false;
        }

        std::uint32_t valueStep = 0;
        if (!parseHex(dwordValue(header.valueText), valueStep))
            return false;

        operation = {};
        operation.sourceFamily = Family::Caetla;
        operation.kind = OperationKind::SerialRepeater;
        operation.count = (header.address >> 16) & 0x0FFFU;
        operation.addressStep = header.address & 0xFFFFU;
        operation.valueStep = valueStep;
        operation.widthBits = widthBits;
        operation.address = maskedPsxAddress(seed.address);
        operation.value = std::move(seedValue);
        operation.sourceLines = {lines[index], lines[index + 1U]};
        operation.suffix = !header.suffix.empty() ? header.suffix : seed.suffix;
        return operation.count != 0U;
    }

    inline bool tryParseCaetlaGameSharkSlider(
        const std::vector<std::string>& lines,
        std::size_t index,
        const ParsedCodeLine& header,
        Operation& operation)
    {
        if (header.addressText.substr(0, 2) != "50" ||
            header.valueText.size() != 4 || index + 1U >= lines.size())
        {
            return false;
        }

        ParsedCodeLine seed;
        if (!parseCodeLine(lines[index + 1U], seed))
            return false;

        const std::string seedPrefix = seed.addressText.substr(0, 2);
        if (seedPrefix != "30" && seedPrefix != "80")
            return false;

        operation = {};
        operation.sourceFamily = Family::Caetla;
        operation.kind = OperationKind::SerialRepeater;
        operation.count = (header.address >> 8) & 0xFFU;
        operation.addressStep = header.address & 0xFFU;
        operation.valueStep = static_cast<std::uint32_t>(
            std::stoul(header.valueText, nullptr, 16));
        operation.widthBits = seedPrefix == "30" ? 8 : 16;
        operation.address = maskedPsxAddress(seed.address);
        operation.value = operation.widthBits == 8
            ? byteValue(seed.valueText)
            : wordValue(seed.valueText);
        operation.sourceLines = {lines[index], lines[index + 1U]};
        operation.suffix = !header.suffix.empty() ? header.suffix : seed.suffix;
        return operation.count != 0U;
    }

    inline std::vector<Operation> parseCaetla(const std::string& input)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<Operation> output;
        CaetlaInterpreterMode mode = CaetlaInterpreterMode::Native;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            ParsedCodeLine line;
            if (!parseCodeLine(lines[i], line))
            {
                output.push_back(makeText(lines[i], Family::Caetla));
                continue;
            }

            if (updateCaetlaInterpreterMode(line, mode))
                continue;

            Operation operation;
            if (tryParseCaetla341Copy(lines, i, line, operation))
            {
                output.push_back(std::move(operation));
                ++i;
                continue;
            }
            if (tryParseCaetla341IndirectWrite(lines, i, line, operation))
            {
                output.push_back(std::move(operation));
                ++i;
                continue;
            }
            if (mode == CaetlaInterpreterMode::Native &&
                tryParseCaetlaNativeSlider(lines, i, line, operation))
            {
                output.push_back(std::move(operation));
                ++i;
                continue;
            }
            if (mode == CaetlaInterpreterMode::Native &&
                line.addressText[0] == 'B' && i + 1U < lines.size())
            {
                ParsedCodeLine seed;
                if (parseCodeLine(lines[i + 1U], seed))
                {
                    output.push_back(makeDeviceSpecific(
                        {lines[i], lines[i + 1U]},
                        "Caetla native Type B uses a non-write seed and is preserved exactly",
                        Family::Caetla));
                    ++i;
                    continue;
                }
            }
            if (mode == CaetlaInterpreterMode::GameShark &&
                tryParseCaetlaGameSharkSlider(lines, i, line, operation))
            {
                output.push_back(std::move(operation));
                ++i;
                continue;
            }

            const std::string prefix2 = line.addressText.substr(0, 2);
            const char prefix1 = line.addressText[0];

            operation = {};
            operation.sourceFamily = Family::Caetla;
            operation.address = maskedPsxAddress(line.address);
            operation.value = line.valueText;
            operation.suffix = line.suffix;
            operation.sourceLines = {lines[i]};

            if (prefix2 == "1F" && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::Scratchpad8;
                operation.address = line.address;
                operation.value = byteValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "10")
            {
                operation.kind = OperationKind::Increment8;
                operation.value = byteValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "11")
            {
                operation.kind = OperationKind::Increment16;
                operation.value = wordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "12" && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::Increment32;
                operation.value = dwordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "20")
            {
                operation.kind = OperationKind::Decrement8;
                operation.value = byteValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "21")
            {
                operation.kind = OperationKind::Decrement16;
                operation.value = wordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "22" && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::Decrement32;
                operation.value = dwordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::GameShark && prefix2 == "10")
            {
                operation.kind = OperationKind::Increment16;
                operation.value = wordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::GameShark && prefix2 == "11")
            {
                operation.kind = OperationKind::Decrement16;
                operation.value = wordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::GameShark && prefix2 == "20")
            {
                operation.kind = OperationKind::Increment8;
                operation.value = byteValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::GameShark && prefix2 == "21")
            {
                operation.kind = OperationKind::Decrement8;
                operation.value = byteValue(line.valueText);
            }
            else if (prefix1 == '3' && line.valueText.size() <= 4)
            {
                operation.kind = OperationKind::Write8;
                operation.value = byteValue(line.valueText);
            }
            else if (prefix1 == '8' && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::Write16;
                operation.value = wordValue(line.valueText);
            }
            else if (mode == CaetlaInterpreterMode::Native &&
                     prefix1 == '9' && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::Write32;
                operation.value = dwordValue(line.valueText);
            }
            else if (prefix2 == "E0") { operation.kind = OperationKind::CompareEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E1") { operation.kind = OperationKind::CompareNotEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E2") { operation.kind = OperationKind::CompareLessOrEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E3") { operation.kind = OperationKind::CompareGreater8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "D0") { operation.kind = OperationKind::CompareEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D1") { operation.kind = OperationKind::CompareNotEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D2") { operation.kind = OperationKind::CompareLessOrEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D3") { operation.kind = OperationKind::CompareGreater16; operation.value = wordValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "50") { operation.kind = OperationKind::BitSet8; operation.value = byteValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "51") { operation.kind = OperationKind::BitSet16; operation.value = wordValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "52" && line.valueText.size() == 8) { operation.kind = OperationKind::BitSet32; operation.value = dwordValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "58") { operation.kind = OperationKind::BitClear8; operation.value = byteValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "59") { operation.kind = OperationKind::BitClear16; operation.value = wordValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native && prefix2 == "5A" && line.valueText.size() == 8) { operation.kind = OperationKind::BitClear32; operation.value = dwordValue(line.valueText); }
            else if (mode == CaetlaInterpreterMode::Native &&
                     (prefix2 == "70" || prefix2 == "71" || prefix2 == "72") &&
                     !hasWildcard(line.valueText))
            {
                std::uint32_t destination = 0;
                if (parseHex(dwordValue(line.valueText), destination))
                {
                    operation.kind = OperationKind::CopyMemory;
                    operation.widthBits = prefix2 == "70" ? 8 : (prefix2 == "71" ? 16 : 32);
                    operation.secondAddress = destination;
                    operation.count = 1;
                    operation.value.clear();
                }
                else
                {
                    operation = makeDeviceSpecific(
                        {lines[i]},
                        "Caetla native copy row has an invalid destination address",
                        Family::Caetla);
                }
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix1 == '6')
            {
                operation = makeDeviceSpecific(
                    {lines[i]},
                    "Caetla native bit-test condition is preserved exactly",
                    Family::Caetla);
            }
            else if (mode == CaetlaInterpreterMode::Native && prefix1 == 'C')
            {
                operation = makeDeviceSpecific(
                    {lines[i]},
                    "Caetla native C-type row is preserved exactly",
                    Family::Caetla);
            }
            else
            {
                operation = makeDeviceSpecific(
                    {lines[i]},
                    mode == CaetlaInterpreterMode::Native
                        ? "Caetla 0.34 native code type has no safe semantic mapping"
                        : "Caetla GameShark-mode code type has no safe semantic mapping",
                    Family::Caetla);
            }

            output.push_back(std::move(operation));
        }

        return output;
    }

    inline void appendCaetlaWrite(
        std::vector<std::string>& lines,
        int widthBits,
        std::uint32_t address,
        const std::string& value,
        const std::string& suffix = {})
    {
        if (widthBits == 8)
            lines.push_back(formatCode(0x30000000U | maskedPsxAddress(address), "00" + byteValue(value), suffix));
        else if (widthBits == 16)
            lines.push_back(formatCode(0x80000000U | maskedPsxAddress(address), wordValue(value), suffix));
        else
            lines.push_back(formatCode(0x90000000U | maskedPsxAddress(address), dwordValue(value), suffix));
    }

    inline void appendMassWriteAsCaetla(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        std::size_t offset = 0;
        while (offset + 3U < operation.payload.size())
        {
            const std::uint32_t value =
                static_cast<std::uint32_t>(operation.payload[offset]) |
                (static_cast<std::uint32_t>(operation.payload[offset + 1U]) << 8) |
                (static_cast<std::uint32_t>(operation.payload[offset + 2U]) << 16) |
                (static_cast<std::uint32_t>(operation.payload[offset + 3U]) << 24);
            appendCaetlaWrite(lines, 32,
                operation.address + static_cast<std::uint32_t>(offset),
                hex(value, 8));
            offset += 4U;
        }
        while (offset + 1U < operation.payload.size())
        {
            const std::uint16_t value = static_cast<std::uint16_t>(
                operation.payload[offset] |
                (static_cast<std::uint16_t>(operation.payload[offset + 1U]) << 8));
            appendCaetlaWrite(lines, 16,
                operation.address + static_cast<std::uint32_t>(offset),
                hex(value, 4));
            offset += 2U;
        }
        if (offset < operation.payload.size())
        {
            appendCaetlaWrite(lines, 8,
                operation.address + static_cast<std::uint32_t>(offset),
                hex(operation.payload[offset], 2));
        }
    }

    inline bool canCombineCaetlaWrite16(
        const Operation& lowHalf,
        const Operation& highHalf) noexcept
    {
        return lowHalf.kind == OperationKind::Write16 &&
               highHalf.kind == OperationKind::Write16 &&
               !lowHalf.defaultOff && !highHalf.defaultOff &&
               lowHalf.suffix.empty() && highHalf.suffix.empty() &&
               (lowHalf.address & 1U) == 0U &&
               highHalf.address == lowHalf.address + 2U;
    }

    inline void setCaetlaOutputMode(
        std::vector<std::string>& lines,
        CaetlaInterpreterMode& current,
        CaetlaInterpreterMode required)
    {
        if (current == required)
            return;
        lines.push_back(formatCode(
            "FFFFFFFF",
            required == CaetlaInterpreterMode::GameShark
                ? "00000001"
                : "00000000"));
        current = required;
    }

    inline bool tryAppendCaetlaTypeBSlider(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        if (operation.count == 0U || operation.count > 0x0FFFU ||
            operation.addressStep > 0xFFFFU || operation.addressDecreases ||
            (operation.widthBits != 8 && operation.widthBits != 16 && operation.widthBits != 32))
        {
            return false;
        }

        if (hasWildcard(operation.value) && operation.valueStep != 0U)
            return false;

        const std::uint32_t encodedValueStep = operation.valueDecreases
            ? (0U - operation.valueStep)
            : operation.valueStep;
        const std::uint32_t header = 0xB0000000U |
            ((operation.count & 0x0FFFU) << 16) |
            (operation.addressStep & 0xFFFFU);

        lines.push_back(formatCode(header, hex(encodedValueStep, 8)));
        appendCaetlaWrite(
            lines,
            operation.widthBits,
            operation.address,
            operation.value,
            operation.suffix);
        return true;
    }

    inline bool tryAppendClassicType5RepeaterAsCaetla(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        if (operation.count == 0U || operation.count > 0xFFU ||
            operation.addressStep > 0xFFU || operation.valueStep > 0xFFFFU ||
            operation.addressDecreases || operation.valueDecreases ||
            (operation.widthBits != 8 && operation.widthBits != 16))
        {
            return false;
        }

        if (hasWildcard(operation.value) && operation.valueStep != 0U)
            return false;

        const std::uint32_t header = 0x50000000U |
            ((operation.count & 0xFFU) << 8) |
            (operation.addressStep & 0xFFU);
        lines.push_back(formatCode(header, hex(operation.valueStep, 4)));
        appendCaetlaWrite(
            lines,
            operation.widthBits,
            operation.address,
            operation.value,
            operation.suffix);
        return true;
    }

    inline bool appendExpandedRepeaterAsCaetla(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        const bool wildcardSeed = hasWildcard(operation.value);
        if (wildcardSeed && operation.valueStep != 0U)
        {
            appendUnsupported(lines, operation, "Caetla 0.34",
                "serial repeater contains wildcard data with a non-zero value step");
            return false;
        }

        std::uint32_t numericValue = 0;
        if (!wildcardSeed && !parseHex(operation.value, numericValue))
        {
            appendUnsupported(lines, operation, "Caetla 0.34",
                "serial repeater value is invalid");
            return false;
        }

        const std::uint32_t valueMask = operation.widthBits == 8
            ? 0xFFU
            : (operation.widthBits == 16 ? 0xFFFFU : 0xFFFFFFFFU);
        for (std::uint32_t index = 0; index < operation.count; ++index)
        {
            const std::uint32_t addressDelta = operation.addressStep * index;
            const std::uint32_t target = operation.addressDecreases
                ? operation.address - addressDelta
                : operation.address + addressDelta;
            const std::uint32_t valueDelta = operation.valueStep * index;
            const std::uint32_t currentValue = operation.valueDecreases
                ? (numericValue - valueDelta) & valueMask
                : (numericValue + valueDelta) & valueMask;

            if (wildcardSeed)
            {
                appendCaetlaWrite(lines, operation.widthBits, target, operation.value);
            }
            else
            {
                appendCaetlaWrite(
                    lines,
                    operation.widthBits,
                    target,
                    hex(currentValue, operation.widthBits == 8 ? 2 : (operation.widthBits == 16 ? 4 : 8)));
            }
        }
        return true;
    }

    inline void appendCaetlaNativeCopy(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        std::uint32_t prefix = 0;
        if (operation.widthBits == 8) prefix = 0x70000000U;
        else if (operation.widthBits == 16) prefix = 0x71000000U;
        else if (operation.widthBits == 32) prefix = 0x72000000U;

        if (prefix == 0U || operation.count > 1U)
        {
            appendUnsupported(lines, operation, "Caetla 0.34",
                "native copy rows transfer one byte, halfword, or word per row");
            return;
        }

        lines.push_back(formatCode(
            prefix | maskedPsxAddress(operation.address),
            hex(operation.secondAddress, 8),
            operation.suffix));
    }

    inline bool isCaetla341NativeArithmeticSource(const Operation& operation)
    {
        if (operation.sourceFamily != Family::Caetla || operation.sourceLines.empty())
            return false;

        ParsedCodeLine source;
        if (!parseCodeLine(operation.sourceLines.front(), source))
            return false;

        const std::string prefix = source.addressText.substr(0, 2);
        switch (operation.kind)
        {
            case OperationKind::Increment8: return prefix == "10";
            case OperationKind::Increment16: return prefix == "11";
            case OperationKind::Increment32: return prefix == "12";
            case OperationKind::Decrement8: return prefix == "20";
            case OperationKind::Decrement16: return prefix == "21";
            case OperationKind::Decrement32: return prefix == "22";
            default: return false;
        }
    }

    inline bool isCaetla341CopySource(const Operation& operation)
    {
        if (operation.sourceFamily != Family::Caetla || operation.sourceLines.size() < 2U)
            return false;
        ParsedCodeLine source;
        return parseCodeLine(operation.sourceLines.front(), source) &&
               source.addressText.substr(0, 2) == "C2";
    }

    inline void appendPreservedCaetlaSource(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        for (const std::string& source : operation.sourceLines)
        {
            ParsedCodeLine parsed;
            lines.push_back(parseCodeLine(source, parsed)
                ? formatCode(parsed.addressText, parsed.valueText, parsed.suffix)
                : source);
        }
    }

    inline bool appendCaetla341Copy(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        std::uint64_t byteCount = operation.count;
        if (operation.widthBits == 8 || operation.widthBits == 16 || operation.widthBits == 32)
            byteCount *= static_cast<std::uint64_t>(operation.widthBits / 8);

        if (byteCount == 0U || byteCount > 0xFFFFU)
            return false;

        lines.push_back(formatCode(
            0xC2000000U | maskedPsxAddress(operation.address),
            hex(static_cast<std::uint32_t>(byteCount), 4),
            operation.suffix));
        lines.push_back(formatCode(
            0x80000000U | maskedPsxAddress(operation.secondAddress),
            "0000"));
        return true;
    }

    inline bool appendCaetla341IndirectWrite(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        std::uint32_t selector = 0;
        if (operation.widthBits == 8) selector = 0;
        else if (operation.widthBits == 16) selector = 1;
        else if (operation.widthBits == 32) selector = 3;
        else return false;

        lines.push_back(formatCode(
            0xC3000000U | maskedPsxAddress(operation.address),
            hex(selector, 4),
            operation.suffix));
        lines.push_back(formatCode(
            0x91000000U | (operation.secondAddress & 0xFFFFU),
            dwordValue(operation.value)));
        return true;
    }

    inline std::vector<std::string> emitCaetla(
        const std::vector<Operation>& operations,
        bool combineConsecutive16BitWrites = false,
        bool emitTypeBSliders = false,
        bool use341ExtendedTypes = false,
        bool firmwareAccurate034 = false)
    {
        std::vector<std::string> lines;
        CaetlaInterpreterMode outputMode = CaetlaInterpreterMode::Native;

        for (std::size_t i = 0; i < operations.size(); ++i)
        {
            const Operation& operation = operations[i];

            if (combineConsecutive16BitWrites &&
                i + 1U < operations.size() &&
                canCombineCaetlaWrite16(operation, operations[i + 1U]))
            {
                setCaetlaOutputMode(lines, outputMode, CaetlaInterpreterMode::Native);
                const Operation& highHalf = operations[i + 1U];
                appendCaetlaWrite(
                    lines,
                    32,
                    operation.address,
                    wordValue(highHalf.value) + wordValue(operation.value));
                ++i;
                continue;
            }

            if (operation.kind == OperationKind::Text)
            {
                // Code names and blank separators commonly mark a new cheat
                // entry. Caetla initializes each entry in native mode, so close
                // any GameShark-compatible section before crossing text.
                setCaetlaOutputMode(lines, outputMode, CaetlaInterpreterMode::Native);
                lines.push_back(operation.text);
                continue;
            }

            if (operation.defaultOff)
                lines.push_back("// Note: Xploder default-off state is not represented by Caetla rows.");

            const bool preserveNative341Arithmetic =
                !use341ExtendedTypes && isCaetla341NativeArithmeticSource(operation);
            const bool emitNativeExtendedArithmetic =
                use341ExtendedTypes || !firmwareAccurate034;
            const bool requiresGameSharkMode =
                firmwareAccurate034 && !use341ExtendedTypes &&
                !preserveNative341Arithmetic &&
                (operation.kind == OperationKind::Increment8 ||
                 operation.kind == OperationKind::Increment16 ||
                 operation.kind == OperationKind::Decrement8 ||
                 operation.kind == OperationKind::Decrement16);
            setCaetlaOutputMode(
                lines,
                outputMode,
                requiresGameSharkMode
                    ? CaetlaInterpreterMode::GameShark
                    : CaetlaInterpreterMode::Native);

            switch (operation.kind)
            {
                case OperationKind::Write8: appendCaetlaWrite(lines, 8, operation.address, operation.value, operation.suffix); break;
                case OperationKind::Write16: appendCaetlaWrite(lines, 16, operation.address, operation.value, operation.suffix); break;
                case OperationKind::Write32: appendCaetlaWrite(lines, 32, operation.address, operation.value, operation.suffix); break;
                case OperationKind::BitSet8: lines.push_back(formatCode(0x50000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::BitSet16: lines.push_back(formatCode(0x51000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::BitSet32: lines.push_back(formatCode(0x52000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
                case OperationKind::BitClear8: lines.push_back(formatCode(0x58000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::BitClear16: lines.push_back(formatCode(0x59000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::BitClear32: lines.push_back(formatCode(0x5A000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareEqual8: lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareNotEqual8: lines.push_back(formatCode(0xE1000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareLessOrEqual8: lines.push_back(formatCode(0xE2000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareGreater8: lines.push_back(formatCode(0xE3000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareEqual16: lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareNotEqual16: lines.push_back(formatCode(0xD1000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareLessOrEqual16: lines.push_back(formatCode(0xD2000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareGreater16: lines.push_back(formatCode(0xD3000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Increment8:
                    if (preserveNative341Arithmetic) appendPreservedCaetlaSource(lines, operation);
                    else lines.push_back(formatCode(
                        (emitNativeExtendedArithmetic ? 0x10000000U : 0x20000000U) | maskedPsxAddress(operation.address),
                        "00" + byteValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Decrement8:
                    if (preserveNative341Arithmetic) appendPreservedCaetlaSource(lines, operation);
                    else lines.push_back(formatCode(
                        (emitNativeExtendedArithmetic ? 0x20000000U : 0x21000000U) | maskedPsxAddress(operation.address),
                        "00" + byteValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Increment16:
                    if (preserveNative341Arithmetic) appendPreservedCaetlaSource(lines, operation);
                    else lines.push_back(formatCode(
                        (emitNativeExtendedArithmetic ? 0x11000000U : 0x10000000U) | maskedPsxAddress(operation.address),
                        wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Decrement16:
                    if (preserveNative341Arithmetic) appendPreservedCaetlaSource(lines, operation);
                    else lines.push_back(formatCode(
                        (emitNativeExtendedArithmetic ? 0x21000000U : 0x11000000U) | maskedPsxAddress(operation.address),
                        wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Increment32:
                    if (use341ExtendedTypes)
                        lines.push_back(formatCode(0x12000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix));
                    else if (preserveNative341Arithmetic)
                        appendPreservedCaetlaSource(lines, operation);
                    else
                        appendUnsupported(lines, operation, "Caetla 0.34", "32-bit increment requires Caetla .341 extended types");
                    break;
                case OperationKind::Decrement32:
                    if (use341ExtendedTypes)
                        lines.push_back(formatCode(0x22000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix));
                    else if (preserveNative341Arithmetic)
                        appendPreservedCaetlaSource(lines, operation);
                    else
                        appendUnsupported(lines, operation, "Caetla 0.34", "32-bit decrement requires Caetla .341 extended types");
                    break;
                case OperationKind::SerialRepeater:
                {
                    if (isDuckStationBitRepeater(operation))
                    {
                        appendUnsupported(lines, operation, "Caetla 0.34", "DuckStation bit-set/bit-clear slides do not have a confirmed Caetla equivalent");
                        break;
                    }
                    ParsedCodeLine sourceHeader;
                    const bool nativeTypeBSource =
                        operation.sourceFamily == Family::Caetla &&
                        !operation.sourceLines.empty() &&
                        parseCodeLine(operation.sourceLines.front(), sourceHeader) &&
                        sourceHeader.addressText[0] == 'B';
                    if (emitTypeBSliders || nativeTypeBSource)
                    {
                        if (!tryAppendCaetlaTypeBSlider(lines, operation))
                            appendExpandedRepeaterAsCaetla(lines, operation);
                    }
                    else if (!tryAppendClassicType5RepeaterAsCaetla(lines, operation))
                    {
                        appendExpandedRepeaterAsCaetla(lines, operation);
                    }
                    break;
                }
                case OperationKind::CopyMemory:
                    if (use341ExtendedTypes)
                    {
                        if (!appendCaetla341Copy(lines, operation))
                            appendUnsupported(lines, operation, "Caetla .341", "C2 copy length must be from 1 to 0xFFFF bytes");
                    }
                    else if (isCaetla341CopySource(operation))
                    {
                        appendPreservedCaetlaSource(lines, operation);
                    }
                    else
                    {
                        appendCaetlaNativeCopy(lines, operation);
                    }
                    break;
                case OperationKind::ConditionalWrite16:
                    lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue)));
                    appendCaetlaWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite16Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in Caetla 0.34; active conditional write follows.");
                    lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue)));
                    appendCaetlaWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite8Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in Caetla 0.34; active conditional write follows.");
                    lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.compareValue)));
                    appendCaetlaWrite(lines, 8, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::Scratchpad8:
                    lines.push_back(formatCode(operation.address, "00" + byteValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Scratchpad16:
                    lines.push_back(formatCode(operation.address, wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Scratchpad32:
                    lines.push_back(formatCode(operation.address, lowHalf32(operation.value), operation.suffix));
                    lines.push_back(formatCode(operation.address + 2U, highHalf32(operation.value)));
                    break;
                case OperationKind::XploderMassWrite:
                    appendMassWriteAsCaetla(lines, operation);
                    break;
                case OperationKind::CaetlaIndirectWrite:
                    if (use341ExtendedTypes)
                    {
                        if (!appendCaetla341IndirectWrite(lines, operation))
                            appendUnsupported(lines, operation, "Caetla .341", "indirect write width must be 8, 16, or 32 bits");
                    }
                    else if (operation.sourceFamily == Family::Caetla)
                    {
                        appendPreservedCaetlaSource(lines, operation);
                    }
                    else
                    {
                        appendUnsupported(lines, operation, "Caetla 0.34", "C3 indirect writes require Caetla .341 extended types");
                    }
                    break;
                case OperationKind::DeviceSpecific:
                    if (operation.sourceFamily == Family::Caetla)
                    {
                        appendPreservedCaetlaSource(lines, operation);
                    }
                    else
                    {
                        appendUnsupported(lines, operation, "Caetla 0.34");
                    }
                    break;
                case OperationKind::CompareEqual32:
                case OperationKind::CompareNotEqual32:
                case OperationKind::CompareLess32:
                case OperationKind::CompareGreater32:
                case OperationKind::CompareLess8:
                case OperationKind::CompareLess16:
                case OperationKind::GlobalCompareEqual16:
                case OperationKind::Joker16:
                case OperationKind::GameSharkControlD5:
                case OperationKind::GameSharkControlD6:
                case OperationKind::XploderMegaCode:
                case OperationKind::BlockCompareEqual16:
                case OperationKind::BlockCompareEqual32:
                case OperationKind::BlockCompareLess8:
                case OperationKind::BlockCompareGreater8:
                case OperationKind::BlockCompareLess16:
                case OperationKind::BlockCompareGreater16:
                case OperationKind::BlockButtonsEqual:
                case OperationKind::BlockButtonsNotEqual:
                case OperationKind::BlockEnd:
                case OperationKind::DuckStationRaw:
                    appendUnsupported(lines, operation, "Caetla 0.34",
                        "the selected Caetla output mode has no confirmed native equivalent");
                    break;
                case OperationKind::Text:
                    break;
            }
        }

        // Do not let GameShark compatibility mode leak into codes appended later.
        setCaetlaOutputMode(lines, outputMode, CaetlaInterpreterMode::Native);
        return lines;
    }

    inline std::string normalizeCaetlaText(const std::string& input)
    {
        std::vector<std::string> normalized;
        for (const std::string& source : splitLines(input))
        {
            ParsedCodeLine line;
            if (parseCodeLine(source, line))
                normalized.push_back(formatCode(line.addressText, line.valueText, line.suffix));
            else
                normalized.push_back(source);
        }
        return joinLines(normalized);
    }
}
