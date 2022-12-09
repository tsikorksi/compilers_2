#include "optimizations.h"

#include "cfg.h"
#include "highlevel.h"

// ConstantPropagation

ConstantPropagation::ConstantPropagation(
        const std::shared_ptr<ControlFlowGraph>& cfg)
        : ControlFlowGraphTransform(cfg){

}


std::shared_ptr<InstructionSequence> ConstantPropagation::transform_basic_block(const InstructionSequence *orig_bb) {
    std::shared_ptr<InstructionSequence> result;
    result = constant_propagation(orig_bb->duplicate());

    return result;
}

/// Implments constant propagation
/// \param block the wrapped instruction sequence
/// \return a shred_ptr to a Instruction Sequence
std::shared_ptr<InstructionSequence> ConstantPropagation::constant_propagation(InstructionSequence *block) {
    std::shared_ptr<InstructionSequence> result(new InstructionSequence());
    // empty  map of register number to constant value
    std::map<int, long> constants;


    for (auto i = block->cbegin(); i != block->cend(); i++) {
        Instruction *instruction = *i;
        auto opcode = static_cast<HighLevelOpcode>(instruction->get_opcode());

        // Make sure not doing invalid instruction, enter jmp etc
        if (instruction->get_num_operands() < 2) {
            result->append(instruction->duplicate());
        } else if(match_hl(HighLevelOpcode::HINS_mov_b, opcode) && instruction->get_operand(1).is_imm_ival()) {
            // We are moving constant into vreg
            constants[instruction->get_operand(0).get_base_reg()] = instruction->get_operand(1).get_imm_ival();
        }
        else {

            Operand origin = instruction->get_operand(0);

            if (instruction->get_num_operands() == 2) {
                Operand left = instruction->get_operand(1);

                if (left.has_base_reg() &&
                    constants.find(left.get_base_reg()) != constants.end() ) {
                    // Make new instruction with constant swapped in
                    Operand copied_constant(Operand::IMM_IVAL, constants[left.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, copied_constant));
                } else {
                    result->append(instruction->duplicate());
                }

            } else if (instruction->get_num_operands() == 3) {
                // Make new instruction with constant swapped in, trying either side
                Operand left = instruction->get_operand(1);
                Operand right = instruction->get_operand(2);

                if (left.has_base_reg() &&
                    constants.find(left.get_base_reg()) != constants.end() ) {

                    Operand copied_constant(Operand::IMM_IVAL, constants[left.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, copied_constant, right));
                } else if (right.has_base_reg() &&
                           constants.find(right.get_base_reg()) != constants.end() ) {

                    Operand copied_constant(Operand::IMM_IVAL, constants[right.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, instruction->get_operand(1), copied_constant));
                } else if (left.has_base_reg() &&
                           constants.find(left.get_base_reg()) != constants.end() && right.has_base_reg() &&
                           constants.find(right.get_base_reg()) != constants.end()) {
                    Operand copied_left(Operand::IMM_IVAL, constants[left.get_base_reg()]);
                    Operand copied_right(Operand::IMM_IVAL, constants[right.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, copied_left, copied_right));
                } else {
                    result->append(instruction->duplicate());
                }
            } else {
                continue;
            }
        }


    }


    return result;
}

// Copied from low level because it's useful
bool ConstantPropagation::match_hl(int base, int hl_opcode) {
    return hl_opcode >= base && hl_opcode < (base + 4);
}


// CopyPropagation

CopyPropagation::CopyPropagation(
        const std::shared_ptr<ControlFlowGraph>& cfg)
        : ControlFlowGraphTransform(cfg){

}

bool CopyPropagation::match_hl(int base, int hl_opcode) {
    return hl_opcode >= base && hl_opcode < (base + 4);
}


bool CopyPropagation::is_caller_saved(int vreg_num) {
    if (vreg_num <= 2) {
        return true;
    }
    return false;
}


std::shared_ptr<InstructionSequence> CopyPropagation::transform_basic_block(const InstructionSequence *orig_bb) {
    std::shared_ptr<InstructionSequence> result;
    result = copy_propagation(orig_bb->duplicate());

    return result;
}

/// Copy propagation, the same as constant propagation but vreg to vreg
/// \param block
/// \return
std::shared_ptr<InstructionSequence> CopyPropagation::copy_propagation(InstructionSequence *block) {
    std::shared_ptr<InstructionSequence> result(new InstructionSequence());
    // empty  map of register number to constant value
    constants.clear();

    for (auto i = block->cbegin(); i != block->cend(); i++) {
        Instruction *instruction = *i;
        auto opcode = static_cast<HighLevelOpcode>(instruction->get_opcode());

        if (HighLevel::is_def(instruction)) {
            if (match_hl(HighLevelOpcode::HINS_mov_b, opcode)) {
                if (instruction->get_operand(1).is_imm_ival()) {
                    constants.erase(instruction->get_operand(0).get_base_reg());
                } else {
                    constants[instruction->get_operand(0).get_base_reg()] = instruction->get_operand(1).get_base_reg();
                }
            }


            Operand origin = instruction->get_operand(0);
            bool same = true;
            if (instruction->get_num_operands() == 2) {
                Operand left = instruction->get_operand(1);
                if (left.has_base_reg() && constants.find(left.get_base_reg()) != constants.end()) {

                    Operand copied_constant(Operand::VREG, constants[left.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, copied_constant));
                    same = false;
                } else {

                }

            } else if (instruction->get_num_operands() == 3) {
                // Make new instruction with constant swapped in, trying either side
                Operand left = instruction->get_operand(1);
                Operand right = instruction->get_operand(2);

                if (left.has_base_reg() && constants.find(left.get_base_reg()) != constants.end()) {
                    same = false;
                    Operand copied_constant(Operand::VREG, constants[left.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, copied_constant, right));
                }
                if (right.has_base_reg() && constants.find(right.get_base_reg()) != constants.end()) {
                    same = false;
                    Operand copied_constant(Operand::VREG, constants[right.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, instruction->get_operand(1),
                                                   copied_constant));
                }
            }
            // No change detected
            if (same) {
                result->append(instruction->duplicate());
            }

        } else {
            result->append(instruction->duplicate());
        }


    }


    return result;
}




std::shared_ptr<ControlFlowGraph> ConstantPropagation::transform_cfg() {
    return ControlFlowGraphTransform::transform_cfg();
}

// LIVE ANALYSIS
LiveRegisters::LiveRegisters(const std::shared_ptr<ControlFlowGraph> &cfg)
        : ControlFlowGraphTransform(cfg)
        , m_live_vregs(cfg) {
    m_live_vregs.execute();
}

LiveRegisters::~LiveRegisters() {
}

std::shared_ptr<InstructionSequence>
LiveRegisters::transform_basic_block(const InstructionSequence *orig_bb) {
    // LiveVregs needs a pointer to a BasicBlock object to get a dataflow
    // fact for that basic block
    const auto *orig_bb_as_basic_block =
            dynamic_cast<const BasicBlock *>(orig_bb);

    std::shared_ptr<InstructionSequence> result_iseq(new InstructionSequence());

    for (auto i = orig_bb->cbegin(); i != orig_bb->cend(); ++i) {
        Instruction *orig_ins = *i;
        bool preserve_instruction = true;

        if (HighLevel::is_def(orig_ins)) {
            Operand dest = orig_ins->get_operand(0);

            LiveVregs::FactType live_after =
                    m_live_vregs.get_fact_after_instruction(orig_bb_as_basic_block, orig_ins);

            if (!live_after.test(dest.get_base_reg()) && !is_caller_saved(dest.get_base_reg()))
                // destination register is dead immediately after this instruction,
                // so it can be eliminated
                preserve_instruction = false;
        }

        if (preserve_instruction)
            result_iseq->append(orig_ins->duplicate());
    }

    return result_iseq;

}


bool LiveRegisters::is_caller_saved(int vreg_num) {
    if (vreg_num <= 2) {
        return true;
    }
    return false;
}

