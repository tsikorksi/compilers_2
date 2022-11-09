#include <iostream>
#include "node.h"
#include "ast.h"
#include "local_storage_allocation.h"

LocalStorageAllocation::LocalStorageAllocation()
        : m_storage_calc(StorageCalculator::UNION, 0)
        , m_total_local_storage(0U), m_next_vreg(VREG_FIRST_LOCAL) {
}

LocalStorageAllocation::~LocalStorageAllocation() = default;

void LocalStorageAllocation::visit_declarator_list(Node *n) {
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        assign_variable_storage(n->get_kid(i));
    }
}

void LocalStorageAllocation::visit_function_definition(Node *n) {

    // Visit parameter list
    visit_children(n->get_kid(2));
    // Visit statement list
    visit(n->get_kid(3));
    std::cout << "/* function '" <<n->get_symbol()->get_name() << "' uses "<< m_total_local_storage <<  " bytes of memory, allocated " << m_next_vreg << " vreg's */\n" << std::endl;

}

void LocalStorageAllocation::visit_function_parameter(Node *n) {
    assign_variable_storage(n);
}

void LocalStorageAllocation::visit_statement_list(Node *n) {
    visit_children(n);
}

void LocalStorageAllocation::assign_variable_storage(Node *n) {
    switch (n->get_tag()) {
        case AST_ARRAY_DECLARATOR: {
            unsigned new_mem = assign_array(n->get_type());
            n->get_symbol()->set_offset(new_mem);
            std::cout << "/* variable '" << n->get_kid(0)->get_str() << "' allocated " << m_storage_calc.get_size() << " bytes of storage at offset " << new_mem << " */" << std::endl;

            m_total_local_storage+=m_storage_calc.get_size();
            break;
        }
        case AST_POINTER_DECLARATOR:
        case AST_NAMED_DECLARATOR: {
            int next_vreg = next();
            n->get_symbol()->set_vreg(next_vreg);
            std::cout << "/* variable '" << n->get_kid(0)->get_str() << "' allocated to vr" << next_vreg << " */" << std::endl;
        }
    }
}

int LocalStorageAllocation::next(){
    int temp = m_next_vreg;
    m_next_vreg++;
    return temp;
}

unsigned LocalStorageAllocation::assign_array(const std::shared_ptr<Type>& type) {
    unsigned temp = m_storage_calc.add_field(type);
    m_storage_calc.finish();
    return temp;
}
