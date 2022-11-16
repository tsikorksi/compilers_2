//
// Created by tadzi on 11/16/2022.
//
#include "node.h"
#include "strings.h"
#include "literal_value.h"

String::String(const PrintHighLevelCode& mMod): m_mod(mMod) {
}

void String::visit_literal_value(Node *n) {
    LiteralValue val = n->get_literal_value();
    if (val.get_kind() == LiteralValueKind::STRING) {
        std::string vreg = next();
        m_mod.collect_string_constant(vreg, val.get_str_value());
        val.set_string_vreg(vreg);
    }
}

std::string String::next() {
    int temp = m_str_count;
    m_str_count++;
    std::string base = "str";
    return base += std::to_string(temp);
}

