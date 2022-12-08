//
// Created by tadzi on 12/7/2022.
//

#ifndef COMPILERS_2_OPTIMIZATIONS_H
#define COMPILERS_2_OPTIMIZATIONS_H

#include "cfg.h"
#include "cfg_transform.h"
#include "live_vregs.h"

class ConstantPropagation : public ControlFlowGraphTransform {
private:
    std::shared_ptr<ControlFlowGraph> cfg;

public:
    explicit ConstantPropagation(const std::shared_ptr<ControlFlowGraph>& cfg);

    std::shared_ptr<ControlFlowGraph> transform_cfg() override;

    std::shared_ptr<InstructionSequence> transform_basic_block(const InstructionSequence *orig_bb) override;

    static std::shared_ptr<InstructionSequence> constant_propagation(InstructionSequence *block);

    static bool match_hl(int base, int hl_opcode);

};

class CopyPropagation : public ControlFlowGraphTransform {
private:
    std::shared_ptr<ControlFlowGraph> cfg;
    std::map<int, int> constants;

public:
    explicit CopyPropagation(const std::shared_ptr<ControlFlowGraph>& cfg);

    std::shared_ptr<InstructionSequence> transform_basic_block(const InstructionSequence *orig_bb) override;


    static bool match_hl(int base, int hl_opcode);

    std::shared_ptr<InstructionSequence> copy_propagation(InstructionSequence *block);

    static bool is_caller_saved(int vreg_num);
};


class LiveRegisters : public ControlFlowGraphTransform {
private:
    LiveVregs m_live_vregs;

public:
    explicit LiveRegisters(const std::shared_ptr<ControlFlowGraph> &cfg);
    ~LiveRegisters();

    std::shared_ptr<InstructionSequence>
    transform_basic_block(const InstructionSequence *orig_bb) override;

    bool is_caller_saved(int vreg_num);
};


#endif //COMPILERS_2_OPTIMIZATIONS_H
