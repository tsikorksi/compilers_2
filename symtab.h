#ifndef SYMTAB_H
#define SYMTAB_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include "type.h"

class SymbolTable;

enum class SymbolKind {
    FUNCTION,
    VARIABLE,
    TYPE,
};

class Symbol {
private:
    SymbolKind m_kind;
    std::string m_name;
    std::shared_ptr<Type> m_type;
    SymbolTable *m_symtab;
    bool m_is_defined;
    int m_vreg = -1;
    unsigned m_offset = -1;
    bool m_on_stack = false;
    bool m_address_taken = false;

    // value semantics prohibited
    Symbol(const Symbol &);
    Symbol &operator=(const Symbol &);

public:
    Symbol(SymbolKind kind, const std::string &name, const std::shared_ptr<Type> &type, SymbolTable *symtab, bool is_defined);
    ~Symbol();

    // a function, variable, or type can be declared
    // and then later defined, so allow m_is_defined to
    // be updated
    void set_is_defined(bool is_defined);

    SymbolKind get_kind() const;
    const std::string &get_name() const;
    std::shared_ptr<Type> get_type() const;
    SymbolTable *get_symtab() const;
    bool is_defined() const;

    bool is_stack() const {if (m_on_stack) {return true;}return false;}

    void take_address() {m_address_taken = true;}
    bool needs_address() {return m_address_taken;}

    int get_vreg() {return m_vreg;};
    void set_vreg(int m_next_vreg) {m_vreg = m_next_vreg;};

    unsigned get_offset() {return m_offset;};
    void set_offset(unsigned m_next_offset);;

    bool in_register() { return m_vreg; }
};

class SymbolTable {
private:
    SymbolTable *m_parent;
    std::string m_scope_name;
    std::vector<Symbol *> m_symbols;
    std::map<std::string, unsigned> m_lookup;
    bool m_has_params; // true if this symbol table contains function parameters
    std::shared_ptr<Type> m_fn_type; // this is set to the type of the enclosing function (if any)

    // value semantics prohibited
    SymbolTable(const SymbolTable &);
    SymbolTable &operator=(const SymbolTable &);

public:
    SymbolTable(SymbolTable *parent, std::string name);
    ~SymbolTable();

    SymbolTable *get_parent() const;
    std::string get_name();


    bool has_params() const;
    void set_has_params(bool has_params);

    // Operations limited to the current (local) scope.
    // Note that the caller should verify that a name is not defined
    // in the current scope before calling declare or define.
    bool has_symbol_local(const std::string &name) const;
    bool has_symbol_recursive(const std::string &name) const;
    unsigned get_num_symbols() const;

    Symbol *lookup_local(const std::string &name) const;
    Symbol *declare(SymbolKind sym_kind, const std::string &name, const std::shared_ptr<Type> &type);
    Symbol *define(SymbolKind sym_kind, const std::string &name, const std::shared_ptr<Type> &type);
    Symbol *define(Symbol *sym);


    // Iterate through the symbol table entries in the order in which they were added
    // to the symbol table. This is important for struct types, where the representation
    // of the type needs to record the fields (and their types) in the exact order
    // in which they appeared in the source code.
    typedef std::vector<Symbol *>::const_iterator const_iterator;
    const_iterator cbegin() const { return m_symbols.cbegin(); }
    const_iterator cend() const { return m_symbols.cend(); }

    // Operations that search recursively starting from the current (local)
    // scope and expanding to outer scopes as necessary
    Symbol *lookup_recursive(const std::string &name) const;
    Symbol *lookup_recursive(const std::string &name, SymbolKind type) const;

    // This can be called on the symbol table representing the function parameter
    // scope of a function to record the exact type of the function
    void set_fn_type(const std::shared_ptr<Type> &fn_type);

    // This returns the function type of the enclosing function, or nullptr
    // if there is no enclosing function. This is helpful for type checking
    // a return statement, to make sure that the value returned is
    // assignment-compatible with the function's return type.
    const Type *get_fn_type() const;
    Symbol *get_symbol(unsigned i) const;


private:
    void add_symbol(Symbol *sym);
    int get_depth() const;

};

#endif // SYMTAB_H
