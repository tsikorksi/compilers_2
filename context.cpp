// Copyright (c) 2021-2022, David H. Hovemeyer <david.hovemeyer@gmail.com>
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

#include <set>
#include <memory>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <sstream>
#include "exceptions.h"
#include "node.h"
#include "ast.h"
#include "parse.tab.h"
#include "lex.yy.h"
#include "parser_state.h"
#include "semantic_analysis.h"
#include "symtab.h"
#include "highlevel_codegen.h"
#include "local_storage_allocation.h"
#include "lowlevel_codegen.h"
#include "context.h"
#include "cfg.h"
#include "optimizations.h"

Context::Context()
        : m_ast(nullptr) {
}

Context::~Context() {
    delete m_ast;
}

struct CloseFile {
    void operator()(FILE *in) {
        if (in != nullptr) {
            fclose(in);
        }
    }
};

namespace {

    template<typename Fn>
    void process_source_file(const std::string &filename, Fn fn) {
        // open the input source file
        std::unique_ptr<FILE, CloseFile> in(fopen(filename.c_str(), "r"));
        if (!in) {
            RuntimeError::raise("Couldn't open '%s'", filename.c_str());
        }

        // create an initialize ParserState; note that its destructor
        // will take responsibility for cleaning up the lexer state
        std::unique_ptr<ParserState> pp(new ParserState);
        pp->cur_loc = Location(filename, 1, 1);

        // prepare the lexer
        yylex_init(&pp->scan_info);
        yyset_in(in.get(), pp->scan_info);

        // make the ParserState available from the lexer state
        yyset_extra(pp.get(), pp->scan_info);

        // use the ParserState to either scan tokens or parse the input
        // to build an AST
        fn(pp.get());
    }

}

void Context::scan_tokens(const std::string &filename, std::vector<Node *> &tokens) {
    auto callback = [&](ParserState *pp) {
        YYSTYPE yylval;

        // the lexer will store pointers to all of the allocated
        // token objects in the ParserState, so all we need to do
        // is call yylex() until we reach the end of the input
        while (yylex(&yylval, pp->scan_info) != 0)
            ;

        std::copy(pp->tokens.begin(), pp->tokens.end(), std::back_inserter(tokens));
    };

    process_source_file(filename, callback);
}

void Context::parse(const std::string &filename) {
    auto callback = [&](ParserState *pp) {
        // parse the input source code
        yyparse(pp);

        // free memory allocated by flex
        yylex_destroy(pp->scan_info);

        m_ast = pp->parse_tree;

        // delete any Nodes that were created by the lexer,
        // but weren't incorporated into the parse tree
        std::set<Node *> tree_nodes;
        m_ast->preorder([&tree_nodes](Node *n) { tree_nodes.insert(n); });
        for (auto i = pp->tokens.begin(); i != pp->tokens.end(); ++i) {
            if (tree_nodes.count(*i) == 0) {
                delete *i;
            }
        }
    };

    process_source_file(filename, callback);
}

void Context::analyze() {
    assert(m_ast != nullptr);
    m_sema.visit(m_ast);
}

void Context::highlevel_codegen(ModuleCollector *module_collector, bool m_optimize) {
    // Assign
    //   - vreg numbers to parameters
    //   - local storage offsets to local variables requiring storage in
    //     memory
    //
    // This will also determine the total local storage requirements
    // for each function.
    //
    // Any local variable not assigned storage in memory will be allocated
    // a vreg as its storage.

    LocalStorageAllocation local_storage_alloc;
    local_storage_alloc.visit(m_ast);

    // collect all of the global variables
    SymbolTable *globals = m_sema.get_global_symtab();
    for (auto i = globals->cbegin(); i != globals->cend(); ++i) {
        Symbol *sym = *i;
        if (sym->get_kind() == SymbolKind::VARIABLE)
            module_collector->collect_global_var(sym->get_name(), sym->get_type());
    }

    // generating high-level code for each function, and then send the
    // generated high-level InstructionSequence to the ModuleCollector
    int next_label_num = 0;
    for (auto i = m_ast->cbegin(); i != m_ast->cend(); ++i) {
        Node *child = *i;
        if (child->get_tag() == AST_FUNCTION_DEFINITION) {
            HighLevelCodegen hl_codegen(next_label_num, local_storage_alloc.next(), m_optimize);
            hl_codegen.visit(child);

            std::shared_ptr<InstructionSequence> hl_iseq;

            if (m_optimize) {
                std::shared_ptr<InstructionSequence> m_hl_iseq = hl_codegen.get_hl_iseq();

                Node *funcdef_ast = m_hl_iseq->get_funcdef_ast();

                // cur_hl_iseq is the "current" version of the high-level IR,
                // which could be a transformed version if we are doing optimizations
                std::shared_ptr<InstructionSequence> cur_hl_iseq(m_hl_iseq);
                // High-level optimizations

                // Create a control-flow graph representation of the high-level code
                HighLevelControlFlowGraphBuilder hl_cfg_builder(cur_hl_iseq);
                std::shared_ptr<ControlFlowGraph> cfg = hl_cfg_builder.build();

                // live instruction analysis
                LiveRegisters live_regs(cfg);
                cfg = live_regs.transform_cfg();


                // Do local optimizations
                ConstantPropagation hl_opts(cfg);
                cfg = hl_opts.transform_cfg();

                // Copy propagation works but does nothing
                CopyPropagation cp_opts(cfg);
                cfg = cp_opts.transform_cfg();



                // Convert the transformed high-level CFG back to an InstructionSequence
                cur_hl_iseq = cfg->create_instruction_sequence();

                // The function definition AST might have information needed for
                // low-level code generation
                cur_hl_iseq->set_funcdef_ast(funcdef_ast);
                hl_iseq = cur_hl_iseq;

            } else {
                hl_iseq = hl_codegen.get_hl_iseq();
            }


            std::vector<std::string> strings = hl_codegen.get_strings();
            for (int l = 0; l < (int) strings.size(); l++){
                std::ostringstream stream;
                stream << "str" << l;
                module_collector->collect_string_constant(stream.str(), hl_codegen.get_strings().at(l));
            }
            std::string fn_name = child->get_kid(1)->get_str();

            // store a pointer to the function definition AST in the
            // high-level InstructionSequence: this is useful in case information
            // about the function definition is needed by the low-level
            // code generator
            hl_iseq->set_funcdef_ast(child);

            module_collector->collect_function(fn_name, hl_iseq);

            // make sure local label numbers are not reused between functions
            next_label_num = hl_codegen.get_next_label_num();
        }
    }
}

namespace {

// ModuleCollector implementation which generates low-level code
// from generated high-level code, and then forwards the generated
// low-level code to a delegate.
    class LowLevelCodeGenModuleCollector : public ModuleCollector {
    private:
        ModuleCollector *m_delegate;
        bool m_optimize;

    public:
        LowLevelCodeGenModuleCollector(ModuleCollector *delegate, bool optimize);
        virtual ~LowLevelCodeGenModuleCollector();

        virtual void collect_string_constant(const std::string &name, const std::string &strval);
        virtual void collect_global_var(const std::string &name, const std::shared_ptr<Type> &type);
        virtual void collect_function(const std::string &name, const std::shared_ptr<InstructionSequence> &iseq);
    };

    LowLevelCodeGenModuleCollector::LowLevelCodeGenModuleCollector(ModuleCollector *delegate, bool optimize)
            : m_delegate(delegate)
            , m_optimize(optimize) {
    }

    LowLevelCodeGenModuleCollector::~LowLevelCodeGenModuleCollector() {
    }

    void LowLevelCodeGenModuleCollector::collect_string_constant(const std::string &name, const std::string &strval) {
        m_delegate->collect_string_constant(name, strval);
    }

    void LowLevelCodeGenModuleCollector::collect_global_var(const std::string &name, const std::shared_ptr<Type> &type) {
        m_delegate->collect_global_var(name, type);
    }

    void LowLevelCodeGenModuleCollector::collect_function(const std::string &name, const std::shared_ptr<InstructionSequence> &iseq) {
        LowLevelCodeGen ll_codegen(m_optimize);

        // translate high-level code to low-level code
        std::shared_ptr<InstructionSequence> ll_iseq = ll_codegen.generate(iseq);

        // send the low-level code on to the delegate (i.e., print the code)
        m_delegate->collect_function(name, ll_iseq);
    }

}

void Context::lowlevel_codegen(ModuleCollector *module_collector, bool optimize) {
    LowLevelCodeGenModuleCollector ll_codegen_module_collector(module_collector, optimize);
    highlevel_codegen(&ll_codegen_module_collector, optimize);
}
