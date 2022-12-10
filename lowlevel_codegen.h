#ifndef LOWLEVEL_CODEGEN_H
#define LOWLEVEL_CODEGEN_H

#include <memory>
#include "instruction_seq.h"

// A LowLevelCodeGen object transforms an InstructionSequence containing
// high-level instructions into an InstructionSequence containing
// low-level (x86-64) instructions.
class LowLevelCodeGen {
private:

    int m_total_memory_storage;
    bool m_optimize;
    int vreg_boundary;
    int next_local_vreg = 0;

public:

    explicit LowLevelCodeGen(bool optimize);
    virtual ~LowLevelCodeGen();

    std::shared_ptr<InstructionSequence> generate(const std::shared_ptr<InstructionSequence> &hl_iseq);

private:
    int get_vreg_boundary () const{return vreg_boundary;}
    std::shared_ptr<InstructionSequence> translate_hl_to_ll(const std::shared_ptr<InstructionSequence> &hl_iseq);
    void translate_instruction(Instruction *hl_ins, const std::shared_ptr<InstructionSequence> &ll_iseq);
    Operand get_ll_operand(Operand hl_operand, int size, const std::shared_ptr<InstructionSequence> &ll_iseq);

    long get_offset(int vreg) const;
};

#endif // LOWLEVEL_CODEGEN_H
