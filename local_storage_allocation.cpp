#include <iostream>
#include "node.h"
#include "local_storage_allocation.h"

LocalStorageAllocation::LocalStorageAllocation()
        : m_total_local_storage(0U)
        , m_next_vreg(VREG_FIRST_LOCAL) {
}

LocalStorageAllocation::~LocalStorageAllocation() = default;

void LocalStorageAllocation::visit_declarator_list(Node *n) {
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        int next_vreg = next();
        n->get_kid(i)->get_symbol()->set_vreg(next_vreg);

        std::cout << "/* variable '" << n->get_kid(i)->get_kid(0)->get_str() << "' allocated to vr" << next_vreg << " */" << std::endl;
    }
}

void LocalStorageAllocation::visit_function_definition(Node *n) {

    // Visit parameter list
    visit_children(n->get_kid(2));
    // Visit statement list
    visit(n->get_kid(3));
    std::cout << "/* function '" <<n->get_symbol()->get_name() << "' allocated " << m_next_vreg - 1 << " vreg's */\n" << std::endl;

}

void LocalStorageAllocation::visit_function_parameter(Node *n) {
    // TODO: assign to memory if array or struct
    int next_vreg = next();
    n->get_symbol()->set_vreg(next_vreg);
    std::cout << "/* variable '" <<n->get_symbol()->get_name() << "' allocated to vr" << next_vreg << " */" << std::endl;
}

void LocalStorageAllocation::visit_statement_list(Node *n) {
    visit_children(n);
}

int LocalStorageAllocation::next(){
    return m_next_vreg++;
};
