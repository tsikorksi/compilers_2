#ifndef OPERAND_H
#define OPERAND_H

#include <string>

// Operand of an Instruction.
// Can be used for both high-level linear IR code and low-level
// (machine) linear IR code.  These have value semantics, and
// can be passed and returned by value.

class Operand {
public:
  enum Kind {
    NONE,            // used only for invalid Operand values

                     // Description                       Example
                     // --------------------------------  ---------------------

    VREG,            // just a vreg                       vr0
    VREG_MEM,        // memref using vreg ptr             (vr0)
    VREG_MEM_IDX,    // memref using vreg ptr+index       (vr0, vr1)
    VREG_MEM_OFF,    // memref using vreg ptr+imm offset  8(vr0q)

    MREG8,           // just an mreg                      %al
    MREG16,          // just an mreg                      %ax
    MREG32,          // just an mreg                      %eax
    MREG64,          // just an mreg                      %rax
    MREG64_MEM,      // memref using mreg ptr             (%rax)
    MREG64_MEM_IDX,  // memref using mreg ptr+index       (%rax,%rsi)
    MREG64_MEM_OFF,  // memref using mreg ptr+imm offset  8(%rax)

    IMM_IVAL,        // immediate 8-bit signed int        $1

    LABEL,           // label                             .L0
    IMM_LABEL,       // immediate label                   $printf
  };

private:
  Kind m_kind;
  int m_basereg, m_index_reg;
  long m_imm_ival;
  std::string m_label;

public:
  Operand(Kind kind = NONE);

  // ival1 is either basereg or imm_ival (depending on operand Kind)
  Operand(Kind kind, long ival1);

  // ival2 is either index_reg or imm_ival (depending on operand kind)
  Operand(Kind kind, int basereg, long ival2);

  // for label or immediate label operands
  Operand(Kind kind, const std::string &label);

  ~Operand();

  // use compiler-generated copy ctor and assignment op

  Kind get_kind() const;

  // Is the operand an immediate integer value?
  bool is_imm_ival() const;

  // Is the operand a non-immediate label?
  bool is_label() const;

  // Is the operand an immediate label?
  bool is_imm_label() const;

  // Does the operand have an index register?
  bool has_index_reg() const;

  // Does the operand have an immediate integer offset?
  bool has_offset() const;

  // Is the operand a non-register operand? (i.e., no base or index reg)
  bool is_non_reg() const;

  // Is the operand a memory reference?
  bool is_memref() const;

  // Does the operand have an immediate integer value?
  // (Either because it *is* an immediate integer value, or because
  // it has an immediate integer offset.)
  bool has_imm_ival() const;

  // Does the operand have a label?
  // (Either because it is a label, or is an immediate label.)
  bool has_label() const;

  int get_base_reg() const;
  int get_index_reg() const;
  long get_imm_ival() const;

  Operand to_memref() const;

  std::string get_label() const;
};

#endif // OPERAND_H
