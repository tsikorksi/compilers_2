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

SemanticAnalysis::~SemanticAnalysis() = default;

void SemanticAnalysis::visit_struct_type(Node *n) {
    // TODO: implement
}

void SemanticAnalysis::visit_union_type(Node *n) {
    RuntimeError::raise("union types aren't supported");
}

void SemanticAnalysis::visit_variable_declaration(Node *n) {
    // visit basic type
    visit(n->get_kid(1));

    // iterate through declarators
    for (unsigned i = 0; i < n->get_kid(2)->get_num_kids(); i++) {

        // the current declarator
        Node * current = n->get_kid(2)->get_kid(i);

        // annotate it with its type
        type_switcher(current ,n->get_kid(1));
        std::shared_ptr<Type> p = current->get_type();

        // add it to the Symbol Table
        m_cur_symtab->define(SymbolKind::VARIABLE,current->get_kid(0)->get_str(),p);
    }
}

/// Annotate a complex type in the leaf node
/// \param declare pointer to the declarator
/// \param type pointer to the base type node
void SemanticAnalysis::type_switcher(Node *declare, Node *type) {
    declare->set_type(type->get_type());

    switch (declare->get_tag()) {
        case AST_NAMED_DECLARATOR:
            break;
        case AST_POINTER_DECLARATOR:
            declare->make_pointer();
            visit_pointer_declarator(declare);
            break;

        case AST_ARRAY_DECLARATOR:
            declare->make_array(stoi(declare->get_kid(1)->get_str()));
            declare->get_kid(0)->set_str(declare->get_kid(0)->get_kid(0)->get_str());
            break;
    }
}
/// Dive into nested pointers looking for the layers. Shift name up the chain, preserving base type
/// \param n the current pointer
void SemanticAnalysis::visit_pointer_declarator(Node *n) {
    if (n->get_kid(0)->get_tag() == ASTNodeTag::AST_NAMED_DECLARATOR) {
        n->set_str(n->get_kid(0)->get_kid(0)->get_str());
    } else {
        visit_pointer_declarator(n->get_kid(0));
        n->make_pointer();
        n->set_str(n->get_kid(0)->get_str());
    }
    n->get_kid(0)->set_str(n->get_kid(0)->get_kid(0)->get_str());
}

void SemanticAnalysis::visit_basic_type(Node *n) {

    if (n->get_num_kids() == 0) {
        SemanticError::raise(n->get_loc(), "No Type specified");
    }

    // Find a void, if so make sure it is the only keyword
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (n->get_kid(i)->get_tag() == TOK_VOID) {
            if (n->get_num_kids() == 1) {
                std::shared_ptr<Type> p(new BasicType(BasicTypeKind::VOID, true));
                n->set_type(p);
                return;
            }
            SemanticError::raise(n->get_loc(), "Cannot have qualifiers on void type");
        }
    }

    // Find any type specifiers
    int type = 0;
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (n->get_kid(i)->get_tag() == TOK_SHORT) {
            type = 1;
        }
        if (n->get_kid(i)->get_tag() == TOK_LONG) {
            type = 2;
        }
    }

    int sign = 0;
    bool a_char = false;
    // Find if char, also looking for signing if present
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (n->get_kid(i)->get_tag() == TOK_CHAR) {
            a_char = true;
        }
        if (n->get_kid(i)->get_tag() == TOK_UNSIGNED) {
            sign = 1;
        }
        if (n->get_kid(i)->get_tag() == TOK_SIGNED) {
            sign = 2;
        }
    }



    // if long or short specify type
    BasicTypeKind kind = BasicTypeKind::INT;
    switch (type) {
        case 0:
            break;
        case 1:
            kind = BasicTypeKind::SHORT;
            break;
        case 2:
            kind = BasicTypeKind::LONG;
            break;
        default:;
    }


    // if it's a char, make sure it's not long or short
    if (a_char) {
        if (type != 0) {
            SemanticError::raise(n->get_loc(), "Cannot specify long or short with char");
        }
        else {
            kind = BasicTypeKind::CHAR;
        }
    }

    std::shared_ptr<Type> p(new BasicType(kind, is_signed(sign)));

    if (n->get_kid(0)->get_tag() == TOK_VOLATILE) {
        std::shared_ptr<QualifiedType> sub(new QualifiedType(p, TypeQualifier::VOLATILE));
        n->set_type(sub);
    } else if (n->get_kid(0)->get_tag() == TOK_CONST) {
        std::shared_ptr<QualifiedType> sub(new QualifiedType(p, TypeQualifier::CONST));
        n->set_type(sub);
    } else {
        n->set_type(p);

    }
}

bool SemanticAnalysis::is_signed(int sign) {
    if (sign == 1) {
        return false;
    }
    return true;
}

void SemanticAnalysis::visit_function_definition(Node *n) {
    visit_function_declaration(n);
    enter_scope();
    define_parameters(n);
    enter_scope();
    visit(n->get_kid(3));
    leave_scope();
    leave_scope();
}

void SemanticAnalysis::define_parameters(Node *n) {
    std::shared_ptr<Type> params = n->get_kid(0)->get_type();
    for (unsigned i = 0; i < params->get_num_members(); i++) {
        Member param = params->get_member(i);
        if (m_cur_symtab->has_symbol_local(param.get_name())) {
            SemanticError::raise(n->get_loc(), "Cannot have 2 params of the same name");
        }
        m_cur_symtab->define(SymbolKind::VARIABLE, param.get_name(), param.get_type());
    }
}

void SemanticAnalysis::visit_function_declaration(Node *n) {
    // visit  function type
    visit(n->get_kid(0));
    Node * params = n->get_kid(2);

    // visit parameters
    visit_children(params);
    n->get_kid(0)->make_function();

    for (unsigned i = 0; i < params->get_num_kids(); i++) {
        auto *mem = new Member(params->get_kid(i)->get_str(), params->get_kid(i)->get_kid(1)->get_type());
        n->get_kid(0)->get_type()->add_member(*mem);
    }

    if (m_cur_symtab->has_symbol_local(n->get_kid(1)->get_str())) {
        SemanticError::raise(n->get_loc(), "Function with same name declared in same scope");
    }
    m_cur_symtab->declare(SymbolKind::FUNCTION, n->get_kid(1)->get_str(), n->get_kid(0)->get_type());
}

void SemanticAnalysis::visit_function_parameter(Node *n) {
    // Get type
    visit(n->get_kid(0));
    type_switcher(n->get_kid(1), n->get_kid(0));
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

void SemanticAnalysis::visit_return_expression_statement(Node *n) {
    //TODO: implement
}

void SemanticAnalysis::enter_scope() {
    auto *scope = new SymbolTable(m_cur_symtab);
    m_cur_symtab = scope;
}

void SemanticAnalysis::leave_scope() {
    m_cur_symtab = m_cur_symtab->get_parent();
    assert(m_cur_symtab != nullptr);
}

