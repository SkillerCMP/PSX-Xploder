#pragma once

// Window-to-window multi-format conversion coordinator.
// Folder-drop batch conversion does not use this file and remains fixed to the
// existing Xploder database decrypt/cleanup pipeline.

#include "CaetlaCodeTypes.hpp"
#include "DuckStationCodeTypes.hpp"
#include "XploderCodeTypes.hpp"
#include "Ps1MipsCodeTypes.hpp"

namespace psx_code_types
{
    struct WindowConversionOptions
    {
        Family inputFamily = Family::XploderEncrypted;
        Family outputFamily = Family::XploderRaw;
        bool combineDuckStation16BitWrites = false;
        bool combineCaetla16BitWrites = false;
        bool caetla341ExtendedTypes = false;
        // API compatibility defaults preserve the converter's earlier Caetla
        // text behavior. The GUI enables firmware-accurate 0.34 output and the
        // native Type B slider option explicitly for Caetla output.
        bool caetlaFirmwareAccurateOutput = false;
        bool caetlaNativeTypeBSliders = false;
        bool condenseDuckStationActivators = false;
        bool condenseBasicSerialRepeaters = false;
        bool packMipsAsXploderType5 = false;
        bool xLinkFormat = false;
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
            case Family::Ps1Mips:
                return parsePs1Mips(input);
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
        if (options.packMipsAsXploderType5 &&
            (options.outputFamily == Family::XploderRaw || options.outputFamily == Family::XploderEncrypted))
        {
            prepared = ps1_mips::packMipsWritesAsType5(prepared);
        }
        const bool supportsNativeSerialRepeater =
            options.outputFamily == Family::GameSharkActionReplay ||
            options.outputFamily == Family::DuckStation ||
            options.outputFamily == Family::Caetla ||
            options.outputFamily == Family::XploderRaw ||
            options.outputFamily == Family::XploderEncrypted;
        if (options.condenseBasicSerialRepeaters && supportsNativeSerialRepeater)
        {
            const bool xploderSliderOutput =
                options.outputFamily == Family::XploderRaw ||
                options.outputFamily == Family::XploderEncrypted;
            const bool caetlaSliderOutput =
                options.outputFamily == Family::Caetla &&
                options.caetlaNativeTypeBSliders;

            prepared = condenseWritesToBasicSerialRepeaters(
                prepared,
                xploderSliderOutput ? 0x8000U : (caetlaSliderOutput ? 0xFFFFU : 0xFFU),
                !xploderSliderOutput,
                xploderSliderOutput,
                caetlaSliderOutput,
                caetlaSliderOutput ? 0x0FFFU : 0x00FFU);
        }

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
                return joinLines(emitCaetla(
                    prepared,
                    options.combineCaetla16BitWrites,
                    options.caetlaNativeTypeBSliders,
                    options.caetla341ExtendedTypes,
                    options.caetlaFirmwareAccurateOutput));
            case Family::Ps1Mips:
                return joinLines(emitPs1Mips(prepared));
            case Family::XploderRaw:
                return joinLines(emitXploderRaw(
                    prepared,
                    options.condenseBasicSerialRepeaters));
            case Family::XploderEncrypted:
            {
                const std::string raw = joinLines(emitXploderRaw(
                    prepared,
                    options.condenseBasicSerialRepeaters));
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

        const bool xLinkOutput =
            options.xLinkFormat &&
            (options.outputFamily == Family::XploderRaw ||
             options.outputFamily == Family::XploderEncrypted);

        // X-Link is an alternative presentation format to CMP DB output. Keep
        // the semantic conversion plain internally so CMP-only '+' and credit
        // transformations are not introduced before the X-Link final pass.
        WindowConversionOptions conversionOptions = options;
        if (xLinkOutput)
            conversionOptions.xploderOptions.outputCmpDbCompatible = false;

        std::string converted;
        bool sameFamilyHandled = false;

        if (conversionOptions.inputFamily == conversionOptions.outputFamily)
        {
            switch (conversionOptions.inputFamily)
            {
                case Family::GameSharkActionReplay:
                    if (conversionOptions.condenseBasicSerialRepeaters)
                        converted = emitSelectedFamily(parseGameSharkActionReplay(input), conversionOptions);
                    else
                        converted = normalizeClassicText(input);
                    sameFamilyHandled = true;
                    break;
                case Family::DuckStation:
                    // Rebuild DuckStation output through the semantic parser so
                    // CMP-style names, notes, credits, and nested groups are
                    // converted into proper DuckStation patch sections even
                    // when both selectors are set to DuckStation.
                    converted = emitSelectedFamily(parseDuckStation(input), conversionOptions);
                    sameFamilyHandled = true;
                    break;
                case Family::Caetla:
                    if (conversionOptions.condenseBasicSerialRepeaters ||
                        conversionOptions.combineCaetla16BitWrites ||
                        conversionOptions.caetla341ExtendedTypes)
                    {
                        converted = emitSelectedFamily(parseCaetla(input), conversionOptions);
                    }
                    else
                    {
                        converted = normalizeCaetlaText(input);
                    }
                    sameFamilyHandled = true;
                    break;
                case Family::Ps1Mips:
                    converted = emitSelectedFamily(parsePs1Mips(input), conversionOptions);
                    sameFamilyHandled = true;
                    break;
                case Family::XploderRaw:
                {
                    if (conversionOptions.condenseBasicSerialRepeaters)
                    {
                        converted = emitSelectedFamily(parseXploderRaw(input), conversionOptions);
                    }
                    else
                    {
                        xploder_converter::Options rawOptions = conversionOptions.xploderOptions;
                        rawOptions.mode = xploder_converter::Mode::Decrypt;
                        rawOptions.groupEncryptedOutput = false;
                        converted = xploder_converter::convertText(input, rawOptions);
                    }
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
            std::vector<Operation> operations = parseSelectedFamily(input, conversionOptions);
            if (conversionOptions.inputFamily == Family::DuckStation &&
                conversionOptions.outputFamily != Family::DuckStation)
            {
                operations = convertDuckStationPatchMetadataToCmpOperations(
                    operations,
                    conversionOptions.xploderOptions.outputCmpDbCompatible);
            }
            converted = emitSelectedFamily(operations, conversionOptions);
        }

        if (conversionOptions.outputFamily == Family::Ps1Mips)
            return converted;

        if (xLinkOutput)
            return xploder_converter::applyOutputXLinkFormatting(converted);

        return xploder_converter::applyOutputCmpDbFormatting(
            converted,
            conversionOptions.xploderOptions.outputCmpDbCompatible);
    }
}
