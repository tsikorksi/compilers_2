#include "node.h"
#include "instruction.h"
#include "highlevel.h"
#include "parse.tab.h"
#include "exceptions.h"
#include "highlevel_codegen.h"
#include "local_storage_allocation.h"

namespace {

// Adjust an opcode for a basic type
    HighLevelOpcode get_opcode(HighLevelOpcode base_opcode, const std::shared_ptr<Type> &type) {
        if (type->is_basic())
            return static_cast<HighLevelOpcode>(int(base_opcode) + int(type->get_basic_type_kind()));
        else if (type->is_pointer() || type->is_array())
            return static_cast<HighLevelOpcode>(int(base_opcode) + int(BasicTypeKind::LONG));
        else
            RuntimeError::raise("attempt to use type '%s' as data in opcode selection", type->as_str().c_str());
    }

}

HighLevelCodegen::HighLevelCodegen(int next_label_num, int next_vreg)
        : m_next_vreg(next_vreg), m_next_label_num(next_label_num), m_hl_iseq(new InstructionSequence()) {
}

HighLevelCodegen::~HighLevelCodegen() = default;

void HighLevelCodegen::visit_function_definition(Node *n) {
    // generate the name of the label that return instructions should target
    std::string fn_name = n->get_kid(1)->get_str();
    m_return_label_name = ".L" + fn_name + "_return";

    unsigned total_local_storage;

    total_local_storage = n->get_symbol()->get_offset();


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
    visit(n->get_kid(1));
    switch(n->get_kid(0)->get_tag()) {
        case TOK_AMPERSAND:
            n->set_operand(n->get_kid(1)->get_operand().to_memref());
            break;
        case TOK_ASTERISK:
            n->set_operand(n->get_kid(1)->get_operand());
            break;
    }
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
    std::string jump_back = next_label();
    std::string jump_end = next_label();

    // Set point to return to after each loop
    m_hl_iseq->define_label(jump_back);
    // visit comparison
    visit(n->get_kid(0));
    m_hl_iseq->append(new Instruction(HINS_cjmp_t, n->get_kid(0)->get_operand() , Operand(Operand::LABEL, jump_end)));
    // visit body
    visit(n->get_kid(1));
    m_hl_iseq->append(new Instruction(HINS_jmp, Operand(Operand::LABEL, jump_back)));
    // Set point to jump to when no longer true
    m_hl_iseq->define_label(jump_end);

}

void HighLevelCodegen::visit_do_while_statement(Node *n) {
    std::string jump_back = next_label();

    // Set point to return to after each loop
    m_hl_iseq->define_label(jump_back);
    // visit body
    visit(n->get_kid(0));
    // visit comparison
    visit(n->get_kid(1));
    // Set point to jump to while true
    m_hl_iseq->append(new Instruction(HINS_cjmp_t, n->get_kid(1)->get_operand() , Operand(Operand::LABEL, jump_back)));
}

void HighLevelCodegen::visit_for_statement(Node *n) {
    // evaluate assignment
    visit(n->get_kid(0));
    std::string jump_back = next_label();
    std::string jump_out = next_label();
    // set return point for loop
    m_hl_iseq->define_label(jump_back);
    // evaluate comparison, leave if false
    visit(n->get_kid(1));
    m_hl_iseq->append(new Instruction(HINS_cjmp_f, n->get_kid(1)->get_operand() , Operand(Operand::LABEL, jump_out)));
    // execute body
    visit(n->get_kid(3));
    // execute change to loop counter
    visit(n->get_kid(2));

    // jump back to start
    m_hl_iseq->append(new Instruction(HINS_jmp, Operand(Operand::LABEL, jump_back)));
    // set label for exiting loop
    m_hl_iseq->define_label(jump_out);

}

void HighLevelCodegen::visit_if_statement(Node *n) {
    // Visit comparison
    visit(n->get_kid(0));
    std::string label = next_label();
    m_hl_iseq->append(new Instruction(HINS_cjmp_f, n->get_kid(0)->get_operand() , Operand(Operand::LABEL, label)));
    // Visit body
    visit(n->get_kid(1));
    m_hl_iseq->define_label(label);
}

void HighLevelCodegen::visit_if_else_statement(Node *n) {
    // Visit comparison
    visit(n->get_kid(0));
    std::string label = next_label();
    std::string skip_false = next_label();
    m_hl_iseq->append(new Instruction(HINS_cjmp_f, n->get_kid(0)->get_operand() , Operand(Operand::LABEL, label)));
    // Visit body
    visit(n->get_kid(1));
    m_hl_iseq->append(new Instruction(HINS_jmp, Operand(Operand::LABEL, skip_false)));
    m_hl_iseq->define_label(label);
    visit(n->get_kid(2));
    m_hl_iseq->define_label(skip_false);
}

void HighLevelCodegen::visit_binary_expression(Node *n) {
    // Visit lhs
    visit(n->get_kid(1));
    Operand lhs = n->get_kid(1)->get_operand();
    // Visit rhs
    visit(n->get_kid(2));
    Operand rhs = n->get_kid(2)->get_operand();

    if (n->get_kid(0)->get_tag() == TOK_ASSIGN) {
        // Assuming we are in assign, we don't need to know anything else
        HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, n->get_kid(1)->get_type());
        // move one into the other
        m_hl_iseq->append(new Instruction (mov_opcode, lhs, rhs));
        n->set_operand(lhs);
        return;
    }

    Operand dest = Operand(Operand::VREG, next_temp_vreg());
    HighLevelOpcode op;

    // Figure out which operator we are using
    switch (n->get_kid(0)->get_tag()) {
        case TOK_PLUS:
            op = HINS_add_b;
            break;
        case TOK_MINUS:
            op = HINS_sub_b;
            break;
        case TOK_DIVIDE:
            op = HINS_div_b;
            break;
        case TOK_ASTERISK:
            op = HINS_mul_b;
            break;
        case TOK_LT:
            op = HighLevelOpcode::HINS_cmplt_b;
            break;
        case TOK_LTE:
            op = HighLevelOpcode::HINS_cmplte_b;
            break;
        case TOK_GT:
            op = HighLevelOpcode::HINS_cmpgt_b;
            break;
        case TOK_GTE:
            op = HighLevelOpcode::HINS_cmpgte_b;
            break;
        case TOK_EQUALITY:
            op = HighLevelOpcode::HINS_cmpeq_b;
            break;
        case TOK_NOT:
            op = HighLevelOpcode::HINS_cmpneq_b;
            break;
        case TOK_LOGICAL_AND:
            op = HINS_and_b;
            break;
        case TOK_LOGICAL_OR:
            op = HINS_or_b;
            break;
    }
    // Make the change
    m_hl_iseq->append(new Instruction(get_opcode(op, n->get_kid(1)->get_type()), dest, lhs, rhs));
    n->set_operand(dest);
}

void HighLevelCodegen::visit_function_call_expression(Node *n) {
    std::string func = n->get_kid(0)->get_symbol()->get_name();
    visit_children(n->get_kid(1));
    if (n->get_kid(1)->get_num_kids() > 9) {
        // NEED TO ALLOCATE TO MEM
    }
    else {
        for (unsigned i = 0; i < n->get_kid(1)->get_num_kids(); i++) {
            Operand param = n->get_kid(1)->get_kid(i)->get_operand();
            HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, n->get_kid(1)->get_kid(i)->get_type());
            m_hl_iseq->append(new Instruction(mov_opcode, Operand(Operand::VREG, i+1), param));
        }
    }
    m_hl_iseq->append(new Instruction(HINS_call, Operand(Operand::LABEL, func)));
    n->set_operand(Operand(Operand::VREG, 0));
}

void HighLevelCodegen::visit_array_element_ref_expression(Node *n) {
   std::shared_ptr<Type> element_type = n->get_type();

    // add offset to local variable
    Operand address_register(Operand::VREG, next_temp_vreg());
    m_hl_iseq->append(new Instruction(HINS_localaddr, address_register, Operand(Operand::IMM_IVAL, n->get_kid(0)->get_symbol()->get_offset())));

    // Move the value of the element location to
    visit(n->get_kid(1));
    Operand elem = n->get_kid(1)->get_operand();
    HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, element_type);
    Operand dest (Operand::VREG, next_temp_vreg());
    m_hl_iseq->append(new Instruction(mov_opcode, dest , elem));

    // upgrade it to make space for multiplication
    Operand dest_up (Operand::VREG, next_temp_vreg());
    HighLevelOpcode upgrade = get_opcode(HINS_sconv_bl, element_type);
    m_hl_iseq->append(new Instruction(upgrade, dest_up, dest));

    // multiply to get to actual address of local variable
    Operand mult_dest(Operand::VREG, next_temp_vreg());
    Operand size_element(Operand::IMM_IVAL, n->get_type()->get_storage_size());
    HighLevelOpcode mul_opcode = get_opcode(HINS_mul_b, element_type);
    m_hl_iseq->append(new Instruction(mul_opcode, mult_dest, dest_up, size_element));

    // add value of offset
    Operand final_dest(Operand::VREG, next_temp_vreg());
    HighLevelOpcode add_opcode = get_opcode(HINS_add_b, element_type);
    m_hl_iseq->append(new Instruction(add_opcode, final_dest, mult_dest, address_register));

    // set Operand to the location of the destination
    n->set_operand(final_dest.to_memref());

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

    Operand rhs;

    switch (val.get_kind()) {
        case LiteralValueKind::INTEGER:
            rhs = Operand(Operand::IMM_IVAL, val.get_int_value());
            break;
        case LiteralValueKind::CHARACTER:
            Operand(Operand::IMM_LABEL, val.get_char_value());
            break;
        case LiteralValueKind::STRING:
            m_rodata.push_back(val.get_str_value());
            rhs = Operand(Operand::IMM_LABEL, &"str" [ (m_rodata.size() - 1)]);
            break;
        case LiteralValueKind::NONE:
            break;
    }
    int vreg = next_temp_vreg();
    Operand dest(Operand::VREG, vreg);
    HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, n->get_type());
    m_hl_iseq->append(new Instruction(mov_opcode, dest, rhs));
    n->set_operand(dest);

}

std::string HighLevelCodegen::next_label() {
    std::string label = ".L" + std::to_string(m_next_label_num++);
    return label;
}

void HighLevelCodegen::visit_field_ref_expression(Node *n) {
    // 	localaddr vr10, $0
    //	mov_q    vr11, $4
    //	add_q    vr12, vr10, vr11
    visit(n->get_kid(0));
    visit(n->get_kid(1));

    std::shared_ptr<Type> struct_type = n->get_kid(0)->get_type();

    // add offset of struct to local variable
    Operand address_register(Operand::VREG, next_temp_vreg());
    m_hl_iseq->append(new Instruction(HINS_localaddr, address_register, Operand(Operand::IMM_IVAL, n->get_kid(0)->get_symbol()->get_offset())));

    // Move the value of the offset of the field
    Operand elem (Operand::IMM_IVAL, struct_type->find_member(n->get_kid(1)->get_str())->get_offset());
    HighLevelOpcode mov_opcode = get_opcode(HINS_mov_b, struct_type->find_member(n->get_kid(1)->get_str())->get_type());
    Operand dest (Operand::VREG, next_temp_vreg());
    m_hl_iseq->append(new Instruction(mov_opcode, dest , elem));

    // add values of offsets
    Operand final_dest(Operand::VREG, next_temp_vreg());
    HighLevelOpcode add_opcode = get_opcode(HINS_add_b, struct_type);
    m_hl_iseq->append(new Instruction(add_opcode, final_dest, dest, address_register));

    n->set_operand(final_dest.to_memref());
}

void HighLevelCodegen::visit_indirect_field_ref_expression(Node *n) {
    ASTVisitor::visit_indirect_field_ref_expression(n);
}

int HighLevelCodegen::next_temp_vreg() {
    int temp = m_next_vreg;
    m_next_vreg++;
    return temp;
}
