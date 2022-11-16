//
// Created by tadzi on 11/16/2022.
//

#ifndef COMPILERS_2_STRINGS_H
#define COMPILERS_2_STRINGS_H


#include "ast_visitor.h"
#include "print_highlevel_code.h"


class String : public ASTVisitor{

private:
    int m_str_count = 0;
    PrintHighLevelCode m_mod;
public:

    explicit String(const PrintHighLevelCode& mMod);

    virtual void visit_literal_value(Node *n);

    std::string next();
};


#endif //COMPILERS_2_STRINGS_H
