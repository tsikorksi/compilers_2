#include <cassert>
#include "node.h"
#include "instruction.h"
#include "highlevel.h"
#include "ast.h"
#include "parse.tab.h"
#include "grammar_symbols.h"
#include "exceptions.h"
#include "highlevel_codegen.h"
#include "local_storage_allocation.h"

namespace {

// Adjust an opcode for a basic type
    HighLevelOpcode get_opcode(HighLevelOpcode base_opcode, const std::shared_ptr<Type> &type) {
        if (type->is_basic())
            return static_cast<HighLevelOpcode>(int(base_opcode) + int(type->get_basic_type_kind()));
        else if (type->is_pointer())
            return static_cast<HighLevelOpcode>(int(base_opcode) + int(BasicTypeKind::LONG));
        else
            RuntimeError::raise("attempt to use type '%s' as data in opcode selection", type->as_str().c_str());
    }

}

HighLevelCodegen::HighLevelCodegen(int next_label_num, int next_vreg)
        : m_next_vreg(next_vreg), m_next_label_num(next_label_num), m_hl_iseq(new InstructionSequence()) {
}

HighLevelCodegen::~HighLevelCodegen() {
}

void HighLevelCodegen::visit_function_definition(Node *n) {
    // generate the name of the label that return instructions should target
    std::string fn_name = n->get_kid(1)->get_str();
    m_return_label_name = ".L" + fn_name + "_return";

    unsigned total_local_storage = 0U;
/*
  total_local_storage = n->get_total_local_storage();
*/

    m_hl_iseq->append(new Instruction(HINS_enter, Operand(Operand::IMM_IVAL, total_local_storage)));

    // visit body
    visit(n->get_kid(3));

    m_hl_iseq->define_label(m_return_label_name);
    m_hl_iseq->append(new Instruction(HINS_leave, Operand(Operand::IMM_IVAL, total_local_storage)));
    m_hl_iseq->append(new Instruction(HINS_ret));
}

void HighLevelCodegen::visit_expression_statement(Node *n) {
    visit(n->get_kid(0));
}

void HighLevelCodegen::visit_unary_expression(Node *n) {
    // Move to mem if neccessary

}

void HighLevelCodegen::visit_return_statement(Node *n) {
    // jump to the return label
    m_hl_iseq->append(new Instruction(HINS_jmp, Operand(Operand::LABEL, m_return_label_name)));
}

void HighLevelCodegen::visit_return_expression_statement(Node *n) {
    // A possible implementation:

    Node *expr = n->get_kid(0);

    // generate code to evaluate the expression
    visit(expr);

    // move the computed value to the return value vreg
    HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, expr->get_type());
    m_hl_iseq->append(new Instruction(mov_opcode, Operand(Operand::VREG, LocalStorageAllocation::VREG_RETVAL), expr->get_operand()));

    // jump to the return label
    visit_return_statement(n);

}

void HighLevelCodegen::visit_while_statement(Node *n) {
    // TODO: implement
}

void HighLevelCodegen::visit_do_while_statement(Node *n) {
    // TODO: implement
}

void HighLevelCodegen::visit_for_statement(Node *n) {
    // TODO: implement
}

void HighLevelCodegen::visit_if_statement(Node *n) {
    // Visit comparison
    visit(n->get_kid(0));
    // Visit body
    visit(n->get_kid(1));
    m_hl_iseq->define_label(next_label());


}

void HighLevelCodegen::visit_if_else_statement(Node *n) {
    // TODO: implement
}

void HighLevelCodegen::visit_binary_expression(Node *n) {
    // Visit lhs
    visit(n->get_kid(1));
    Operand lhs = n->get_kid(1)->get_operand();
    // Visit rhs
    visit(n->get_kid(2));
    Operand rhs = n->get_kid(2)->get_operand();

    switch (n->get_kid(0)->get_tag()) {
        case TOK_ASSIGN: {
            HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, n->get_kid(1)->get_type());
            m_hl_iseq->append(new Instruction (mov_opcode, lhs, rhs));
            break;
        }
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_DIVIDE:
        case TOK_ASTERISK:
            break;
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE:
        case TOK_EQUALITY:
        case TOK_LOGICAL_AND:
        case TOK_LOGICAL_OR:
            break;
    }
}



void HighLevelCodegen::visit_function_call_expression(Node *n) {
    // TODO: implement
}

void HighLevelCodegen::visit_array_element_ref_expression(Node *n) {
    // TODO: implement
}

/// Set local operand to the stored vreg of the variable
/// \param n local node
void HighLevelCodegen::visit_variable_ref(Node *n) {
    Symbol * sym = n->get_symbol();
    Operand local(Operand::VREG, sym->get_vreg());
    n->set_operand(local);
}

void HighLevelCodegen::visit_literal_value(Node *n) {
    // A partial implementation (note that this won't work correctly
    // for string constants!):

    LiteralValue val = n->get_literal_value();
    int vreg = next_temp_vreg();
    Operand dest(Operand::VREG, vreg);
    HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, n->get_type());
    m_hl_iseq->append(new Instruction(mov_opcode, dest, Operand(Operand::IMM_IVAL, val.get_int_value())));
    n->set_operand(dest);

}

std::string HighLevelCodegen::next_label() {
    std::string label = ".L" + std::to_string(m_next_label_num++);
    return label;
}

void HighLevelCodegen::visit_field_ref_expression(Node *n) {
    ASTVisitor::visit_field_ref_expression(n);
}

void HighLevelCodegen::visit_indirect_field_ref_expression(Node *n) {
    ASTVisitor::visit_indirect_field_ref_expression(n);
}

int HighLevelCodegen::next_temp_vreg() {
    int temp = m_next_vreg;
    m_next_vreg++;
    return temp;
}

// TODO: additional private member functions
