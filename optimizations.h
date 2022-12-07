//
// Created by tadzi on 12/7/2022.
//

#ifndef COMPILERS_2_OPTIMIZATIONS_H
#define COMPILERS_2_OPTIMIZATIONS_H

#include "cfg.h"
#include "cfg_transform.h"

class LocalOptimizationHighLevel : public ControlFlowGraphTransform {
private:
    std::shared_ptr<ControlFlowGraph> cfg;

public:
    explicit LocalOptimizationHighLevel(const std::shared_ptr<ControlFlowGraph>& cfg);

    std::shared_ptr<ControlFlowGraph> transform_cfg() override;

    std::shared_ptr<InstructionSequence> transform_basic_block(const InstructionSequence *orig_bb);
};

class LocalOptimizationLowLevel {

private:
    std::shared_ptr<ControlFlowGraph> cfg;

public:
    explicit LocalOptimizationLowLevel(ControlFlowGraph cfg);

    std::shared_ptr<ControlFlowGraph> transform_cfg();
};

#endif //COMPILERS_2_OPTIMIZATIONS_H
