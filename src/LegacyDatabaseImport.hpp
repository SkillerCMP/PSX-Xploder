#pragma once

// Import helpers for legacy PlayStation cheat-device databases.
//
// Supported inputs:
//   * Xploder/Xplorer encrypted .FCD ROMs and decrypted .ROM images.
//   * Action Replay/GameShark aligned v1-style and compact v2-style
//     databases, either standalone or embedded in a full firmware ROM.
//   * Datel v2.x/v3.x encrypted Action Replay/GameShark firmware images,
//     detected from their contents rather than their filename extension.
//
// The Xploder ROM byte transform is compatible with the GPLv3 xpcrypt
// implementation by misfire/mlafeldt. The Datel v2/v3 transform follows
// Hanimar's undatel 0.30 algorithm. This project is GPLv3.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace legacy_database_import
{
    enum class Kind
    {
        XploderRom,
        ActionReplayGameSharkV1,
        ActionReplayGameShark = ActionReplayGameSharkV1
    };

    struct Result
    {
        Kind kind = Kind::XploderRom;
        std::string text;
        std::size_t games = 0;
        std::size_t cheats = 0;
        std::size_t codeLines = 0;
        std::size_t databaseOffset = 0;
        int datelEncryptionVersion = 0;
        std::string formatDescription;
    };

    namespace detail
    {
        constexpr std::size_t XploderRomBlockSize = 512U;

        inline constexpr std::array<std::uint8_t, XploderRomBlockSize> XploderRomSeeds1 = {
        0x45, 0x44, 0x44, 0x45, 0x43, 0x42, 0x4E, 0x4F, 0x41, 0x40, 0x40, 0x41, 0x47, 0x46, 0x42, 0x43,
        0x5D, 0x5C, 0x5C, 0x5D, 0x5B, 0x5A, 0x46, 0x47, 0x49, 0x48, 0x48, 0x49, 0x4F, 0x4E, 0x4A, 0x4B,
        0x75, 0x74, 0x74, 0x75, 0x73, 0x72, 0x7E, 0x7F, 0x71, 0x70, 0x70, 0x71, 0x77, 0x76, 0x72, 0x73,
        0x6D, 0x6C, 0x6C, 0x6D, 0x6B, 0x6A, 0x56, 0x57, 0x59, 0x58, 0x58, 0x59, 0x5F, 0x5E, 0x5A, 0x5B,
        0x25, 0x24, 0x24, 0x25, 0x23, 0x22, 0x2E, 0x2F, 0x21, 0x20, 0x20, 0x21, 0x27, 0x26, 0x22, 0x23,
        0x3D, 0x3C, 0x3C, 0x3D, 0x3B, 0x3A, 0x26, 0x27, 0x29, 0x28, 0x28, 0x29, 0x2F, 0x2E, 0x2A, 0x2B,
        0x15, 0x14, 0x14, 0x15, 0x13, 0x12, 0x1E, 0x1F, 0x11, 0x10, 0x10, 0x11, 0x17, 0x16, 0x12, 0x13,
        0x0D, 0x0C, 0x0C, 0x0D, 0x0B, 0x0A, 0x76, 0x77, 0x79, 0x78, 0x78, 0x79, 0x7F, 0x7E, 0x7A, 0x7B,
        0x05, 0x04, 0x04, 0x05, 0x03, 0x02, 0x0E, 0x0F, 0x01, 0x00, 0x00, 0x01, 0x07, 0x06, 0x02, 0x03,
        0x1D, 0x1C, 0x1C, 0x1D, 0x1B, 0x1A, 0x06, 0x07, 0x09, 0x08, 0x08, 0x09, 0x0F, 0x0E, 0x0A, 0x0B,
        0x35, 0x34, 0x34, 0x35, 0x33, 0x32, 0x3E, 0x3F, 0x31, 0x30, 0x30, 0x31, 0x37, 0x36, 0x32, 0x33,
        0x2D, 0x2C, 0x2C, 0x2D, 0x2B, 0x2A, 0x16, 0x17, 0x19, 0x18, 0x18, 0x19, 0x1F, 0x1E, 0x1A, 0x1B,
        0x65, 0x64, 0x64, 0x65, 0x63, 0x62, 0x6E, 0x6F, 0x61, 0x60, 0x60, 0x61, 0x67, 0x66, 0x62, 0x63,
        0x7D, 0x7C, 0x7C, 0x7D, 0x7B, 0x7A, 0x66, 0x67, 0x69, 0x68, 0x68, 0x69, 0x6F, 0x6E, 0x6A, 0x6B,
        0x55, 0x54, 0x54, 0x55, 0x53, 0x52, 0x5E, 0x5F, 0x51, 0x50, 0x50, 0x51, 0x57, 0x56, 0x52, 0x53,
        0x4D, 0x4C, 0x4C, 0x4D, 0x4B, 0x4A, 0x36, 0x37, 0x39, 0x38, 0x38, 0x39, 0x3F, 0x3E, 0x3A, 0x3B,
        0x45, 0x44, 0x44, 0x45, 0x43, 0x42, 0x4E, 0x4F, 0x41, 0x40, 0x40, 0x41, 0x47, 0x46, 0x42, 0x43,
        0x5D, 0x5C, 0x5C, 0x5D, 0x5B, 0x5A, 0x46, 0x47, 0x49, 0x48, 0x48, 0x49, 0x4F, 0x4E, 0x4A, 0x4B,
        0x75, 0x74, 0x74, 0x75, 0x73, 0x72, 0x7E, 0x7F, 0x71, 0x70, 0x70, 0x71, 0x77, 0x76, 0x72, 0x73,
        0x6D, 0x6C, 0x6C, 0x6D, 0x6B, 0x6A, 0x56, 0x57, 0x59, 0x58, 0x58, 0x59, 0x5F, 0x5E, 0x5A, 0x5B,
        0x25, 0x24, 0x24, 0x25, 0x23, 0x22, 0x2E, 0x2F, 0x21, 0x20, 0x20, 0x21, 0x27, 0x26, 0x22, 0x23,
        0x3D, 0x3C, 0x3C, 0x3D, 0x3B, 0x3A, 0x26, 0x27, 0x29, 0x28, 0x28, 0x29, 0x2F, 0x2E, 0x2A, 0x2B,
        0x15, 0x14, 0x14, 0x15, 0x13, 0x12, 0x1E, 0x1F, 0x11, 0x10, 0x10, 0x11, 0x17, 0x16, 0x12, 0x13,
        0x0D, 0x0C, 0x0C, 0x0D, 0x0B, 0x0A, 0x76, 0x77, 0x79, 0x78, 0x78, 0x79, 0x7F, 0x7E, 0x7A, 0x7B,
        0x05, 0x04, 0x04, 0x05, 0x03, 0x02, 0x0E, 0x0F, 0x01, 0x00, 0x00, 0x01, 0x07, 0x06, 0x02, 0x03,
        0x1D, 0x1C, 0x1C, 0x1D, 0x1B, 0x1A, 0x06, 0x07, 0x09, 0x08, 0x08, 0x09, 0x0F, 0x0E, 0x0A, 0x0B,
        0x35, 0x34, 0x34, 0x35, 0x33, 0x32, 0x3E, 0x3F, 0x31, 0x30, 0x30, 0x31, 0x37, 0x36, 0x32, 0x33,
        0x2D, 0x2C, 0x2C, 0x2D, 0x2B, 0x2A, 0x16, 0x17, 0x19, 0x18, 0x18, 0x19, 0x1F, 0x1E, 0x1A, 0x1B,
        0x65, 0x64, 0x64, 0x65, 0x63, 0x62, 0x6E, 0x6F, 0x61, 0x60, 0x60, 0x61, 0x67, 0x66, 0x62, 0x63,
        0x7D, 0x7C, 0x7C, 0x7D, 0x7B, 0x7A, 0x66, 0x67, 0x69, 0x68, 0x68, 0x69, 0x6F, 0x6E, 0x6A, 0x6B,
        0x55, 0x54, 0x54, 0x55, 0x53, 0x52, 0x5E, 0x5F, 0x51, 0x50, 0x50, 0x51, 0x57, 0x56, 0x52, 0x53,
        0x4D, 0x4C, 0x4C, 0x4D, 0x4B, 0x4A, 0x36, 0x37, 0x39, 0x38, 0x38, 0x39, 0x3F, 0x3E, 0x3A, 0x3B,
        };

        inline constexpr std::array<std::uint8_t, XploderRomBlockSize> XploderRomSeeds2 = {
        0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x28, 0x29, 0x2A, 0x2B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x39, 0x3A, 0x3B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x08, 0x09, 0x0A, 0x0B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B,
        0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB,
        0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB,
        0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B,
        0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B,
        0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB,
        0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB,
        0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B,
        0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x19, 0x1A, 0x1B,
        0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB,
        0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB,
        0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B,
        0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B,
        0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xA8, 0xA9, 0xAA, 0xAB,
        0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xB8, 0xB9, 0xBA, 0xBB,
        0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x88, 0x89, 0x8A, 0x8B,
        0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0x99, 0x9A, 0x9B,
        };

        inline std::string trimAndCollapseSpaces(std::string text)
        {
            std::string out;
            out.reserve(text.size());
            bool pendingSpace = false;
            for (unsigned char ch : text)
            {
                if (std::isspace(ch))
                {
                    if (!out.empty())
                        pendingSpace = true;
                    continue;
                }
                if (pendingSpace)
                {
                    out.push_back(' ');
                    pendingSpace = false;
                }
                out.push_back(static_cast<char>(ch));
            }
            return out;
        }

        inline void appendLatin1AsUtf8(std::string& out, std::uint8_t byte)
        {
            if (byte < 0x80U)
            {
                out.push_back(static_cast<char>(byte));
            }
            else
            {
                out.push_back(static_cast<char>(0xC0U | (byte >> 6U)));
                out.push_back(static_cast<char>(0x80U | (byte & 0x3FU)));
            }
        }

        inline std::string decodeName(
            const std::vector<std::uint8_t>& bytes,
            std::size_t begin,
            std::size_t end,
            bool stripArControlBytes)
        {
            std::string decoded;
            decoded.reserve(end > begin ? end - begin : 0U);
            bool started = false;
            for (std::size_t i = begin; i < end; ++i)
            {
                const std::uint8_t byte = bytes[i];
                if (stripArControlBytes && byte >= 0xF9U)
                {
                    // AR/GS v1 databases use F9-FF as display/control glyphs.
                    // Suppress leading controls and turn embedded controls into
                    // a separator instead of leaking mojibake into the editor.
                    if (started && !decoded.empty() && decoded.back() != ' ')
                        decoded.push_back(' ');
                    continue;
                }
                if (byte < 0x20U || byte == 0x7FU)
                {
                    if (started && !decoded.empty() && decoded.back() != ' ')
                        decoded.push_back(' ');
                    continue;
                }
                appendLatin1AsUtf8(decoded, byte);
                started = true;
            }
            return trimAndCollapseSpaces(decoded);
        }

        inline bool allRemainingIsFF(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            if (offset >= bytes.size())
                return true;
            return std::all_of(
                bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                bytes.end(),
                [](std::uint8_t byte) { return byte == 0xFFU; });
        }

        inline bool readCStringEnd(
            const std::vector<std::uint8_t>& bytes,
            std::size_t offset,
            std::size_t maximumLength,
            std::size_t& end)
        {
            const std::size_t limit = std::min(bytes.size(), offset + maximumLength + 1U);
            for (std::size_t i = offset; i < limit; ++i)
            {
                if (bytes[i] == 0U)
                {
                    end = i;
                    return true;
                }
            }
            return false;
        }

        inline bool isPlausibleXploderName(
            const std::vector<std::uint8_t>& bytes,
            std::size_t begin,
            std::size_t end)
        {
            if (end <= begin)
                return false;
            for (std::size_t i = begin; i < end; ++i)
            {
                const std::uint8_t byte = bytes[i];
                if (byte < 0x20U || byte == 0x7FU)
                    return false;
            }
            return true;
        }

        inline bool isPlausibleArName(
            const std::vector<std::uint8_t>& bytes,
            std::size_t begin,
            std::size_t end,
            bool allowControlOnly)
        {
            if (end <= begin)
                return allowControlOnly;
            bool hasReadable = false;
            for (std::size_t i = begin; i < end; ++i)
            {
                const std::uint8_t byte = bytes[i];
                if (byte >= 0xF9U)
                    continue;
                if (byte >= 0x20U && byte != 0x7FU)
                {
                    hasReadable = true;
                    continue;
                }
                return false;
            }
            return hasReadable || allowControlOnly;
        }

        inline std::string formatCode6(const std::uint8_t* code)
        {
            std::ostringstream out;
            out << '$' << std::uppercase << std::hex << std::setfill('0');
            for (int i = 0; i < 4; ++i)
                out << std::setw(2) << static_cast<unsigned int>(code[i]);
            out << ' ';
            for (int i = 4; i < 6; ++i)
                out << std::setw(2) << static_cast<unsigned int>(code[i]);
            return out.str();
        }

        inline std::string formatArCode(std::uint32_t address, std::uint16_t value)
        {
            std::ostringstream out;
            out << '$' << std::uppercase << std::hex << std::setfill('0')
                << std::setw(8) << address << ' '
                << std::setw(4) << static_cast<unsigned int>(value);
            return out.str();
        }

        struct XploderCandidate
        {
            std::size_t start = 0;
            std::size_t end = 0;
            std::size_t games = 0;
            std::size_t cheats = 0;
            std::size_t codeLines = 0;
            std::string text;
        };

        inline bool parseXploderDatabaseAt(
            const std::vector<std::uint8_t>& rom,
            std::size_t start,
            bool buildText,
            XploderCandidate& candidate)
        {
            candidate = {};
            candidate.start = start;
            std::size_t position = start;
            std::ostringstream output;

            while (position + 4U <= rom.size())
            {
                if (rom[position] == 0xFFU && rom[position + 1U] == 0xFFU &&
                    rom[position + 2U] == 0xFFU && rom[position + 3U] == 0xFFU)
                {
                    candidate.end = position + 4U;
                    if (buildText)
                        candidate.text = output.str();
                    return candidate.games != 0U && candidate.cheats != 0U;
                }

                std::size_t gameNameEnd = 0;
                if (!readCStringEnd(rom, position, 127U, gameNameEnd) ||
                    !isPlausibleXploderName(rom, position, gameNameEnd))
                {
                    return false;
                }

                const std::string gameName = decodeName(rom, position, gameNameEnd, false);
                position = gameNameEnd + 1U;
                if (position >= rom.size())
                    return false;
                const std::size_t cheatCount = rom[position++];
                if (cheatCount > 200U)
                    return false;

                if (buildText)
                    output << "^3 = NAME: " << gameName << '\n';
                ++candidate.games;

                for (std::size_t cheatIndex = 0; cheatIndex < cheatCount; ++cheatIndex)
                {
                    std::size_t cheatNameEnd = 0;
                    if (!readCStringEnd(rom, position, 255U, cheatNameEnd) ||
                        !isPlausibleXploderName(rom, position, cheatNameEnd))
                    {
                        return false;
                    }

                    std::string cheatName = decodeName(rom, position, cheatNameEnd, false);
                    if (cheatName.empty())
                        cheatName = "Unnamed Code";
                    position = cheatNameEnd + 1U;
                    if (position >= rom.size())
                        return false;
                    const std::size_t lineCount = rom[position++];
                    if (lineCount > 200U || position + lineCount * 6U > rom.size())
                        return false;

                    if (buildText)
                        output << '+' << cheatName << '\n';
                    ++candidate.cheats;
                    candidate.codeLines += lineCount;
                    for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
                    {
                        if (buildText)
                            output << formatCode6(&rom[position]) << '\n';
                        position += 6U;
                    }
                    if (buildText)
                        output << '\n';
                }
                if (buildText)
                    output << '\n';

                if (candidate.games > 10000U || candidate.codeLines > 1000000U)
                    return false;
            }
            return false;
        }

        inline bool findXploderDatabase(
            const std::vector<std::uint8_t>& rom,
            XploderCandidate& best)
        {
            best = {};

            // The standard FCD firmware database begins at 0x25000. Prefer
            // that canonical location whenever it validates completely; the
            // ROM can contain other byte ranges that coincidentally resemble
            // a smaller database.
            if (rom.size() > 0x25000U)
            {
                XploderCandidate standard;
                if (parseXploderDatabaseAt(rom, 0x25000U, false, standard) &&
                    standard.games >= 2U && standard.cheats >= 2U)
                {
                    return parseXploderDatabaseAt(rom, 0x25000U, true, best);
                }
            }

            std::vector<std::size_t> candidates;
            for (std::size_t offset = 0; offset + 4U < rom.size(); offset += 0x100U)
                candidates.push_back(offset);

            std::size_t bestScore = 0;
            for (std::size_t offset : candidates)
            {
                XploderCandidate parsed;
                if (!parseXploderDatabaseAt(rom, offset, false, parsed))
                    continue;
                const std::size_t score = parsed.games * 1000U + parsed.cheats * 10U + parsed.codeLines;
                if (score > bestScore)
                {
                    bestScore = score;
                    best = parsed;
                }
            }

            if (bestScore == 0U || best.games < 2U || best.cheats < 2U)
                return false;
            return parseXploderDatabaseAt(rom, best.start, true, best);
        }

        inline std::uint32_t readLe32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return static_cast<std::uint32_t>(bytes[offset]) |
                   (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
                   (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
                   (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
        }

        inline std::uint16_t readLe16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(bytes[offset]) |
                (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
        }

        inline std::uint32_t readBe32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                   (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                   (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                   static_cast<std::uint32_t>(bytes[offset + 3U]);
        }

        inline std::uint16_t readBe16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                static_cast<std::uint16_t>(bytes[offset + 1U]));
        }

        inline void writeLe32(
            std::vector<std::uint8_t>& bytes,
            std::size_t offset,
            std::uint32_t value)
        {
            bytes[offset] = static_cast<std::uint8_t>(value);
            bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
            bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
            bytes[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
        }

        inline constexpr std::array<std::uint32_t, 16> DatelSeeds = {
            0xACD27B21U, 0xAB543673U, 0xAC35D821U, 0x458092D4U,
            0x43672D12U, 0xAC427D21U, 0x12654634U, 0xE2DCE2B1U,
            0x5654D335U, 0xD1235322U, 0xA2D87894U, 0x45D0CCDEU,
            0x42D77312U, 0xAC236D45U, 0x1472332EU, 0x8742DC4EU
        };

        inline constexpr std::array<std::array<std::uint8_t, 16>, 8> DatelV3Tables = {{
            {{ 0x3, 0x2, 0x1, 0x0, 0x7, 0x6, 0x5, 0x4, 0xB, 0xA, 0x9, 0x8, 0xF, 0xE, 0xD, 0xC }},
            {{ 0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0 }},
            {{ 0xD, 0xC, 0xF, 0xE, 0x9, 0x8, 0xB, 0xA, 0x5, 0x4, 0x7, 0x6, 0x1, 0x0, 0x3, 0x2 }},
            {{ 0x6, 0x7, 0x4, 0x5, 0x2, 0x3, 0x0, 0x1, 0xE, 0xF, 0xC, 0xD, 0xA, 0xB, 0x8, 0x9 }},
            {{ 0x2, 0x3, 0x0, 0x1, 0x6, 0x7, 0x4, 0x5, 0xA, 0xB, 0x8, 0x9, 0xE, 0xF, 0xC, 0xD }},
            {{ 0xA, 0xB, 0x8, 0x9, 0xE, 0xF, 0xC, 0xD, 0x2, 0x3, 0x0, 0x1, 0x6, 0x7, 0x4, 0x5 }},
            {{ 0xB, 0xA, 0x9, 0x8, 0xF, 0xE, 0xD, 0xC, 0x3, 0x2, 0x1, 0x0, 0x7, 0x6, 0x5, 0x4 }},
            {{ 0x9, 0x8, 0xB, 0xA, 0xD, 0xC, 0xF, 0xE, 0x1, 0x0, 0x3, 0x2, 0x5, 0x4, 0x7, 0x6 }}
        }};

        inline std::uint32_t datelV3FlipNibbles(std::uint32_t word)
        {
            std::uint32_t output = 0U;
            for (std::size_t byteIndex = 0; byteIndex < 4U; ++byteIndex)
            {
                const std::uint8_t byte = static_cast<std::uint8_t>(word >> (byteIndex * 8U));
                const std::uint8_t swapped = static_cast<std::uint8_t>((byte << 4U) | (byte >> 4U));
                output |= static_cast<std::uint32_t>(swapped) << (byteIndex * 8U);
            }
            return output;
        }

        inline std::uint32_t datelV3Substitute(std::uint32_t word)
        {
            std::uint32_t output = 0U;
            for (std::size_t byteIndex = 0; byteIndex < 4U; ++byteIndex)
            {
                const std::uint8_t byte = static_cast<std::uint8_t>(word >> (byteIndex * 8U));
                const std::uint8_t high = DatelV3Tables[byteIndex * 2U][byte >> 4U];
                const std::uint8_t low = DatelV3Tables[byteIndex * 2U + 1U][byte & 0x0FU];
                const std::uint8_t substituted = static_cast<std::uint8_t>((high << 4U) | low);
                output |= static_cast<std::uint32_t>(substituted) << (byteIndex * 8U);
            }
            return output;
        }

        inline bool decryptDatelRom(
            const std::vector<std::uint8_t>& input,
            int version,
            std::vector<std::uint8_t>& output)
        {
            if ((version != 2 && version != 3) || input.size() < 8U)
                return false;

            output = input;
            while ((output.size() & 3U) != 0U)
                output.push_back(0xFFU);

            const std::size_t wordCount = output.size() / 4U;
            if (version >= 3)
            {
                const std::size_t transformedWords = std::min<std::size_t>(wordCount, 8000U);
                for (std::size_t i = 0; i < transformedWords; ++i)
                {
                    std::uint32_t word = readLe32(output, i * 4U);
                    word = datelV3Substitute(word);
                    word = datelV3FlipNibbles(word);
                    writeLe32(output, i * 4U, word);
                }
            }

            for (std::size_t i = wordCount; i-- > 1U;)
            {
                const std::uint32_t current = readLe32(output, i * 4U);
                const std::uint32_t previous = readLe32(output, (i - 1U) * 4U);
                writeLe32(output, i * 4U, current ^ previous);
            }

            for (std::size_t i = 0; i < wordCount; ++i)
            {
                const std::uint32_t seed = DatelSeeds[i & 0x0FU];
                std::uint32_t word = readLe32(output, i * 4U);
                word = (word ^ seed) - (seed & 0xFFU);
                writeLe32(output, i * 4U, word);
            }
            return true;
        }

        inline bool hasDatelV3ContentMarker(const std::vector<std::uint8_t>& bytes)
        {
            return bytes.size() >= 8U && readLe32(bytes, 4U) == 0x6E748473U;
        }

        inline bool allRemainingIsPadding(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            if (offset >= bytes.size())
                return true;
            return std::all_of(
                bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                bytes.end(),
                [](std::uint8_t byte) { return byte == 0x00U || byte == 0xFFU; });
        }

        inline bool hasDatabasePaddingTerminator(
            const std::vector<std::uint8_t>& bytes,
            std::size_t offset,
            std::size_t minimumRun = 16U)
        {
            if (offset >= bytes.size())
                return true;
            std::size_t end = offset;
            while (end < bytes.size() && (bytes[end] == 0x00U || bytes[end] == 0xFFU))
                ++end;
            return end == bytes.size() || end - offset >= minimumRun;
        }

        inline bool isPlausibleArCheatName(
            const std::vector<std::uint8_t>& bytes,
            std::size_t begin,
            std::size_t end)
        {
            if (end <= begin)
                return true;

            bool hasReadable = false;
            bool controlOnly = true;
            for (std::size_t i = begin; i < end; ++i)
            {
                const std::uint8_t byte = bytes[i];
                if (byte >= 0xF9U || byte <= 0x1FU)
                    continue;
                controlOnly = false;
                if (byte >= 0x20U && byte != 0x7FU)
                {
                    hasReadable = true;
                    continue;
                }
                return false;
            }
            return hasReadable || (controlOnly && end - begin <= 8U);
        }

        enum class ArDatabaseLayout
        {
            V1Aligned,
            V2Compact
        };

        struct ArCandidate
        {
            ArDatabaseLayout layout = ArDatabaseLayout::V1Aligned;
            std::size_t start = 0;
            std::size_t end = 0;
            std::size_t games = 0;
            std::size_t cheats = 0;
            std::size_t codeLines = 0;
            std::size_t headings = 0;
            std::string text;
        };

        inline std::string fallbackArCheatName(
            const std::vector<std::uint8_t>& bytes,
            std::size_t begin,
            std::size_t end,
            std::size_t lineCount)
        {
            if (lineCount == 0U)
                return {};
            for (std::size_t i = begin; i < end; ++i)
            {
                if (bytes[i] >= 0xF9U)
                    return "Auto Activation";
            }
            return "Unnamed Code";
        }

        inline bool parseArV1At(
            const std::vector<std::uint8_t>& bytes,
            std::size_t start,
            bool buildText,
            ArCandidate& candidate)
        {
            candidate = {};
            candidate.layout = ArDatabaseLayout::V1Aligned;
            candidate.start = start;
            std::size_t position = start;
            std::ostringstream output;

            auto alignForRecords = [&]() -> bool
            {
                const std::size_t aligned = (position + 3U) & ~std::size_t(3U);
                if (aligned > bytes.size())
                    return false;
                for (std::size_t i = position; i < aligned; ++i)
                {
                    if (bytes[i] != 0U)
                        return false;
                }
                position = aligned;
                return true;
            };

            auto readRecord = [&](std::uint32_t& address, std::uint16_t& value) -> bool
            {
                if (position + 8U > bytes.size())
                    return false;
                address = readLe32(bytes, position);
                value = readLe16(bytes, position + 4U);
                if (bytes[position + 6U] != 0U || bytes[position + 7U] != 0U)
                    return false;
                position += 8U;
                return true;
            };

            while (position < bytes.size())
            {
                if (hasDatabasePaddingTerminator(bytes, position))
                {
                    candidate.end = position;
                    if (buildText)
                        candidate.text = output.str();
                    return candidate.games >= 2U && candidate.codeLines != 0U;
                }

                std::size_t gameNameEnd = 0;
                if (!readCStringEnd(bytes, position, 255U, gameNameEnd) ||
                    !isPlausibleArName(bytes, position, gameNameEnd, false))
                {
                    return false;
                }
                const std::string gameName = decodeName(bytes, position, gameNameEnd, true);
                position = gameNameEnd + 1U;
                if (position + 2U > bytes.size() ||
                    bytes[position] < 0x30U || bytes[position + 1U] < 0x30U)
                {
                    return false;
                }

                const std::size_t cheatCount = static_cast<std::size_t>(bytes[position++] - 0x30U);
                const std::size_t masterLineCount = static_cast<std::size_t>(bytes[position++] - 0x30U);
                if (cheatCount > 207U || masterLineCount > 64U)
                    return false;

                if (buildText)
                    output << "^3 = NAME: " << gameName << '\n';
                ++candidate.games;

                if (masterLineCount != 0U)
                {
                    if (!alignForRecords())
                        return false;
                    if (buildText)
                        output << "+Auto Activation\n";
                    for (std::size_t lineIndex = 0; lineIndex < masterLineCount; ++lineIndex)
                    {
                        std::uint32_t address = 0;
                        std::uint16_t value = 0;
                        if (!readRecord(address, value))
                            return false;
                        if (buildText)
                            output << formatArCode(address, value) << '\n';
                        ++candidate.codeLines;
                    }
                    if (buildText)
                        output << '\n';
                    ++candidate.cheats;
                }

                for (std::size_t cheatIndex = 0; cheatIndex < cheatCount; ++cheatIndex)
                {
                    const std::size_t cheatNameBegin = position;
                    std::size_t cheatNameEnd = 0;
                    if (!readCStringEnd(bytes, position, 255U, cheatNameEnd))
                        return false;
                    position = cheatNameEnd + 1U;
                    if (position + 2U > bytes.size() || bytes[position] < 0x30U)
                        return false;
                    const std::size_t lineCount = static_cast<std::size_t>(bytes[position++] - 0x30U);
                    (void)bytes[position++];
                    if (lineCount > 207U ||
                        !isPlausibleArCheatName(bytes, cheatNameBegin, cheatNameEnd))
                    {
                        return false;
                    }

                    std::string cheatName = decodeName(bytes, cheatNameBegin, cheatNameEnd, true);
                    if (cheatName.empty())
                        cheatName = fallbackArCheatName(bytes, cheatNameBegin, cheatNameEnd, lineCount);

                    if (lineCount != 0U && !alignForRecords())
                        return false;

                    const bool emitEntry = !cheatName.empty() || lineCount != 0U;
                    if (buildText && emitEntry)
                        output << '+' << (cheatName.empty() ? "Unnamed Code" : cheatName) << '\n';

                    for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
                    {
                        std::uint32_t address = 0;
                        std::uint16_t value = 0;
                        if (!readRecord(address, value))
                            return false;
                        if (buildText && emitEntry)
                            output << formatArCode(address, value) << '\n';
                        ++candidate.codeLines;
                    }

                    if (emitEntry)
                    {
                        if (buildText)
                            output << '\n';
                        ++candidate.cheats;
                    }
                    else
                    {
                        ++candidate.headings;
                    }
                }

                if (buildText)
                    output << '\n';
                if (candidate.games > 10000U || candidate.codeLines > 10000000U)
                    return false;
            }
            return false;
        }

        inline bool parseArV2At(
            const std::vector<std::uint8_t>& bytes,
            std::size_t start,
            bool buildText,
            ArCandidate& candidate)
        {
            candidate = {};
            candidate.layout = ArDatabaseLayout::V2Compact;
            candidate.start = start;
            if (start + 4U > bytes.size())
                return false;

            const std::size_t gameCount = static_cast<std::size_t>(readBe32(bytes, start));
            if (gameCount < 2U || gameCount > 4096U)
                return false;

            std::size_t position = start + 4U;
            std::ostringstream output;
            for (std::size_t gameIndex = 0; gameIndex < gameCount; ++gameIndex)
            {
                std::size_t gameNameEnd = 0;
                if (!readCStringEnd(bytes, position, 255U, gameNameEnd) ||
                    !isPlausibleArName(bytes, position, gameNameEnd, false))
                {
                    return false;
                }
                const std::string gameName = decodeName(bytes, position, gameNameEnd, true);
                position = gameNameEnd + 1U;
                if (position + 2U > bytes.size())
                    return false;
                const std::size_t cheatCount = bytes[position++];
                (void)bytes[position++]; // Game menu/state byte.

                if (buildText)
                    output << "^3 = NAME: " << gameName << '\n';
                ++candidate.games;

                for (std::size_t cheatIndex = 0; cheatIndex < cheatCount; ++cheatIndex)
                {
                    const std::size_t cheatNameBegin = position;
                    std::size_t cheatNameEnd = 0;
                    if (!readCStringEnd(bytes, position, 255U, cheatNameEnd))
                        return false;
                    position = cheatNameEnd + 1U;
                    if (position >= bytes.size() ||
                        !isPlausibleArCheatName(bytes, cheatNameBegin, cheatNameEnd))
                    {
                        return false;
                    }

                    const std::uint8_t stateAndCount = bytes[position++];
                    const std::size_t lineCount = static_cast<std::size_t>(stateAndCount & 0x7FU);
                    std::string cheatName = decodeName(bytes, cheatNameBegin, cheatNameEnd, true);
                    if (cheatName.empty())
                        cheatName = fallbackArCheatName(bytes, cheatNameBegin, cheatNameEnd, lineCount);

                    if (position + lineCount * 6U > bytes.size())
                        return false;
                    const bool emitEntry = !cheatName.empty() || lineCount != 0U;
                    if (buildText && emitEntry)
                        output << '+' << (cheatName.empty() ? "Unnamed Code" : cheatName) << '\n';

                    for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
                    {
                        const std::uint32_t address = readBe32(bytes, position);
                        const std::uint16_t value = readBe16(bytes, position + 4U);
                        if (buildText && emitEntry)
                            output << formatArCode(address, value) << '\n';
                        position += 6U;
                        ++candidate.codeLines;
                    }

                    if (emitEntry)
                    {
                        if (buildText)
                            output << '\n';
                        ++candidate.cheats;
                    }
                    else
                    {
                        ++candidate.headings;
                    }
                }

                if (buildText)
                    output << '\n';
            }

            if (!hasDatabasePaddingTerminator(bytes, position))
                return false;
            candidate.end = position;
            if (buildText)
                candidate.text = output.str();
            return candidate.games == gameCount && candidate.codeLines != 0U;
        }

        inline std::size_t scoreArCandidate(const ArCandidate& candidate)
        {
            return candidate.codeLines * 100000U + candidate.cheats * 100U + candidate.games;
        }

        inline void appendUniqueOffset(std::vector<std::size_t>& offsets, std::size_t offset, std::size_t size)
        {
            if (offset >= size || std::find(offsets.begin(), offsets.end(), offset) != offsets.end())
                return;
            offsets.push_back(offset);
        }

        inline bool findArDatabase(const std::vector<std::uint8_t>& bytes, ArCandidate& best)
        {
            best = {};
            std::vector<std::size_t> offsets;
            appendUniqueOffset(offsets, 0U, bytes.size());
            for (std::size_t offset = 0; offset + 4U < bytes.size(); offset += 0x100U)
                appendUniqueOffset(offsets, offset, bytes.size());

            const std::array<std::string_view, 3> signatures = {
                std::string_view("NEW CODE!\0", 10U),
                std::string_view("--- New Game ---\0", 17U),
                std::string_view("--- NEW GAME ---\0", 17U)
            };
            for (const std::string_view signature : signatures)
            {
                const auto begin = bytes.begin();
                auto found = std::search(begin, bytes.end(), signature.begin(), signature.end());
                while (found != bytes.end())
                {
                    appendUniqueOffset(offsets, static_cast<std::size_t>(found - begin), bytes.size());
                    found = std::search(found + 1, bytes.end(), signature.begin(), signature.end());
                }
            }

            std::size_t bestScore = 0U;
            auto tryOffset = [&](std::size_t offset)
            {
                ArCandidate parsed;
                if (parseArV2At(bytes, offset, false, parsed))
                {
                    const std::size_t score = scoreArCandidate(parsed);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = parsed;
                    }
                }
                if (parseArV1At(bytes, offset, false, parsed))
                {
                    const std::size_t score = scoreArCandidate(parsed);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = parsed;
                    }
                }
            };

            for (const std::size_t offset : offsets)
                tryOffset(offset);

            if (bestScore == 0U)
            {
                for (std::size_t offset = 0; offset + 8U < bytes.size(); offset += 4U)
                {
                    const std::uint32_t count = readBe32(bytes, offset);
                    if (count >= 2U && count <= 4096U)
                        tryOffset(offset);
                    if (bytes[offset] >= 0x20U && bytes[offset] != 0x7FU)
                        tryOffset(offset);
                }
            }

            if (bestScore == 0U || best.games < 2U || best.codeLines == 0U)
                return false;
            return best.layout == ArDatabaseLayout::V2Compact
                ? parseArV2At(bytes, best.start, true, best)
                : parseArV1At(bytes, best.start, true, best);
        }
    }

    inline bool importXploderRom(
        const std::vector<std::uint8_t>& input,
        Result& result,
        std::string& error)
    {
        result = {};
        error.clear();
        if (input.size() < detail::XploderRomBlockSize)
        {
            error = "The file is too small to be an Xploder ROM.";
            return false;
        }

        std::vector<std::uint8_t> rom = input;
        const bool alreadyDecrypted =
            rom.size() >= 0x14U && rom[0x10U] == 'S' && rom[0x11U] == 'o' &&
            rom[0x12U] == 'n' && rom[0x13U] == 'y';
        if (!alreadyDecrypted)
        {
            for (std::size_t i = 0; i < rom.size(); ++i)
            {
                rom[i] = static_cast<std::uint8_t>(
                    static_cast<unsigned int>(rom[i] ^ detail::XploderRomSeeds1[i % detail::XploderRomBlockSize]) +
                    detail::XploderRomSeeds2[i % detail::XploderRomBlockSize]);
            }
        }

        if (rom.size() < 0x14U || rom[0x10U] != 'S' || rom[0x11U] != 'o' ||
            rom[0x12U] != 'n' || rom[0x13U] != 'y')
        {
            error = "The file does not contain a valid Xploder ROM header.";
            return false;
        }

        detail::XploderCandidate database;
        if (!detail::findXploderDatabase(rom, database))
        {
            error = "A valid Xploder code database was not found in the ROM.";
            return false;
        }

        result.kind = Kind::XploderRom;
        result.text = std::move(database.text);
        result.games = database.games;
        result.cheats = database.cheats;
        result.codeLines = database.codeLines;
        result.databaseOffset = database.start;
        result.formatDescription = "Xploder ROM database";
        return true;
    }

    inline bool importActionReplayGameShark(
        const std::vector<std::uint8_t>& bytes,
        Result& result,
        std::string& error)
    {
        result = {};
        error.clear();
        if (bytes.size() < 16U)
        {
            error = "The file is too small to contain an Action Replay/GameShark database.";
            return false;
        }

        const auto acceptDatabase = [&](
            const std::vector<std::uint8_t>& candidateBytes,
            int datelVersion) -> bool
        {
            detail::ArCandidate database;
            if (!detail::findArDatabase(candidateBytes, database))
                return false;

            result = {};
            result.kind = Kind::ActionReplayGameShark;
            result.text = std::move(database.text);
            result.games = database.games;
            result.cheats = database.cheats;
            result.codeLines = database.codeLines;
            result.databaseOffset = database.start;
            result.datelEncryptionVersion = datelVersion;

            const std::string layoutDescription =
                database.layout == detail::ArDatabaseLayout::V2Compact
                    ? "AR/GS compact v2 database"
                    : "AR/GS aligned v1 database";
            if (datelVersion != 0)
            {
                result.formatDescription =
                    "Datel v" + std::to_string(datelVersion) +
                    ".x encrypted ROM / " + layoutDescription;
            }
            else
            {
                result.formatDescription = layoutDescription;
            }
            return true;
        };

        // Always prefer an already-decrypted/standalone database. Only if
        // that fails do we try the Datel transforms. A successful import is
        // accepted only after the decrypted bytes contain a complete,
        // structurally valid AR/GS database. The filename and extension are
        // deliberately not consulted.
        if (acceptDatabase(bytes, 0))
            return true;

        const bool v3Marker = detail::hasDatelV3ContentMarker(bytes);
        const std::array<int, 2> versions = v3Marker
            ? std::array<int, 2>{ 3, 2 }
            : std::array<int, 2>{ 2, 3 };

        for (const int version : versions)
        {
            std::vector<std::uint8_t> decrypted;
            if (!detail::decryptDatelRom(bytes, version, decrypted))
                continue;
            if (acceptDatabase(decrypted, version))
                return true;
        }

        error = "A supported Action Replay/GameShark code database was not found in the original data "
                "or after content-based Datel v2.x/v3.x decryption. Supported database layouts are "
                "the aligned v1-style database and the compact v2-style database.";
        return false;
    }

    // Kept as a source-compatible wrapper for older GUI/test code. It now
    // accepts both supported Action Replay/GameShark database layouts.
    inline bool importActionReplayGameSharkV1(
        const std::vector<std::uint8_t>& bytes,
        Result& result,
        std::string& error)
    {
        return importActionReplayGameShark(bytes, result, error);
    }

    inline bool autoDetect(
        const std::vector<std::uint8_t>& bytes,
        Result& result,
        std::string& error)
    {
        std::string xploderError;
        if (importXploderRom(bytes, result, xploderError))
            return true;

        std::string arError;
        if (importActionReplayGameShark(bytes, result, arError))
            return true;

        error = "The dropped binary was not recognized as an Xploder ROM, an Action Replay/GameShark database, or a Datel v2.x/v3.x encrypted ROM.";
        if (!xploderError.empty() || !arError.empty())
            error += " Xploder: " + xploderError + " AR/GS: " + arError;
        return false;
    }
}
