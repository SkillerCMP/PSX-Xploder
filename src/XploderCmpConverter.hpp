#pragma once

// XploderCmpConverter.hpp
// Full CMP-style text converter using XploderMemoryCryptEngine.hpp.
//
// Decrypt mode:
//   XplorerPro / FX encrypted text -> canonical external CMP-style RAW text.
//   Type 5/6 headers retain their canonical external size fields. Type 5 uses
//   a direct byte count; Type 6 counts bytes after its first two inline payload bytes.
//   Loader-only expanded sizes stay internal.
//
// Encrypt mode:
//   canonical external CMP-style RAW text -> XplorerPro / FX encrypted text.
//   Type 5 and Type 6 are parsed as complete structured blocks. A Type 5 block
//   immediately following Type 6 is handled as the next block, not as embedded
//   Type 6 payload data.

#include "XploderMemoryCryptEngine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstddef>
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
        // Controls CMP presentation only: + names, %Credits extraction, and
        // $ prefixes on hexadecimal rows. It does not change code semantics.
        bool outputCmpDbCompatible = true;
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


    inline bool startsWithIgnoreCase(std::string_view text, std::string_view prefix) noexcept
    {
        if (text.size() < prefix.size())
            return false;

        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            const unsigned char left = static_cast<unsigned char>(text[i]);
            const unsigned char right = static_cast<unsigned char>(prefix[i]);
            if (std::tolower(left) != std::tolower(right))
                return false;
        }
        return true;
    }

    inline std::size_t findIgnoreCase(
        std::string_view text,
        std::string_view needle,
        std::size_t start = 0) noexcept
    {
        if (needle.empty())
            return start <= text.size() ? start : std::string_view::npos;
        if (start > text.size() || needle.size() > text.size() - start)
            return std::string_view::npos;

        for (std::size_t position = start; position + needle.size() <= text.size(); ++position)
        {
            bool matches = true;
            for (std::size_t i = 0; i < needle.size(); ++i)
            {
                const unsigned char left = static_cast<unsigned char>(text[position + i]);
                const unsigned char right = static_cast<unsigned char>(needle[i]);
                if (std::tolower(left) != std::tolower(right))
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return position;
        }

        return std::string_view::npos;
    }

    inline std::size_t rfindIgnoreCase(
        std::string_view text,
        std::string_view needle,
        std::size_t before = std::string_view::npos) noexcept
    {
        if (needle.empty())
            return std::min(before, text.size());
        if (needle.size() > text.size())
            return std::string_view::npos;

        const std::size_t latestStart = text.size() - needle.size();
        std::size_t start = before == std::string_view::npos
            ? latestStart
            : std::min(before, latestStart);

        for (;;)
        {
            bool matches = true;
            for (std::size_t i = 0; i < needle.size(); ++i)
            {
                const unsigned char left = static_cast<unsigned char>(text[start + i]);
                const unsigned char right = static_cast<unsigned char>(needle[i]);
                if (std::tolower(left) != std::tolower(right))
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return start;
            if (start == 0)
                break;
            --start;
        }

        return std::string_view::npos;
    }

    // Splits title lines such as:
    //   +Infinite Health by Code Master , Crypt: Pro Action Replay/GameShark
    // into a title that keeps its `Crypt:` metadata and a separate CMP credit line.
    // Only the inline `by Author` portion is removed from the title.
    inline bool trySplitBatchInlineCreditLine(
        std::string_view line,
        std::string& title,
        std::string& normalizedCredit)
    {
        title.clear();
        normalizedCredit.clear();

        const std::string t = trim(line);
        if (t.empty())
            return false;

        const std::size_t cryptPosition = findIgnoreCase(t, "crypt:");
        if (cryptPosition == std::string_view::npos)
            return false;

        std::size_t commaPosition = cryptPosition;
        while (commaPosition > 0 &&
               std::isspace(static_cast<unsigned char>(t[commaPosition - 1])))
        {
            --commaPosition;
        }
        if (commaPosition == 0 || t[commaPosition - 1] != ',')
            return false;
        --commaPosition;

        const std::size_t bySpacePosition = rfindIgnoreCase(
            t, " by ", commaPosition);
        const std::size_t byColonPosition = rfindIgnoreCase(
            t, " by:", commaPosition);

        std::size_t byPosition = std::string_view::npos;
        std::size_t authorStart = 0;
        if (bySpacePosition != std::string_view::npos &&
            (byColonPosition == std::string_view::npos || bySpacePosition > byColonPosition))
        {
            byPosition = bySpacePosition;
            authorStart = byPosition + 4U;
        }
        else if (byColonPosition != std::string_view::npos)
        {
            byPosition = byColonPosition;
            authorStart = byPosition + 4U;
        }
        else
        {
            return false;
        }

        const std::string titlePrefix = trim(std::string_view(t).substr(0, byPosition));
        const std::string cryptSuffix = trim(std::string_view(t).substr(commaPosition));
        title = titlePrefix;
        if (!cryptSuffix.empty())
        {
            if (!title.empty())
                title.push_back(' ');
            title += cryptSuffix;
        }

        const std::string author = trim(
            std::string_view(t).substr(authorStart, commaPosition - authorStart));
        if (titlePrefix.empty() || author.empty())
        {
            title.clear();
            return false;
        }

        normalizedCredit = "%Credits: " + author;
        return true;
    }

    inline bool tryNormalizeBatchCreditLine(std::string_view line, std::string& normalized)
    {
        normalized.clear();
        const std::string t = trim(line);
        if (t.empty())
            return false;

        std::string author;
        if (startsWithIgnoreCase(t, "%credits:"))
        {
            author = trim(std::string_view(t).substr(9));
        }
        else if (startsWithIgnoreCase(t, "by:"))
        {
            author = trim(std::string_view(t).substr(3));
        }
        else if (startsWithIgnoreCase(t, "by "))
        {
            author = trim(std::string_view(t).substr(3));
        }
        else
        {
            return false;
        }

        while (!author.empty() && std::isspace(static_cast<unsigned char>(author.back())))
            author.pop_back();
        if (!author.empty() && author.back() == ',')
        {
            author.pop_back();
            author = trim(author);
        }

        if (author.empty())
            return false;

        normalized = "%Credits: " + author;
        return true;
    }

    inline bool tryNormalizeBatchMetadataLine(std::string_view line, std::string& normalized)
    {
        normalized.clear();
        const std::string t = trim(line);
        if (t.empty())
            return false;

        // CMP metadata directives such as ^3 = NAME: and ^2 = GameID:
        // are structure lines, not code names. Remove an accidental leading +
        // from earlier conversions and otherwise preserve the line unchanged.
        if (t[0] == '^')
        {
            normalized = t;
            return true;
        }

        if (t.size() > 1U && t[0] == '+' && t[1] == '^')
        {
            normalized = t.substr(1U);
            return true;
        }

        return false;
    }

    // DuckStation extended cheat rows use an 8-hex address and an 8-character
    // value field (16 total code characters), for example:
    //   9006D9D8 EF6FF7C8
    //   9000C03C 240B????
    // They are not Xploder 6-byte records and must never be decrypted as one.
    // Normalize their display for folder batches and preserve them unchanged in
    // the normal conversion pass. Wildcards are allowed only in the value field.
    inline bool tryNormalizeDuckStationCodeLine(
        std::string_view line,
        std::string& normalized)
    {
        normalized.clear();
        const std::string t = trim(line);
        if (t.empty())
            return false;

        std::size_t position = 0;
        if (t[position] == '$')
        {
            ++position;
            while (position < t.size() &&
                   std::isspace(static_cast<unsigned char>(t[position])))
            {
                ++position;
            }
        }

        auto appendAddressHex = [&](std::size_t start, std::string& destination) -> bool
        {
            if (start + 8U > t.size())
                return false;
            for (std::size_t i = 0; i < 8U; ++i)
            {
                const char c = t[start + i];
                if (!isHex(c))
                    return false;
                destination.push_back(static_cast<char>(
                    std::toupper(static_cast<unsigned char>(c))));
            }
            return true;
        };

        auto appendValue = [&](std::size_t start, std::string& destination) -> bool
        {
            if (start + 8U > t.size())
                return false;
            for (std::size_t i = 0; i < 8U; ++i)
            {
                const char c = t[start + i];
                if (c == '?')
                {
                    destination.push_back('?');
                }
                else if (isHex(c))
                {
                    destination.push_back(static_cast<char>(
                        std::toupper(static_cast<unsigned char>(c))));
                }
                else
                {
                    return false;
                }
            }
            return true;
        };

        std::string address;
        address.reserve(8U);
        if (!appendAddressHex(position, address))
            return false;

        std::size_t valueStart = position + 8U;
        const std::size_t separatorStart = valueStart;
        while (valueStart < t.size() &&
               std::isspace(static_cast<unsigned char>(t[valueStart])))
        {
            ++valueStart;
        }

        // Accept both the normal spaced form and compact 16-character form.
        // A non-space separator is not a DuckStation row.
        if (valueStart == separatorStart && valueStart < t.size() &&
            !isHex(t[valueStart]) && t[valueStart] != '?')
        {
            return false;
        }

        std::string value;
        value.reserve(8U);
        if (!appendValue(valueStart, value))
            return false;

        const std::size_t end = valueStart + 8U;
        if (end < t.size() && (isHex(t[end]) || t[end] == '?'))
            return false;
        if (end < t.size() &&
            !std::isspace(static_cast<unsigned char>(t[end])) &&
            t[end] != ';' && t[end] != '/' && t[end] != '#')
        {
            return false;
        }

        normalized = "$" + address + " " + value;
        const std::string suffix = trim(std::string_view(t).substr(end));
        if (!suffix.empty())
            normalized += "\t" + suffix;
        return true;
    }

    inline bool tryNormalizeBatchCodeLine(std::string_view line, std::string& normalized)
    {
        normalized.clear();
        const std::string t = trim(line);
        if (t.empty())
            return false;

        // Check DuckStation's 8+8 format before Xploder's shorter 8+4 and
        // wildcard forms so the first 12 characters are never misread as a
        // complete Xploder code.
        if (tryNormalizeDuckStationCodeLine(t, normalized))
            return true;

        std::size_t position = 0;
        if (t[position] == '$')
        {
            ++position;
            while (position < t.size() && std::isspace(static_cast<unsigned char>(t[position])))
                ++position;
        }

        auto appendHex = [&](std::size_t start, std::size_t count, std::string& destination) -> bool
        {
            if (start + count > t.size())
                return false;
            for (std::size_t i = 0; i < count; ++i)
            {
                const char c = t[start + i];
                if (!isHex(c))
                    return false;
                destination.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            return true;
        };

        auto appendWildcardValue = [&](std::size_t start, std::size_t count, std::string& destination) -> bool
        {
            if (start + count > t.size())
                return false;

            bool containsWildcard = false;
            for (std::size_t i = 0; i < count; ++i)
            {
                const char c = t[start + i];
                if (c == '?')
                {
                    containsWildcard = true;
                    destination.push_back('?');
                }
                else if (isHex(c))
                {
                    destination.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                }
                else
                {
                    return false;
                }
            }
            return containsWildcard;
        };

        auto validCodeEnd = [&](std::size_t end) -> bool
        {
            if (end < t.size() && (isHex(t[end]) || t[end] == '?'))
                return false;

            return end >= t.size() ||
                std::isspace(static_cast<unsigned char>(t[end])) ||
                t[end] == ';' || t[end] == '/' || t[end] == '#';
        };

        auto finishWildcard = [&](const std::string& address, const std::string& value, std::size_t end) -> bool
        {
            if (!validCodeEnd(end))
                return false;

            normalized = "$" + address + " " + value;
            const std::string suffix = trim(std::string_view(t).substr(end));
            if (!suffix.empty())
                normalized += "\t" + suffix;
            return true;
        };

        // Wildcard values are display/template rows and are intentionally not
        // decrypted. Accept either two or four value characters, compact or
        // separated, and allow mixed hexadecimal/question-mark placeholders.
        // Examples: 700FB57E??, 700FB57E ????, 700F-B57E-12??.
        {
            std::string address;
            address.reserve(8);

            std::size_t valueStart = std::string_view::npos;
            if (position + 10U <= t.size() &&
                t[position + 4U] == '-' && t[position + 9U] == '-' &&
                appendHex(position, 4U, address) &&
                appendHex(position + 5U, 4U, address))
            {
                valueStart = position + 10U;
            }
            else
            {
                // Space-grouped 4 4 4 form, for example:
                // 8007 8448 ???? or 8007 8448 ??
                address.clear();
                std::string groupedAddress;
                groupedAddress.reserve(8);
                if (appendHex(position, 4U, groupedAddress))
                {
                    std::size_t secondGroup = position + 4U;
                    const std::size_t firstSeparator = secondGroup;
                    while (secondGroup < t.size() &&
                           std::isspace(static_cast<unsigned char>(t[secondGroup])))
                    {
                        ++secondGroup;
                    }

                    if (secondGroup > firstSeparator &&
                        appendHex(secondGroup, 4U, groupedAddress))
                    {
                        std::size_t groupedValue = secondGroup + 4U;
                        const std::size_t secondSeparator = groupedValue;
                        while (groupedValue < t.size() &&
                               std::isspace(static_cast<unsigned char>(t[groupedValue])))
                        {
                            ++groupedValue;
                        }

                        if (groupedValue > secondSeparator)
                        {
                            address = std::move(groupedAddress);
                            valueStart = groupedValue;
                        }
                    }
                }

                if (valueStart == std::string_view::npos)
                {
                    address.clear();
                    if (appendHex(position, 8U, address))
                    {
                        valueStart = position + 8U;
                        while (valueStart < t.size() &&
                               std::isspace(static_cast<unsigned char>(t[valueStart])))
                        {
                            ++valueStart;
                        }
                    }
                }
            }

            if (valueStart != std::string_view::npos)
            {
                for (const std::size_t valueLength : {4U, 2U})
                {
                    std::string value;
                    value.reserve(valueLength);
                    if (appendWildcardValue(valueStart, valueLength, value) &&
                        finishWildcard(address, value, valueStart + valueLength))
                    {
                        return true;
                    }
                }
            }
        }

        // RAW 8-bit write shorthand may use an 8-digit address with a
        // two-character value, for example 3007DE60 00. Recognize the same
        // compact, hyphen-grouped, and space-grouped layouts as the normal
        // 8 + 4 form. These rows are normalized for display and intentionally
        // pass through decrypt mode unchanged because they are already RAW.
        {
            std::string address;
            address.reserve(8);
            std::size_t valueStart = std::string_view::npos;

            // 3007-DE60-00
            if (position + 12U <= t.size() &&
                t[position + 4U] == '-' && t[position + 9U] == '-' &&
                appendHex(position, 4U, address) &&
                appendHex(position + 5U, 4U, address))
            {
                valueStart = position + 10U;
            }
            else
            {
                // 3007 DE60 00
                address.clear();
                std::string groupedAddress;
                groupedAddress.reserve(8);
                if (appendHex(position, 4U, groupedAddress))
                {
                    std::size_t secondGroup = position + 4U;
                    const std::size_t firstSeparator = secondGroup;
                    while (secondGroup < t.size() &&
                           std::isspace(static_cast<unsigned char>(t[secondGroup])))
                    {
                        ++secondGroup;
                    }

                    if (secondGroup > firstSeparator &&
                        appendHex(secondGroup, 4U, groupedAddress))
                    {
                        std::size_t groupedValue = secondGroup + 4U;
                        const std::size_t secondSeparator = groupedValue;
                        while (groupedValue < t.size() &&
                               std::isspace(static_cast<unsigned char>(t[groupedValue])))
                        {
                            ++groupedValue;
                        }

                        if (groupedValue > secondSeparator)
                        {
                            address = std::move(groupedAddress);
                            valueStart = groupedValue;
                        }
                    }
                }

                if (valueStart == std::string_view::npos)
                {
                    address.clear();
                    if (appendHex(position, 8U, address))
                    {
                        valueStart = position + 8U;
                        while (valueStart < t.size() &&
                               std::isspace(static_cast<unsigned char>(t[valueStart])))
                        {
                            ++valueStart;
                        }
                    }
                }
            }

            if (valueStart != std::string_view::npos)
            {
                std::string value;
                value.reserve(2U);
                const std::size_t end = valueStart + 2U;
                if (appendHex(valueStart, 2U, value) && validCodeEnd(end))
                {
                    normalized = "$" + address + " " + value;
                    const std::string suffix = trim(std::string_view(t).substr(end));
                    if (!suffix.empty())
                        normalized += "\t" + suffix;
                    return true;
                }
            }
        }

        std::string hex;
        hex.reserve(12);
        std::size_t end = position;

        // Grouped display style: 1234-5678-9ABC.
        if (position + 14U <= t.size() &&
            t[position + 4U] == '-' && t[position + 9U] == '-')
        {
            std::string candidate;
            candidate.reserve(12);
            if (appendHex(position, 4U, candidate) &&
                appendHex(position + 5U, 4U, candidate) &&
                appendHex(position + 10U, 4U, candidate))
            {
                hex = std::move(candidate);
                end = position + 14U;
            }
        }

        // Space-grouped display style: 1234 5678 9ABC.
        if (hex.empty())
        {
            std::string candidate;
            candidate.reserve(12);
            if (appendHex(position, 4U, candidate))
            {
                std::size_t secondGroup = position + 4U;
                const std::size_t firstSeparator = secondGroup;
                while (secondGroup < t.size() &&
                       std::isspace(static_cast<unsigned char>(t[secondGroup])))
                {
                    ++secondGroup;
                }

                if (secondGroup > firstSeparator &&
                    appendHex(secondGroup, 4U, candidate))
                {
                    std::size_t thirdGroup = secondGroup + 4U;
                    const std::size_t secondSeparator = thirdGroup;
                    while (thirdGroup < t.size() &&
                           std::isspace(static_cast<unsigned char>(t[thirdGroup])))
                    {
                        ++thirdGroup;
                    }

                    if (thirdGroup > secondSeparator &&
                        appendHex(thirdGroup, 4U, candidate))
                    {
                        hex = std::move(candidate);
                        end = thirdGroup + 4U;
                    }
                }
            }
        }

        // Compact 12-hex display style: 123456789ABC.
        if (hex.empty())
        {
            std::string candidate;
            candidate.reserve(12);
            if (appendHex(position, 12U, candidate))
            {
                hex = std::move(candidate);
                end = position + 12U;
            }
        }

        // CMP display style: 12345678 9ABC.
        if (hex.empty())
        {
            std::string candidate;
            candidate.reserve(12);
            if (appendHex(position, 8U, candidate))
            {
                std::size_t value = position + 8U;
                const std::size_t separator = value;
                while (value < t.size() && std::isspace(static_cast<unsigned char>(t[value])))
                    ++value;

                if (value > separator && appendHex(value, 4U, candidate))
                {
                    hex = std::move(candidate);
                    end = value + 4U;
                }
            }
        }

        if (hex.size() != 12U)
            return false;

        if (!validCodeEnd(end))
            return false;

        normalized = "$" + hex.substr(0, 8) + " " + hex.substr(8, 4);
        const std::string suffix = trim(std::string_view(t).substr(end));
        if (!suffix.empty())
            normalized += "\t" + suffix;
        return true;
    }

    // Folder-drop cleanup performed before the normal decrypt pass. This does
    // not define a new conversion format: it normalizes textual code lines and,
    // when requested, applies CMP-style author metadata before decryption.
    inline std::string normalizeBatchDecryptInput(
        const std::string& input,
        bool outputCmpDbCompatible = true)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<std::string> output;
        output.reserve(lines.size());

        // Plain-output mode keeps code names and author text exactly in their
        // normal readable form. It still normalizes code spacing/case for the
        // decrypt engine, but deliberately does not add '+', create %Credits:,
        // or retain the CMP '$' marker.
        if (!outputCmpDbCompatible)
        {
            for (const std::string& original : lines)
            {
                std::string metadata;
                if (tryNormalizeBatchMetadataLine(original, metadata))
                {
                    output.push_back(metadata);
                    continue;
                }

                std::string code;
                if (tryNormalizeBatchCodeLine(original, code))
                {
                    if (!code.empty() && code.front() == '$')
                        code.erase(code.begin());
                    output.push_back(std::move(code));
                    continue;
                }

                output.push_back(original);
            }

            std::ostringstream joined;
            for (std::size_t i = 0; i < output.size(); ++i)
            {
                if (i != 0)
                    joined << '\n';
                joined << output[i];
            }
            return joined.str();
        }

        std::vector<std::string> pendingCredits;
        bool inCodeRun = false;
        std::size_t firstCodeIndex = 0;

        for (const std::string& original : lines)
        {
            std::string metadata;
            if (tryNormalizeBatchMetadataLine(original, metadata))
            {
                if (inCodeRun)
                    inCodeRun = false;
                output.push_back(metadata);
                continue;
            }

            std::string credit;
            std::string inlineTitle;
            if (trySplitBatchInlineCreditLine(original, inlineTitle, credit))
            {
                if (inCodeRun)
                    inCodeRun = false;

                // Inline credits already identify their code-name line, so write
                // the normalized credit immediately below that title. This also
                // keeps joker templates such as `+D00981DA ????` from carrying
                // the credit into the next normal code entry.
                for (const std::string& pending : pendingCredits)
                    output.push_back(pending);
                pendingCredits.clear();
                output.push_back(inlineTitle);
                output.push_back(credit);
                continue;
            }

            if (tryNormalizeBatchCreditLine(original, credit))
            {
                if (inCodeRun)
                {
                    output.insert(output.begin() + static_cast<std::ptrdiff_t>(firstCodeIndex), credit);
                    ++firstCodeIndex;
                }
                else
                {
                    pendingCredits.push_back(credit);
                }
                continue;
            }

            std::string code;
            if (tryNormalizeBatchCodeLine(original, code))
            {
                if (!inCodeRun)
                {
                    while (!output.empty() && trim(output.back()).empty())
                        output.pop_back();

                    for (const std::string& pending : pendingCredits)
                        output.push_back(pending);
                    pendingCredits.clear();

                    firstCodeIndex = output.size();
                    inCodeRun = true;
                }

                output.push_back(code);
                continue;
            }

            if (inCodeRun)
                inCodeRun = false;

            output.push_back(original);
        }

        for (const std::string& pending : pendingCredits)
            output.push_back(pending);

        std::ostringstream joined;
        for (std::size_t i = 0; i < output.size(); ++i)
        {
            if (i != 0)
                joined << '\n';
            joined << output[i];
        }
        return joined.str();
    }

    inline bool isOutputMetadataOrCommentLine(std::string_view line)
    {
        const std::string text = trim(line);
        if (text.empty())
            return true;

        if (startsWith(text, "!!") || text[0] == '!' || text[0] == '^' ||
            text[0] == ';' || startsWith(text, "//") || text[0] == '[')
        {
            return true;
        }

        return startsWithIgnoreCase(text, "Type =") ||
               startsWithIgnoreCase(text, "Activation =") ||
               startsWithIgnoreCase(text, "Description =") ||
               startsWithIgnoreCase(text, "Author =") ||
               startsWithIgnoreCase(text, "OptionRange =") ||
               startsWithIgnoreCase(text, "ERROR:") ||
               startsWithIgnoreCase(text, "WARNING:");
    }

    // Extract an inline author from a normal code-name line while retaining an
    // optional trailing `, Crypt: ...` section on the title. Unlike the older
    // folder helper, this also supports ordinary `Name by Author` lines that do
    // not contain Crypt metadata.
    inline bool trySplitOutputInlineCreditLine(
        std::string_view line,
        std::string& title,
        std::string& normalizedCredit)
    {
        title.clear();
        normalizedCredit.clear();

        std::string text = trim(line);
        if (text.empty())
            return false;
        if (text.front() == '+')
            text = trim(std::string_view(text).substr(1));
        if (text.empty() || isOutputMetadataOrCommentLine(text))
            return false;

        std::string ignoredCode;
        if (tryNormalizeBatchCodeLine(text, ignoredCode))
            return false;

        const std::size_t cryptPosition = findIgnoreCase(text, "crypt:");
        std::size_t authorLimit = cryptPosition == std::string_view::npos
            ? text.size()
            : cryptPosition;

        // If Crypt metadata is present, exclude its preceding comma and spaces
        // from the author field while keeping that suffix on the title.
        std::size_t cryptSuffixStart = std::string_view::npos;
        if (cryptPosition != std::string_view::npos)
        {
            cryptSuffixStart = cryptPosition;
            while (cryptSuffixStart > 0U &&
                   std::isspace(static_cast<unsigned char>(text[cryptSuffixStart - 1U])))
            {
                --cryptSuffixStart;
            }
            if (cryptSuffixStart > 0U && text[cryptSuffixStart - 1U] == ',')
            {
                --cryptSuffixStart;
                while (cryptSuffixStart > 0U &&
                       std::isspace(static_cast<unsigned char>(text[cryptSuffixStart - 1U])))
                {
                    --cryptSuffixStart;
                }
            }
            authorLimit = cryptSuffixStart;
        }

        const std::size_t bySpacePosition = rfindIgnoreCase(text, " by ", authorLimit);
        const std::size_t byColonPosition = rfindIgnoreCase(text, " by:", authorLimit);

        std::size_t byPosition = std::string_view::npos;
        std::size_t authorStart = 0U;
        if (bySpacePosition != std::string_view::npos &&
            (byColonPosition == std::string_view::npos || bySpacePosition > byColonPosition))
        {
            byPosition = bySpacePosition;
            authorStart = byPosition + 4U;
        }
        else if (byColonPosition != std::string_view::npos)
        {
            byPosition = byColonPosition;
            authorStart = byPosition + 4U;
        }
        else
        {
            return false;
        }

        std::string author = trim(std::string_view(text).substr(
            authorStart, authorLimit - authorStart));
        while (!author.empty() && (author.back() == ',' || author.back() == ';'))
        {
            author.pop_back();
            author = trim(author);
        }

        const std::string name = trim(std::string_view(text).substr(0, byPosition));
        if (name.empty() || author.empty())
            return false;

        title = name;
        if (cryptSuffixStart != std::string_view::npos)
        {
            const std::string cryptSuffix = trim(
                std::string_view(text).substr(cryptSuffixStart));
            if (!cryptSuffix.empty())
                title += " " + cryptSuffix;
        }

        normalizedCredit = "%Credits: " + author;
        return true;
    }

    // Final output-style pass shared by direct Xploder conversion, all editor
    // code-family emitters, and folder batch output. Semantic conversion is
    // complete before this runs; this function changes only CMP DB presentation.
    inline std::string applyOutputCmpDbFormatting(
        const std::string& input,
        bool outputCmpDbCompatible)
    {
        const std::vector<std::string> lines = splitLines(input);
        std::vector<std::string> output;
        output.reserve(lines.size() + 8U);

        for (const std::string& original : lines)
        {
            const std::string text = trim(original);
            if (text.empty())
            {
                output.push_back({});
                continue;
            }

            std::string normalizedCode;
            if (tryNormalizeBatchCodeLine(text, normalizedCode))
            {
                std::string body = text;
                if (!body.empty() && body.front() == '$')
                    body = trim(std::string_view(body).substr(1));
                output.push_back(outputCmpDbCompatible ? "$" + body : body);
                continue;
            }

            // Existing comments, game metadata, DuckStation section fields,
            // and other structural directives are not code names.
            if (isOutputMetadataOrCommentLine(text))
            {
                output.push_back(text);
                continue;
            }

            // Credit-only lines are normalized only when CMP DB mode is on.
            std::string normalizedCredit;
            if (outputCmpDbCompatible &&
                tryNormalizeBatchCreditLine(text, normalizedCredit))
            {
                output.push_back(normalizedCredit);
                continue;
            }

            std::string plainName = text;
            if (!plainName.empty() && plainName.front() == '+')
                plainName = trim(std::string_view(plainName).substr(1));

            if (!outputCmpDbCompatible)
            {
                output.push_back(plainName);
                continue;
            }

            std::string title;
            if (trySplitOutputInlineCreditLine(plainName, title, normalizedCredit))
            {
                output.push_back("+" + title);
                output.push_back(normalizedCredit);
            }
            else
            {
                output.push_back("+" + plainName);
            }
        }

        std::ostringstream joined;
        for (std::size_t i = 0; i < output.size(); ++i)
        {
            if (i != 0)
                joined << '\n';
            joined << output[i];
        }
        return joined.str();
    }

    inline std::string formatEncrypted(const xploder_psx::Code& code, bool grouped)
    {
        // Default CMP/Xploder display uses an 8-digit code word followed by
        // the 4-digit value. The optional grouped form remains 4-4-4.
        return grouped ? xploder_psx::toGroupedEncryptedText(code) : xploder_psx::toRawCmpText(code);
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

    inline std::string hex8(std::uint32_t value)
    {
        static constexpr char lut[] = "0123456789ABCDEF";
        std::string s(8, '0');
        for (int i = 7; i >= 0; --i)
        {
            s[static_cast<std::size_t>(i)] = lut[value & 0xF];
            value >>= 4;
        }
        return s;
    }

    inline std::uint32_t readBigEndian32(
        const xploder_psx::Code& row,
        std::size_t offset = 0U) noexcept
    {
        return
            (static_cast<std::uint32_t>(row[offset]) << 24) |
            (static_cast<std::uint32_t>(row[offset + 1U]) << 16) |
            (static_cast<std::uint32_t>(row[offset + 2U]) << 8) |
            static_cast<std::uint32_t>(row[offset + 3U]);
    }

    inline int readBigEndian16(
        const xploder_psx::Code& row,
        std::size_t offset) noexcept
    {
        return
            (static_cast<int>(row[offset]) << 8) |
            static_cast<int>(row[offset + 1U]);
    }

    inline std::string buildType6SourceRowComment(
        const xploder_psx::MassWriteInfo& info,
        int rowIndex,
        const xploder_psx::Code& rawRow,
        bool encryptedOutput = false)
    {
        std::ostringstream ss;
        ss << "// ";
        if (encryptedOutput)
            ss << "encrypted row; RAW meaning: ";

        if (rowIndex == 0)
        {
            ss << "Type 6 breakpoint descriptor"
               << " | breakAddress=0x" << hex8(readBigEndian32(rawRow))
               << " breakType=0x" << hex4(readBigEndian16(rawRow, 4U));
            return ss.str();
        }

        const int sourceStart =
            rowIndex * static_cast<int>(xploder_psx::CodeLength);
        const int sourceBytesUsed = std::min(
            static_cast<int>(xploder_psx::CodeLength),
            std::max(0, info.sourcePayloadSize - sourceStart));
        const int paddingBytes =
            static_cast<int>(xploder_psx::CodeLength) - sourceBytesUsed;

        int payloadStartForRow = -1;
        int payloadBytesForRow = 0;

        if (rowIndex == 1)
        {
            ss << "Type 6 breakpoint mask=0x"
               << hex8(readBigEndian32(rawRow));

            payloadStartForRow = 0;
            payloadBytesForRow = std::min(2, info.payloadByteCount);
            if (payloadBytesForRow > 0)
            {
                ss << " | payload bytes 0000-"
                   << hex4(payloadBytesForRow - 1);
            }
        }
        else
        {
            payloadStartForRow = sourceStart - 0x0A;
            payloadBytesForRow = std::min(
                static_cast<int>(xploder_psx::CodeLength),
                std::max(0, info.payloadByteCount - payloadStartForRow));

            if (payloadBytesForRow > 0)
            {
                ss << "Type 6 payload bytes "
                   << hex4(payloadStartForRow) << '-'
                   << hex4(payloadStartForRow + payloadBytesForRow - 1);
            }
            else
            {
                ss << "Type 6 padding row";
            }
        }

        if (paddingBytes > 0)
            ss << " | paddingBytes=" << paddingBytes;

        const int leadingNibble = rawRow[0] >> 4;
        if (leadingNibble == 9 && payloadBytesForRow == static_cast<int>(xploder_psx::CodeLength))
        {
            const std::uint32_t codeWord = readBigEndian32(rawRow);
            const std::uint32_t compareAddress = codeWord & 0x0FFFFFFFU;
            const int compareValue = readBigEndian16(rawRow, 4U);
            ss << " | embedded Type 9 record inside Type 6 payload"
               << " | compareAddress=0x" << hex8(compareAddress)
               << " compareValue=0x" << hex4(compareValue);
        }
        else if (leadingNibble != 0)
        {
            ss << " | leading nibble " << std::uppercase << std::hex
               << leadingNibble
               << " is structured Type 6 data, not a standalone code type";
        }
        return ss.str();
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
            case 0x9: ss << "skip following cheat if 16-bit value is equal"
                         << " | if [0x" << hex7(address) << "] == 0x"
                         << hex4(value) << " then skip (otherwise execute)"; break;
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

    inline bool tryPreserveRawMassWriteBlock(
        const std::vector<std::string>& lines,
        std::size_t startIndex,
        const Options& options,
        std::string& preservedBlock,
        std::size_t& lastConsumedIndex)
    {
        preservedBlock.clear();
        lastConsumedIndex = startIndex;

        std::string headerBody;
        if (!tryGetCodeBody(lines[startIndex], headerBody))
            return false;

        xploder_psx::Code header;
        if (!parseCodeBody(headerBody, Mode::Decrypt, header))
            return false;

        const int codeType = header[0] & 0xF0;
        if ((codeType != 0x50 && codeType != 0x60) ||
            (header[0] & 0x0F) != 0 ||
            xploder_psx::routeForCode(header) != xploder_psx::Route::Copy)
        {
            return false;
        }

        xploder_psx::MassWriteInfo info;
        if (!xploder_psx::tryGetMassWriteInfoFromPublicHeader(header, info))
            return false;

        if (info.isType6)
        {
            // Key 6/7 Type 6 rows still need payload decryption. Only preserve
            // copy-route RAW Type 6 blocks here.
            if (isConfirmedMassWritePayloadKey(info.payloadKey))
                return false;
        }
        else
        {
            // Canonical external RAW Type 5 headers are keyless. A high-nibble
            // key means this is a public header that still needs conversion.
            if (info.payloadKey != 0)
                return false;
        }

        if (info.payloadLineCount <= 0 ||
            startIndex + static_cast<std::size_t>(info.payloadLineCount) >= lines.size())
        {
            return false;
        }

        std::vector<xploder_psx::Code> rows;
        rows.reserve(static_cast<std::size_t>(info.payloadLineCount));
        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            const std::size_t lineIndex =
                startIndex + 1U + static_cast<std::size_t>(i);
            std::string body;
            if (!tryGetCodeBody(lines[lineIndex], body))
                return false;

            xploder_psx::Code row;
            if (!parseCodeBody(body, Mode::Decrypt, row))
                return false;
            rows.push_back(row);
        }

        std::ostringstream out;
        out << '$' << xploder_psx::toRawCmpText(header);
        if (options.annotateCodeTypes)
        {
            if (info.isType6)
            {
                out << "\t// already-RAW Type 6 header"
                    << " | continuationBytes=0x" << hex4(info.payloadSize)
                    << " payloadBytes=0x" << hex4(info.payloadByteCount)
                    << " descriptorBytes=0x000A"
                    << " sourceRows=" << info.payloadLineCount;
            }
            else
            {
                out << "\t// already-RAW Type 5 header"
                    << " | payloadBytes=0x" << hex4(info.payloadByteCount)
                    << " sourceRows=" << info.payloadLineCount;
            }
        }

        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            const xploder_psx::Code& row =
                rows[static_cast<std::size_t>(i)];
            out << "\n$" << xploder_psx::toRawCmpText(row);

            if (options.annotateCodeTypes)
            {
                if (info.isType6)
                {
                    out << "\t"
                        << buildType6SourceRowComment(info, i, row);
                }
                else
                {
                    const int firstByteOffset =
                        i * static_cast<int>(xploder_psx::CodeLength);
                    const int bytesUsed = std::min(
                        static_cast<int>(xploder_psx::CodeLength),
                        std::max(0, info.sourcePayloadSize - firstByteOffset));
                    out << "\t// preserved Type 5 payload bytes "
                        << hex4(firstByteOffset) << '-'
                        << hex4(firstByteOffset + bytesUsed - 1);
                }
            }
        }

        preservedBlock = out.str();
        lastConsumedIndex =
            startIndex + static_cast<std::size_t>(info.payloadLineCount);
        return true;
    }

    inline bool tryConvertMassWriteBlock(
        const std::vector<std::string>& lines,
        std::size_t startIndex,
        const Options& options,
        std::string& convertedBlock,
        std::size_t& lastConsumedIndex)
    {
        convertedBlock.clear();
        lastConsumedIndex = startIndex;

        std::string headerBody;
        if (!tryGetCodeBody(lines[startIndex], headerBody))
            return false;

        xploder_psx::Code rawHeader;
        if (!parseCodeBody(headerBody, Mode::Decrypt, rawHeader))
            return false;

        xploder_psx::MassWriteInfo info;
        if (!xploder_psx::decryptMassWriteHeaderToActive(rawHeader, info))
            return false;

        if (!info.isType6 &&
            !isConfirmedMassWritePayloadKey(info.payloadKey))
        {
            return false;
        }

        if (info.sourcePayloadSize <= 0 || info.payloadLineCount <= 0 ||
            startIndex + static_cast<std::size_t>(info.payloadLineCount) >= lines.size())
        {
            return false;
        }

        std::vector<xploder_psx::Code> rows;
        rows.reserve(static_cast<std::size_t>(info.payloadLineCount));
        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            const std::size_t lineIndex =
                startIndex + 1U + static_cast<std::size_t>(i);
            std::string body;
            if (!tryGetCodeBody(lines[lineIndex], body))
                return false;

            xploder_psx::Code row;
            if (!parseCodeBody(body, Mode::Decrypt, row))
                return false;
            rows.push_back(row);
        }

        std::ostringstream out;
        out << '$' << xploder_psx::toRawCmpText(rawHeader);
        if (options.annotateCodeTypes)
        {
            if (info.isType6)
            {
                out << "\t// Type 6 external RAW header"
                    << " | payloadKey=" << info.payloadKey
                    << " continuationBytes=0x" << hex4(info.payloadSize)
                    << " payloadBytes=0x" << hex4(info.payloadByteCount)
                    << " descriptorBytes=0x000A"
                    << " sourceRows=" << info.payloadLineCount;
            }
            else
            {
                out << "\t// Type 5 external RAW header"
                    << " | payloadKey=" << info.payloadKey
                    << " payloadBytes=0x" << hex4(info.payloadByteCount)
                    << " sourceRows=" << info.payloadLineCount;
            }
        }

        for (int i = 0; i < info.payloadLineCount; ++i)
        {
            xploder_psx::Code payload =
                rows[static_cast<std::size_t>(i)];

            if (isConfirmedMassWritePayloadKey(info.payloadKey))
            {
                if (!xploder_psx::decryptPayloadChunk(
                        payload, info.payloadKey))
                {
                    return false;
                }

                if (!info.isType6)
                    xploder_psx::swapType5PayloadByteOrder(payload);
            }
            else if (!info.isType6)
            {
                return false;
            }
            // Type 6 routes other than 6/7 are copy routes.

            out << "\n$" << xploder_psx::toRawCmpText(payload);
            if (options.annotateCodeTypes)
            {
                if (info.isType6)
                {
                    out << "\t"
                        << buildType6SourceRowComment(info, i, payload);
                }
                else
                {
                    const int firstByteOffset =
                        i * static_cast<int>(xploder_psx::CodeLength);
                    const int bytesUsed = std::min(
                        static_cast<int>(xploder_psx::CodeLength),
                        std::max(0, info.sourcePayloadSize - firstByteOffset));
                    out << "\t// Type 5 payload bytes "
                        << hex4(firstByteOffset) << '-'
                        << hex4(firstByteOffset + bytesUsed - 1);
                }
            }
        }

        convertedBlock = out.str();
        lastConsumedIndex =
            startIndex + static_cast<std::size_t>(info.payloadLineCount);
        return true;
    }

    inline bool tryEncryptMassWriteBlock(
        const std::vector<std::string>& lines,
        std::size_t startIndex,
        const Options& options,
        std::string& convertedBlock,
        std::size_t& lastConsumedIndex)
    {
        convertedBlock.clear();
        lastConsumedIndex = startIndex;

        std::string headerBody;
        if (!tryGetCodeBody(lines[startIndex], headerBody))
            return false;

        xploder_psx::Code rawHeader;
        if (!parseCodeBody(headerBody, Mode::Encrypt, rawHeader))
            return false;

        const int codeType = rawHeader[0] & 0xF0;
        if (codeType != 0x50 && codeType != 0x60)
            return false;

        const bool isType6 = (codeType == 0x60);
        xploder_psx::MassWriteInfo rawInfo;
        if (!xploder_psx::tryGetMassWriteInfoFromPublicHeader(rawHeader, rawInfo))
            return false;

        const int baseSize = rawInfo.payloadSize;
        const int sourcePayloadSize = rawInfo.sourcePayloadSize;
        const int payloadLineCount = rawInfo.payloadLineCount;

        if (sourcePayloadSize <= 0 || payloadLineCount <= 0 ||
            startIndex + static_cast<std::size_t>(payloadLineCount) >= lines.size())
        {
            return false;
        }

        const int effectivePayloadKey =
            isType6 ? (rawHeader[3] & 0x0F) : options.massWritePayloadKey;

        if (!isType6 &&
            !isConfirmedMassWritePayloadKey(effectivePayloadKey))
        {
            return false;
        }

        std::vector<xploder_psx::Code> rows;
        rows.reserve(static_cast<std::size_t>(payloadLineCount));
        for (int i = 0; i < payloadLineCount; ++i)
        {
            const std::size_t lineIndex =
                startIndex + 1U + static_cast<std::size_t>(i);
            xploder_psx::Code row;
            int byteCount = 0;
            if (!parsePayloadLineBytes(lines[lineIndex], row, byteCount) ||
                byteCount == 0)
            {
                return false;
            }
            rows.push_back(row);
        }

        xploder_psx::Code encryptedHeader = rawHeader;
        xploder_psx::MassWriteInfo info;
        if (!xploder_psx::encryptMassWriteHeaderFromActive(
                encryptedHeader,
                options.encryptionKey,
                effectivePayloadKey,
                &info))
        {
            return false;
        }

        std::ostringstream out;
        out << '$' << formatEncrypted(
            encryptedHeader, options.groupEncryptedOutput);
        if (options.annotateCodeTypes)
        {
            if (isType6)
            {
                out << "\t// Type 6 encrypted header"
                    << " | payloadKey=" << effectivePayloadKey
                    << " continuationBytes=0x" << hex4(baseSize)
                    << " payloadBytes=0x" << hex4(rawInfo.payloadByteCount)
                    << " descriptorBytes=0x000A"
                    << " sourceRows=" << payloadLineCount;
            }
            else
            {
                out << "\t// Type 5 encrypted header"
                    << " | payloadKey=" << effectivePayloadKey
                    << " payloadBytes=0x" << hex4(rawInfo.payloadByteCount)
                    << " sourceRows=" << payloadLineCount;
            }
        }

        for (int i = 0; i < payloadLineCount; ++i)
        {
            const xploder_psx::Code rawPayload =
                rows[static_cast<std::size_t>(i)];
            xploder_psx::Code payload = rawPayload;

            if (isConfirmedMassWritePayloadKey(effectivePayloadKey))
            {
                if (!isType6)
                    xploder_psx::swapType5PayloadByteOrder(payload);

                if (!xploder_psx::encryptPayloadChunk(
                        payload, effectivePayloadKey))
                {
                    return false;
                }
            }
            else if (!isType6)
            {
                return false;
            }
            // Type 6 routes other than 6/7 are copied unchanged.

            out << "\n$" << formatEncrypted(
                payload, options.groupEncryptedOutput);
            if (options.annotateCodeTypes)
            {
                if (isType6)
                {
                    out << "\t" << buildType6SourceRowComment(
                        rawInfo, i, rawPayload, true);
                }
                else
                {
                    const int firstByteOffset =
                        i * static_cast<int>(xploder_psx::CodeLength);
                    const int bytesUsed = std::min(
                        static_cast<int>(xploder_psx::CodeLength),
                        std::max(0, sourcePayloadSize - firstByteOffset));
                    out << "\t// encrypted Type 5 payload bytes "
                        << hex4(firstByteOffset) << '-'
                        << hex4(firstByteOffset + bytesUsed - 1);
                }
            }
        }

        convertedBlock = out.str();
        lastConsumedIndex =
            startIndex + static_cast<std::size_t>(payloadLineCount);
        return true;
    }

    inline std::string convertLine(const std::string& line, const Options& options)
    {
        if (trim(line).empty())
            return std::string{};

        const std::string t = trim(line);

        if (startsWith(t, "!!") || t[0] == '!' || t[0] == '%' || t[0] == ';' || startsWith(t, "//"))
            return t;

        // CMP metadata directives are not cheat names and must never receive
        // the normal leading '+' name marker. Also repair older +^ output.
        if (t[0] == '^')
            return t;
        if (t.size() > 1U && t[0] == '+' && t[1] == '^')
            return t.substr(1U);

        // DuckStation 8+8 rows are a separate emulator format. Preserve them
        // instead of feeding their first 12 characters to the Xploder engine.
        std::string duckStationCode;
        if (tryNormalizeDuckStationCodeLine(t, duckStationCode))
            return duckStationCode;

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

        return options.outputCmpDbCompatible ? "+" + t : t;
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

            if (options.mode == Mode::Decrypt && tryPreserveRawMassWriteBlock(lines, i, options, block, lastConsumed))
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

        return applyOutputCmpDbFormatting(
            out.str(), options.outputCmpDbCompatible);
    }
}
