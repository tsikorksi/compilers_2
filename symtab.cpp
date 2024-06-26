#include <cassert>
#include <cstdio>
#include <utility>
#include "symtab.h"



////////////////////////////////////////////////////////////////////////
// Symbol implementation
////////////////////////////////////////////////////////////////////////

Symbol::Symbol(SymbolKind kind, const std::string &name, const std::shared_ptr<Type> &type, SymbolTable *symtab, bool is_defined)
        : m_kind(kind)
        , m_name(name)
        , m_type(type)
        , m_symtab(symtab)
        , m_is_defined(is_defined),m_vreg(-1), m_offset(-1), m_on_stack(false) {
}

Symbol::~Symbol() {
}

void Symbol::set_is_defined(bool is_defined) {
    m_is_defined = is_defined;
}

SymbolKind Symbol::get_kind() const {
    return m_kind;
}

const std::string &Symbol::get_name() const {
    return m_name;
}

std::shared_ptr<Type> Symbol::get_type() const {
    return m_type;
}

SymbolTable *Symbol::get_symtab() const {
    return m_symtab;
}

bool Symbol::is_defined() const {
    return m_is_defined;
}

void Symbol::set_offset(unsigned int m_next_offset) {
    m_on_stack = true;
    m_offset = m_next_offset;
}

////////////////////////////////////////////////////////////////////////
// SymbolTable implementation
////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolTable(SymbolTable *parent, std::string name)
        : m_parent(parent)
        , m_has_params(false) {
    m_scope_name = std::move(name);

}


SymbolTable::~SymbolTable() {
}

SymbolTable *SymbolTable::get_parent() const {
    return m_parent;
}

std::string SymbolTable::get_name() {
    return m_scope_name;
}

bool SymbolTable::has_params() const {
    return m_has_params;
}

void SymbolTable::set_has_params(bool has_params) {
    m_has_params = has_params;
}

bool SymbolTable::has_symbol_local(const std::string &name) const {
    return lookup_local(name) != nullptr;
}

bool SymbolTable::has_symbol_recursive(const std::string &name) const {
    return lookup_recursive(name) != nullptr;
}

Symbol *SymbolTable::lookup_local(const std::string &name) const {
    auto i = m_lookup.find(name);
    return (i != m_lookup.end()) ? m_symbols[i->second] : nullptr;
}

Symbol *SymbolTable::get_symbol(unsigned i) const {
    return m_symbols.at(i);
}

Symbol *SymbolTable::declare(SymbolKind sym_kind, const std::string &name, const std::shared_ptr<Type> &type) {
    Symbol *sym = new Symbol(sym_kind, name, type, this, true);
    add_symbol(sym);
    return sym;
}

Symbol *SymbolTable::define(SymbolKind sym_kind, const std::string &name, const std::shared_ptr<Type> &type) {
    Symbol *sym = new Symbol(sym_kind, name, type, this, false);
    add_symbol(sym);
    return sym;
}

Symbol *SymbolTable::define(Symbol * sym) {
    add_symbol(sym);
    return sym;
}

Symbol *SymbolTable::lookup_recursive(const std::string &name) const {
    const SymbolTable *scope = this;

    while (scope != nullptr) {
        Symbol *sym = scope->lookup_local(name);
        if (sym != nullptr)
            return sym;
        scope = scope->get_parent();
    }

    return nullptr;
}

Symbol *SymbolTable::lookup_recursive(const std::string &name,  SymbolKind type) const {
    const SymbolTable *scope = this;

    while (scope != nullptr) {
        Symbol *sym = scope->lookup_local(name);
        if (sym != nullptr && sym->get_kind() == type)
            return sym;
        scope = scope->get_parent();
    }

    return nullptr;
}

void SymbolTable::set_fn_type(const std::shared_ptr<Type> &fn_type) {
    assert(!m_fn_type);
    m_fn_type = fn_type;
}

const Type *SymbolTable::get_fn_type() const {
    const SymbolTable *symtab = this;
    while (symtab != nullptr) {
        if (m_fn_type)
            return m_fn_type.get();
        symtab = symtab->m_parent;
    }

    return nullptr;
}

void SymbolTable::add_symbol(Symbol *sym) {
    assert(!has_symbol_local(sym->get_name()));

    auto pos = unsigned(m_symbols.size());
    m_symbols.push_back(sym);
    m_lookup[sym->get_name()] = pos;

    // Assignment 3 only: print out symbol table entries as they are added
//    printf("%d|", get_depth());
//    printf("%s|", sym->get_name().c_str());
//    switch (sym->get_kind()) {
//        case SymbolKind::FUNCTION:
//            printf("function|"); break;
//        case SymbolKind::VARIABLE:
//            printf("variable|"); break;
//        case SymbolKind::TYPE:
//            printf("type|"); break;
//        default:
//            assert(false);
//    }
//
//    printf("%s\n", sym->get_type()->as_str().c_str());
}

int SymbolTable::get_depth() const {
    int depth = 0;

    const SymbolTable *symtab = m_parent;
    while (symtab != nullptr) {
        ++depth;
        symtab = symtab->m_parent;
    }

    return depth;
}

unsigned SymbolTable::get_num_symbols() const {
    return m_symbols.size();
}
