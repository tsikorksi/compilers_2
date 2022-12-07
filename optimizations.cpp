#include "optimizations.h"

#include <utility>
#include "cfg.h"

// HIGH LEVEL

LocalOptimizationHighLevel::LocalOptimizationHighLevel(std::shared_ptr<ControlFlowGraph> cfg) : cfg(std::move(cfg)) {

}

std::shared_ptr <ControlFlowGraph> LocalOptimizationHighLevel::transform_cfg() {
    return nullptr;
}


// LOW LEVEL


std::shared_ptr<ControlFlowGraph> LocalOptimizationLowLevel::transform_cfg() {
    return std::shared_ptr<ControlFlowGraph>();
}

LocalOptimizationLowLevel::LocalOptimizationLowLevel(ControlFlowGraph cfg) {

}
