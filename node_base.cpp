// Copyright (c) 2021, David H. Hovemeyer <david.hovemeyer@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <cassert>
#include <memory>
#include "node_base.h"

NodeBase::NodeBase() = default;

NodeBase::~NodeBase() = default;

void NodeBase::set_symbol(Symbol *symbol) {
    assert(!has_symbol());
    assert(m_type == nullptr);
    m_symbol = symbol;
}

void NodeBase::set_type(const std::shared_ptr<Type> &type) {
    assert(!has_symbol());
    assert(!m_type);
    m_type = type;
}

bool NodeBase::has_symbol() const {
    return m_symbol != nullptr;
}

bool NodeBase::has_type() const {
    return m_type != nullptr;
}

Symbol *NodeBase::get_symbol() const {
    return m_symbol;
}

std::shared_ptr<Type> NodeBase::get_type() const {
    // this shouldn't be called unless there is actually a type
    // associated with this node

    if (has_symbol())
        return m_symbol->get_type(); // Symbol will definitely have a valid Type
    else {
        assert(m_type); // make sure a Type object actually exists
        return m_type;
    }
}

void NodeBase::make_function() {
    m_type = std::make_shared<FunctionType>(m_type);
    assert(m_type->is_function());
}

void NodeBase::make_pointer() {
    m_type = std::make_shared<PointerType>(m_type);
    assert(m_type->is_pointer());
}

void NodeBase::un_pointer() {
    assert(m_type->is_pointer());
    m_type = m_type->get_base_type();
}

void NodeBase::make_array(unsigned size) {
    m_type = std::make_shared<ArrayType>(m_type, size);
    assert(m_type->is_array());
}

void NodeBase::un_array() {
        assert(m_type->is_array());
        m_type = m_type->get_base_type();
}
