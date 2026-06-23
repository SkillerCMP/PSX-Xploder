#pragma once

// Window-to-window multi-format conversion coordinator.
// Folder-drop batch conversion does not use this file and remains fixed to the
// existing Xploder database decrypt/cleanup pipeline.

#include "CaetlaCodeTypes.hpp"
#include "DuckStationCodeTypes.hpp"
#include "XploderCodeTypes.hpp"

namespace psx_code_types
{
    struct WindowConversionOptions
    {
        Family inputFamily = Family::XploderEncrypted;
        Family outputFamily = Family::XploderRaw;
        bool combineDuckStation16BitWrites = false;
        bool condenseDuckStationActivators = false;
        bool condenseBasicSerialRepeaters = false;
        xploder_converter::Options xploderOptions{};
    };

    inline std::string normalizeClassicText(const std::string& input)
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

    inline std::vector<Operation> parseSelectedFamily(
        const std::string& input,
        const WindowConversionOptions& options)
    {
        switch (options.inputFamily)
        {
            case Family::GameSharkActionReplay:
                return parseGameSharkActionReplay(input);
            case Family::DuckStation:
                return parseDuckStation(input);
            case Family::Caetla:
                return parseCaetla(input);
            case Family::XploderRaw:
                return parseXploderRaw(input);
            case Family::XploderEncrypted:
            {
                xploder_converter::Options decryptOptions = options.xploderOptions;
                decryptOptions.mode = xploder_converter::Mode::Decrypt;
                decryptOptions.groupEncryptedOutput = false;
                const std::string raw = xploder_converter::convertText(input, decryptOptions);
                std::vector<Operation> operations = parseXploderRaw(raw);
                for (Operation& operation : operations)
                {
                    if (operation.kind != OperationKind::Text)
                        operation.sourceFamily = Family::XploderEncrypted;
                }
                return operations;
            }
        }
        return {};
    }

    inline std::string emitSelectedFamily(
        const std::vector<Operation>& operations,
        const WindowConversionOptions& options)
    {
        std::vector<Operation> prepared = operations;
        const bool supportsBasicType5 =
            options.outputFamily == Family::GameSharkActionReplay ||
            options.outputFamily == Family::DuckStation ||
            options.outputFamily == Family::Caetla;
        if (options.condenseBasicSerialRepeaters && supportsBasicType5)
            prepared = condenseWritesToBasicSerialRepeaters(prepared);

        switch (options.outputFamily)
        {
            case Family::GameSharkActionReplay:
                return joinLines(emitGameSharkActionReplay(prepared));
            case Family::DuckStation:
                return joinLines(emitDuckStation(
                    prepared,
                    options.combineDuckStation16BitWrites,
                    options.condenseDuckStationActivators));
            case Family::Caetla:
                return joinLines(emitCaetla(prepared));
            case Family::XploderRaw:
                return joinLines(emitXploderRaw(prepared));
            case Family::XploderEncrypted:
            {
                const std::string raw = joinLines(emitXploderRaw(prepared));
                xploder_converter::Options encryptOptions = options.xploderOptions;
                encryptOptions.mode = xploder_converter::Mode::Encrypt;
                return xploder_converter::convertText(raw, encryptOptions);
            }
        }
        return {};
    }

    inline std::string convertWindowText(
        const std::string& input,
        const WindowConversionOptions& options)
    {
        if (input.empty())
            return {};

        std::string converted;
        bool sameFamilyHandled = false;

        if (options.inputFamily == options.outputFamily)
        {
            switch (options.inputFamily)
            {
                case Family::GameSharkActionReplay:
                    if (options.condenseBasicSerialRepeaters)
                        converted = emitSelectedFamily(parseGameSharkActionReplay(input), options);
                    else
                        converted = normalizeClassicText(input);
                    sameFamilyHandled = true;
                    break;
                case Family::DuckStation:
                    // Rebuild DuckStation output through the semantic parser so
                    // CMP-style names, notes, credits, and nested groups are
                    // converted into proper DuckStation patch sections even
                    // when both selectors are set to DuckStation.
                    converted = emitSelectedFamily(parseDuckStation(input), options);
                    sameFamilyHandled = true;
                    break;
                case Family::Caetla:
                    if (options.condenseBasicSerialRepeaters)
                        converted = emitSelectedFamily(parseCaetla(input), options);
                    else
                        converted = normalizeCaetlaText(input);
                    sameFamilyHandled = true;
                    break;
                case Family::XploderRaw:
                {
                    xploder_converter::Options rawOptions = options.xploderOptions;
                    rawOptions.mode = xploder_converter::Mode::Decrypt;
                    rawOptions.groupEncryptedOutput = false;
                    converted = xploder_converter::convertText(input, rawOptions);
                    sameFamilyHandled = true;
                    break;
                }
                case Family::XploderEncrypted:
                    // Rebuild through RAW so the chosen encryption key and output
                    // spacing options are applied consistently.
                    break;
            }
        }

        if (!sameFamilyHandled)
        {
            std::vector<Operation> operations = parseSelectedFamily(input, options);
            if (options.inputFamily == Family::DuckStation &&
                options.outputFamily != Family::DuckStation)
            {
                operations = convertDuckStationPatchMetadataToCmpOperations(
                    operations,
                    options.xploderOptions.outputCmpDbCompatible);
            }
            converted = emitSelectedFamily(operations, options);
        }

        return xploder_converter::applyOutputCmpDbFormatting(
            converted,
            options.xploderOptions.outputCmpDbCompatible);
    }
}
