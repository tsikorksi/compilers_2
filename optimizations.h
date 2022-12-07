//
// Created by tadzi on 12/7/2022.
//

#ifndef COMPILERS_2_OPTIMIZATIONS_H
#define COMPILERS_2_OPTIMIZATIONS_H
#include "cfg.h"

class LocalOptimizationHighLevel{
private:
    std::shared_ptr<ControlFlowGraph> cfg;

public:
    explicit LocalOptimizationHighLevel(std::shared_ptr<ControlFlowGraph> cfg);
    std::shared_ptr<ControlFlowGraph> transform_cfg();
};

class LocalOptimizationLowLevel{

private:
    std::shared_ptr<ControlFlowGraph> cfg;

public:
    explicit LocalOptimizationLowLevel(ControlFlowGraph cfg);
    std::shared_ptr<ControlFlowGraph> transform_cfg();
};

#endif //COMPILERS_2_OPTIMIZATIONS_H
