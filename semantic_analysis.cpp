#include <cassert>
#include <algorithm>
#include <memory>
#include <utility>
#include "grammar_symbols.h"
#include "parse.tab.h"
#include "node.h"
#include "ast.h"
#include "exceptions.h"
#include "semantic_analysis.h"

SemanticAnalysis::SemanticAnalysis()
        : m_global_symtab(new SymbolTable(nullptr, "root")) {
    m_cur_symtab = m_global_symtab;
}

SemanticAnalysis::~SemanticAnalysis()  {

}

void SemanticAnalysis::visit_struct_type(Node *n) {
    // If struct doesn't exist, declare it
    if (m_cur_symtab->has_symbol_recursive("struct " + n->get_kid(0)->get_str())) {
        std::shared_ptr<Type> p = m_cur_symtab->lookup_recursive("struct " + n->get_kid(0)->get_str())->get_type();
        n->set_type(p);
    } else {
        SemanticError::raise(n->get_loc(), "Unknown Struct");
    }
}

void SemanticAnalysis::visit_union_type(Node *n) {
    RuntimeError::raise("union types aren't supported");
}

void SemanticAnalysis::visit_variable_declaration(Node *n) {
    // visit type
    visit(n->get_kid(1));

    // iterate through declarators
    for (unsigned i = 0; i < n->get_kid(2)->get_num_kids(); i++) {

        // the current declarator
        std::shared_ptr<Type> p;
        Node * current = n->get_kid(2)->get_kid(i);
        // annotate it with its type. recursively
        type_switcher(current ,n->get_kid(1)->get_type());
        p = current->get_type();

        // add it to the Symbol Table
        if (m_cur_symtab->has_symbol_local(current->get_kid(0)->get_str())) {
            SemanticError::raise(n->get_loc(), "Variable %s already exists", current->get_kid(0)->get_str().c_str());
        }
        current->clear_type_for_symbol(m_cur_symtab->define(SymbolKind::VARIABLE,current->get_kid(0)->get_str(),p));
    }
}

/// Annotate a layered type in the leaf node
/// \param declare pointer to the declarator
/// \param type pointer to the base type node
void SemanticAnalysis::type_switcher(Node *declare, const std::shared_ptr<Type>& type) {

    switch (declare->get_tag()) {
        case AST_NAMED_DECLARATOR:
            declare->set_type(type);
            break;
        case AST_POINTER_DECLARATOR:
            type_switcher(declare->get_kid(0), type);
            declare->set_type(declare->get_kid(0)->get_type());
            // Possibly remove, dependant on https://courselore.org/courses/5505975532/conversations/100
            // To me this is the result of a parser logical error, which means that the Pointer AST node
            // is above the array AST node, but fixing that is beyond my scope.
            if (declare->get_type()->is_array()) {
                unsigned size = declare->get_type()->get_array_size();
                declare->un_array();
                declare->make_pointer();
                declare->make_array(size);
            } else {
                declare->make_pointer();
            }
            break;
        case AST_ARRAY_DECLARATOR:
            type_switcher(declare->get_kid(0), type);
            declare->set_type(declare->get_kid(0)->get_type());
            declare->make_array(stoi(declare->get_kid(1)->get_str()));
            break;
    }
    // Move variable name up the chain
    declare->set_str(declare->get_kid(0)->get_str());
}

void SemanticAnalysis::visit_basic_type(Node *n) {

    if (n->get_num_kids() == 0) {
        SemanticError::raise(n->get_loc(), "No Type specified");
    }

    // Find a void, if so make sure it is the only keyword
    for (unsigned i = 0; i < n->get_num_kids(); i++) {
        if (n->get_kid(i)->get_tag() == TOK_VOID) {
            if (n->get_num_kids() == 1) {
                // void always signed
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

    // BaseType and Sign already determined
    std::shared_ptr<Type> p(new BasicType(kind, is_signed(sign)));

    // Check for Qualified types which are always first
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
    // This is necessary because sign has 3 possible positions, so can't use bool
    if (sign == 1) {
        return false;
    }
    return true;
}

void SemanticAnalysis::visit_function_definition(Node *n) {
    // redeclare function
    visit_function_declaration(n);
    // Define params in new scope, making the function scope be named after the function, allowing return type checking
    enter_scope(n->get_kid(1)->get_str());
    define_parameters(n);
    // Statement list
    visit(n->get_kid(3));
    leave_scope();
}

void SemanticAnalysis::define_parameters(Node *n) {
    std::shared_ptr<Type> params = n->get_kid(0)->get_type();
    for (unsigned i = 0; i < params->get_num_members(); i++) {
        Member param = params->get_member(i);

        if (m_cur_symtab->has_symbol_local(param.get_name())) {
            SemanticError::raise(n->get_loc(), "Cannot have 2 params of the same name");
        }
        // add to local scope
        m_cur_symtab->define(n->get_kid(2)->get_kid(i)->get_symbol());
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
    Symbol *sym = m_cur_symtab->declare(SymbolKind::FUNCTION, n->get_kid(1)->get_str(), n->get_kid(0)->get_type());
    n->set_symbol(sym);
}

void SemanticAnalysis::visit_function_parameter(Node *n) {
    // Get type
    visit(n->get_kid(0));
    type_switcher(n->get_kid(1), n->get_kid(0)->get_type());
    n->set_str(n->get_kid(1)->get_kid(0)->get_str());
    auto *sym  = new Symbol (SymbolKind::VARIABLE, n->get_str(), n->get_kid(1)->get_type(), m_cur_symtab, false);
    n->set_symbol(sym);
}

void SemanticAnalysis::visit_statement_list(Node *n) {
    enter_scope(m_cur_symtab->get_name());
    visit_children(n);
    leave_scope();
}

void SemanticAnalysis::visit_struct_type_definition(Node *n) {
    std::string name = n->get_kid(0)->get_str();
    if(m_cur_symtab->has_symbol_recursive("struct " + name)) {
        SemanticError::raise(n->get_loc(), "Struct already defined");
    }
    std::shared_ptr<Type> struct_type(new StructType(name));

    m_cur_symtab->define(SymbolKind::TYPE, "struct " + name, struct_type);
    enter_scope("struct");
    Node * fields = n->get_kid(1);
    for (unsigned i = 0; i < fields->get_num_kids(); i++) {
        visit(fields->get_kid(i));

    }
    for (unsigned i = 0; i < m_cur_symtab->get_num_symbols(); i++) {
        Symbol * sym = m_cur_symtab->get_symbol(i);
        Member mem(sym->get_name(), sym->get_type());
        struct_type->add_member(mem);
    }

    leave_scope();
}

void SemanticAnalysis::visit_binary_expression(Node *n) {
    // visit left
    visit(n->get_kid(1));
    // visit right
    visit(n->get_kid(2));

    switch (n->get_kid(0)->get_tag()) {
        case TOK_ASSIGN:
            if (n->get_kid(1)->get_tag() == AST_BINARY_EXPRESSION || n->get_kid(1)->get_tag()  == AST_LITERAL_VALUE) {
                SemanticError::raise(n->get_loc(), "Tried to assign to non-lvalue");
            }
            visit_assign(n);
            break;
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_DIVIDE:
        case TOK_ASTERISK:
            visit_math(n);
            break;
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE:
        case TOK_EQUALITY:
        case TOK_LOGICAL_AND:
        case TOK_LOGICAL_OR:
            visit_comparison(n);
            break;
    }
    // annotate with type of result
    n->set_type( n->get_kid(1)->get_type());
}

void SemanticAnalysis::visit_assign(Node *n) {
    std::shared_ptr<Type> lhs = n->get_kid(1)->get_type();
    std::shared_ptr<Type> rhs = n->get_kid(2)->get_type();

    // Not base types
    if (!lhs->is_basic() && !rhs->is_basic()) {

        if (!lhs->get_base_type()->is_volatile() && rhs->get_base_type()->is_volatile()) {
            SemanticError::raise(n->get_loc(), "Tried to assign volatile variable to non-volatile variable");
        }
        if (rhs->get_base_type()->is_const()) {
            SemanticError::raise(n->get_loc(), "Tried to assign const to non-const variable");
        }
        if (lhs->is_pointer() && rhs->is_integral()) {
            SemanticError::raise(n->get_loc(), "Cannot assign integral to pointer");
        }
    }
    if (!lhs->is_basic() && rhs->is_basic()) {
        if (lhs->is_pointer()) {
            if (!lhs->get_base_type()->is_struct() && !lhs->get_base_type()->is_array() && !lhs->get_base_type()->is_pointer()) {
                SemanticError::raise(n->get_loc(), "Cannot assign integral to pointer");
            }
        }
    }


    // Not lvalue
    if (!lhs->is_integral()) {
        if (!(n->get_kid(1)->has_symbol() || lhs->is_pointer() || lhs->is_array() || lhs->is_struct() || lhs->is_same(rhs.get()) )) {
            SemanticError::raise(n->get_loc(), "Left hand side is not an L-Value");
        }
    }

    // Const error
    if (lhs->is_const()) {
        SemanticError::raise(n->get_loc(), "Tried to assign value to const variable");
    }

    // Cannot do arr = arr
    if (lhs->is_array() && rhs->is_array()) {
        SemanticError::raise(n->get_loc(), "Tried to assign array to array");
    }

    // bad pointer assignments
    if (lhs->is_pointer()) {
        if (!rhs->is_pointer() && !rhs->is_array() && !rhs->is_integral()) {
            SemanticError::raise(n->get_loc(), "Tried to assign non pointer to pointer");
        }
    }

    if (lhs->is_struct() || rhs->is_struct()) {
        if ((lhs->is_struct() != rhs->is_struct() ) && !lhs->is_pointer()) {
            SemanticError::raise(n->get_loc(), "Tried to assign struct to non struct");
        }

    } else {
        if (lhs->is_integral() && !rhs->is_integral()) {
            SemanticError::raise(n->get_loc(), "Tried to assign non integer to integer");
        }
    }
}

void SemanticAnalysis::visit_math(Node *n) {
    std::shared_ptr<Type> lhs = n->get_kid(1)->get_type();

    if (lhs->is_integral() && lhs->get_basic_type_kind() < BasicTypeKind::INT) {
        // Promote operand if necessary
        n->set_kid(1, promote_to_int(n->get_kid(1)));
        lhs = n->get_kid(1)->get_type();
    }
    std::shared_ptr<Type> rhs = n->get_kid(2)->get_type();


    if (lhs->is_void() || rhs->is_void()) {
        SemanticError::raise(n->get_loc(), "Cannot do math on Void type");
    }
    if (rhs->is_pointer() && !lhs->is_pointer()) {
        SemanticError::raise(n->get_loc(), "Cannot have pointer on right hand side of equation");
    }

}

void SemanticAnalysis::visit_comparison(Node *n) {
    std::shared_ptr<Type> lhs = n->get_kid(1)->get_type();
    std::shared_ptr<Type> rhs = n->get_kid(2)->get_type();

    if (lhs->is_pointer() != rhs->is_pointer()) {
        SemanticError::raise(n->get_loc(), "Tried to compare pointer and non pointer");
    }
    if (lhs->is_function() != rhs->is_function()) {
        SemanticError::raise(n->get_loc(), "Tried to compare function and non function");
    }
    if (lhs->is_struct() != rhs->is_struct()) {
        SemanticError::raise(n->get_loc(), "Tried to compare struct and non struct");
    }
}

void SemanticAnalysis::visit_unary_expression(Node *n) {
    visit(n->get_kid(1));
    n->set_type(n->get_kid(1)->get_type());
    if (n->get_kid(0)->get_tag() == TOK_AMPERSAND) {
        if (n->get_kid(1)->get_tag() == AST_LITERAL_VALUE) {
            SemanticError::raise(n->get_loc(), "Tried to reference a literal");
        }
        n->make_pointer();
    } else if (n->get_kid(0)->get_tag() == TOK_ASTERISK) {
        n->make_pointer();
    }
}

void SemanticAnalysis::visit_postfix_expression(Node *n) {
    // None of these three appear in any example, and frankly I don't even know what they look like
}

void SemanticAnalysis::visit_conditional_expression(Node *n) {
    // None of these three appear in any example, and frankly I don't even know what they look like
}

void SemanticAnalysis::visit_cast_expression(Node *n) {
    // None of these three appear in any example, and frankly I don't even know what they look like
}

void SemanticAnalysis::visit_function_call_expression(Node *n) {
    // visit name
    visit(n->get_kid(0));
    Symbol *func = m_cur_symtab->lookup_recursive(n->get_kid(0)->get_symbol()->get_name());
    if (func == nullptr) {
        SemanticError::raise(n->get_loc(), "Function %s does not exist", n->get_kid(0)->get_symbol()->get_name().c_str());
    }

    // visit args
    if (func->get_type()->get_num_members() != n->get_kid(1)->get_num_kids()) {
        SemanticError::raise(n->get_loc(), "Number of arguments does not match number of parameters");
    }

    for (unsigned i = 0; i < func->get_type()->get_num_members(); i++) {
        visit(n->get_kid(1)->get_kid(i));
        std::shared_ptr<Type> arg = n->get_kid(1)->get_kid(i)->get_type();
        std::shared_ptr<Type> param = func->get_type()->get_member(i).get_type();
        // Comparing symbol member type to regular type doesn't work, even when the BaseTypeKind is the same
        if (!(check_different(arg, param))) {
            SemanticError::raise(n->get_loc(), "Argument type does not match parameter type");
        }
    }
    n->set_type(func->get_type()->get_base_type());
}

bool SemanticAnalysis::check_different(const std::shared_ptr<Type>& a, const std::shared_ptr<Type>& b) {
    // Clean Param vs Arg dif checker
    if (a->is_pointer() != b->is_pointer() || a->is_array() != b->is_array()) {
        if ((a->is_pointer() && b->is_array()) || (a->is_array() && b->is_pointer())){
            return true;
        }
        return false;
    } else if (a->is_struct() != b->is_struct()) {
        return false;
    }
    if (a->is_basic() != b->is_basic()) {
        if (a->get_basic_type_kind() != b->get_basic_type_kind()) {
            return false;
        }
        return false;
    }
    return true;
}

void SemanticAnalysis::visit_field_ref_expression(Node *n) {
    visit(n->get_kid(0));

    std::shared_ptr<Type> var = n->get_kid(0)->get_type();

    if (var->is_pointer()) {
        SemanticError::raise(n->get_loc(), "Direct reference to pointer");
    }

    std::shared_ptr<Type> field_type = var->find_member(n->get_kid(1)->get_str())->get_type();
    n->set_type(field_type);
    // if it's an array of char's it's actually a pointer to char's
    if (field_type->is_array() && field_type->get_base_type()->get_basic_type_kind() == BasicTypeKind::CHAR) {

        n->make_pointer();
    }
}

void SemanticAnalysis::visit_indirect_field_ref_expression(Node *n) {

    // This and the above function are almost identical since they are virtual and thus cannot have default values
    visit(n->get_kid(0));


    std::shared_ptr<Type> var = n->get_kid(0)->get_type();

    // If it's a pointer, which it should be
    if (var->is_pointer()) {
        // Dereference it, so that the correct base type is passed up the chain
        var = var->get_base_type();
    } else {
        SemanticError::raise(n->get_loc(), "Indirect reference to non-pointer");
    }
    std::shared_ptr<Type> field_type = var->find_member(n->get_kid(1)->get_str())->get_type();
    n->set_type(field_type);
}

void SemanticAnalysis::visit_array_element_ref_expression(Node *n) {
    visit(n->get_kid(0));
    visit(n->get_kid(1));
    n->set_type(n->get_kid(0)->get_type());
    if (n->get_type()->is_pointer()) {
        n->un_pointer();
    }
    if (n->get_type()->is_array()) {
        n->un_array();
    }
}

void SemanticAnalysis::visit_variable_ref(Node *n) {
    //  annotate with symbol
    if (m_cur_symtab->has_symbol_recursive(n->get_kid(0)->get_str())) {
        n->set_symbol(m_cur_symtab->lookup_recursive(n->get_kid(0)->get_str()));
    } else if (m_cur_symtab->has_symbol_recursive("struct " + n->get_kid(0)->get_str())) {
        // Must also search in structs
        n->set_symbol(m_cur_symtab->lookup_recursive("struct " + n->get_kid(0)->get_str()));

    } else {
        SemanticError::raise(n->get_loc(), "Variable %s does not exist in Symbol Table", n->get_kid(0)->get_str().c_str());
    }
}

void SemanticAnalysis::visit_literal_value(Node *n) {
    std::shared_ptr<Type> p;
    switch (n->get_kid(0)->get_tag()) {
        case TOK_INT_LIT: {
            LiteralValue lit = LiteralValue::from_int_literal(n->get_kid(0)->get_str(), n->get_loc());
            p = static_cast<const std::shared_ptr<Type>>(new BasicType((lit.is_long()) ? BasicTypeKind::LONG : BasicTypeKind::INT, !lit.is_unsigned()));
            n->set_type(p);
            n->set_literal_value(lit);
            break;
        }
        case TOK_CHAR_LIT: {
            LiteralValue lit = LiteralValue::from_char_literal(n->get_kid(0)->get_str(), n->get_loc());
            p = static_cast<const std::shared_ptr<Type>>(new BasicType(BasicTypeKind::CHAR, true));
            n->set_type(p);
            n->set_literal_value(lit);
            break;
        }
        case TOK_STR_LIT:{
            LiteralValue lit = LiteralValue::from_str_literal(n->get_kid(0)->get_str(), n->get_loc());
            p = static_cast<const std::shared_ptr<Type>>(new BasicType(BasicTypeKind::CHAR, true));
            n->set_type(p);
            // this is just a char pointer so
            n->make_pointer();
            n->set_literal_value(lit);
        }

    }
}

void SemanticAnalysis::visit_return_expression_statement(Node *n) {
    visit(n->get_kid(0));
    std::shared_ptr<Type> return_type = m_cur_symtab->lookup_recursive(m_cur_symtab->get_name(), SymbolKind::FUNCTION)->get_type()->get_base_type();
    // check name of symbol table
    if (!return_type->is_same(n->get_kid(0)->get_type().get())) {
        SemanticError::raise(n->get_loc(), "Return type does not match function declaration");
    }
}

Node *SemanticAnalysis::promote_to_int(Node *n) {
    assert(n->get_type()->is_integral());
    assert(n->get_type()->get_basic_type_kind() < BasicTypeKind::INT);
    std::shared_ptr<Type> type(new BasicType(BasicTypeKind::INT, n->get_type()->is_signed()));
    return implicit_conversion(n, type);
}

Node *SemanticAnalysis::implicit_conversion(Node *n, const std::shared_ptr<Type> &type) {
    std::unique_ptr<Node> conversion(new Node(AST_IMPLICIT_CONVERSION, {n}));
    conversion->set_type(type);
    return conversion.release();
}


void SemanticAnalysis::enter_scope(std::string name) {
    auto *scope = new SymbolTable(m_cur_symtab, std::move(name));
    m_cur_symtab = scope;
}

void SemanticAnalysis::leave_scope() {
    SymbolTable * temp = m_cur_symtab;
    m_cur_symtab = m_cur_symtab->get_parent();
    delete temp;
    assert(m_cur_symtab != nullptr);
}

