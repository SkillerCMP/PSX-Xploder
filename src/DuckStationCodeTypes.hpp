#pragma once

// DuckStation PlayStation cheat parser and emitter.
// Includes ordinary GameShark-compatible rows and DuckStation's extended 8+8 types.

#include "GameSharkActionReplayCodeTypes.hpp"

namespace psx_code_types
{
    inline std::vector<Operation> parseDuckStation(const std::string& input)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<Operation> output;
        bool assemblySection = false;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const std::string trimmed = trim(lines[i]);
            if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']')
            {
                assemblySection = false;
                output.push_back(makeText(lines[i], Family::DuckStation));
                continue;
            }
            if (startsWithIgnoreCase(trimmed, "Type ="))
            {
                const std::string typeValue = trim(std::string_view(trimmed).substr(6));
                assemblySection = startsWithIgnoreCase(typeValue, "Assembly");
                output.push_back(makeText(lines[i], Family::DuckStation));
                continue;
            }
            if (assemblySection)
            {
                if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#')
                    output.push_back(makeText(lines[i], Family::DuckStation));
                else
                {
                    Operation operation;
                    operation.kind = OperationKind::DuckStationRaw;
                    operation.sourceFamily = Family::DuckStation;
                    operation.sourceLines = {lines[i]};
                    operation.detail = "DuckStation Assembly body row";
                    output.push_back(std::move(operation));
                }
                continue;
            }

            ParsedCodeLine line;
            if (!parseCodeLine(lines[i], line))
            {
                output.push_back(makeText(lines[i], Family::DuckStation));
                continue;
            }

            const std::string prefix2 = line.addressText.substr(0, 2);
            const char prefix1 = line.addressText[0];

            auto appendRawRange = [&](std::size_t last, std::string detail) {
                Operation raw;
                raw.kind = OperationKind::DuckStationRaw;
                raw.sourceFamily = Family::DuckStation;
                raw.detail = std::move(detail);
                for (std::size_t sourceIndex = i; sourceIndex <= last && sourceIndex < lines.size(); ++sourceIndex)
                    raw.sourceLines.push_back(lines[sourceIndex]);
                output.push_back(std::move(raw));
                i = std::min(last, lines.size() - 1U);
            };

            // These instructions consume additional rows or control a whole
            // block. Preserve the complete structure as one operation so a
            // conversion to another device cannot accidentally emit the body
            // as unconditional writes.
            if (prefix2 == "F4")
            {
                appendRawRange(std::min(i + 4U, lines.size() - 1U),
                    "DuckStation F4 find-and-replace structure preserved verbatim");
                continue;
            }
            if (prefix2 == "F3")
            {
                appendRawRange(std::min(i + 1U, lines.size() - 1U),
                    "DuckStation F3 range structure preserved verbatim");
                continue;
            }
            if (prefix2 == "D7" || prefix2 == "52" || prefix2 == "F6")
            {
                std::size_t end = i;
                for (std::size_t cursor = i + 1U; cursor < lines.size(); ++cursor)
                {
                    const std::string candidate = trim(lines[cursor]);
                    if (!candidate.empty() && candidate.front() == '[' && candidate.back() == ']')
                        break;
                    end = cursor;
                    ParsedCodeLine terminator;
                    if (parseCodeLine(lines[cursor], terminator) &&
                        terminator.addressText == "00000000" && terminator.valueText == "FFFF")
                    {
                        break;
                    }
                }
                appendRawRange(end,
                    "DuckStation advanced block conditional preserved verbatim");
                continue;
            }
            if (prefix2 == "C1")
            {
                std::size_t end = i;
                for (std::size_t cursor = i + 1U; cursor < lines.size(); ++cursor)
                {
                    const std::string candidate = trim(lines[cursor]);
                    if (!candidate.empty() && candidate.front() == '[' && candidate.back() == ']')
                        break;
                    end = cursor;
                }
                appendRawRange(end,
                    "DuckStation delayed-activation row and dependent section body preserved verbatim");
                continue;
            }

            if (prefix2 == "53" && line.valueText.size() == 8 && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next))
                {
                    const std::string writePrefix = next.addressText.substr(0, 2);
                    if (writePrefix == "30" || writePrefix == "31" || writePrefix == "32" ||
                        writePrefix == "80" || writePrefix == "81" || writePrefix == "82" ||
                        writePrefix == "90" || writePrefix == "91" || writePrefix == "92")
                    {
                        Operation operation;
                        operation.sourceFamily = Family::DuckStation;
                        operation.kind = OperationKind::SerialRepeater;
                        operation.addressDecreases = line.addressText[2] == '1';
                        operation.valueDecreases = line.addressText[3] == '1';
                        operation.count = static_cast<std::uint32_t>(std::stoul(line.addressText.substr(4, 4), nullptr, 16));
                        operation.addressStep = static_cast<std::uint32_t>(std::stoul(line.valueText.substr(0, 4), nullptr, 16));
                        operation.valueStep = static_cast<std::uint32_t>(std::stoul(line.valueText.substr(4, 4), nullptr, 16));
                        operation.widthBits = writePrefix[0] == '3' ? 8 : (writePrefix[0] == '8' ? 16 : 32);
                        operation.opcode = writePrefix;
                        operation.address = maskedPsxAddress(next.address);
                        operation.value = operation.widthBits == 8
                            ? byteValue(next.valueText)
                            : (operation.widthBits == 16 ? wordValue(next.valueText) : dwordValue(next.valueText));
                        operation.sourceLines = {lines[i], lines[i + 1U]};
                        output.push_back(std::move(operation));
                        ++i;
                        continue;
                    }
                }
            }

            if (prefix2 == "50" && line.valueText.size() == 4 && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next) &&
                    (next.addressText.substr(0, 2) == "30" || next.addressText.substr(0, 2) == "80" ||
                     next.addressText.substr(0, 2) == "90"))
                {
                    const std::uint32_t count = static_cast<std::uint32_t>(
                        std::stoul(line.addressText.substr(4, 2), nullptr, 16));
                    Operation operation;
                    operation.sourceFamily = Family::DuckStation;
                    operation.kind = OperationKind::SerialRepeater;
                    operation.count = count;
                    operation.addressStep = static_cast<std::uint32_t>(std::stoul(line.addressText.substr(6, 2), nullptr, 16));
                    operation.valueStep = static_cast<std::uint32_t>(std::stoul(line.valueText, nullptr, 16));
                    const std::string writePrefix = next.addressText.substr(0, 2);
                    operation.widthBits = writePrefix == "30" ? 8 : (writePrefix == "80" ? 16 : 32);
                    operation.opcode = writePrefix;
                    operation.address = maskedPsxAddress(next.address);
                    operation.value = operation.widthBits == 8
                        ? byteValue(next.valueText)
                        : (operation.widthBits == 16 ? wordValue(next.valueText) : "0000" + wordValue(next.valueText));
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    output.push_back(std::move(operation));
                    ++i;
                    continue;
                }
            }

            if (prefix2 == "C2" && line.valueText.size() == 4 && i + 1U < lines.size())
            {
                ParsedCodeLine next;
                if (parseCodeLine(lines[i + 1U], next))
                {
                    const std::uint32_t count = static_cast<std::uint32_t>(
                        std::stoul(line.valueText, nullptr, 16));
                    Operation operation;
                    operation.sourceFamily = Family::DuckStation;
                    operation.kind = OperationKind::CopyMemory;
                    operation.address = maskedPsxAddress(line.address);
                    operation.secondAddress = maskedPsxAddress(next.address);
                    operation.count = count;
                    operation.sourceLines = {lines[i], lines[i + 1U]};
                    output.push_back(std::move(operation));
                    ++i;
                    continue;
                }
            }

            Operation operation;
            operation.sourceFamily = Family::DuckStation;
            operation.address = maskedPsxAddress(line.address);
            operation.value = line.valueText;
            operation.suffix = line.suffix;
            operation.sourceLines = {lines[i]};

            if (line.addressText == "00000000" && line.valueText == "FFFF")
            {
                operation.kind = OperationKind::BlockEnd;
            }
            else if (prefix2 == "C0" && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::BlockCompareEqual16;
                operation.value = wordValue(line.valueText);
            }
            else if (prefix2 == "90" && line.valueText.size() == 8) { operation.kind = OperationKind::Write32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "91" && line.valueText.size() == 8) { operation.kind = OperationKind::BitSet32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "92" && line.valueText.size() == 8) { operation.kind = OperationKind::BitClear32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "A0" && line.valueText.size() == 8) { operation.kind = OperationKind::CompareEqual32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "A1" && line.valueText.size() == 8) { operation.kind = OperationKind::CompareNotEqual32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "A2" && line.valueText.size() == 8) { operation.kind = OperationKind::CompareLess32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "A3" && line.valueText.size() == 8) { operation.kind = OperationKind::CompareGreater32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "60" && line.valueText.size() == 8) { operation.kind = OperationKind::Increment32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "61" && line.valueText.size() == 8) { operation.kind = OperationKind::Decrement32; operation.value = dwordValue(line.valueText); }
            else if (prefix2 == "A5" && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::Scratchpad32;
                operation.address = 0x1F800000U | (line.address & 0xFFFU);
                operation.value = dwordValue(line.valueText);
            }
            else if (prefix2 == "A6" && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::ConditionalWrite16;
                operation.compareValue = line.valueText.substr(0, 4);
                operation.value = line.valueText.substr(4, 4);
            }
            else if (prefix2 == "A7" && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::ConditionalWrite16Restore;
                operation.compareValue = line.valueText.substr(0, 4);
                operation.value = line.valueText.substr(4, 4);
                operation.restoreOnDisable = true;
            }
            else if (prefix2 == "A8" && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::ConditionalWrite8Restore;
                operation.compareValue = line.valueText.substr(0, 2);
                operation.value = line.valueText.substr(2, 2);
                operation.restoreOnDisable = true;
            }
            else if (prefix1 == '8' && line.valueText.size() == 4)
            {
                if (prefix2 == "81") operation.kind = OperationKind::BitSet16;
                else if (prefix2 == "82") operation.kind = OperationKind::BitClear16;
                else operation.kind = OperationKind::Write16;
                operation.value = wordValue(line.valueText);
            }
            else if (prefix1 == '3' && line.valueText.size() <= 4)
            {
                if (prefix2 == "31") operation.kind = OperationKind::BitSet8;
                else if (prefix2 == "32") operation.kind = OperationKind::BitClear8;
                else operation.kind = OperationKind::Write8;
                operation.value = byteValue(line.valueText);
            }
            else if (prefix2 == "E0") { operation.kind = OperationKind::CompareEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E1") { operation.kind = OperationKind::CompareNotEqual8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E2") { operation.kind = OperationKind::CompareLess8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "E3") { operation.kind = OperationKind::CompareGreater8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "D0") { operation.kind = OperationKind::CompareEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D1") { operation.kind = OperationKind::CompareNotEqual16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D2") { operation.kind = OperationKind::CompareLess16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D3") { operation.kind = OperationKind::CompareGreater16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "D4")
            {
                operation.kind = OperationKind::Joker16;
                operation.value = wordValue(line.valueText);
                operation.detail = "GameShark controller-state compare; the encoded address field is ignored by GS Pro 3.1";
            }
            else if (prefix2 == "D5")
            {
                operation.kind = OperationKind::BlockButtonsEqual;
                operation.value = wordValue(line.valueText);
                operation.detail = "DuckStation block executes when the exact controller state equals the value";
            }
            else if (prefix2 == "D6")
            {
                operation.kind = OperationKind::BlockButtonsNotEqual;
                operation.value = wordValue(line.valueText);
                operation.detail = "DuckStation block executes when the exact controller state does not equal the value";
            }
            else if (prefix2 == "10") { operation.kind = OperationKind::Increment16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "11") { operation.kind = OperationKind::Decrement16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "20") { operation.kind = OperationKind::Increment8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "21") { operation.kind = OperationKind::Decrement8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "1F" && line.valueText.size() == 4)
            {
                operation.kind = OperationKind::Scratchpad16;
                operation.address = 0x1F800000U | (line.address & 0x3FFU);
                operation.value = wordValue(line.valueText);
            }
            else if (prefix2 == "A4" && line.valueText.size() == 8)
            {
                operation.kind = OperationKind::BlockCompareEqual32;
                operation.value = dwordValue(line.valueText);
            }
            else if (prefix2 == "C3") { operation.kind = OperationKind::BlockCompareLess8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "C4") { operation.kind = OperationKind::BlockCompareGreater8; operation.value = byteValue(line.valueText); }
            else if (prefix2 == "C5") { operation.kind = OperationKind::BlockCompareLess16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "C6") { operation.kind = OperationKind::BlockCompareGreater16; operation.value = wordValue(line.valueText); }
            else if (prefix2 == "C1" || prefix2 == "D7" || prefix2 == "F0" || prefix2 == "F1" ||
                     prefix2 == "F2" || prefix2 == "F3" || prefix2 == "F4" || prefix2 == "F5" ||
                     prefix2 == "F6" || prefix2 == "51" || prefix2 == "52")
            {
                operation.kind = OperationKind::DuckStationRaw;
                operation.detail = "DuckStation-specific code type preserved verbatim";
            }
            else
            {
                operation = makeDeviceSpecific({lines[i]}, "DuckStation-specific code type has no exact mapping to the selected destination", Family::DuckStation);
            }
            output.push_back(std::move(operation));
        }

        return output;
    }

    inline void appendDuckStationWrite(
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

    inline void appendMassWriteAsDuckStation(
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
            appendDuckStationWrite(lines, 32, operation.address + static_cast<std::uint32_t>(offset), hex(value, 8));
            offset += 4U;
        }
        if (offset + 1U < operation.payload.size())
        {
            const std::uint16_t value = static_cast<std::uint16_t>(
                operation.payload[offset] |
                (static_cast<std::uint16_t>(operation.payload[offset + 1U]) << 8));
            appendDuckStationWrite(lines, 16, operation.address + static_cast<std::uint32_t>(offset), hex(value, 4));
            offset += 2U;
        }
        if (offset < operation.payload.size())
            appendDuckStationWrite(lines, 8, operation.address + static_cast<std::uint32_t>(offset), hex(operation.payload[offset], 2));
    }

    struct DuckStationPatchMetadata
    {
        std::string name = "Unnamed Cheat";
        std::string description;
        std::string author;
        std::string type = "Gameshark";
        std::string activation = "EndFrame";
        std::vector<std::string> extraProperties;
        std::vector<std::string> groupPath;
    };

    struct DuckStationPatchSection
    {
        DuckStationPatchMetadata metadata;
        std::vector<Operation> operations;
        bool hasExplicitName = false;
    };

    inline std::size_t findIgnoreCase(
        std::string_view text,
        std::string_view needle,
        std::size_t start = 0) noexcept
    {
        if (needle.empty() || start > text.size() || needle.size() > text.size() - start)
            return std::string_view::npos;

        for (std::size_t i = start; i + needle.size() <= text.size(); ++i)
        {
            bool equal = true;
            for (std::size_t j = 0; j < needle.size(); ++j)
            {
                if (std::toupper(static_cast<unsigned char>(text[i + j])) !=
                    std::toupper(static_cast<unsigned char>(needle[j])))
                {
                    equal = false;
                    break;
                }
            }
            if (equal)
                return i;
        }
        return std::string_view::npos;
    }

    inline std::string decodeDuckStationHtmlEntities(std::string text)
    {
        struct Entity
        {
            const char* encoded;
            const char* decoded;
        };
        static constexpr Entity entities[] = {
            {"&amp;", "&"},
            {"&quot;", "\""},
            {"&#39;", "'"},
            {"&apos;", "'"},
            {"&lt;", "<"},
            {"&gt;", ">"},
            {"&nbsp;", " "}
        };

        for (const Entity& entity : entities)
        {
            std::size_t position = 0;
            while ((position = findIgnoreCase(text, entity.encoded, position)) != std::string::npos)
            {
                text.replace(position, std::char_traits<char>::length(entity.encoded), entity.decoded);
                position += std::char_traits<char>::length(entity.decoded);
            }
        }
        return text;
    }

    inline std::string cleanDuckStationMetadataText(std::string text)
    {
        text = decodeDuckStationHtmlEntities(std::move(text));

        std::string withoutTags;
        withoutTags.reserve(text.size());
        bool insideTag = false;
        bool pendingSpace = false;

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '<')
            {
                insideTag = true;
                pendingSpace = true;
                continue;
            }
            if (insideTag)
            {
                if (c == '>')
                    insideTag = false;
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(c)))
            {
                pendingSpace = true;
                continue;
            }

            if (pendingSpace && !withoutTags.empty())
                withoutTags.push_back(' ');
            pendingSpace = false;
            withoutTags.push_back(c);
        }

        return trim(withoutTags);
    }

    inline void trimDuckStationMetadataPunctuation(std::string& text)
    {
        text = trim(text);
        while (!text.empty() && (text.back() == ',' || text.back() == ';'))
        {
            text.pop_back();
            text = trim(text);
        }
    }

    inline std::string sanitizeDuckStationSectionName(std::string text)
    {
        text = cleanDuckStationMetadataText(std::move(text));
        for (char& c : text)
        {
            if (c == '[')
                c = '(';
            else if (c == ']')
                c = ')';
        }
        if (text.empty())
            text = "Unnamed Cheat";
        return text;
    }

    inline bool parseDuckStationCreditsLine(std::string_view source, std::string& author)
    {
        const std::string text = trim(source);
        if (!startsWithIgnoreCase(text, "%Credits:"))
            return false;

        author = cleanDuckStationMetadataText(text.substr(std::string("%Credits:").size()));
        trimDuckStationMetadataPunctuation(author);
        return true;
    }

    // CMP database groups are represented by a stack:
    //   !Player Info:   -> push "Player Info"
    //   !Movement:      -> push "Movement" as a subgroup
    //   !!              -> pop the most recently opened group
    // The active stack becomes DuckStation's backslash-delimited patch path.
    inline bool parseDuckStationGroupOpenLine(
        std::string_view source,
        std::string& groupName)
    {
        const std::string text = trim(source);
        if (text.size() < 3U || text.front() != '!' || text == "!!" || text.back() != ':')
            return false;

        groupName = sanitizeDuckStationSectionName(text.substr(1U, text.size() - 2U));
        return !groupName.empty() && groupName != "Unnamed Cheat";
    }

    inline bool isDuckStationGroupCloseLine(std::string_view source)
    {
        return trim(source) == "!!";
    }

    inline std::string buildDuckStationPatchName(const DuckStationPatchMetadata& metadata)
    {
        std::string fullName;
        for (const std::string& group : metadata.groupPath)
        {
            const std::string cleanGroup = sanitizeDuckStationSectionName(group);
            if (cleanGroup.empty() || cleanGroup == "Unnamed Cheat")
                continue;
            if (!fullName.empty())
                fullName.push_back('\\');
            fullName += cleanGroup;
        }

        const std::string cleanName = sanitizeDuckStationSectionName(metadata.name);
        if (!fullName.empty())
            fullName.push_back('\\');
        fullName += cleanName;
        return fullName;
    }

    inline bool parseDuckStationCodeNameLine(
        std::string_view source,
        DuckStationPatchMetadata& metadata)
    {
        std::string text = trim(source);
        if (text.empty())
            return false;

        if (text.front() == '+')
        {
            text = trim(std::string_view(text).substr(1));
        }
        else if (text.front() == '[' && text.back() == ']' && text.size() > 2U)
        {
            metadata = {};
            const std::string pathText = text.substr(1, text.size() - 2U);
            std::size_t start = 0U;
            while (start <= pathText.size())
            {
                const std::size_t slash = pathText.find('\\', start);
                const std::size_t end = slash == std::string::npos ? pathText.size() : slash;
                const std::string part = sanitizeDuckStationSectionName(
                    pathText.substr(start, end - start));
                if (slash == std::string::npos)
                {
                    metadata.name = part;
                    break;
                }
                if (!part.empty() && part != "Unnamed Cheat")
                    metadata.groupPath.push_back(part);
                start = slash + 1U;
            }
            if (metadata.name.empty())
                metadata.name = "Unnamed Cheat";
            return true;
        }
        else
        {
            // Plain code names are valid input too. Reject known metadata,
            // comments, and DuckStation property lines so they are not turned
            // into patch-section names.
            if (text.front() == '%' || text.front() == '!' ||
                text.front() == '^' || text.front() == ';' ||
                startsWithIgnoreCase(text, "//") ||
                startsWithIgnoreCase(text, "Type =") ||
                startsWithIgnoreCase(text, "Activation =") ||
                startsWithIgnoreCase(text, "Description =") ||
                startsWithIgnoreCase(text, "Author =") ||
                startsWithIgnoreCase(text, "Option =") ||
                startsWithIgnoreCase(text, "OptionRange =") ||
                startsWithIgnoreCase(text, "DisallowForAchievements =") ||
                startsWithIgnoreCase(text, "Ignore ="))
            {
                return false;
            }
        }

        metadata = {};

        const std::size_t noteStart = text.find('{');
        if (noteStart != std::string::npos)
        {
            const std::size_t noteEnd = text.rfind('}');
            if (noteEnd != std::string::npos && noteEnd > noteStart)
            {
                metadata.description = cleanDuckStationMetadataText(
                    text.substr(noteStart + 1U, noteEnd - noteStart - 1U));
                text.erase(noteStart, noteEnd - noteStart + 1U);
            }
        }

        const std::size_t cryptPosition = findIgnoreCase(text, "Crypt:");
        if (cryptPosition != std::string::npos)
        {
            std::size_t erasePosition = cryptPosition;
            while (erasePosition > 0U && std::isspace(static_cast<unsigned char>(text[erasePosition - 1U])))
                --erasePosition;
            if (erasePosition > 0U && text[erasePosition - 1U] == ',')
            {
                --erasePosition;
                while (erasePosition > 0U && std::isspace(static_cast<unsigned char>(text[erasePosition - 1U])))
                    --erasePosition;
            }
            text.erase(erasePosition);
        }

        trimDuckStationMetadataPunctuation(text);

        std::size_t byPosition = std::string::npos;
        std::size_t searchPosition = 0;
        while (true)
        {
            const std::size_t found = findIgnoreCase(text, " by ", searchPosition);
            if (found == std::string::npos)
                break;
            byPosition = found;
            searchPosition = found + 4U;
        }

        if (byPosition != std::string::npos)
        {
            metadata.author = cleanDuckStationMetadataText(text.substr(byPosition + 4U));
            trimDuckStationMetadataPunctuation(metadata.author);
            text.erase(byPosition);
        }

        trimDuckStationMetadataPunctuation(text);
        metadata.name = sanitizeDuckStationSectionName(text);
        return true;
    }

    inline bool parseDuckStationPropertyLine(
        std::string_view source,
        std::string_view propertyName,
        std::string& value)
    {
        const std::string text = trim(source);
        if (!startsWithIgnoreCase(text, propertyName))
            return false;

        value = cleanDuckStationMetadataText(
            trim(std::string_view(text).substr(propertyName.size())));
        trimDuckStationMetadataPunctuation(value);
        return true;
    }

    inline bool canCombineDuckStationWrite16(
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

    inline void appendDuckStationOperation(
        std::vector<std::string>& lines,
        const Operation& operation)
    {
        if (operation.kind == OperationKind::Text)
        {
            const std::string text = trim(operation.text);
            if (text.empty())
                return;
            if (startsWithIgnoreCase(text, "//"))
                lines.push_back("; " + trim(std::string_view(text).substr(2)));
            else if (text.front() == ';' || text.front() == '#')
                lines.push_back(text);
            else
                lines.push_back("; " + text);
            return;
        }

        if (operation.defaultOff)
            lines.push_back("; Note: Xploder default-off state is not represented by DuckStation code rows.");

        switch (operation.kind)
        {
            case OperationKind::Write8: appendDuckStationWrite(lines, 8, operation.address, operation.value, operation.suffix); break;
            case OperationKind::Write16: appendDuckStationWrite(lines, 16, operation.address, operation.value, operation.suffix); break;
            case OperationKind::Write32: appendDuckStationWrite(lines, 32, operation.address, operation.value, operation.suffix); break;
            case OperationKind::BitSet8: lines.push_back(formatCode(0x31000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::BitClear8: lines.push_back(formatCode(0x32000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::BitSet16: lines.push_back(formatCode(0x81000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BitClear16: lines.push_back(formatCode(0x82000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BitSet32: lines.push_back(formatCode(0x91000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::BitClear32: lines.push_back(formatCode(0x92000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareEqual8: lines.push_back(formatCode(0xE0000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareNotEqual8: lines.push_back(formatCode(0xE1000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareLess8: lines.push_back(formatCode(0xE2000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareLessOrEqual8:
                appendUnsupported(lines, operation, "DuckStation", "DuckStation E2 is strict less-than, not less-than-or-equal");
                break;
            case OperationKind::CompareGreater8: lines.push_back(formatCode(0xE3000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareEqual16: lines.push_back(formatCode(0xD0000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareNotEqual16: lines.push_back(formatCode(0xD1000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareLess16: lines.push_back(formatCode(0xD2000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareLessOrEqual16:
                appendUnsupported(lines, operation, "DuckStation", "DuckStation D2 is strict less-than, not less-than-or-equal");
                break;
            case OperationKind::CompareGreater16: lines.push_back(formatCode(0xD3000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareEqual32: lines.push_back(formatCode(0xA0000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareNotEqual32: lines.push_back(formatCode(0xA1000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareLess32: lines.push_back(formatCode(0xA2000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::CompareGreater32: lines.push_back(formatCode(0xA3000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockCompareEqual16: lines.push_back(formatCode(0xC0000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockCompareEqual32: lines.push_back(formatCode(0xA4000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockCompareLess8: lines.push_back(formatCode(0xC3000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockCompareGreater8: lines.push_back(formatCode(0xC4000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockCompareLess16: lines.push_back(formatCode(0xC5000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockCompareGreater16: lines.push_back(formatCode(0xC6000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockButtonsEqual: lines.push_back(formatCode("D5000000", wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockButtonsNotEqual: lines.push_back(formatCode("D6000000", wordValue(operation.value), operation.suffix)); break;
            case OperationKind::BlockEnd: lines.push_back(formatCode("00000000", "FFFF", operation.suffix)); break;
            case OperationKind::Increment8: lines.push_back(formatCode(0x20000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::Decrement8: lines.push_back(formatCode(0x21000000U | maskedPsxAddress(operation.address), "00" + byteValue(operation.value), operation.suffix)); break;
            case OperationKind::Increment16: lines.push_back(formatCode(0x10000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::Decrement16: lines.push_back(formatCode(0x11000000U | maskedPsxAddress(operation.address), wordValue(operation.value), operation.suffix)); break;
            case OperationKind::Increment32: lines.push_back(formatCode(0x60000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::Decrement32: lines.push_back(formatCode(0x61000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), operation.suffix)); break;
            case OperationKind::Joker16: lines.push_back(formatCode("D4000000", wordValue(operation.value), operation.suffix)); break;
            case OperationKind::CopyMemory:
                if (operation.count <= 0xFFFFU)
                {
                    lines.push_back(formatCode(0xC2000000U | maskedPsxAddress(operation.address), hex(operation.count, 4), operation.suffix));
                    lines.push_back(formatCode(0x80000000U | maskedPsxAddress(operation.secondAddress), "0000"));
                }
                else
                {
                    std::vector<std::string> unsupported;
                    appendUnsupported(unsupported, operation, "DuckStation", "C2 copy length must be from 1 to 0xFFFF bytes");
                    for (std::string& warning : unsupported)
                    {
                        if (startsWithIgnoreCase(warning, "//"))
                            warning = "; " + trim(std::string_view(warning).substr(2));
                        lines.push_back(std::move(warning));
                    }
                }
                break;
            case OperationKind::SerialRepeater:
            {
                if (operation.count <= 0xFFU && operation.addressStep <= 0xFFU && operation.valueStep <= 0xFFFFU &&
                    (operation.widthBits == 8 || operation.widthBits == 16 || operation.widthBits == 32) &&
                    !operation.addressDecreases && !operation.valueDecreases &&
                    (operation.opcode.empty() || operation.opcode == "30" || operation.opcode == "80" || operation.opcode == "90"))
                {
                    const std::uint32_t header = 0x50000000U | ((operation.count & 0xFFU) << 8) | (operation.addressStep & 0xFFU);
                    lines.push_back(formatCode(header, hex(operation.valueStep, 4)));
                    appendDuckStationWrite(lines, operation.widthBits, operation.address, operation.value);
                }
                else if (operation.count <= 0xFFFFU && operation.addressStep <= 0xFFFFU && operation.valueStep <= 0xFFFFU &&
                    (operation.widthBits == 8 || operation.widthBits == 16 || operation.widthBits == 32))
                {
                    const std::uint32_t header = 0x53000000U |
                        (operation.addressDecreases ? 0x00100000U : 0U) |
                        (operation.valueDecreases ? 0x00010000U : 0U) |
                        (operation.count & 0xFFFFU);
                    lines.push_back(formatCode(header, hex(operation.addressStep, 4) + hex(operation.valueStep, 4)));
                    if (operation.opcode == "31" || operation.opcode == "32")
                        lines.push_back(formatCode((operation.opcode == "31" ? 0x31000000U : 0x32000000U) | maskedPsxAddress(operation.address), "00" + byteValue(operation.value)));
                    else if (operation.opcode == "81" || operation.opcode == "82")
                        lines.push_back(formatCode((operation.opcode == "81" ? 0x81000000U : 0x82000000U) | maskedPsxAddress(operation.address), wordValue(operation.value)));
                    else if (operation.opcode == "91" || operation.opcode == "92")
                        lines.push_back(formatCode((operation.opcode == "91" ? 0x91000000U : 0x92000000U) | maskedPsxAddress(operation.address), dwordValue(operation.value)));
                    else
                        appendDuckStationWrite(lines, operation.widthBits, operation.address, operation.value);
                }
                else
                {
                    appendUnsupported(lines, operation, "DuckStation", "serial repeater fields exceed DuckStation limits");
                }
                break;
            }
            case OperationKind::ConditionalWrite16:
                lines.push_back(formatCode(0xA6000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue) + wordValue(operation.value), operation.suffix));
                break;
            case OperationKind::ConditionalWrite16Restore:
                lines.push_back(formatCode(0xA7000000U | maskedPsxAddress(operation.address), wordValue(operation.compareValue) + wordValue(operation.value), operation.suffix));
                break;
            case OperationKind::ConditionalWrite8Restore:
                lines.push_back(formatCode(0xA8000000U | maskedPsxAddress(operation.address), byteValue(operation.compareValue) + byteValue(operation.value), operation.suffix));
                break;
            case OperationKind::Scratchpad8:
                lines.push_back(formatCode(operation.address, "00" + byteValue(operation.value), operation.suffix));
                break;
            case OperationKind::Scratchpad16:
                lines.push_back(formatCode(operation.address, wordValue(operation.value), operation.suffix));
                break;
            case OperationKind::Scratchpad32:
                lines.push_back(formatCode(0xA5000000U | (operation.address & 0xFFFU), dwordValue(operation.value), operation.suffix));
                break;
            case OperationKind::DuckStationRaw:
                if (operation.sourceFamily == Family::DuckStation)
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
                    appendUnsupported(lines, operation, "DuckStation");
                }
                break;
            case OperationKind::XploderMassWrite:
                appendMassWriteAsDuckStation(lines, operation);
                break;
            case OperationKind::GlobalCompareEqual16:
            case OperationKind::GameSharkControlD5:
            case OperationKind::GameSharkControlD6:
            case OperationKind::XploderMegaCode:
            case OperationKind::CaetlaIndirectWrite:
            case OperationKind::DeviceSpecific:
            {
                std::vector<std::string> unsupported;
                appendUnsupported(unsupported, operation, "DuckStation");
                for (std::string& line : unsupported)
                {
                    if (startsWithIgnoreCase(line, "//"))
                        line = "; " + trim(std::string_view(line).substr(2));
                    lines.push_back(std::move(line));
                }
                break;
            }
            case OperationKind::Text:
                break;
        }
    }

    inline bool sameDuckStationEqual16Condition(
        const Operation& left,
        const Operation& right)
    {
        return left.kind == OperationKind::CompareEqual16 &&
               right.kind == OperationKind::CompareEqual16 &&
               left.address == right.address &&
               wordValue(left.value) == wordValue(right.value) &&
               left.defaultOff == right.defaultOff &&
               left.suffix.empty() && right.suffix.empty();
    }

    inline bool isDuckStationCondensableControlledOperation(const Operation& operation)
    {
        return operation.kind == OperationKind::Write8 ||
               operation.kind == OperationKind::Write16 ||
               operation.kind == OperationKind::Write32;
    }

    inline void appendDuckStationOperationRange(
        std::vector<std::string>& lines,
        const std::vector<Operation>& operations,
        bool combineConsecutive16BitWrites)
    {
        for (std::size_t i = 0; i < operations.size(); ++i)
        {
            const Operation& operation = operations[i];
            if (combineConsecutive16BitWrites &&
                i + 1U < operations.size() &&
                canCombineDuckStationWrite16(operation, operations[i + 1U]))
            {
                const Operation& highHalf = operations[i + 1U];
                appendDuckStationWrite(
                    lines,
                    32,
                    operation.address,
                    wordValue(highHalf.value) + wordValue(operation.value));
                ++i;
                continue;
            }

            appendDuckStationOperation(lines, operation);
        }
    }

    inline void appendDuckStationPatchSection(
        std::vector<std::string>& lines,
        const DuckStationPatchSection& section,
        bool combineConsecutive16BitWrites,
        bool condenseRepeatedActivators)
    {
        if (!lines.empty() && !lines.back().empty())
            lines.push_back({});

        lines.push_back("[" + buildDuckStationPatchName(section.metadata) + "]");
        lines.push_back("Type = " + (section.metadata.type.empty() ? std::string("Gameshark") : section.metadata.type));
        lines.push_back("Activation = " + (section.metadata.activation.empty() ? std::string("EndFrame") : section.metadata.activation));
        if (!section.metadata.description.empty())
            lines.push_back("Description = " + section.metadata.description);
        if (!section.metadata.author.empty())
            lines.push_back("Author = " + section.metadata.author);
        for (const std::string& property : section.metadata.extraProperties)
            lines.push_back(property);

        for (std::size_t i = 0; i < section.operations.size();)
        {
            if (condenseRepeatedActivators &&
                section.operations[i].kind == OperationKind::CompareEqual16 &&
                i + 1U < section.operations.size() &&
                isDuckStationCondensableControlledOperation(section.operations[i + 1U]))
            {
                const Operation& condition = section.operations[i];
                std::vector<Operation> controlled;
                std::size_t cursor = i;
                while (cursor + 1U < section.operations.size() &&
                       sameDuckStationEqual16Condition(condition, section.operations[cursor]) &&
                       isDuckStationCondensableControlledOperation(section.operations[cursor + 1U]))
                {
                    controlled.push_back(section.operations[cursor + 1U]);
                    cursor += 2U;
                }

                if (controlled.size() >= 2U)
                {
                    lines.push_back(formatCode(
                        0xC0000000U | maskedPsxAddress(condition.address),
                        wordValue(condition.value)));
                    appendDuckStationOperationRange(
                        lines, controlled, combineConsecutive16BitWrites);
                    lines.push_back(formatCode("00000000", "FFFF"));
                    i = cursor;
                    continue;
                }
            }

            if (combineConsecutive16BitWrites &&
                i + 1U < section.operations.size() &&
                canCombineDuckStationWrite16(
                    section.operations[i], section.operations[i + 1U]))
            {
                const Operation& lowHalf = section.operations[i];
                const Operation& highHalf = section.operations[i + 1U];
                appendDuckStationWrite(
                    lines,
                    32,
                    lowHalf.address,
                    wordValue(highHalf.value) + wordValue(lowHalf.value));
                i += 2U;
                continue;
            }

            appendDuckStationOperation(lines, section.operations[i]);
            ++i;
        }
    }

    inline std::vector<DuckStationPatchSection> collectDuckStationPatchSections(
        const std::vector<Operation>& operations)
    {
        std::vector<DuckStationPatchSection> sections;
        DuckStationPatchSection current;
        bool haveCurrent = false;
        std::string pendingAuthor;
        std::vector<std::string> activeGroups;

        auto flushCurrent = [&]() {
            if (haveCurrent && !current.operations.empty())
                sections.push_back(std::move(current));
            current = {};
            haveCurrent = false;
        };

        auto startUnnamedSection = [&]() {
            current = {};
            current.metadata.groupPath = activeGroups;
            if (!pendingAuthor.empty())
            {
                current.metadata.author = pendingAuthor;
                pendingAuthor.clear();
            }
            haveCurrent = true;
        };

        for (const Operation& operation : operations)
        {
            if (operation.kind == OperationKind::Text)
            {
                std::string groupName;
                if (parseDuckStationGroupOpenLine(operation.text, groupName))
                {
                    flushCurrent();
                    activeGroups.push_back(std::move(groupName));
                    continue;
                }

                if (isDuckStationGroupCloseLine(operation.text))
                {
                    flushCurrent();
                    if (!activeGroups.empty())
                        activeGroups.pop_back();
                    continue;
                }

                DuckStationPatchMetadata metadata;
                if (parseDuckStationCodeNameLine(operation.text, metadata))
                {
                    flushCurrent();
                    current = {};
                    current.metadata = std::move(metadata);
                    current.hasExplicitName = true;
                    if (!activeGroups.empty())
                    {
                        current.metadata.groupPath.insert(
                            current.metadata.groupPath.begin(),
                            activeGroups.begin(),
                            activeGroups.end());
                    }
                    if (!pendingAuthor.empty())
                    {
                        current.metadata.author = pendingAuthor;
                        pendingAuthor.clear();
                    }
                    haveCurrent = true;
                    continue;
                }

                std::string credits;
                if (parseDuckStationCreditsLine(operation.text, credits))
                {
                    if (haveCurrent)
                        current.metadata.author = std::move(credits);
                    else
                        pendingAuthor = std::move(credits);
                    continue;
                }

                std::string propertyValue;
                if (parseDuckStationPropertyLine(operation.text, "Description =", propertyValue))
                {
                    if (haveCurrent)
                        current.metadata.description = std::move(propertyValue);
                    continue;
                }
                if (parseDuckStationPropertyLine(operation.text, "Author =", propertyValue))
                {
                    if (haveCurrent)
                        current.metadata.author = std::move(propertyValue);
                    else
                        pendingAuthor = std::move(propertyValue);
                    continue;
                }
                if (parseDuckStationPropertyLine(operation.text, "Type =", propertyValue))
                {
                    if (haveCurrent)
                        current.metadata.type = std::move(propertyValue);
                    continue;
                }
                if (parseDuckStationPropertyLine(operation.text, "Activation =", propertyValue))
                {
                    if (haveCurrent)
                        current.metadata.activation = std::move(propertyValue);
                    continue;
                }

                const std::string trimmedProperty = trim(operation.text);
                if (startsWithIgnoreCase(trimmedProperty, "Option =") ||
                    startsWithIgnoreCase(trimmedProperty, "OptionRange =") ||
                    startsWithIgnoreCase(trimmedProperty, "DisallowForAchievements =") ||
                    startsWithIgnoreCase(trimmedProperty, "Ignore ="))
                {
                    if (haveCurrent)
                        current.metadata.extraProperties.push_back(trimmedProperty);
                    continue;
                }

                const std::string text = trim(operation.text);
                if (text.empty())
                    continue;

                if (haveCurrent && (text.front() == ';' || text.front() == '#' || startsWithIgnoreCase(text, "//")))
                    current.operations.push_back(operation);
                continue;
            }

            if (!haveCurrent)
                startUnnamedSection();
            current.operations.push_back(operation);
        }
        flushCurrent();
        return sections;
    }

    inline Operation makeSequentialEqual16Condition(const Operation& blockCondition)
    {
        Operation condition;
        condition.kind = OperationKind::CompareEqual16;
        condition.sourceFamily = Family::DuckStation;
        condition.address = blockCondition.address;
        condition.value = wordValue(blockCondition.value);
        condition.sourceLines = blockCondition.sourceLines;
        return condition;
    }

    inline bool isDuckStationBlockCondition(OperationKind kind) noexcept
    {
        return kind == OperationKind::BlockCompareEqual16 ||
               kind == OperationKind::BlockCompareEqual32 ||
               kind == OperationKind::BlockCompareLess8 ||
               kind == OperationKind::BlockCompareGreater8 ||
               kind == OperationKind::BlockCompareLess16 ||
               kind == OperationKind::BlockCompareGreater16 ||
               kind == OperationKind::BlockButtonsEqual ||
               kind == OperationKind::BlockButtonsNotEqual;
    }

    inline std::vector<Operation> atomizeDuckStationBlockOperation(
        const Operation& operation)
    {
        if (operation.kind != OperationKind::Write32)
            return {operation};

        Operation low = operation;
        low.kind = OperationKind::Write16;
        low.value = lowHalf32(operation.value);

        Operation high = operation;
        high.kind = OperationKind::Write16;
        high.address = operation.address + 2U;
        high.value = highHalf32(operation.value);
        high.suffix.clear();

        return {std::move(low), std::move(high)};
    }

    inline std::vector<Operation> expandDuckStationBlockConditionals(
        const std::vector<Operation>& operations)
    {
        std::vector<Operation> output;
        for (std::size_t i = 0; i < operations.size(); ++i)
        {
            const Operation& operation = operations[i];
            if (operation.kind == OperationKind::BlockEnd)
            {
                output.push_back(makeText(
                    "// Warning: unmatched DuckStation block terminator was removed.",
                    Family::DuckStation));
                continue;
            }

            if (!isDuckStationBlockCondition(operation.kind))
            {
                output.push_back(operation);
                continue;
            }

            std::size_t end = i + 1U;
            bool nested = false;
            for (; end < operations.size(); ++end)
            {
                if (isDuckStationBlockCondition(operations[end].kind))
                {
                    nested = true;
                    break;
                }
                if (operations[end].kind == OperationKind::BlockEnd)
                    break;
            }

            if (operation.kind != OperationKind::BlockCompareEqual16 || nested || end >= operations.size())
            {
                std::vector<std::string> sourceLines;
                const std::size_t last = end < operations.size() ? end : i;
                for (std::size_t sourceIndex = i; sourceIndex <= last; ++sourceIndex)
                {
                    const Operation& sourceOperation = operations[sourceIndex];
                    if (!sourceOperation.sourceLines.empty())
                    {
                        sourceLines.insert(
                            sourceLines.end(),
                            sourceOperation.sourceLines.begin(),
                            sourceOperation.sourceLines.end());
                    }
                    else if (sourceOperation.kind == OperationKind::Text && !sourceOperation.text.empty())
                    {
                        sourceLines.push_back(sourceOperation.text);
                    }
                }
                output.push_back(makeDeviceSpecific(
                    std::move(sourceLines),
                    operation.kind == OperationKind::BlockCompareEqual16
                        ? "DuckStation C0 block is nested or missing 00000000 FFFF and cannot be converted safely"
                        : "DuckStation block conditional has no exact destination-device equivalent",
                    Family::DuckStation));
                if (end < operations.size())
                    i = end;
                continue;
            }

            const Operation condition = makeSequentialEqual16Condition(operation);
            for (std::size_t childIndex = i + 1U; childIndex < end; ++childIndex)
            {
                const Operation& child = operations[childIndex];
                if (child.kind == OperationKind::Text)
                {
                    output.push_back(child);
                    continue;
                }

                const std::vector<Operation> atomic =
                    atomizeDuckStationBlockOperation(child);
                for (const Operation& atom : atomic)
                {
                    output.push_back(condition);
                    output.push_back(atom);
                }
            }

            i = end;
        }
        return output;
    }

    // Converts DuckStation [Group\\Subgroup\\Name] patch metadata back into the
    // CMP-style group stack used by the other output families. Consecutive
    // patches sharing a path keep that group open; only changed path levels are
    // closed/opened. Type and Activation are intentionally consumed.
    inline std::vector<Operation> convertDuckStationPatchMetadataToCmpOperations(
        const std::vector<Operation>& operations,
        bool outputCmpDbCompatible)
    {
        const std::vector<DuckStationPatchSection> sections =
            collectDuckStationPatchSections(operations);
        std::vector<Operation> output;
        std::vector<std::string> activeGroups;

        auto commonPrefixLength = [](const std::vector<std::string>& left,
                                     const std::vector<std::string>& right) {
            std::size_t count = 0U;
            while (count < left.size() && count < right.size() &&
                   left[count] == right[count])
            {
                ++count;
            }
            return count;
        };

        for (const DuckStationPatchSection& section : sections)
        {
            const std::size_t common = commonPrefixLength(
                activeGroups, section.metadata.groupPath);

            for (std::size_t i = activeGroups.size(); i > common; --i)
                output.push_back(makeText("!!", Family::DuckStation));
            activeGroups.resize(common);

            for (std::size_t i = common; i < section.metadata.groupPath.size(); ++i)
            {
                const std::string group = sanitizeDuckStationSectionName(
                    section.metadata.groupPath[i]);
                output.push_back(makeText("!" + group + ":", Family::DuckStation));
                activeGroups.push_back(group);
            }

            if (section.hasExplicitName)
            {
                std::string name = sanitizeDuckStationSectionName(section.metadata.name);
                if (!section.metadata.description.empty())
                    name += " {" + cleanDuckStationMetadataText(section.metadata.description) + "}";

                if (outputCmpDbCompatible)
                {
                    output.push_back(makeText("+" + name, Family::DuckStation));
                    if (!section.metadata.author.empty())
                    {
                        output.push_back(makeText(
                            "%Credits: " + cleanDuckStationMetadataText(section.metadata.author),
                            Family::DuckStation));
                    }
                }
                else
                {
                    if (!section.metadata.author.empty())
                    {
                        name += ", by " + cleanDuckStationMetadataText(
                            section.metadata.author);
                    }
                    output.push_back(makeText(name, Family::DuckStation));
                }
            }

            const std::vector<Operation> expandedOperations =
                expandDuckStationBlockConditionals(section.operations);
            output.insert(
                output.end(),
                expandedOperations.begin(),
                expandedOperations.end());
        }

        for (std::size_t i = activeGroups.size(); i > 0U; --i)
            output.push_back(makeText("!!", Family::DuckStation));

        return output;
    }

    inline std::vector<std::string> emitDuckStation(
        const std::vector<Operation>& operations,
        bool combineConsecutive16BitWrites = false,
        bool condenseRepeatedActivators = false)
    {
        const std::vector<DuckStationPatchSection> sections =
            collectDuckStationPatchSections(operations);

        std::vector<std::string> lines;
        for (const DuckStationPatchSection& section : sections)
            appendDuckStationPatchSection(
                lines,
                section,
                combineConsecutive16BitWrites,
                condenseRepeatedActivators);
        return lines;
    }

    inline std::string normalizeDuckStationText(const std::string& input)
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
