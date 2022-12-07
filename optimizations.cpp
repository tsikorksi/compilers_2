#include "optimizations.h"

#include "cfg.h"

// HIGH LEVEL

LocalOptimizationHighLevel::LocalOptimizationHighLevel(
        const std::shared_ptr<ControlFlowGraph>& cfg)
        : ControlFlowGraphTransform(cfg){

}

std::shared_ptr<ControlFlowGraph> LocalOptimizationHighLevel::transform_cfg() {
    return nullptr;
}

std::shared_ptr<InstructionSequence> LocalOptimizationHighLevel::transform_basic_block(const InstructionSequence *orig_bb) {

}


// LOW LEVEL


std::shared_ptr<ControlFlowGraph> LocalOptimizationLowLevel::transform_cfg() {
    return std::shared_ptr<ControlFlowGraph>();
}

LocalOptimizationLowLevel::LocalOptimizationLowLevel(ControlFlowGraph cfg) {

}
