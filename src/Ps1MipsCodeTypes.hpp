#pragma once

// PS1 MIPS I assembler/disassembler used by the window-to-window
// converter. The PS1 CPU is a little-endian MIPS R3000A (MIPS I).

#include "CodeTypeCommon.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

namespace psx_code_types
{
    namespace ps1_mips
    {
        struct SourceRecord
        {
            enum class Kind { Empty, Comment, Org, Label, Instruction, Byte, Half } kind = Kind::Empty;
            std::string original;
            std::string text;
            std::string label;
            std::vector<std::uint8_t> data;
            std::vector<std::uint16_t> halfData;
            std::uint32_t address = 0;
            bool addressValid = false;
            std::size_t lineNumber = 0;
        };

        inline std::string lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        inline std::string stripAssemblyComment(const std::string& line)
        {
            std::size_t cut = std::string::npos;
            const auto update = [&](std::size_t pos) {
                if (pos != std::string::npos && (cut == std::string::npos || pos < cut))
                    cut = pos;
            };
            update(line.find("//"));
            update(line.find('#'));
            update(line.find(';'));
            return trim(cut == std::string::npos ? line : line.substr(0, cut));
        }

        inline bool parseInteger(std::string text, std::int64_t& value)
        {
            text = trim(text);
            if (text.empty())
                return false;

            int base = 10;
            std::size_t prefix = 0;
            bool negative = false;
            if (text[0] == '+' || text[0] == '-')
            {
                negative = text[0] == '-';
                prefix = 1;
            }
            if (text.size() >= prefix + 2U && text[prefix] == '0' &&
                (text[prefix + 1U] == 'x' || text[prefix + 1U] == 'X'))
            {
                base = 16;
            }
            else
            {
                for (std::size_t i = prefix; i < text.size(); ++i)
                {
                    if ((text[i] >= 'A' && text[i] <= 'F') || (text[i] >= 'a' && text[i] <= 'f'))
                    {
                        base = 16;
                        break;
                    }
                }
            }

            errno = 0;
            char* end = nullptr;
            const long long parsed = std::strtoll(text.c_str(), &end, base);
            if (errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            value = static_cast<std::int64_t>(parsed);
            (void)negative;
            return true;
        }

        inline bool parseAddressOrLabel(
            const std::string& token,
            const std::map<std::string, std::uint32_t>& labels,
            std::uint32_t& value)
        {
            const std::string key = lower(trim(token));
            const auto found = labels.find(key);
            if (found != labels.end())
            {
                value = found->second;
                return true;
            }
            std::int64_t parsed = 0;
            if (!parseInteger(token, parsed) || parsed < 0 || parsed > 0xFFFFFFFFLL)
                return false;
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }

        inline const std::array<const char*, 32>& registerNames()
        {
            static constexpr std::array<const char*, 32> names = {
                "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
                "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
                "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
                "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
            };
            return names;
        }

        inline bool parseRegister(std::string token, int& index)
        {
            token = lower(trim(token));
            if (token.empty())
                return false;
            if (token[0] != '$')
                token.insert(token.begin(), '$');

            static const std::unordered_map<std::string, int> aliases = {
                {"$zero",0},{"$0",0},{"$at",1},{"$1",1},{"$v0",2},{"$2",2},{"$v1",3},{"$3",3},
                {"$a0",4},{"$4",4},{"$a1",5},{"$5",5},{"$a2",6},{"$6",6},{"$a3",7},{"$7",7},
                {"$t0",8},{"$8",8},{"$t1",9},{"$9",9},{"$t2",10},{"$10",10},{"$t3",11},{"$11",11},
                {"$t4",12},{"$12",12},{"$t5",13},{"$13",13},{"$t6",14},{"$14",14},{"$t7",15},{"$15",15},
                {"$s0",16},{"$16",16},{"$s1",17},{"$17",17},{"$s2",18},{"$18",18},{"$s3",19},{"$19",19},
                {"$s4",20},{"$20",20},{"$s5",21},{"$21",21},{"$s6",22},{"$22",22},{"$s7",23},{"$23",23},
                {"$t8",24},{"$24",24},{"$t9",25},{"$25",25},{"$k0",26},{"$26",26},{"$k1",27},{"$27",27},
                {"$gp",28},{"$28",28},{"$sp",29},{"$29",29},{"$fp",30},{"$s8",30},{"$30",30},{"$ra",31},{"$31",31}
            };
            const auto found = aliases.find(token);
            if (found == aliases.end())
                return false;
            index = found->second;
            return true;
        }

        inline std::vector<std::string> splitOperands(const std::string& text)
        {
            std::vector<std::string> output;
            std::string current;
            int parentheses = 0;
            for (char c : text)
            {
                if (c == '(') ++parentheses;
                if (c == ')') --parentheses;
                if (c == ',' && parentheses == 0)
                {
                    output.push_back(trim(current));
                    current.clear();
                }
                else
                {
                    current.push_back(c);
                }
            }
            if (!trim(current).empty())
                output.push_back(trim(current));
            return output;
        }

        inline bool parseByteDirective(const std::string& text, std::vector<std::uint8_t>& bytes, std::string& error)
        {
            const std::string cleaned = trim(text);
            const std::size_t split = cleaned.find_first_of(" \t");
            const std::string mnemonic = lower(split == std::string::npos ? cleaned : cleaned.substr(0, split));
            if (mnemonic != ".byte")
                return false;

            const std::string operandText = split == std::string::npos ? std::string{} : trim(cleaned.substr(split + 1U));
            const std::vector<std::string> operands = splitOperands(operandText);
            if (operands.empty())
            {
                error = ".byte requires at least one value";
                return true;
            }

            bytes.clear();
            for (const std::string& operand : operands)
            {
                std::int64_t value = 0;
                if (!parseInteger(operand, value) || value < 0 || value > 0xFF)
                {
                    error = "invalid .byte value: " + operand;
                    bytes.clear();
                    return true;
                }
                bytes.push_back(static_cast<std::uint8_t>(value));
            }
            return true;
        }

        inline bool parseHalfDirective(const std::string& text, std::vector<std::uint16_t>& halves, std::string& error)
        {
            const std::string cleaned = trim(text);
            const std::size_t split = cleaned.find_first_of(" \t");
            const std::string mnemonic = lower(split == std::string::npos ? cleaned : cleaned.substr(0, split));
            if (mnemonic != ".half")
                return false;

            const std::string operandText = split == std::string::npos ? std::string{} : trim(cleaned.substr(split + 1U));
            const std::vector<std::string> operands = splitOperands(operandText);
            if (operands.empty())
            {
                error = ".half requires at least one value";
                return true;
            }

            halves.clear();
            for (const std::string& operand : operands)
            {
                std::int64_t value = 0;
                if (!parseInteger(operand, value) || value < 0 || value > 0xFFFF)
                {
                    error = "invalid .half value: " + operand;
                    halves.clear();
                    return true;
                }
                halves.push_back(static_cast<std::uint16_t>(value));
            }
            return true;
        }

        inline bool parseMemoryOperand(const std::string& operand, std::int64_t& offset, int& base)
        {
            const std::size_t left = operand.find('(');
            const std::size_t right = operand.find(')', left == std::string::npos ? 0 : left + 1U);
            if (left == std::string::npos || right == std::string::npos || right + 1U != trim(operand).size())
                return false;
            const std::string offsetText = trim(operand.substr(0, left));
            const std::string registerText = trim(operand.substr(left + 1U, right - left - 1U));
            offset = 0;
            if (!offsetText.empty() && !parseInteger(offsetText, offset))
                return false;
            return parseRegister(registerText, base);
        }

        inline std::uint32_t encodeR(int rs, int rt, int rd, int shamt, int funct)
        {
            return (static_cast<std::uint32_t>(rs & 31) << 21) |
                   (static_cast<std::uint32_t>(rt & 31) << 16) |
                   (static_cast<std::uint32_t>(rd & 31) << 11) |
                   (static_cast<std::uint32_t>(shamt & 31) << 6) |
                   static_cast<std::uint32_t>(funct & 63);
        }

        inline std::uint32_t encodeI(int opcode, int rs, int rt, std::uint16_t immediate)
        {
            return (static_cast<std::uint32_t>(opcode & 63) << 26) |
                   (static_cast<std::uint32_t>(rs & 31) << 21) |
                   (static_cast<std::uint32_t>(rt & 31) << 16) |
                   immediate;
        }

        inline bool encodeSigned16(std::int64_t value, std::uint16_t& encoded)
        {
            if (value < -32768 || value > 65535)
                return false;
            encoded = static_cast<std::uint16_t>(value & 0xFFFF);
            return true;
        }

        inline bool encodeBranchOffset(std::uint32_t pc, std::uint32_t target, std::uint16_t& encoded)
        {
            const std::int64_t delta = static_cast<std::int64_t>(static_cast<std::int32_t>(target - (pc + 4U)));
            if ((delta & 3LL) != 0)
                return false;
            const std::int64_t words = delta / 4LL;
            if (words < -32768 || words > 32767)
                return false;
            encoded = static_cast<std::uint16_t>(words & 0xFFFF);
            return true;
        }

        inline bool assembleInstruction(
            const std::string& instruction,
            std::uint32_t pc,
            const std::map<std::string, std::uint32_t>& labels,
            std::uint32_t& word,
            std::string& error)
        {
            const std::string cleaned = trim(instruction);
            const std::size_t split = cleaned.find_first_of(" \t");
            const std::string mnemonic = lower(split == std::string::npos ? cleaned : cleaned.substr(0, split));
            const std::string operandText = split == std::string::npos ? std::string{} : trim(cleaned.substr(split + 1U));
            const std::vector<std::string> operands = splitOperands(operandText);
            word = 0;

            const auto fail = [&](const std::string& message) {
                error = message;
                return false;
            };
            const auto need = [&](std::size_t count) {
                return operands.size() == count;
            };
            const auto immediate16 = [&](const std::string& token, std::uint16_t& value) {
                std::int64_t parsed = 0;
                return parseInteger(token, parsed) && encodeSigned16(parsed, value);
            };

            if (mnemonic == "nop")
            {
                if (!operands.empty()) return fail("nop takes no operands");
                word = 0;
                return true;
            }
            if (mnemonic == ".word")
            {
                if (!need(1)) return fail(".word requires one value");
                std::uint32_t value = 0;
                if (!parseAddressOrLabel(operands[0], labels, value)) return fail("invalid .word value");
                word = value;
                return true;
            }

            if (mnemonic == "j" || mnemonic == "jal")
            {
                if (!need(1)) return fail(mnemonic + " requires one target");
                std::uint32_t target = 0;
                if (!parseAddressOrLabel(operands[0], labels, target)) return fail("invalid jump target");
                if ((target & 3U) != 0U) return fail("jump target is not word aligned");
                if (((pc + 4U) & 0xF0000000U) != (target & 0xF0000000U))
                    return fail("jump target is outside the current 256 MB region");
                word = (mnemonic == "j" ? 0x08000000U : 0x0C000000U) | ((target >> 2) & 0x03FFFFFFU);
                return true;
            }

            if (mnemonic == "b")
            {
                if (!need(1)) return fail("b requires one target");
                std::uint32_t target = 0;
                std::uint16_t offset = 0;
                if (!parseAddressOrLabel(operands[0], labels, target) || !encodeBranchOffset(pc, target, offset))
                    return fail("invalid branch target");
                word = encodeI(0x04, 0, 0, offset);
                return true;
            }

            if (mnemonic == "beqz" || mnemonic == "bnez")
            {
                if (!need(2)) return fail(mnemonic + " requires register,target");
                int rs = 0;
                std::uint32_t target = 0;
                std::uint16_t offset = 0;
                if (!parseRegister(operands[0], rs)) return fail("invalid branch register");
                if (!parseAddressOrLabel(operands[1], labels, target) || !encodeBranchOffset(pc, target, offset))
                    return fail("invalid branch target");
                word = encodeI(mnemonic == "beqz" ? 0x04 : 0x05, rs, 0, offset);
                return true;
            }

            if (mnemonic == "beq" || mnemonic == "bne")
            {
                if (!need(3)) return fail(mnemonic + " requires rs,rt,target");
                int rs = 0, rt = 0;
                std::uint32_t target = 0;
                std::uint16_t offset = 0;
                if (!parseRegister(operands[0], rs) || !parseRegister(operands[1], rt))
                    return fail("invalid branch register");
                if (!parseAddressOrLabel(operands[2], labels, target) || !encodeBranchOffset(pc, target, offset))
                    return fail("invalid branch target");
                word = encodeI(mnemonic == "beq" ? 0x04 : 0x05, rs, rt, offset);
                return true;
            }

            if (mnemonic == "blez" || mnemonic == "bgtz" || mnemonic == "bltz" || mnemonic == "bgez" ||
                mnemonic == "bltzal" || mnemonic == "bgezal")
            {
                if (!need(2)) return fail(mnemonic + " requires rs,target");
                int rs = 0;
                std::uint32_t target = 0;
                std::uint16_t offset = 0;
                if (!parseRegister(operands[0], rs)) return fail("invalid branch register");
                if (!parseAddressOrLabel(operands[1], labels, target) || !encodeBranchOffset(pc, target, offset))
                    return fail("invalid branch target");
                if (mnemonic == "blez") word = encodeI(0x06, rs, 0, offset);
                else if (mnemonic == "bgtz") word = encodeI(0x07, rs, 0, offset);
                else
                {
                    int rt = 0;
                    if (mnemonic == "bgez") rt = 1;
                    else if (mnemonic == "bltzal") rt = 16;
                    else if (mnemonic == "bgezal") rt = 17;
                    word = encodeI(0x01, rs, rt, offset);
                }
                return true;
            }

            if (mnemonic == "move")
            {
                if (!need(2)) return fail("move requires rd,rs");
                int rd = 0, rs = 0;
                if (!parseRegister(operands[0], rd) || !parseRegister(operands[1], rs)) return fail("invalid move register");
                word = encodeR(rs, 0, rd, 0, 0x21);
                return true;
            }

            if (mnemonic == "jr")
            {
                if (!need(1)) return fail("jr requires rs");
                int rs = 0;
                if (!parseRegister(operands[0], rs)) return fail("invalid jr register");
                word = encodeR(rs, 0, 0, 0, 0x08);
                return true;
            }

            if (mnemonic == "jalr")
            {
                if (operands.size() != 1U && operands.size() != 2U) return fail("jalr requires rs or rd,rs");
                int rd = 31, rs = 0;
                if (operands.size() == 1U)
                {
                    if (!parseRegister(operands[0], rs)) return fail("invalid jalr register");
                }
                else if (!parseRegister(operands[0], rd) || !parseRegister(operands[1], rs))
                {
                    return fail("invalid jalr register");
                }
                word = encodeR(rs, 0, rd, 0, 0x09);
                return true;
            }

            static const std::unordered_map<std::string, int> shiftFuncts = {
                {"sll",0x00},{"srl",0x02},{"sra",0x03}
            };
            if (const auto found = shiftFuncts.find(mnemonic); found != shiftFuncts.end())
            {
                if (!need(3)) return fail(mnemonic + " requires rd,rt,shamt");
                int rd = 0, rt = 0;
                std::int64_t shamt = 0;
                if (!parseRegister(operands[0], rd) || !parseRegister(operands[1], rt) ||
                    !parseInteger(operands[2], shamt) || shamt < 0 || shamt > 31)
                    return fail("invalid shift operands");
                word = encodeR(0, rt, rd, static_cast<int>(shamt), found->second);
                return true;
            }

            static const std::unordered_map<std::string, int> variableShiftFuncts = {
                {"sllv",0x04},{"srlv",0x06},{"srav",0x07}
            };
            if (const auto found = variableShiftFuncts.find(mnemonic); found != variableShiftFuncts.end())
            {
                if (!need(3)) return fail(mnemonic + " requires rd,rt,rs");
                int rd = 0, rt = 0, rs = 0;
                if (!parseRegister(operands[0], rd) || !parseRegister(operands[1], rt) || !parseRegister(operands[2], rs))
                    return fail("invalid variable shift operands");
                word = encodeR(rs, rt, rd, 0, found->second);
                return true;
            }

            static const std::unordered_map<std::string, int> threeRegisterFuncts = {
                {"add",0x20},{"addu",0x21},{"sub",0x22},{"subu",0x23},{"and",0x24},{"or",0x25},
                {"xor",0x26},{"nor",0x27},{"slt",0x2A},{"sltu",0x2B}
            };
            if (const auto found = threeRegisterFuncts.find(mnemonic); found != threeRegisterFuncts.end())
            {
                if (!need(3)) return fail(mnemonic + " requires rd,rs,rt");
                int rd = 0, rs = 0, rt = 0;
                if (!parseRegister(operands[0], rd) || !parseRegister(operands[1], rs) || !parseRegister(operands[2], rt))
                    return fail("invalid register operands");
                word = encodeR(rs, rt, rd, 0, found->second);
                return true;
            }

            static const std::unordered_map<std::string, int> twoRegisterFuncts = {
                {"mult",0x18},{"multu",0x19},{"div",0x1A},{"divu",0x1B}
            };
            if (const auto found = twoRegisterFuncts.find(mnemonic); found != twoRegisterFuncts.end())
            {
                if (!need(2)) return fail(mnemonic + " requires rs,rt");
                int rs = 0, rt = 0;
                if (!parseRegister(operands[0], rs) || !parseRegister(operands[1], rt)) return fail("invalid register operands");
                word = encodeR(rs, rt, 0, 0, found->second);
                return true;
            }

            static const std::unordered_map<std::string, int> moveFromFuncts = {
                {"mfhi",0x10},{"mflo",0x12}
            };
            if (const auto found = moveFromFuncts.find(mnemonic); found != moveFromFuncts.end())
            {
                if (!need(1)) return fail(mnemonic + " requires rd");
                int rd = 0;
                if (!parseRegister(operands[0], rd)) return fail("invalid destination register");
                word = encodeR(0, 0, rd, 0, found->second);
                return true;
            }

            static const std::unordered_map<std::string, int> moveToFuncts = {
                {"mthi",0x11},{"mtlo",0x13}
            };
            if (const auto found = moveToFuncts.find(mnemonic); found != moveToFuncts.end())
            {
                if (!need(1)) return fail(mnemonic + " requires rs");
                int rs = 0;
                if (!parseRegister(operands[0], rs)) return fail("invalid source register");
                word = encodeR(rs, 0, 0, 0, found->second);
                return true;
            }

            static const std::unordered_map<std::string, int> immediateOpcodes = {
                {"addi",0x08},{"addiu",0x09},{"slti",0x0A},{"sltiu",0x0B},
                {"andi",0x0C},{"ori",0x0D},{"xori",0x0E}
            };
            if (const auto found = immediateOpcodes.find(mnemonic); found != immediateOpcodes.end())
            {
                if (!need(3)) return fail(mnemonic + " requires rt,rs,immediate");
                int rt = 0, rs = 0;
                std::uint16_t immediate = 0;
                if (!parseRegister(operands[0], rt) || !parseRegister(operands[1], rs) || !immediate16(operands[2], immediate))
                    return fail("invalid immediate operands");
                word = encodeI(found->second, rs, rt, immediate);
                return true;
            }

            if (mnemonic == "lui")
            {
                if (!need(2)) return fail("lui requires rt,immediate");
                int rt = 0;
                std::uint16_t immediate = 0;
                if (!parseRegister(operands[0], rt) || !immediate16(operands[1], immediate)) return fail("invalid lui operands");
                word = encodeI(0x0F, 0, rt, immediate);
                return true;
            }

            if (mnemonic == "li")
            {
                if (!need(2)) return fail("li requires rt,immediate");
                int rt = 0;
                std::int64_t immediate = 0;
                if (!parseRegister(operands[0], rt) || !parseInteger(operands[1], immediate)) return fail("invalid li operands");
                if (immediate >= -32768 && immediate <= 32767)
                    word = encodeI(0x09, 0, rt, static_cast<std::uint16_t>(immediate & 0xFFFF));
                else if (immediate >= 0 && immediate <= 0xFFFF)
                    word = encodeI(0x0D, 0, rt, static_cast<std::uint16_t>(immediate));
                else
                    return fail("li values larger than 16 bits require two instructions and are not supported");
                return true;
            }

            static const std::unordered_map<std::string, int> memoryOpcodes = {
                {"lb",0x20},{"lh",0x21},{"lwl",0x22},{"lw",0x23},{"lbu",0x24},{"lhu",0x25},{"lwr",0x26},
                {"sb",0x28},{"sh",0x29},{"swl",0x2A},{"sw",0x2B},{"swr",0x2E}
            };
            if (const auto found = memoryOpcodes.find(mnemonic); found != memoryOpcodes.end())
            {
                if (!need(2)) return fail(mnemonic + " requires rt,offset(base)");
                int rt = 0, base = 0;
                std::int64_t offset = 0;
                if (!parseRegister(operands[0], rt) || !parseMemoryOperand(operands[1], offset, base) || offset < -32768 || offset > 65535)
                    return fail("invalid memory operands");
                word = encodeI(found->second, base, rt, static_cast<std::uint16_t>(offset & 0xFFFF));
                return true;
            }

            if (mnemonic == "mfc0" || mnemonic == "mtc0")
            {
                if (!need(2)) return fail(mnemonic + " requires rt,cop0-register");
                int rt = 0;
                std::int64_t rd = 0;
                std::string rdText = lower(trim(operands[1]));
                if (!rdText.empty() && rdText[0] == '$') rdText.erase(rdText.begin());
                if (startsWithIgnoreCase(rdText, "c0r")) rdText = rdText.substr(3);
                if (!parseRegister(operands[0], rt) || !parseInteger(rdText, rd) || rd < 0 || rd > 31)
                    return fail("invalid COP0 operands");
                const int rs = mnemonic == "mfc0" ? 0 : 4;
                word = (0x10U << 26) | (static_cast<std::uint32_t>(rs) << 21) |
                       (static_cast<std::uint32_t>(rt) << 16) | (static_cast<std::uint32_t>(rd) << 11);
                return true;
            }

            if (mnemonic == "rfe")
            {
                if (!operands.empty()) return fail("rfe takes no operands");
                word = 0x42000010U;
                return true;
            }

            return fail("unsupported MIPS I instruction: " + mnemonic);
        }

        inline std::vector<SourceRecord> parseSourceRecords(
            const std::string& input,
            std::map<std::string, std::uint32_t>& labels,
            std::vector<std::string>& errors)
        {
            std::vector<SourceRecord> records;
            std::uint32_t pc = 0;
            bool havePc = false;
            bool followHookJumpTarget = false;
            const std::vector<std::string> lines = splitLines(input);

            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                const std::string original = lines[i];
                const std::string code = stripAssemblyComment(original);
                SourceRecord record;
                record.original = original;
                record.lineNumber = i + 1U;

                if (code.empty())
                {
                    record.kind = trim(original).empty() ? SourceRecord::Kind::Empty : SourceRecord::Kind::Comment;
                    records.push_back(std::move(record));
                    continue;
                }

                if (startsWithIgnoreCase(code, ".org"))
                {
                    const std::string operand = trim(std::string_view(code).substr(4));
                    std::int64_t address = 0;
                    if (!parseInteger(operand, address) || address < 0 || address > 0xFFFFFFFFLL)
                    {
                        errors.push_back("Line " + std::to_string(i + 1U) + ": invalid .org address");
                    }
                    else
                    {
                        pc = static_cast<std::uint32_t>(address);
                        havePc = true;
                        followHookJumpTarget = false;
                        record.kind = SourceRecord::Kind::Org;
                        record.address = pc;
                        record.addressValid = true;
                    }
                    records.push_back(std::move(record));
                    continue;
                }

                const std::size_t colon = code.find(':');
                if (colon != std::string::npos)
                {
                    const std::string left = trim(code.substr(0, colon));
                    const std::string right = trim(code.substr(colon + 1U));
                    std::int64_t address = 0;
                    if (!left.empty() && parseInteger(left, address) && address >= 0 && address <= 0xFFFFFFFFLL)
                    {
                        pc = static_cast<std::uint32_t>(address);
                        havePc = true;
                        record.kind = SourceRecord::Kind::Org;
                        record.address = pc;
                        record.addressValid = true;
                        followHookJumpTarget = lower(right) == "hook";
                        if (!right.empty())
                            labels[lower(right)] = pc;
                        records.push_back(std::move(record));
                        continue;
                    }

                    if (!left.empty())
                    {
                        if (!havePc)
                            errors.push_back("Line " + std::to_string(i + 1U) + ": label appears before an address/.org");
                        else
                            labels[lower(left)] = pc;
                        record.kind = SourceRecord::Kind::Label;
                        record.label = left;
                        record.address = pc;
                        record.addressValid = havePc;
                        records.push_back(std::move(record));
                        if (right.empty())
                            continue;
                        // Support "label: instruction" and label-prefixed data directives.
                        SourceRecord instructionRecord;
                        instructionRecord.original = original;
                        instructionRecord.text = right;
                        instructionRecord.address = pc;
                        instructionRecord.addressValid = havePc;
                        instructionRecord.lineNumber = i + 1U;
                        std::string directiveError;
                        if (parseByteDirective(right, instructionRecord.data, directiveError))
                        {
                            instructionRecord.kind = SourceRecord::Kind::Byte;
                            if (!directiveError.empty())
                                errors.push_back("Line " + std::to_string(i + 1U) + ": " + directiveError);
                            const std::size_t byteCount = instructionRecord.data.empty() ? 1U : instructionRecord.data.size();
                            pc += static_cast<std::uint32_t>(byteCount);
                        }
                        else if (parseHalfDirective(right, instructionRecord.halfData, directiveError))
                        {
                            instructionRecord.kind = SourceRecord::Kind::Half;
                            if ((pc & 1U) != 0U)
                            {
                                errors.push_back("Line " + std::to_string(i + 1U) + ": .half address is not 2-byte aligned");
                                instructionRecord.addressValid = false;
                            }
                            if (!directiveError.empty())
                                errors.push_back("Line " + std::to_string(i + 1U) + ": " + directiveError);
                            const std::size_t halfCount = instructionRecord.halfData.empty() ? 1U : instructionRecord.halfData.size();
                            pc += static_cast<std::uint32_t>(halfCount * 2U);
                        }
                        else
                        {
                            instructionRecord.kind = SourceRecord::Kind::Instruction;
                            if ((pc & 3U) != 0U)
                            {
                                errors.push_back("Line " + std::to_string(i + 1U) + ": instruction/.word address is not 4-byte aligned");
                                instructionRecord.addressValid = false;
                            }
                            pc += 4U;
                        }
                        records.push_back(std::move(instructionRecord));
                        continue;
                    }
                }

                record.text = code;
                std::string directiveError;
                const bool isByte = parseByteDirective(code, record.data, directiveError);
                const bool isHalf = !isByte && parseHalfDirective(code, record.halfData, directiveError);
                record.kind = isByte ? SourceRecord::Kind::Byte
                    : (isHalf ? SourceRecord::Kind::Half : SourceRecord::Kind::Instruction);
                if (!directiveError.empty())
                    errors.push_back("Line " + std::to_string(i + 1U) + ": " + directiveError);

                if (!havePc)
                {
                    errors.push_back("Line " + std::to_string(i + 1U) + ": instruction/data requires .org or '0xADDRESS : Label'");
                    record.addressValid = false;
                }
                else
                {
                    record.address = pc;
                    record.addressValid = true;
                    if (isHalf && (pc & 1U) != 0U)
                    {
                        errors.push_back("Line " + std::to_string(i + 1U) + ": .half address is not 2-byte aligned");
                        record.addressValid = false;
                    }
                    else if (!isByte && !isHalf && (pc & 3U) != 0U)
                    {
                        errors.push_back("Line " + std::to_string(i + 1U) + ": instruction/.word address is not 4-byte aligned");
                        record.addressValid = false;
                    }
                    const std::size_t byteCount = isByte
                        ? (record.data.empty() ? 1U : record.data.size())
                        : (isHalf ? (record.halfData.empty() ? 2U : record.halfData.size() * 2U) : 4U);
                    pc += static_cast<std::uint32_t>(byteCount);

                    // Convenience syntax used by the project notes:
                    //   0x800D0A3C : Hook
                    //   j 0x8000FF1C
                    //   ...cave instructions...
                    // "Hook" means only the jump itself is written at the hook;
                    // following assembly begins at that numeric jump target.
                    if (followHookJumpTarget)
                    {
                        const std::string instructionText = trim(record.text);
                        const std::size_t mnemonicEnd = instructionText.find_first_of(" \t");
                        const std::string mnemonic = lower(mnemonicEnd == std::string::npos
                            ? instructionText
                            : instructionText.substr(0, mnemonicEnd));
                        const std::string targetText = mnemonicEnd == std::string::npos
                            ? std::string{}
                            : trim(instructionText.substr(mnemonicEnd + 1U));
                        std::int64_t target = 0;
                        if (!isByte && !isHalf && (mnemonic == "j" || mnemonic == "jal") &&
                            parseInteger(targetText, target) && target >= 0 && target <= 0xFFFFFFFFLL &&
                            (target & 3LL) == 0)
                        {
                            pc = static_cast<std::uint32_t>(target);
                        }
                        else
                        {
                            errors.push_back("Line " + std::to_string(i + 1U) +
                                ": Hook convenience form requires a numeric j/jal target");
                        }
                        followHookJumpTarget = false;
                    }
                }
                records.push_back(std::move(record));
            }
            return records;
        }

        inline std::vector<Operation> parseAssembly(const std::string& input)
        {
            std::map<std::string, std::uint32_t> labels;
            std::vector<std::string> errors;
            const std::vector<SourceRecord> records = parseSourceRecords(input, labels, errors);
            std::vector<Operation> output;

            for (const std::string& error : errors)
                output.push_back(makeText("// MIPS error: " + error, Family::Ps1Mips));

            for (const SourceRecord& record : records)
            {
                if (!record.addressValid)
                    continue;

                if (record.kind == SourceRecord::Kind::Byte)
                {
                    for (std::size_t index = 0; index < record.data.size(); ++index)
                    {
                        Operation operation;
                        operation.kind = OperationKind::Write8;
                        operation.sourceFamily = Family::Ps1Mips;
                        operation.address = maskedPsxAddress(record.address + static_cast<std::uint32_t>(index));
                        operation.value = hex(record.data[index], 2);
                        operation.sourceLines = {record.original};
                        output.push_back(std::move(operation));
                    }
                    continue;
                }

                if (record.kind == SourceRecord::Kind::Half)
                {
                    for (std::size_t index = 0; index < record.halfData.size(); ++index)
                    {
                        Operation operation;
                        operation.kind = OperationKind::Write16;
                        operation.sourceFamily = Family::Ps1Mips;
                        operation.address = maskedPsxAddress(record.address + static_cast<std::uint32_t>(index * 2U));
                        operation.value = hex(record.halfData[index], 4);
                        operation.sourceLines = {record.original};
                        output.push_back(std::move(operation));
                    }
                    continue;
                }

                if (record.kind != SourceRecord::Kind::Instruction)
                    continue;

                std::uint32_t word = 0;
                std::string error;
                if (!assembleInstruction(record.text, record.address, labels, word, error))
                {
                    output.push_back(makeText(
                        "// MIPS error line " + std::to_string(record.lineNumber) + ": " + error + " | " + record.original,
                        Family::Ps1Mips));
                    continue;
                }

                Operation operation;
                operation.kind = OperationKind::Write32;
                operation.sourceFamily = Family::Ps1Mips;
                operation.address = maskedPsxAddress(record.address);
                operation.value = hex(word, 8);
                operation.sourceLines = {record.original};
                output.push_back(std::move(operation));
            }
            return output;
        }

        inline std::string signedHex16(std::uint16_t value)
        {
            return "0x" + hex(value, 4);
        }

        inline std::string registerName(int index)
        {
            return registerNames()[static_cast<std::size_t>(index & 31)];
        }

        inline std::string disassembleWordUnchecked(std::uint32_t word, std::uint32_t pc)
        {
            if (word == 0)
                return "nop";

            const int opcode = static_cast<int>((word >> 26) & 0x3F);
            const int rs = static_cast<int>((word >> 21) & 0x1F);
            const int rt = static_cast<int>((word >> 16) & 0x1F);
            const int rd = static_cast<int>((word >> 11) & 0x1F);
            const int shamt = static_cast<int>((word >> 6) & 0x1F);
            const int funct = static_cast<int>(word & 0x3F);
            const std::uint16_t immediate = static_cast<std::uint16_t>(word & 0xFFFF);
            const std::int16_t signedImmediate = static_cast<std::int16_t>(immediate);

            const auto threeReg = [&](const char* name) {
                return std::string(name) + " " + registerName(rd) + ", " + registerName(rs) + ", " + registerName(rt);
            };
            const auto branch = [&](const char* name, bool includeRt) {
                const std::int64_t displacement = static_cast<std::int64_t>(signedImmediate) * 4LL;
                const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + displacement);
                std::string result = std::string(name) + " " + registerName(rs) + ", ";
                if (includeRt) result += registerName(rt) + ", ";
                result += "0x" + hex(target, 8);
                return result;
            };

            if (opcode == 0)
            {
                switch (funct)
                {
                    case 0x00: return "sll " + registerName(rd) + ", " + registerName(rt) + ", " + std::to_string(shamt);
                    case 0x02: return "srl " + registerName(rd) + ", " + registerName(rt) + ", " + std::to_string(shamt);
                    case 0x03: return "sra " + registerName(rd) + ", " + registerName(rt) + ", " + std::to_string(shamt);
                    case 0x04: return "sllv " + registerName(rd) + ", " + registerName(rt) + ", " + registerName(rs);
                    case 0x06: return "srlv " + registerName(rd) + ", " + registerName(rt) + ", " + registerName(rs);
                    case 0x07: return "srav " + registerName(rd) + ", " + registerName(rt) + ", " + registerName(rs);
                    case 0x08: return "jr " + registerName(rs);
                    case 0x09: return "jalr " + registerName(rd) + ", " + registerName(rs);
                    case 0x10: return "mfhi " + registerName(rd);
                    case 0x11: return "mthi " + registerName(rs);
                    case 0x12: return "mflo " + registerName(rd);
                    case 0x13: return "mtlo " + registerName(rs);
                    case 0x18: return "mult " + registerName(rs) + ", " + registerName(rt);
                    case 0x19: return "multu " + registerName(rs) + ", " + registerName(rt);
                    case 0x1A: return "div " + registerName(rs) + ", " + registerName(rt);
                    case 0x1B: return "divu " + registerName(rs) + ", " + registerName(rt);
                    case 0x20: return threeReg("add");
                    case 0x21: return threeReg("addu");
                    case 0x22: return threeReg("sub");
                    case 0x23: return threeReg("subu");
                    case 0x24: return threeReg("and");
                    case 0x25: return threeReg("or");
                    case 0x26: return threeReg("xor");
                    case 0x27: return threeReg("nor");
                    case 0x2A: return threeReg("slt");
                    case 0x2B: return threeReg("sltu");
                    default: break;
                }
            }

            if (opcode == 0x02 || opcode == 0x03)
            {
                const std::uint32_t target = ((pc + 4U) & 0xF0000000U) | ((word & 0x03FFFFFFU) << 2);
                return std::string(opcode == 0x02 ? "j " : "jal ") + "0x" + hex(target, 8);
            }
            if (opcode == 0x04) return branch("beq", true);
            if (opcode == 0x05) return branch("bne", true);
            if (opcode == 0x06) return branch("blez", false);
            if (opcode == 0x07) return branch("bgtz", false);
            if (opcode == 0x01)
            {
                const char* name = nullptr;
                if (rt == 0) name = "bltz";
                else if (rt == 1) name = "bgez";
                else if (rt == 16) name = "bltzal";
                else if (rt == 17) name = "bgezal";
                if (name != nullptr) return branch(name, false);
            }

            static const std::unordered_map<int, const char*> immediateNames = {
                {0x08,"addi"},{0x09,"addiu"},{0x0A,"slti"},{0x0B,"sltiu"},
                {0x0C,"andi"},{0x0D,"ori"},{0x0E,"xori"}
            };
            if (const auto found = immediateNames.find(opcode); found != immediateNames.end())
            {
                const bool logical = opcode == 0x0C || opcode == 0x0D || opcode == 0x0E;
                const std::string immText = logical
                    ? "0x" + hex(immediate, 4)
                    : std::to_string(static_cast<int>(signedImmediate));
                return std::string(found->second) + " " + registerName(rt) + ", " + registerName(rs) + ", " + immText;
            }
            if (opcode == 0x0F)
                return "lui " + registerName(rt) + ", 0x" + hex(immediate, 4);

            static const std::unordered_map<int, const char*> memoryNames = {
                {0x20,"lb"},{0x21,"lh"},{0x22,"lwl"},{0x23,"lw"},{0x24,"lbu"},{0x25,"lhu"},{0x26,"lwr"},
                {0x28,"sb"},{0x29,"sh"},{0x2A,"swl"},{0x2B,"sw"},{0x2E,"swr"}
            };
            if (const auto found = memoryNames.find(opcode); found != memoryNames.end())
            {
                return std::string(found->second) + " " + registerName(rt) + ", 0x" + hex(immediate, 4) + "(" + registerName(rs) + ")";
            }

            if (opcode == 0x10)
            {
                if (word == 0x42000010U) return "rfe";
                if (rs == 0) return "mfc0 " + registerName(rt) + ", $c0r" + std::to_string(rd);
                if (rs == 4) return "mtc0 " + registerName(rt) + ", $c0r" + std::to_string(rd);
            }

            return ".word 0x" + hex(word, 8);
        }

        inline std::string disassembleWord(std::uint32_t word, std::uint32_t pc)
        {
            const std::string decoded = disassembleWordUnchecked(word, pc);
            if (startsWithIgnoreCase(decoded, ".word"))
                return decoded;

            // Only emit a mnemonic when assembling that exact text recreates the
            // original 32-bit word. This preserves noncanonical/reserved encodings
            // whose unused fields would otherwise be normalized by the assembler.
            std::uint32_t rebuilt = 0;
            std::string error;
            const std::map<std::string, std::uint32_t> noLabels;
            if (!assembleInstruction(decoded, pc, noLabels, rebuilt, error) || rebuilt != word)
                return ".word 0x" + hex(word, 8);
            return decoded;
        }

        struct WordAtAddress
        {
            std::uint32_t address = 0;
            std::string value;
            int widthBits = 32;
        };

        inline std::vector<std::uint8_t> canonicalType5Bytes(const Operation& operation)
        {
            std::vector<std::uint8_t> bytes;
            if (operation.sourceLines.size() <= 1U)
                return operation.payload;
            for (std::size_t i = 1; i < operation.sourceLines.size(); ++i)
            {
                ParsedCodeLine row;
                if (!parseCodeLine(operation.sourceLines[i], row) || row.valueText.size() != 4 || hasWildcard(row.valueText))
                    return {};
                const std::string compact = row.addressText + row.valueText;
                for (std::size_t pos = 0; pos + 1U < compact.size(); pos += 2U)
                {
                    const int hi = hexValue(compact[pos]);
                    const int lo = hexValue(compact[pos + 1U]);
                    if (hi < 0 || lo < 0) return {};
                    bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
                }
            }
            if (!operation.sourceLines.empty())
            {
                ParsedCodeLine header;
                if (parseCodeLine(operation.sourceLines.front(), header))
                {
                    std::uint32_t size = 0;
                    if (parseHex(header.valueText, size) && bytes.size() > size)
                        bytes.resize(size);
                }
            }
            return bytes;
        }

        inline std::vector<WordAtAddress> collectWords(const std::vector<Operation>& operations, std::vector<std::string>& comments)
        {
            std::vector<WordAtAddress> words;
            for (std::size_t i = 0; i < operations.size(); ++i)
            {
                const Operation& operation = operations[i];
                if (operation.kind == OperationKind::Text)
                {
                    const std::string text = trim(operation.text);
                    if (!text.empty() && text[0] != '[' && !startsWithIgnoreCase(text, "Type =") &&
                        !startsWithIgnoreCase(text, "Activation =") && !startsWithIgnoreCase(text, "Description =") &&
                        !startsWithIgnoreCase(text, "Author ="))
                    {
                        comments.push_back("# " + text);
                    }
                    continue;
                }
                if ((operation.kind == OperationKind::CompareNotEqual16 ||
                     operation.kind == OperationKind::CompareEqual16) &&
                    i + 1U < operations.size() &&
                    operations[i + 1U].kind == OperationKind::XploderMegaCode)
                {
                    comments.push_back(
                        std::string("# Xploder Type ") +
                        (operation.kind == OperationKind::CompareNotEqual16 ? "9" : "7") +
                        " installation guard: execute the following Type 6 block when [0x" +
                        hex(0x80000000U | maskedPsxAddress(operation.address), 8) +
                        (operation.kind == OperationKind::CompareNotEqual16 ? "] != 0x" : "] == 0x") +
                        wordValue(operation.value));
                    continue;
                }
                if (operation.kind == OperationKind::Write8)
                {
                    words.push_back({0x80000000U | maskedPsxAddress(operation.address), byteValue(operation.value), 8});
                    continue;
                }
                if (operation.kind == OperationKind::Scratchpad8)
                {
                    words.push_back({operation.address, byteValue(operation.value), 8});
                    continue;
                }
                if (operation.kind == OperationKind::Scratchpad16)
                {
                    words.push_back({operation.address, wordValue(operation.value), 16});
                    continue;
                }
                if (operation.kind == OperationKind::Scratchpad32)
                {
                    words.push_back({operation.address, dwordValue(operation.value), 32});
                    continue;
                }
                if (operation.kind == OperationKind::Write32)
                {
                    words.push_back({0x80000000U | maskedPsxAddress(operation.address), dwordValue(operation.value), 32});
                    continue;
                }
                if (operation.kind == OperationKind::Write16 && i + 1U < operations.size())
                {
                    const Operation& next = operations[i + 1U];
                    if (next.kind == OperationKind::Write16 && next.address == operation.address + 2U &&
                        !operation.defaultOff && !next.defaultOff && operation.suffix.empty() && next.suffix.empty())
                    {
                        words.push_back({0x80000000U | maskedPsxAddress(operation.address), wordValue(next.value) + wordValue(operation.value), 32});
                        ++i;
                        continue;
                    }
                }
                if (operation.kind == OperationKind::Write16)
                {
                    words.push_back({0x80000000U | maskedPsxAddress(operation.address), wordValue(operation.value), 16});
                    continue;
                }
                if (operation.kind == OperationKind::XploderMegaCode)
                {
                    if (operation.payload.empty() || (operation.payload.size() % 4U) != 0U)
                    {
                        comments.push_back("# Skipped malformed Xploder Type 6 payload while producing MIPS output");
                        continue;
                    }

                    comments.push_back(
                        "# Xploder Type 6 breakpoint address: 0x" +
                        hex(operation.secondAddress, 8) +
                        (operation.compareValue.empty()
                            ? std::string{}
                            : " | control: 0x" + operation.compareValue));
                    comments.push_back(
                        "# Xploder Type 6 executable installed at 0x80000040 (" +
                        std::to_string(operation.payload.size()) + " bytes)");

                    for (std::size_t offset = 0; offset < operation.payload.size(); offset += 4U)
                    {
                        const std::uint32_t word =
                            static_cast<std::uint32_t>(operation.payload[offset]) |
                            (static_cast<std::uint32_t>(operation.payload[offset + 1U]) << 8) |
                            (static_cast<std::uint32_t>(operation.payload[offset + 2U]) << 16) |
                            (static_cast<std::uint32_t>(operation.payload[offset + 3U]) << 24);
                        words.push_back({
                            0x80000000U | maskedPsxAddress(
                                operation.address + static_cast<std::uint32_t>(offset)),
                            hex(word, 8),
                            32});
                    }
                    continue;
                }
                if (operation.kind == OperationKind::XploderMassWrite)
                {
                    const std::vector<std::uint8_t> bytes = canonicalType5Bytes(operation);
                    std::size_t offset = 0;
                    while (offset + 3U < bytes.size())
                    {
                        const std::uint32_t word = static_cast<std::uint32_t>(bytes[offset]) |
                            (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8) |
                            (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16) |
                            (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24);
                        words.push_back({0x80000000U | maskedPsxAddress(operation.address + static_cast<std::uint32_t>(offset)), hex(word, 8), 32});
                        offset += 4U;
                    }
                    if (offset + 1U < bytes.size())
                    {
                        const std::uint16_t half = static_cast<std::uint16_t>(bytes[offset]) |
                            static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8);
                        words.push_back({
                            0x80000000U | maskedPsxAddress(operation.address + static_cast<std::uint32_t>(offset)),
                            hex(half, 4),
                            16});
                        offset += 2U;
                    }
                    if (offset < bytes.size())
                    {
                        words.push_back({
                            0x80000000U | maskedPsxAddress(operation.address + static_cast<std::uint32_t>(offset)),
                            hex(bytes[offset], 2),
                            8});
                    }
                    continue;
                }
                comments.push_back("# Skipped non-instruction operation while producing MIPS output");
            }
            return words;
        }

        inline std::vector<std::string> emitAssembly(const std::vector<Operation>& operations)
        {
            std::vector<std::string> comments;
            const std::vector<WordAtAddress> words = collectWords(operations, comments);

            std::vector<std::string> lines;
            for (const std::string& comment : comments)
                lines.push_back(comment);
            if (!comments.empty() && !words.empty()) lines.push_back({});

            std::uint32_t expected = 0;
            bool first = true;
            for (const WordAtAddress& item : words)
            {
                if (first || item.address != expected)
                {
                    if (!first) lines.push_back({});
                    lines.push_back(".org 0x" + hex(item.address, 8));
                    lines.push_back({});
                }
                if (item.widthBits == 8)
                {
                    lines.push_back(".byte 0x" + byteValue(item.value));
                    expected = item.address + 1U;
                }
                else if (item.widthBits == 16)
                {
                    lines.push_back(".half 0x" + wordValue(item.value));
                    expected = item.address + 2U;
                }
                else
                {
                    if (hasWildcard(item.value))
                        lines.push_back(".word 0x" + dwordValue(item.value));
                    else
                    {
                        std::uint32_t word = 0;
                        parseHex(item.value, word);
                        lines.push_back(disassembleWord(word, item.address));
                    }
                    expected = item.address + 4U;
                }
                first = false;
            }
            return lines;
        }

        inline std::vector<Operation> packMipsWritesAsType5(const std::vector<Operation>& operations)
        {
            const auto isPackable = [](const Operation& operation) {
                return operation.sourceFamily == Family::Ps1Mips &&
                    (operation.kind == OperationKind::Write32 || operation.kind == OperationKind::Write16 || operation.kind == OperationKind::Write8) &&
                    !hasWildcard(operation.value);
            };
            const auto widthBytes = [](const Operation& operation) -> std::uint32_t {
                return operation.kind == OperationKind::Write8 ? 1U : (operation.kind == OperationKind::Write16 ? 2U : 4U);
            };
            const auto appendBytes = [](const Operation& operation, std::vector<std::uint8_t>& payload) {
                std::uint32_t value = 0;
                parseHex(operation.value, value);
                if (operation.kind == OperationKind::Write8)
                {
                    payload.push_back(static_cast<std::uint8_t>(value & 0xFFU));
                    return;
                }
                if (operation.kind == OperationKind::Write16)
                {
                    payload.push_back(static_cast<std::uint8_t>(value & 0xFFU));
                    payload.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
                    return;
                }
                payload.push_back(static_cast<std::uint8_t>(value & 0xFFU));
                payload.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
                payload.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
                payload.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
            };

            std::vector<Operation> output;
            std::size_t i = 0;
            while (i < operations.size())
            {
                const Operation& first = operations[i];
                if (!isPackable(first))
                {
                    output.push_back(first);
                    ++i;
                    continue;
                }

                std::size_t end = i + 1U;
                std::uint32_t nextAddress = first.address + widthBytes(first);
                while (end < operations.size())
                {
                    const Operation& candidate = operations[end];
                    if (!isPackable(candidate) || candidate.address != nextAddress)
                        break;
                    nextAddress += widthBytes(candidate);
                    ++end;
                }

                Operation mass;
                mass.kind = OperationKind::XploderMassWrite;
                mass.sourceFamily = Family::Ps1Mips;
                mass.address = first.address;
                for (std::size_t item = i; item < end; ++item)
                    appendBytes(operations[item], mass.payload);
                output.push_back(std::move(mass));
                i = end;
            }
            return output;
        }

    }

    inline std::vector<Operation> parsePs1Mips(const std::string& input)
    {
        return ps1_mips::parseAssembly(input);
    }

    inline std::vector<std::string> emitPs1Mips(const std::vector<Operation>& operations)
    {
        return ps1_mips::emitAssembly(operations);
    }
}
