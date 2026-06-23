#pragma once

// GameShark / Action Replay PlayStation code parser and emitter.
// PlayStation Action Replay uses the same ordinary code types as GameShark.

#include "CodeTypeCommon.hpp"

namespace psx_code_types
{
    inline std::vector<Operation> parseGameSharkActionReplay(const std::string& input)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<Operation> output;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            ParsedCodeLine line;
            if (!parseCodeLine(lines[i], line))
            {
                output.push_back(makeText(lines[i], Family::GameSharkActionReplay));
                continue;
            }

            const std::string prefix2 = line.addressText.substr(0, 2);
            const char prefix1 = line.addressText[0];

            if (prefix2 == "50" && line.valueText.size() == 4 && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next) &&
                    (next.addressText.substr(0, 2) == "30" || next.addressText.substr(0, 2) == "80"))
                {
                    Operation operation;
                    operation.sourceFamily = Family::GameSharkActionReplay;
                    operation.kind = OperationKind::SerialRepeater;
                    operation.count = static_cast<std::uint32_t>(std::stoul(line.addressText.substr(4, 2), nullptr, 16));
                    operation.addressStep = static_cast<std::uint32_t>(std::stoul(line.addressText.substr(6, 2), nullptr, 16));
                    operation.valueStep = static_cast<std::uint32_t>(std::stoul(line.valueText, nullptr, 16));
                    operation.widthBits = next.addressText.substr(0, 2) == "30" ? 8 : 16;
                    operation.address = maskedPsxAddress(next.address);
                    operation.value = operation.widthBits == 8
                        ? byteValue(next.valueText)
                        : wordValue(next.valueText);
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    operation.suffix = line.suffix;
                    output.push_back(std::move(operation));
                    ++i;
                    continue;
                }
            }

            if (prefix2 == "C2" && line.valueText.size() == 4 && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next) &&
                    next.addressText.substr(0, 2) == "80" && next.valueText == "0000")
                {
                    Operation operation;
                    operation.sourceFamily = Family::GameSharkActionReplay;
                    operation.kind = OperationKind::CopyMemory;
                    operation.address = maskedPsxAddress(line.address);
                    operation.secondAddress = maskedPsxAddress(next.address);
                    operation.count = static_cast<std::uint32_t>(std::stoul(line.valueText, nullptr, 16));
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    output.push_back(std::move(operation));
                    ++i;
                    continue;
                }
            }

            Operation operation;
            operation.sourceFamily = Family::GameSharkActionReplay;
            operation.address = maskedPsxAddress(line.address);
            operation.value = line.valueText;
            operation.suffix = line.suffix;
            operation.sourceLines = {lines[i]};

            if (prefix1 == '3' && line.valueText.size() <= 4)
            {
                operation.kind = OperationKind::Write8;
                operation.value = byteValue(line.valueText);
            }
            else if (prefix1 == '8' && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::Write16;
                operation.value = wordValue(line.valueText);
            }
            else if (prefix2 == "E0") { operation.kind = OperationKind::CompareEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E1") { operation.kind = OperationKind::CompareNotEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E2") { operation.kind = OperationKind::CompareLess8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E3") { operation.kind = OperationKind::CompareGreater8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "D0") { operation.kind = OperationKind::CompareEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D1") { operation.kind = OperationKind::CompareNotEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D2") { operation.kind = OperationKind::CompareLess16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D3") { operation.kind = OperationKind::CompareGreater16; operation.value = wordValue(line.valueText); }
            else if (line.addressText == "D4000000") { operation.kind = OperationKind::Joker16; operation.value = wordValue(line.valueText); }
            else if (line.addressText == "D5000000") { operation.kind = OperationKind::CodesOn; operation.value = wordValue(line.valueText); }
            else if (line.addressText == "D6000000") { operation.kind = OperationKind::CodesOff; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "10") { operation.kind = OperationKind::Increment16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "11") { operation.kind = OperationKind::Decrement16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "20") { operation.kind = OperationKind::Increment8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "21") { operation.kind = OperationKind::Decrement8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "1F" && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::Scratchpad16;
                operation.address = line.address;
                operation.value = wordValue(line.valueText);
            }
            else
            {
                operation = makeDeviceSpecific({lines[i]}, "GameShark / Action Replay code type has no semantic mapping yet", Family::GameSharkActionReplay);
            }
            output.push_back(std::move(operation));
        }

        return output;
    }

    inline void appendGameSharkWrite(
        std::vector<std::string>& lines,
        int widthBits,
        std::uint32_t address,
        const std::string& value,
        const std::string& suffix = {})
    {
        if (widthBits == 8)
            lines.push_back(formatCode(0x30000000U | maskedPsxAddress(address), "00" + byteValue(value), suffix));
        else
            lines.push_back(formatCode(0x80000000U | maskedPsxAddress(address), wordValue(value), suffix));
    }

    inline bool appendExpandedRepeater(
        std::vector<std::string>& lines,
        const Operation& operation,
        std::string_view destination)
    {
        const bool wildcardSeed = hasWildcard(operation.value);
        if (wildcardSeed && operation.valueStep != 0U)
        {
            appendUnsupported(lines, operation, destination, "serial repeater contains wildcard data with a non-zero value step");
            return false;
        }

        std::uint32_t numericValue = 0;
        if (!wildcardSeed && !parseHex(operation.value, numericValue))
        {
            appendUnsupported(lines, operation, destination, "serial repeater value is invalid");
            return false;
        }

        const std::uint32_t valueMask = operation.widthBits == 8
            ? 0xFFU
            : (operation.widthBits == 16 ? 0xFFFFU : 0xFFFFFFFFU);
        for (std::uint32_t i = 0; i < operation.count; ++i)
        {
            const std::uint32_t addressDelta = operation.addressStep * i;
            const std::uint32_t target = operation.addressDecreases
                ? operation.address - addressDelta
                : operation.address + addressDelta;
            const std::uint32_t valueDelta = operation.valueStep * i;
            const std::uint32_t currentValue = operation.valueDecreases
                ? (numericValue - valueDelta) & valueMask
                : (numericValue + valueDelta) & valueMask;
            if (wildcardSeed)
            {
                if (operation.widthBits == 32)
                {
                    appendGameSharkWrite(lines, 16, target, lowHalf32(operation.value));
                    appendGameSharkWrite(lines, 16, target + 2U, highHalf32(operation.value));
                }
                else
                {
                    appendGameSharkWrite(lines, operation.widthBits, target, operation.value);
                }
            }
            else if (operation.widthBits == 32)
            {
                const std::string dword = hex(currentValue, 8);
                appendGameSharkWrite(lines, 16, target, lowHalf32(dword));
                appendGameSharkWrite(lines, 16, target + 2U, highHalf32(dword));
            }
            else
            {
                appendGameSharkWrite(lines, operation.widthBits, target,
                    hex(currentValue, operation.widthBits == 8 ? 2 : 4));
            }
        }
        return true;
    }

    inline void appendMassWriteAsGameShark(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        std::size_t offset = 0;
        while (offset + 1U < operation.payload.size())
        {
            const std::uint16_t value = static_cast<std::uint16_t>(
                operation.payload[offset] |
                (static_cast<std::uint16_t>(operation.payload[offset + 1U]) << 8));
            appendGameSharkWrite(lines, 16, operation.address + static_cast<std::uint32_t>(offset), hex(value, 4));
            offset += 2U;
        }
        if (offset < operation.payload.size())
            appendGameSharkWrite(lines, 8, operation.address + static_cast<std::uint32_t>(offset), hex(operation.payload[offset], 2));
    }

    inline std::vector<std::string> emitGameSharkActionReplay(const std::vector<Operation>& operations)
    {
        std::vector<std::string> lines;
        for (const Operation& operation : operations)
        {
            if (operation.kind == OperationKind::Text)
            {
                lines.push_back(operation.text);
                continue;
            }

            if (operation.defaultOff)
                lines.push_back("// Note: Xploder default-off state is not represented by GameShark / Action Replay.");

            switch (operation.kind)
            {
                case OperationKind::Write8:
                    appendGameSharkWrite(lines, 8, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::Write16:
                    appendGameSharkWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::Write32:
                    appendGameSharkWrite(lines, 16, operation.address, lowHalf32(operation.value), operation.suffix);
                    appendGameSharkWrite(lines, 16, operation.address + 2U, highHalf32(operation.value));
                    break;
                case OperationKind::CompareEqual8: lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareNotEqual8: lines.push_back(formatCode(0xE1000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareLess8: lines.push_back(formatCode(0xE2000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareGreater8: lines.push_back(formatCode(0xE3000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareEqual16: lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareNotEqual16: lines.push_back(formatCode(0xD1000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareLess16: lines.push_back(formatCode(0xD2000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareGreater16: lines.push_back(formatCode(0xD3000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Increment8: lines.push_back(formatCode(0x20000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::Decrement8: lines.push_back(formatCode(0x21000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::Increment16: lines.push_back(formatCode(0x10000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Decrement16: lines.push_back(formatCode(0x11000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Joker16: lines.push_back(formatCode("D4000000", wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CodesOn: lines.push_back(formatCode("D5000000", wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CodesOff: lines.push_back(formatCode("D6000000", wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CopyMemory:
                    lines.push_back(formatCode(0xC2000000U | maskedPsxAddress(operation.address), hex(operation.count, 4)));
                    lines.push_back(formatCode(0x80000000U | maskedPsxAddress(operation.secondAddress), "0000"));
                    break;
                case OperationKind::SerialRepeater:
                    if (operation.count <= 0xFFU && operation.addressStep <= 0xFFU &&
                        operation.valueStep <= 0xFFFFU &&
                        (operation.widthBits == 8 || operation.widthBits == 16) &&
                        !operation.addressDecreases && !operation.valueDecreases)
                    {
                        const std::uint32_t header = 0x50000000U |
                            ((operation.count & 0xFFU) << 8) |
                            (operation.addressStep & 0xFFU);
                        lines.push_back(formatCode(header, hex(operation.valueStep, 4)));
                        appendGameSharkWrite(lines, operation.widthBits, operation.address, operation.value);
                    }
                    else
                    {
                        appendExpandedRepeater(lines, operation, "GameShark / Action Replay");
                    }
                    break;
                case OperationKind::ConditionalWrite16:
                    lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue)));
                    appendGameSharkWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite16Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in GameShark / Action Replay; active conditional write follows.");
                    lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue)));
                    appendGameSharkWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite8Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in GameShark / Action Replay; active conditional write follows.");
                    lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.compareValue)));
                    appendGameSharkWrite(lines, 8, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::Scratchpad16:
                    lines.push_back(formatCode(operation.address, wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Scratchpad32:
                    lines.push_back(formatCode(operation.address, lowHalf32(operation.value), operation.suffix));
                    lines.push_back(formatCode(operation.address + 2U, highHalf32(operation.value)));
                    break;
                case OperationKind::XploderMassWrite:
                    appendMassWriteAsGameShark(lines, operation);
                    break;
                case OperationKind::BitSet8:
                case OperationKind::BitSet16:
                case OperationKind::BitSet32:
                case OperationKind::BitClear8:
                case OperationKind::BitClear16:
                case OperationKind::BitClear32:
                case OperationKind::CompareEqual32:
                case OperationKind::CompareNotEqual32:
                case OperationKind::CompareLess32:
                case OperationKind::CompareGreater32:
                case OperationKind::Increment32:
                case OperationKind::Decrement32:
                case OperationKind::XploderMegaCode:
                case OperationKind::CaetlaIndirectWrite:
                case OperationKind::BlockCompareEqual16:
                case OperationKind::BlockEnd:
                case OperationKind::DeviceSpecific:
                    appendUnsupported(lines, operation, "GameShark / Action Replay");
                    break;
                case OperationKind::Text:
                    break;
            }
        }
        return lines;
    }
}
