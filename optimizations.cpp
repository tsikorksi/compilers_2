#include "optimizations.h"

#include "cfg.h"
#include "highlevel.h"

// HIGH LEVEL

LocalOptimizationHighLevel::LocalOptimizationHighLevel(
        const std::shared_ptr<ControlFlowGraph>& cfg)
        : ControlFlowGraphTransform(cfg){

}


std::shared_ptr<InstructionSequence> LocalOptimizationHighLevel::transform_basic_block(const InstructionSequence *orig_bb) {
    std::shared_ptr<InstructionSequence> result;
    result = constant_propagation(orig_bb->duplicate());
    result = copy_propagation(result->duplicate());


    return result;
}

/// Implments constant propagation
/// \param block the wrapped instruction sequence
/// \return a shred_ptr to a Instruction Sequence
std::shared_ptr<InstructionSequence> LocalOptimizationHighLevel::constant_propagation(InstructionSequence *block) {
    std::shared_ptr<InstructionSequence> result(new InstructionSequence());
    // empty  map of register number to constant value
    std::map<int, int> constants;


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

///// Copy propagation, the same as constant propagation but vreg to vreg
///// \param block
///// \return
std::shared_ptr<InstructionSequence> LocalOptimizationHighLevel::copy_propagation(InstructionSequence *block) {
    std::shared_ptr<InstructionSequence> result(new InstructionSequence());
    // empty  map of register number to constant value
    std::map<int, long> constants;


    for (auto i = block->cbegin(); i != block->cend(); i++) {
        Instruction *instruction = *i;
        auto opcode = static_cast<HighLevelOpcode>(instruction->get_opcode());

        // Make sure not doing invalid instruction, enter jmp etc
        if (instruction->get_num_operands() < 2) {
            result->append(instruction->duplicate());
        } else if(match_hl(HighLevelOpcode::HINS_mov_b, opcode) && instruction->get_operand(1).has_base_reg()) {
        // We are moving constant into vreg
            constants[instruction->get_operand(0).get_base_reg()] = instruction->get_operand(1).get_base_reg();
        }
        else {

            Operand origin = instruction->get_operand(0);

            if (instruction->get_num_operands() == 2) {
                Operand left = instruction->get_operand(1);

                if (left.has_base_reg() &&
                    constants.find(left.get_base_reg()) != constants.end() ) {
                    // Make new instruction with constant swapped in
                    Operand copied_constant(Operand::VREG, constants[left.get_base_reg()]);
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

                    Operand copied_constant(Operand::VREG, constants[left.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, copied_constant, right));
                } else if (right.has_base_reg() &&
                           constants.find(right.get_base_reg()) != constants.end() ) {

                    Operand copied_constant(Operand::VREG, constants[right.get_base_reg()]);
                    result->append(new Instruction(instruction->get_opcode(), origin, instruction->get_operand(1), copied_constant));
                } else if (left.has_base_reg() &&
                           constants.find(left.get_base_reg()) != constants.end() && right.has_base_reg() &&
                           constants.find(right.get_base_reg()) != constants.end()) {
                    Operand copied_left(Operand::VREG, constants[left.get_base_reg()]);
                    Operand copied_right(Operand::VREG, constants[right.get_base_reg()]);
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



// Copied from low level because its useful
bool LocalOptimizationHighLevel::match_hl(int base, int hl_opcode) {
    return hl_opcode >= base && hl_opcode < (base + 4);
}

std::shared_ptr<ControlFlowGraph> LocalOptimizationHighLevel::transform_cfg() {
    return ControlFlowGraphTransform::transform_cfg();
}

// LOW LEVEL


std::shared_ptr<ControlFlowGraph> LocalOptimizationLowLevel::transform_cfg() {
    return {};
}

LocalOptimizationLowLevel::LocalOptimizationLowLevel(ControlFlowGraph cfg) {

}
