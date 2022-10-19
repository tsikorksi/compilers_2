#include <cassert>
#include <algorithm>
#include <utility>
#include <map>
#include "grammar_symbols.h"
#include "parse.tab.h"
#include "node.h"
#include "ast.h"
#include "exceptions.h"
#include "semantic_analysis.h"

SemanticAnalysis::SemanticAnalysis()
        : m_global_symtab(new SymbolTable(nullptr)) {
    m_cur_symtab = m_global_symtab;
}

SemanticAnalysis::~SemanticAnalysis() {
}

void SemanticAnalysis::visit_struct_type(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_union_type(Node *n) {
    RuntimeError::raise("union types aren't supported");
}

void SemanticAnalysis::visit_variable_declaration(Node *n) {
    // visit basic type
    visit(n->get_kid(1));
    for (unsigned i = 0; i < n->get_kid(2)->get_num_kids(); i++) {
        Node * current = n->get_kid(2)->get_kid(i);
        switch (current->get_tag()) {
            case AST_NAMED_DECLARATOR:
                break;
            case AST_POINTER_DECLARATOR:
                n->get_kid(1)->make_pointer();
                break;
            case AST_ARRAY_DECLARATOR:
                n->get_kid(1)->make_array(stoi(current->get_kid(1)->get_str()));
                current->get_kid(0)->set_str(current->get_kid(0)->get_kid(0)->get_str());
        }
        std::shared_ptr<Type> p = n->get_kid(1)->get_type();
        m_cur_symtab->define(SymbolKind::VARIABLE,current->get_kid(0)->get_str(),p);
    }
}

void SemanticAnalysis::visit_basic_type(Node *n) {
    if (n->get_num_kids() == 0) {
        SemanticError::raise(n->get_loc(), "No Type specified");
    }
    if (n->get_num_kids() == 1 && n->get_kid(0)->get_tag() == TOK_VOID) {
        std::shared_ptr<Type> p(new BasicType(BasicTypeKind::VOID, false));
        n->set_type(p);
        return;
    }

    bool sign = true;
    bool a_char = false;
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (n->get_kid(i)->get_tag() == TOK_CHAR) {
            a_char = true;
        }
        if (n->get_kid(i)->get_tag() == TOK_UNSIGNED) {
            sign = false;
        }
    }

    if (a_char) {
        std::shared_ptr<Type> p(new BasicType(BasicTypeKind::CHAR, sign));
        n->set_type(p);
        return;
    }

    int type = -1;
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (n->get_kid(i)->get_tag() == TOK_INT) {
            type = 0;
        }
        if (n->get_kid(i)->get_tag() == TOK_SHORT) {
            type = 1;
        }
        if (n->get_kid(i)->get_tag() == TOK_LONG) {
            type = 2;
        }
    }
    if (type < 0) {
        SemanticError::raise(n->get_loc(), "Invalid Basic Type");
    }
    BasicTypeKind kind;
    switch (type) {
        case 0:
            kind = BasicTypeKind::INT;
            break;
        case 1:
            kind = BasicTypeKind::SHORT;
            break;
        case 2:
            kind = BasicTypeKind::LONG;
            break;
    }
    std::shared_ptr<Type> p(new BasicType(kind, sign));
    n->set_type(p);
}

void SemanticAnalysis::visit_function_definition(Node *n) {
    visit_function_declaration(n);
    enter_scope();
    visit(n->get_kid(3));
    leave_scope();
}

void SemanticAnalysis::visit_function_declaration(Node *n) {
    // visit  function type
    visit(n->get_kid(0));

    // visit parameters
    visit_children(n->get_kid(2));
    n->get_kid(0)->make_function();

    Node * params = n->get_kid(2);
    for (unsigned i = 0; i < params->get_num_kids(); i++) {
        auto *mem = new Member(params->get_kid(i)->get_str(), params->get_kid(i)->get_type());
        n->get_kid(0)->get_type()->add_member(*mem);
    }


    m_cur_symtab->declare(SymbolKind::FUNCTION, n->get_kid(1)->get_str(), n->get_kid(0)->get_type());
}

void SemanticAnalysis::visit_function_parameter(Node *n) {
    // Get type
    visit(n->get_kid(0));
    n->set_str(n->get_kid(1)->get_kid(0)->get_str());
    n->set_type(n->get_kid(0)->get_type());

}

void SemanticAnalysis::visit_statement_list(Node *n) {
    visit_children(n);
}

void SemanticAnalysis::visit_struct_type_definition(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_binary_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_unary_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_postfix_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_conditional_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_cast_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_function_call_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_field_ref_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_indirect_field_ref_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_array_element_ref_expression(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_variable_ref(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_literal_value(Node *n) {
    // TODO: implement
}

int SemanticAnalysis::search_for_tag(Node * n, BasicTypeKind type) {
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (static_cast<BasicTypeKind>(n->get_kid(i)->get_tag()) == type) {
            return i;
        }
    }
    return -1;
}

void SemanticAnalysis::enter_scope() {
    auto *scope = new SymbolTable(m_cur_symtab);
    m_cur_symtab = scope;
}

void SemanticAnalysis::leave_scope() {
    m_cur_symtab = m_cur_symtab->get_parent();
    assert(m_cur_symtab != nullptr);
}

