#include <iostream>
#include "node.h"
#include "ast.h"
#include "local_storage_allocation.h"

LocalStorageAllocation::LocalStorageAllocation()
        : m_storage_calc(StorageCalculator::STRUCT, 0)
        , m_total_local_storage(0U), m_next_vreg(VREG_FIRST_LOCAL), vreg_boundary(0) {
}

LocalStorageAllocation::~LocalStorageAllocation() = default;

void LocalStorageAllocation::visit_declarator_list(Node *n) {
    Node *maybe_struct = n->get_kid(0);
    if (maybe_struct->has_symbol() && maybe_struct->get_symbol()->get_type()->is_struct()) {
        StorageCalculator struct_calc;

        for (unsigned i = 0; i < maybe_struct->get_type()->get_num_members(); i++) {
            Member member = maybe_struct->get_type()->get_member(i);
            unsigned new_mem = struct_calc.add_field(member.get_type());
            maybe_struct->get_type()->get_member(i).set_offset(new_mem);
        }
        struct_calc.finish();
        maybe_struct->get_symbol()->set_offset(m_total_local_storage);
        std::cout << "/* struct '" << maybe_struct->get_str() << "' allocated " << struct_calc.get_size() << " bytes at offset " << m_total_local_storage <<  " */" << std::endl;
        m_total_local_storage += struct_calc.get_size();
        m_storage_calc.add_field(maybe_struct->get_type());

    } else {
        for (unsigned i = 0; i < n->get_num_kids(); i++) {
            if (!n->get_kid(i)->get_symbol()->get_type()->is_struct()) {
                assign_variable_storage(n->get_kid(i) ,n->get_kid(i));
            }
        }
    }

}

void LocalStorageAllocation::visit_function_definition(Node *n) {

    // Visit parameter list
    visit_children(n->get_kid(2));
    // Visit statement list
    visit(n->get_kid(3));
    std::cout << "/* function '" <<n->get_symbol()->get_name() << "' uses "<< m_total_local_storage <<  " bytes of memory, allocated " << m_next_vreg << " vreg's */\n" << std::endl;
    n->get_symbol()->set_offset(m_total_local_storage);
    n->get_symbol()->set_vreg(m_next_vreg);
}

void LocalStorageAllocation::visit_function_parameter(Node *n) {
    assign_variable_storage(n, n->get_kid(1));
}

void LocalStorageAllocation::visit_statement_list(Node *n) {
    visit_children(n);
}

void LocalStorageAllocation::assign_variable_storage(Node *declarator, Node *base) {
    switch (base->get_tag()) {
        case AST_POINTER_DECLARATOR:
            assign_variable_storage(declarator, base->get_kid(0));
            break;
        case AST_ARRAY_DECLARATOR: {
            assign_variable_storage(declarator, base->get_kid(0));
            break;
        }
        case AST_NAMED_DECLARATOR: {
            if ((declarator->get_type()->is_integral() || declarator->get_type()->is_pointer()) && !declarator->get_symbol()->is_stack() && !declarator->get_symbol()->needs_address()) {
                int next_vreg = next();
                declarator->get_symbol()->set_vreg(next_vreg);
                std::cout << "/* variable '" << declarator->get_str() << "' allocated to vr" << next_vreg << " */" << std::endl;
                vreg_boundary+=declarator->get_symbol()->get_type()->get_storage_size();
            } else {
                StorageCalculator array_calc;
                array_calc.add_field(declarator->get_type());
                unsigned global_mem = m_storage_calc.add_field(declarator->get_type());
                declarator->get_symbol()->set_offset(global_mem);
                array_calc.finish();
                std::cout << "/* variable '" << declarator->get_str() << "' allocated " << array_calc.get_size() << " bytes of storage at offset " << global_mem << " */" << std::endl;
                m_total_local_storage+=array_calc.get_size();
            }

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
