#pragma once

// Caetla .341 parser and emitter.
// Caetla accepts ordinary GameShark / Action Replay rows, while adding native
// increment/decrement widths and a two-line indirect write type. The optional
// FFFFFFFF 0001 marker makes the next conflicting row use GameShark 2.2 meaning.

#include "GameSharkActionReplayCodeTypes.hpp"

namespace psx_code_types
{
    inline std::vector<Operation> parseCaetla(const std::string& input)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<Operation> output;
        bool gameSharkMeaningForNext = false;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            ParsedCodeLine line;
            if (!parseCodeLine(lines[i], line))
            {
                output.push_back(makeText(lines[i], Family::Caetla));
                continue;
            }

            if (line.addressText == "FFFFFFFF" && line.valueText == "0001")
            {
                gameSharkMeaningForNext = true;
                continue;
            }

            const std::string prefix2 = line.addressText.substr(0, 2);
            const char prefix1 = line.addressText[0];

            if (prefix2 == "C3" && line.valueText.size() == 4 &&
                line.valueText.substr(0, 3) == "000" && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next) &&
                    next.addressText.substr(0, 4) == "9100" && next.valueText.size() == 8)
                {
                    Operation operation;
                    operation.sourceFamily = Family::Caetla;
                    operation.kind = OperationKind::CaetlaIndirectWrite;
                    operation.address = maskedPsxAddress(line.address);
                    operation.secondAddress = next.address & 0xFFFFU;
                    const int sizeCode = hexValue(line.valueText[3]);
                    operation.widthBits = sizeCode == 0 ? 8 : (sizeCode == 1 ? 16 : (sizeCode == 2 ? 32 : 0));
                    operation.value = next.valueText;
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    if (operation.widthBits == 0)
                        operation.detail = "Caetla indirect write uses an unknown size selector";
                    output.push_back(std::move(operation));
                    ++i;
                    gameSharkMeaningForNext = false;
                    continue;
                }
            }

            if (prefix2 == "50" && line.valueText.size() == 4 && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next) &&
                    (next.addressText.substr(0, 2) == "30" || next.addressText.substr(0, 2) == "80"))
                {
                    Operation operation;
                    operation.sourceFamily = Family::Caetla;
                    operation.kind = OperationKind::SerialRepeater;
                    operation.count = static_cast<std::uint32_t>(std::stoul(line.addressText.substr(4, 2), nullptr, 16));
                    operation.addressStep = static_cast<std::uint32_t>(std::stoul(line.addressText.substr(6, 2), nullptr, 16));
                    operation.valueStep = static_cast<std::uint32_t>(std::stoul(line.valueText, nullptr, 16));
                    operation.widthBits = next.addressText.substr(0, 2) == "30" ? 8 : 16;
                    operation.address = maskedPsxAddress(next.address);
                    operation.value = operation.widthBits == 8 ? byteValue(next.valueText) : wordValue(next.valueText);
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    output.push_back(std::move(operation));
                    ++i;
                    gameSharkMeaningForNext = false;
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
                    operation.sourceFamily = Family::Caetla;
                    operation.kind = OperationKind::CopyMemory;
                    operation.address = maskedPsxAddress(line.address);
                    operation.secondAddress = maskedPsxAddress(next.address);
                    operation.count = static_cast<std::uint32_t>(std::stoul(line.valueText, nullptr, 16));
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    output.push_back(std::move(operation));
                    ++i;
                    gameSharkMeaningForNext = false;
                    continue;
                }
            }

            Operation operation;
            operation.sourceFamily = Family::Caetla;
            operation.address = maskedPsxAddress(line.address);
            operation.value = line.valueText;
            operation.suffix = line.suffix;
            operation.sourceLines = {lines[i]};

            if (!gameSharkMeaningForNext && prefix2 == "10") { operation.kind = OperationKind::Increment8; operation.value = byteValue(line.valueText); }
            else if (!gameSharkMeaningForNext && prefix2 == "11") { operation.kind = OperationKind::Increment16; operation.value = wordValue(line.valueText); }
            else if (!gameSharkMeaningForNext && prefix2 == "12" && line.valueText.size() == 8) { operation.kind = OperationKind::Increment32; operation.value = dwordValue(line.valueText); }
            else if (!gameSharkMeaningForNext && prefix2 == "20") { operation.kind = OperationKind::Decrement8; operation.value = byteValue(line.valueText); }
            else if (!gameSharkMeaningForNext && prefix2 == "21") { operation.kind = OperationKind::Decrement16; operation.value = wordValue(line.valueText); }
            else if (!gameSharkMeaningForNext && prefix2 == "22" && line.valueText.size() == 8) { operation.kind = OperationKind::Decrement32; operation.value = dwordValue(line.valueText); }
            else if (gameSharkMeaningForNext && prefix2 == "10") { operation.kind = OperationKind::Increment16; operation.value = wordValue(line.valueText); }
            else if (gameSharkMeaningForNext && prefix2 == "11") { operation.kind = OperationKind::Decrement16; operation.value = wordValue(line.valueText); }
            else if (gameSharkMeaningForNext && prefix2 == "20") { operation.kind = OperationKind::Increment8; operation.value = byteValue(line.valueText); }
            else if (gameSharkMeaningForNext && prefix2 == "21") { operation.kind = OperationKind::Decrement8; operation.value = byteValue(line.valueText); }
            else if (prefix1 == '3' && line.valueText.size() <= 4) { operation.kind = OperationKind::Write8; operation.value = byteValue(line.valueText); }
            else if (prefix1 == '8' && line.valueText.size() == 4) { operation.kind = OperationKind::Write16; operation.value = wordValue(line.valueText); }
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
            else if (prefix2 == "1F" && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::Scratchpad16;
                operation.address = line.address;
                operation.value = wordValue(line.valueText);
            }
            else
            {
                operation = makeDeviceSpecific({lines[i]}, "Caetla or GameShark-compatible code type has no semantic mapping yet", Family::Caetla);
            }

            output.push_back(std::move(operation));
            gameSharkMeaningForNext = false;
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
        else
            lines.push_back(formatCode(0x80000000U | maskedPsxAddress(address), wordValue(value), suffix));
    }

    inline void appendMassWriteAsCaetla(std::vector<std::string>& lines, const Operation& operation)
    {
        std::size_t offset = 0;
        while (offset + 1U < operation.payload.size())
        {
            const std::uint16_t value = static_cast<std::uint16_t>(
                operation.payload[offset] |
                (static_cast<std::uint16_t>(operation.payload[offset + 1U]) << 8));
            appendCaetlaWrite(lines, 16, operation.address + static_cast<std::uint32_t>(offset), hex(value, 4));
            offset += 2U;
        }
        if (offset < operation.payload.size())
            appendCaetlaWrite(lines, 8, operation.address + static_cast<std::uint32_t>(offset), hex(operation.payload[offset], 2));
    }

    inline std::vector<std::string> emitCaetla(const std::vector<Operation>& operations)
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
                lines.push_back("// Note: Xploder default-off state is not represented by Caetla code rows.");

            switch (operation.kind)
            {
                case OperationKind::Write8: appendCaetlaWrite(lines, 8, operation.address, operation.value, operation.suffix); break;
                case OperationKind::Write16: appendCaetlaWrite(lines, 16, operation.address, operation.value, operation.suffix); break;
                case OperationKind::Write32:
                    appendCaetlaWrite(lines, 16, operation.address, lowHalf32(operation.value), operation.suffix);
                    appendCaetlaWrite(lines, 16, operation.address + 2U, highHalf32(operation.value));
                    break;
                case OperationKind::CompareEqual8: lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareNotEqual8: lines.push_back(formatCode(0xE1000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareLess8: lines.push_back(formatCode(0xE2000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareGreater8: lines.push_back(formatCode(0xE3000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareEqual16: lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareNotEqual16: lines.push_back(formatCode(0xD1000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareLess16: lines.push_back(formatCode(0xD2000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CompareGreater16: lines.push_back(formatCode(0xD3000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Increment8: lines.push_back(formatCode(0x10000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::Increment16: lines.push_back(formatCode(0x11000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Increment32: lines.push_back(formatCode(0x12000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
                case OperationKind::Decrement8: lines.push_back(formatCode(0x20000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
                case OperationKind::Decrement16: lines.push_back(formatCode(0x21000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
                case OperationKind::Decrement32: lines.push_back(formatCode(0x22000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
                case OperationKind::Joker16: lines.push_back(formatCode("D4000000", wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CodesOn: lines.push_back(formatCode("D5000000", wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CodesOff: lines.push_back(formatCode("D6000000", wordValue(operation.value), operation.suffix)); break;
                case OperationKind::CopyMemory:
                    lines.push_back(formatCode(0xC2000000U | maskedPsxAddress(operation.address), hex(operation.count, 4)));
                    lines.push_back(formatCode(0x80000000U | maskedPsxAddress(operation.secondAddress), "0000"));
                    break;
                case OperationKind::SerialRepeater:
                    if (operation.count <= 0xFFU && operation.addressStep <= 0xFFU && operation.valueStep <= 0xFFFFU &&
                        (operation.widthBits == 8 || operation.widthBits == 16) && !operation.addressDecreases && !operation.valueDecreases)
                    {
                        const std::uint32_t header = 0x50000000U | ((operation.count & 0xFFU) << 8) | (operation.addressStep & 0xFFU);
                        lines.push_back(formatCode(header, hex(operation.valueStep, 4)));
                        appendCaetlaWrite(lines, operation.widthBits, operation.address, operation.value);
                    }
                    else
                    {
                        appendExpandedRepeater(lines, operation, "Caetla");
                    }
                    break;
                case OperationKind::ConditionalWrite16:
                    lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue)));
                    appendCaetlaWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite16Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in Caetla; active conditional write follows.");
                    lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue)));
                    appendCaetlaWrite(lines, 16, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite8Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in Caetla; active conditional write follows.");
                    lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.compareValue)));
                    appendCaetlaWrite(lines, 8, operation.address, operation.value, operation.suffix);
                    break;
                case OperationKind::Scratchpad16:
                    lines.push_back(formatCode(operation.address, wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::Scratchpad32:
                    lines.push_back(formatCode(operation.address, lowHalf32(operation.value), operation.suffix));
                    lines.push_back(formatCode(operation.address + 2U, highHalf32(operation.value)));
                    break;
                case OperationKind::CaetlaIndirectWrite:
                {
                    const int sizeCode = operation.widthBits == 8 ? 0 : (operation.widthBits == 16 ? 1 : 2);
                    lines.push_back(formatCode(0xC3000000U | maskedPsxAddress(operation.address), "000" + hex(static_cast<std::uint32_t>(sizeCode), 1)));
                    lines.push_back(formatCode(0x91000000U | (operation.secondAddress & 0xFFFFU), dwordValue(operation.value), operation.suffix));
                    break;
                }
                case OperationKind::XploderMassWrite:
                    appendMassWriteAsCaetla(lines, operation);
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
                case OperationKind::XploderMegaCode:
                case OperationKind::BlockCompareEqual16:
                case OperationKind::BlockEnd:
                case OperationKind::DeviceSpecific:
                    appendUnsupported(lines, operation, "Caetla");
                    break;
                case OperationKind::Text:
                    break;
            }
        }
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
