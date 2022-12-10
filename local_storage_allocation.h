#ifndef LOCAL_STORAGE_ALLOCATION_H
#define LOCAL_STORAGE_ALLOCATION_H

#include "storage.h"
#include "ast_visitor.h"

class LocalStorageAllocation : public ASTVisitor {
public:
    // vr0 is the return value vreg
    static const int VREG_RETVAL = 0;

    // vr1 is 1st argument vreg
    static const int VREG_FIRST_ARG = 1;

    // local variable allocation starts at vr16
    static const int VREG_FIRST_LOCAL = 16;

private:
    StorageCalculator m_storage_calc;
    unsigned m_total_local_storage;
    int m_next_vreg;
    unsigned vreg_boundary;

public:
    LocalStorageAllocation();
    virtual ~LocalStorageAllocation();

    virtual void visit_declarator_list(Node *n);
    virtual void visit_function_definition(Node *n);
    virtual void visit_function_parameter(Node *n);
    virtual void visit_statement_list(Node *n);
    int next();

private:
    void assign_variable_storage(Node *declarator, Node *base);

    unsigned int assign_array(const std::shared_ptr<Type> &type);

    void assign_struct_storage(Node *declarator, const StorageCalculator &calc);

    void assign_struct_storage(Node *base, Node *declarator, StorageCalculator calc);
};

#endif // LOCAL_STORAGE_ALLOCATION_H
