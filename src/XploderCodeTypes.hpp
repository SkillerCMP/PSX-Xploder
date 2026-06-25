#pragma once

// Xploder / CodeBreaker / X-Terminator RAW semantic parser and emitter.
// Encryption/decryption remains in XploderCmpConverter.hpp; this file handles
// translation between RAW Xploder operations and the shared semantic model.

#include "CodeTypeCommon.hpp"
#include "XploderCmpConverter.hpp"

namespace psx_code_types
{
    inline bool parsedLineToXploderCode(const ParsedCodeLine& line, xploder_psx::Code& code)
    {
        if (line.valueText.size() != 4 || hasWildcard(line.valueText))
            return false;
        return xploder_psx::codeFromHex(line.addressText + line.valueText, code);
    }

    inline std::vector<Operation> parseXploderRaw(const std::string& input)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<Operation> output;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            ParsedCodeLine line;
            if (!parseCodeLine(lines[i], line) || line.valueText.size() != 4)
            {
                output.push_back(makeText(lines[i], Family::XploderRaw));
                continue;
            }

            const char type = line.addressText[0];
            const bool defaultOff = line.addressText[1] == '8';

            if ((type == '5' || type == '6') && !hasWildcard(line.valueText))
            {
                xploder_psx::Code header;
                if (parsedLineToXploderCode(line, header))
                {
                    xploder_psx::MassWriteInfo info;
                    if (xploder_psx::tryGetMassWriteInfoFromPublicHeader(header, info) &&
                        info.payloadLineCount > 0 &&
                        i + static_cast<std::size_t>(info.payloadLineCount) < lines.size())
                    {
                        std::vector<std::string> sourceLines;
                        sourceLines.push_back(lines[i]);
                        bool allRowsValid = true;
                        std::vector<xploder_psx::Code> rows;
                        for (int rowIndex = 0; rowIndex < info.payloadLineCount; ++rowIndex)
                        {
                            ParsedCodeLine payloadLine;
                            xploder_psx::Code row;
                            const std::size_t sourceIndex = i + 1U + static_cast<std::size_t>(rowIndex);
                            sourceLines.push_back(lines[sourceIndex]);
                            if (!parseCodeLine(lines[sourceIndex], payloadLine) ||
                                !parsedLineToXploderCode(payloadLine, row))
                            {
                                allRowsValid = false;
                            }
                            rows.push_back(row);
                        }

                        Operation operation;
                        operation.sourceFamily = Family::XploderRaw;
                        operation.defaultOff = defaultOff;
                        operation.sourceLines = sourceLines;

                        if (info.isType6)
                        {
                            operation.kind = OperationKind::XploderMegaCode;
                            operation.detail = "Xploder Type 6 breakpoint/MIPS payload has no general equivalent";
                        }
                        else if (allRowsValid)
                        {
                            operation.kind = OperationKind::XploderMassWrite;
                            operation.address = maskedPsxAddress(line.address);
                            operation.payload.reserve(static_cast<std::size_t>(info.payloadByteCount));
                            int remaining = info.payloadByteCount;
                            for (xploder_psx::Code row : rows)
                            {
                                xploder_psx::swapType5PayloadByteOrder(row);
                                const int used = std::min<int>(remaining, static_cast<int>(xploder_psx::CodeLength));
                                for (int byteIndex = 0; byteIndex < used; ++byteIndex)
                                    operation.payload.push_back(row[static_cast<std::size_t>(byteIndex)]);
                                remaining -= used;
                            }
                        }
                        else
                        {
                            operation.kind = OperationKind::DeviceSpecific;
                            operation.detail = "Xploder structured block contains wildcard or invalid payload rows";
                        }

                        output.push_back(std::move(operation));
                        i += static_cast<std::size_t>(info.payloadLineCount);
                        continue;
                    }
                }
            }

            Operation operation;
            operation.sourceFamily = Family::XploderRaw;
            operation.address = maskedPsxAddress(line.address);
            operation.value = line.valueText;
            operation.suffix = line.suffix;
            operation.defaultOff = defaultOff;
            operation.sourceLines = {lines[i]};

            if (type == '3')
            {
                operation.kind = OperationKind::Write8;
                operation.value = byteValue(line.valueText);
            }
            else if (type == '8')
            {
                operation.kind = OperationKind::Write16;
                operation.value = wordValue(line.valueText);
            }
            else if (type == '0')
            {
                operation.kind = OperationKind::Write32;
                operation.value = "0000" + wordValue(line.valueText);
                operation.defaultOff = false;
            }
            else if (type == '7')
            {
                operation.kind = OperationKind::CompareEqual16;
                operation.value = wordValue(line.valueText);
            }
            else if (type == '9')
            {
                operation.kind = OperationKind::CompareNotEqual16;
                operation.value = wordValue(line.valueText);
            }
            else
            {
                operation = makeDeviceSpecific(
                    {lines[i]},
                    "Xploder RAW code type has no confirmed semantic mapping",
                    Family::XploderRaw);
            }
            output.push_back(std::move(operation));
        }

        return output;
    }

    inline std::uint32_t xploderAddress(char type, bool defaultOff, std::uint32_t address)
    {
        const std::uint32_t typeNibble = static_cast<std::uint32_t>(hexValue(type)) << 28;
        return typeNibble | (defaultOff ? 0x08000000U : 0U) | maskedPsxAddress(address);
    }

    inline void appendXploderWrite(
        std::vector<std::string>& lines,
        int widthBits,
        std::uint32_t address,
        const std::string& value,
        bool defaultOff = false,
        const std::string& suffix = {})
    {
        if (widthBits == 8)
            lines.push_back(formatCode(xploderAddress('3', defaultOff, address), "00" + byteValue(value), suffix));
        else
            lines.push_back(formatCode(xploderAddress('8', defaultOff, address), wordValue(value), suffix));
    }

    inline void appendExpandedRepeaterAsXploder(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        const bool wildcardSeed = hasWildcard(operation.value);
        if (wildcardSeed && operation.valueStep != 0U)
        {
            appendUnsupported(lines, operation, "Xploder RAW", "serial repeater contains wildcard data with a non-zero value step");
            return;
        }
        std::uint32_t numericValue = 0;
        if (!wildcardSeed && !parseHex(operation.value, numericValue))
        {
            appendUnsupported(lines, operation, "Xploder RAW", "serial repeater value is invalid");
            return;
        }
        const std::uint32_t valueMask = operation.widthBits == 8 ? 0xFFU : (operation.widthBits == 16 ? 0xFFFFU : 0xFFFFFFFFU);
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
            const bool off = operation.defaultOff && index == 0;
            if (wildcardSeed)
            {
                if (operation.widthBits == 32)
                {
                    appendXploderWrite(lines, 16, target, lowHalf32(operation.value), off);
                    appendXploderWrite(lines, 16, target + 2U, highHalf32(operation.value));
                }
                else
                {
                    appendXploderWrite(lines, operation.widthBits, target, operation.value, off);
                }
            }
            else if (operation.widthBits == 32)
            {
                const std::string dword = hex(currentValue, 8);
                appendXploderWrite(lines, 16, target, lowHalf32(dword), off);
                appendXploderWrite(lines, 16, target + 2U, highHalf32(dword));
            }
            else
            {
                appendXploderWrite(lines, operation.widthBits, target,
                    hex(currentValue, operation.widthBits == 8 ? 2 : 4), off);
            }
        }
    }


    inline void appendGeneratedXploderMassWrite(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        if (operation.payload.empty() || operation.payload.size() > 0xFFFFU)
        {
            appendUnsupported(lines, operation, "Xploder RAW", "generated Type 5 payload is empty or too large");
            return;
        }

        lines.push_back(formatCode(
            0x50000000U | maskedPsxAddress(operation.address),
            hex(static_cast<std::uint32_t>(operation.payload.size()), 4)));

        for (std::size_t offset = 0; offset < operation.payload.size(); offset += 6U)
        {
            std::array<std::uint8_t, 6> row{};
            const std::size_t count = std::min<std::size_t>(6U, operation.payload.size() - offset);
            for (std::size_t index = 0; index < count; ++index)
                row[index] = operation.payload[offset + index];

            std::string addressText;
            std::string valueText;
            for (std::size_t index = 0; index < 4U; ++index)
                addressText += hex(row[index], 2);
            for (std::size_t index = 4U; index < 6U; ++index)
                valueText += hex(row[index], 2);
            lines.push_back(formatCode(addressText, valueText));
        }
    }

    inline std::vector<std::string> emitXploderRaw(const std::vector<Operation>& operations)
    {
        std::vector<std::string> lines;
        for (const Operation& operation : operations)
        {
            if (operation.kind == OperationKind::Text)
            {
                lines.push_back(operation.text);
                continue;
            }

            switch (operation.kind)
            {
                case OperationKind::Write8:
                    appendXploderWrite(lines, 8, operation.address, operation.value, operation.defaultOff, operation.suffix);
                    break;
                case OperationKind::Write16:
                    appendXploderWrite(lines, 16, operation.address, operation.value, operation.defaultOff, operation.suffix);
                    break;
                case OperationKind::Write32:
                {
                    const std::string dword = dwordValue(operation.value);
                    if (highHalf32(dword) == "0000")
                    {
                        lines.push_back(formatCode(maskedPsxAddress(operation.address), lowHalf32(dword), operation.suffix));
                    }
                    else
                    {
                        appendXploderWrite(lines, 16, operation.address, lowHalf32(dword), operation.defaultOff, operation.suffix);
                        appendXploderWrite(lines, 16, operation.address + 2U, highHalf32(dword));
                    }
                    break;
                }
                case OperationKind::CompareEqual16:
                    lines.push_back(formatCode(xploderAddress('7', operation.defaultOff, operation.address), wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::CompareNotEqual16:
                    lines.push_back(formatCode(xploderAddress('9', operation.defaultOff, operation.address), wordValue(operation.value), operation.suffix));
                    break;
                case OperationKind::SerialRepeater:
                    appendExpandedRepeaterAsXploder(lines, operation);
                    break;
                case OperationKind::ConditionalWrite16:
                    lines.push_back(formatCode(xploderAddress('7', operation.defaultOff, operation.address), wordValue(operation.compareValue)));
                    appendXploderWrite(lines, 16, operation.address, operation.value, false, operation.suffix);
                    break;
                case OperationKind::ConditionalWrite16Restore:
                    lines.push_back("// Warning: restore-on-disable behavior is not available in Xploder RAW; active conditional write follows.");
                    lines.push_back(formatCode(xploderAddress('7', operation.defaultOff, operation.address), wordValue(operation.compareValue)));
                    appendXploderWrite(lines, 16, operation.address, operation.value, false, operation.suffix);
                    break;
                case OperationKind::XploderMassWrite:
                    if (operation.sourceFamily == Family::Ps1Mips)
                    {
                        appendGeneratedXploderMassWrite(lines, operation);
                    }
                    else if (operation.sourceFamily == Family::XploderRaw || operation.sourceFamily == Family::XploderEncrypted)
                    {
                        for (const std::string& source : operation.sourceLines)
                        {
                            ParsedCodeLine parsed;
                            lines.push_back(parseCodeLine(source, parsed)
                                ? formatCode(parsed.addressText, parsed.valueText, parsed.suffix)
                                : source);
                        }
                    }
                    else
                    {
                        appendUnsupported(lines, operation, "Xploder RAW");
                    }
                    break;
                case OperationKind::XploderMegaCode:
                    if (operation.sourceFamily == Family::XploderRaw || operation.sourceFamily == Family::XploderEncrypted)
                    {
                        for (const std::string& source : operation.sourceLines)
                        {
                            ParsedCodeLine parsed;
                            lines.push_back(parseCodeLine(source, parsed)
                                ? formatCode(parsed.addressText, parsed.valueText, parsed.suffix)
                                : source);
                        }
                    }
                    else
                    {
                        appendUnsupported(lines, operation, "Xploder RAW");
                    }
                    break;
                case OperationKind::BlockCompareEqual16:
                case OperationKind::BlockEnd:
                case OperationKind::DeviceSpecific:
                    if (operation.sourceFamily == Family::XploderRaw || operation.sourceFamily == Family::XploderEncrypted)
                    {
                        for (const std::string& source : operation.sourceLines)
                        {
                            ParsedCodeLine parsed;
                            lines.push_back(parseCodeLine(source, parsed)
                                ? formatCode(parsed.addressText, parsed.valueText, parsed.suffix)
                                : source);
                        }
                    }
                    else
                    {
                        appendUnsupported(lines, operation, "Xploder RAW");
                    }
                    break;
                case OperationKind::CompareEqual8:
                case OperationKind::CompareNotEqual8:
                case OperationKind::CompareLess8:
                case OperationKind::CompareGreater8:
                case OperationKind::CompareLess16:
                case OperationKind::CompareGreater16:
                case OperationKind::CompareEqual32:
                case OperationKind::CompareNotEqual32:
                case OperationKind::CompareLess32:
                case OperationKind::CompareGreater32:
                case OperationKind::BitSet8:
                case OperationKind::BitSet16:
                case OperationKind::BitSet32:
                case OperationKind::BitClear8:
                case OperationKind::BitClear16:
                case OperationKind::BitClear32:
                case OperationKind::Increment8:
                case OperationKind::Increment16:
                case OperationKind::Increment32:
                case OperationKind::Decrement8:
                case OperationKind::Decrement16:
                case OperationKind::Decrement32:
                case OperationKind::CopyMemory:
                case OperationKind::Joker16:
                case OperationKind::CodesOn:
                case OperationKind::CodesOff:
                case OperationKind::ConditionalWrite8Restore:
                case OperationKind::Scratchpad16:
                case OperationKind::Scratchpad32:
                case OperationKind::CaetlaIndirectWrite:
                    appendUnsupported(lines, operation, "Xploder RAW");
                    break;
                case OperationKind::Text:
                    break;
            }
        }
        return lines;
    }
}
