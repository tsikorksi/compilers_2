#include <cassert>
#include "instruction.h"
#include "exceptions.h"
#include "lowlevel.h"
#include "lowlevel_formatter.h"

namespace {

const int BYTE  = 0;
const int WORD  = 1;
const int DWORD = 2;
const int QUAD  = 3;

// names of machine registers for the 8 bit, 16 bit,
// 32 bit, and 64 bit sizes
const char *mreg_operand_names[][4] = {
  { "al",   "ax",   "eax",  "rax" },
  { "bl",   "bx",   "ebx",  "rbx" },
  { "cl",   "cx",   "ecx",  "rcx" },
  { "dl",   "dx",   "edx",  "rdx" },
  { "sil",  "si",   "esi",  "rsi" },
  { "dil",  "di",   "edi",  "rdi" },
  { "spl",  "sp",   "esp",  "rsp" },
  { "bpl",  "bp",   "ebp",  "rbp" },
  { "r8b",  "r8w",  "r8d",  "r8" },
  { "r9b",  "r9w",  "r9d",  "r9" },
  { "r10b", "r10w", "r10d", "r10" },
  { "r11b", "r11w", "r11d", "r11" },
  { "r12b", "r12w", "r12d", "r12" },
  { "r13b", "r13w", "r13d", "r13" },
  { "r14b", "r14w", "r14d", "r14" },
  { "r15b", "r15w", "r15d", "r15" },
};

const int num_mregs = sizeof(mreg_operand_names) / sizeof(mreg_operand_names[0]);

std::string format_reg(int regnum, int size) {
  assert(regnum >= 0 && regnum < num_mregs);
  assert(size >= BYTE && size <= QUAD);
  return std::string("%") + mreg_operand_names[regnum][size];
}

}

LowLevelFormatter::LowLevelFormatter() {
}

LowLevelFormatter::~LowLevelFormatter() {
}

std::string LowLevelFormatter::format_operand(const Operand &operand) const {
  if (operand.is_non_reg()) {
    // non-register operands are handled by the base class
    return Formatter::format_operand(operand);
  }

  switch (operand.get_kind()) {
  case Operand::MREG8:
    return format_reg(operand.get_base_reg(), BYTE);

  case Operand::MREG16:
    return format_reg(operand.get_base_reg(), WORD);

  case Operand::MREG32:
    return format_reg(operand.get_base_reg(), DWORD);

  case Operand::MREG64:
    return format_reg(operand.get_base_reg(), QUAD);

  case Operand::MREG64_MEM:
    return "(" + format_reg(operand.get_base_reg(), QUAD) + ")";

  case Operand::MREG64_MEM_IDX:
    return "(" + format_reg(operand.get_base_reg(), QUAD) + "," + format_reg(operand.get_index_reg(), QUAD) + ")";

  case Operand::MREG64_MEM_OFF:
    return std::to_string(operand.get_offset()) + "(" + format_reg(operand.get_base_reg(), QUAD) + ")";

  default:
    assert(false);
    return "<unknown operand kind>";
  }
}

std::string LowLevelFormatter::format_instruction(const Instruction *ins) const {
  LowLevelOpcode opcode = LowLevelOpcode(ins->get_opcode());

  const char *mnemonic_ptr = lowlevel_opcode_to_str(opcode);
  if (mnemonic_ptr == nullptr)
    RuntimeError::raise("Unknown low level opcode: %d", int(opcode));

  std::string mnemonic(mnemonic_ptr);

  std::string buf;

  buf += mnemonic;

  // pad mnemonics to 8 characters
  unsigned padding = (mnemonic.size() < 8U) ? 8U - mnemonic.size() : 0;
  buf += ("         " + (8U - padding));
  for (unsigned i = 0; i < ins->get_num_operands(); i++) {
    if (i > 0) {
      buf += ", ";
    }
    buf += format_operand(ins->get_operand(i));
  }

  return buf;
}
