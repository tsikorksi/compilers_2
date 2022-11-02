#include "node.h"
#include "local_storage_allocation.h"

LocalStorageAllocation::LocalStorageAllocation()
        : m_total_local_storage(0U)
        , m_next_vreg(VREG_FIRST_LOCAL) {
}

LocalStorageAllocation::~LocalStorageAllocation() = default;

void LocalStorageAllocation::visit_declarator_list(Node *n) {
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        n->get_kid(i)->set_vreg(next());
    }
}

void LocalStorageAllocation::visit_function_definition(Node *n) {

    // Visit parameter list
    visit_children(n->get_kid(2));
    // Visit statement list
    visit(n->get_kid(3));
}

void LocalStorageAllocation::visit_function_parameter(Node *n) {
    n->set_vreg(next());
}

void LocalStorageAllocation::visit_statement_list(Node *n) {
    visit_children(n);
}

int LocalStorageAllocation::next(){
    int temp = m_next_vreg;
    m_next_vreg += 1;
    return temp;
};
