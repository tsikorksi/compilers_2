#include <cassert>
#include <map>
#include <iostream>
#include "node.h"
#include "instruction.h"
#include "operand.h"
#include "local_storage_allocation.h"
#include "highlevel.h"
#include "lowlevel.h"
#include "exceptions.h"
#include "lowlevel_codegen.h"
#include "cfg.h"
#include "optimizations.h"

namespace {

// This map has some "obvious" translations of high-level opcodes to
// low-level opcodes.
    const std::map<HighLevelOpcode, LowLevelOpcode> HL_TO_LL = {
            { HINS_nop, MINS_NOP},
            { HINS_add_b, MINS_ADDB },
            { HINS_add_w, MINS_ADDW },
            { HINS_add_l, MINS_ADDL },
            { HINS_add_q, MINS_ADDQ },
            { HINS_sub_b, MINS_SUBB },
            { HINS_sub_w, MINS_SUBW },
            { HINS_sub_l, MINS_SUBL },
            { HINS_sub_q, MINS_SUBQ },
            { HINS_mul_l, MINS_IMULL },
            { HINS_mul_q, MINS_IMULQ },
            { HINS_mov_b, MINS_MOVB },
            { HINS_mov_w, MINS_MOVW },
            { HINS_mov_l, MINS_MOVL },
            { HINS_mov_q, MINS_MOVQ },
            { HINS_sconv_bw, MINS_MOVSBW },
            { HINS_sconv_bl, MINS_MOVSBL },
            { HINS_sconv_bq, MINS_MOVSBQ },
            { HINS_sconv_wl, MINS_MOVSWL },
            { HINS_sconv_wq, MINS_MOVSWQ },
            { HINS_sconv_lq, MINS_MOVSLQ },
            { HINS_uconv_bw, MINS_MOVZBW },
            { HINS_uconv_bl, MINS_MOVZBL },
            { HINS_uconv_bq, MINS_MOVZBQ },
            { HINS_uconv_wl, MINS_MOVZWL },
            { HINS_uconv_wq, MINS_MOVZWQ },
            { HINS_uconv_lq, MINS_MOVZLQ },
            { HINS_ret, MINS_RET },
            { HINS_jmp, MINS_JMP },
            { HINS_call, MINS_CALL },

            // For comparisons, it is expected that the code generator will first
            // generate a cmpb/cmpw/cmpl/cmpq instruction to compare the operands,
            // and then generate a setXX instruction to put the result of the
            // comparison into the destination operand. These entries indicate
            // the apprpropriate setXX instruction to use.
            { HINS_cmplt_b, MINS_SETL },
            { HINS_cmplt_w, MINS_SETL },
            { HINS_cmplt_l, MINS_SETL },
            { HINS_cmplt_q, MINS_SETL },
            { HINS_cmplte_b, MINS_SETLE },
            { HINS_cmplte_w, MINS_SETLE },
            { HINS_cmplte_l, MINS_SETLE },
            { HINS_cmplte_q, MINS_SETLE },
            { HINS_cmpgt_b, MINS_SETG },
            { HINS_cmpgt_w, MINS_SETG },
            { HINS_cmpgt_l, MINS_SETG },
            { HINS_cmpgt_q, MINS_SETG },
            { HINS_cmpgte_b, MINS_SETGE },
            { HINS_cmpgte_w, MINS_SETGE },
            { HINS_cmpgte_l, MINS_SETGE },
            { HINS_cmpgte_q, MINS_SETGE },
            { HINS_cmpeq_b, MINS_SETE },
            { HINS_cmpeq_w, MINS_SETE },
            { HINS_cmpeq_l, MINS_SETE },
            { HINS_cmpeq_q, MINS_SETE },
            { HINS_cmpneq_b, MINS_SETNE },
            { HINS_cmpneq_w, MINS_SETNE },
            { HINS_cmpneq_l, MINS_SETNE },
            { HINS_cmpneq_q, MINS_SETNE },
    };

}

LowLevelCodeGen::LowLevelCodeGen(bool optimize)
        : m_total_memory_storage(0)
        , m_optimize(optimize) {
}

LowLevelCodeGen::~LowLevelCodeGen() {
}

std::shared_ptr<InstructionSequence> LowLevelCodeGen::generate(const std::shared_ptr<InstructionSequence> &hl_iseq) {

    std::shared_ptr<InstructionSequence> ll_iseq = translate_hl_to_ll(hl_iseq);

    // TODO: if optimizations are enabled, could do analysis/transformation of low-level code
    if (m_optimize) {

    }
    return ll_iseq;
}

std::shared_ptr<InstructionSequence> LowLevelCodeGen::translate_hl_to_ll(const std::shared_ptr<InstructionSequence> &hl_iseq) {
    std::shared_ptr<InstructionSequence> ll_iseq(new InstructionSequence());

    // The high-level InstructionSequence will have a pointer to the Node
    // representing the function definition. Useful information could be stored
    // there (for example, about the amount of memory needed for local storage,
    // maximum number of virtual registers used, etc.)
    Node *funcdef_ast = hl_iseq->get_funcdef_ast();
    assert(funcdef_ast != nullptr);

    // It's not a bad idea to store the pointer to the function definition AST
    // in the low-level InstructionSequence as well, in case it's needed by
    // optimization passes.
    ll_iseq->set_funcdef_ast(funcdef_ast);

    // Determine the total number of bytes of memory storage
    // that the function needs. This should include both variables that
    // *must* have storage allocated in memory (e.g., arrays), and also
    // any additional memory that is needed for virtual registers,
    // spilled machine registers, etc.
    vreg_boundary = ll_iseq->get_funcdef_ast()->get_symbol()->get_vreg();
    vreg_boundary = (vreg_boundary - 8) * 8;
    std::cout << "/* Function '"<<ll_iseq->get_funcdef_ast()->get_symbol()->get_name().c_str() << "': uses "<< vreg_boundary <<" total bytes of memory storage for vregs */" << std::endl;
    std::cout << "/* Function '"<<ll_iseq->get_funcdef_ast()->get_symbol()->get_name().c_str() << "': placing vreg storage at offset -" << vreg_boundary << " from %rbp */" << std::endl;

    m_total_memory_storage = ll_iseq->get_funcdef_ast()->get_symbol()->get_offset();
    m_total_memory_storage += vreg_boundary;
    // The function prologue will push %rbp, which should guarantee that the
    // stack pointer (%rsp) will contain an address that is a multiple of 16.
    // If the total memory storage required is not a multiple of 16, add to
    // it so that it is.
    if ((m_total_memory_storage) % 16 != 0)
        m_total_memory_storage += (16 - (m_total_memory_storage % 16));

    std::cout << "/* Function '"<<ll_iseq->get_funcdef_ast()->get_symbol()->get_name().c_str() << "': " << m_total_memory_storage << " bytes of local storage allocated in stack frame  */" << std::endl;



    // Iterate through high level instructions
    for (auto i = hl_iseq->cbegin(); i != hl_iseq->cend(); ++i) {
        Instruction *hl_ins = *i;

        // If the high-level instruction has a label, define an equivalent
        // label in the low-level instruction sequence
        if (i.has_label())
            ll_iseq->define_label(i.get_label());
        // Translate the high-level instruction into one or more low-level instructions
        translate_instruction(hl_ins, ll_iseq);
    }

    return ll_iseq;
}

namespace {

// These helper functions are provided to make it easier to handle
// the way that instructions and operands vary based on operand size
// ('b'=1 byte, 'w'=2 bytes, 'l'=4 bytes, 'q'=8 bytes.)

// Check whether hl_opcode matches a range of opcodes, where base
// is a _b variant opcode. Return true if the hl opcode is any variant
// of that base.
    bool match_hl(int base, int hl_opcode) {
        return hl_opcode >= base && hl_opcode < (base + 4);
    }

// For a low-level instruction with 4 size variants, return the correct
// variant. base_opcode should be the "b" variant, and operand_size
// should be the operand size in bytes (1, 2, 4, or 8.)
    LowLevelOpcode select_ll_opcode(LowLevelOpcode base_opcode, int operand_size) {
        int off;

        switch (operand_size) {
            case 1: // 'b' variant
                off = 0; break;
            case 2: // 'w' variant
                off = 1; break;
            case 4: // 'l' variant
                off = 2; break;
            case 8: // 'q' variant
                off = 3; break;
            default:
                assert(false);
                off = 3;
        }

        return LowLevelOpcode(int(base_opcode) + off);
    }

// Get the correct Operand::Kind value for a machine register
// of the specified size (1, 2, 4, or 8 bytes.)
    Operand::Kind select_mreg_kind(int operand_size) {
        switch (operand_size) {
            case 1:
                return Operand::MREG8;
            case 2:
                return Operand::MREG16;
            case 4:
                return Operand::MREG32;
            case 8:
                return Operand::MREG64;
            default:
                assert(false);
                return Operand::MREG64;
        }
    }

}

void LowLevelCodeGen::translate_instruction(Instruction *hl_ins, const std::shared_ptr<InstructionSequence> &ll_iseq) {
    auto hl_opcode = HighLevelOpcode(hl_ins->get_opcode());


    if (hl_opcode == HINS_enter) {
        // Function prologue: this will create an ABI-compliant stack frame.
        // The local variable area is *below* the address in %rbp, and local storage
        // can be accessed at negative offsets from %rbp. For example, the topmost
        // 4 bytes in the local storage area are at -4(%rbp).
        ll_iseq->append(new Instruction(MINS_PUSHQ, Operand(Operand::MREG64, MREG_RBP)));
        ll_iseq->append(new Instruction(MINS_MOVQ, Operand(Operand::MREG64, MREG_RSP), Operand(Operand::MREG64, MREG_RBP)));
        ll_iseq->append(new Instruction(MINS_SUBQ, Operand(Operand::IMM_IVAL, m_total_memory_storage), Operand(Operand::MREG64, MREG_RSP)));

        return;
    }

    if (hl_opcode == HINS_leave) {
        // Function epilogue: deallocate local storage area and restore original value
        // of %rbp
        ll_iseq->append(new Instruction(MINS_ADDQ, Operand(Operand::IMM_IVAL, m_total_memory_storage), Operand(Operand::MREG64, MREG_RSP)));
        ll_iseq->append(new Instruction(MINS_POPQ, Operand(Operand::MREG64, MREG_RBP)));

        return;
    }

    if (hl_opcode == HINS_ret) {
        ll_iseq->append(new Instruction(MINS_RET));
        return;
    }

    // Note that you can use the highlevel_opcode_get_source_operand_size() and
    // highlevel_opcode_get_dest_operand_size() functions to determine the
    // size (in bytes, 1, 2, 4, or 8) of either the source operands or
    // destination operand of a high-level instruction. This should be useful
    // for choosing the appropriate low-level instructions and
    // machine register operands.



    // 1 OPERAND

    Operand dest_operand = hl_ins->get_operand(0);

    if (hl_opcode == HINS_jmp) {
        ll_iseq->append(new Instruction(MINS_JMP, dest_operand));
        return;
    }

    if (hl_opcode == HINS_nop) {
        ll_iseq->append(new Instruction(MINS_NOP));
        return;
    }

    if (hl_opcode == HINS_call) {
        ll_iseq->append(new Instruction(MINS_CALL, dest_operand));
        return;
    }


    // 2 OPERAND
    int src_size = highlevel_opcode_get_source_operand_size(hl_opcode);
    Operand src_operand = get_ll_operand(hl_ins->get_operand(1), src_size, ll_iseq);
    int dest_size = highlevel_opcode_get_dest_operand_size(hl_opcode);
    dest_operand = get_ll_operand(hl_ins->get_operand(0), dest_size, ll_iseq);

    //conv
    if (match_hl(HINS_uconv_bw, hl_opcode) || match_hl(HINS_uconv_bq, hl_opcode)
        || match_hl(HINS_sconv_bw, hl_opcode)  || match_hl(HINS_sconv_bq, hl_opcode)) {
        int before = 1;
        int after;
        switch (hl_opcode) {
            case HINS_sconv_bw:
            case HINS_uconv_bw:
                after = 2;
                break;
            case HINS_sconv_bl:
            case HINS_uconv_bl:
                after = 4;
                break;
            case HINS_sconv_bq:
            case HINS_uconv_bq:
                after = 8;
                break;
            case HINS_sconv_wl:
            case HINS_uconv_wl:
                before = 2;
                after = 4;
                break;
            case HINS_sconv_wq:
            case HINS_uconv_wq:
                before = 2;
                after = 8;
                break;
            case HINS_sconv_lq:
            case HINS_uconv_lq:
                before = 4;
                after = 8;
                break;
            default:
                RuntimeError::raise("Non-conversion passed into conversion check");
        }
        // Move value into small temp
        LowLevelOpcode old_move = select_ll_opcode(MINS_MOVB, before);
        Operand r10_small(select_mreg_kind(before), MREG_R10);
        ll_iseq->append(new Instruction(old_move, src_operand, r10_small));

        // Convert it
        Operand r10_big(select_mreg_kind(after), MREG_R10);
        ll_iseq->append(new Instruction(HL_TO_LL.at(hl_opcode), r10_small, r10_big));
        // Move it from big temp to destination
        LowLevelOpcode new_move = select_ll_opcode(MINS_MOVB, after);
        ll_iseq->append(new Instruction(new_move, r10_big, dest_operand));
        return;
    }

    // mov
    if (match_hl(HINS_mov_b, hl_opcode)) {

        LowLevelOpcode mov_opcode = select_ll_opcode(MINS_MOVB, src_size);


        if (src_operand.is_memref() && dest_operand.is_memref()) {
            // move source operand into a temporary register
            Operand::Kind mreg_kind = select_mreg_kind(src_size);
            Operand r10(mreg_kind, MREG_R10);
            ll_iseq->append(new Instruction(mov_opcode, src_operand, r10));
            src_operand = r10;
        }

        ll_iseq->append(new Instruction(mov_opcode, src_operand, dest_operand));
        return;
    }

    // cjmp
    if (hl_opcode == HINS_cjmp_t || hl_opcode == HINS_cjmp_f) {
        // The source of a HINS_cjmp does not have a size so we make it L
        ll_iseq->append(new Instruction(MINS_CMPL, Operand(Operand::IMM_IVAL, 0), dest_operand));

        if (hl_opcode == HINS_cjmp_t) {
            ll_iseq->append(new Instruction(MINS_JNE, src_operand));
            return;
        }
        ll_iseq->append(new Instruction(MINS_JE, src_operand));
        return;
    }

    // localaddr
    if (hl_opcode == HINS_localaddr) {
        // Always take 64 bit size as per spec
        src_operand = get_ll_operand(hl_ins->get_operand(1), 8, ll_iseq);
        dest_operand = get_ll_operand(hl_ins->get_operand(0), 8, ll_iseq);
        Operand temp(select_mreg_kind(8), MREG_R10);

        // Get the actual offset, not the offset that hl gen passes in
        Operand memory_ref(Operand::MREG64_MEM_OFF, MREG_RBP, -1*(m_total_memory_storage - src_operand.get_imm_ival()));

        // Do the thing
        ll_iseq->append(new Instruction(MINS_LEAQ, memory_ref, temp));
        ll_iseq->append(new Instruction(MINS_MOVQ, temp, dest_operand));
        return;
    }

    // 3 OPERAND
    int src_second_size = highlevel_opcode_get_source_operand_size(hl_opcode);
    Operand src_second_operand = get_ll_operand(hl_ins->get_operand(2), src_second_size, ll_iseq);
    LowLevelOpcode mov_opcode = select_ll_opcode(MINS_MOVB, src_second_size);
    Operand temp(select_mreg_kind(src_second_size), MREG_R10);

    // math
    if (match_hl(HINS_add_b, hl_opcode) || match_hl(HINS_sub_b, hl_opcode)
        || match_hl(HINS_mul_b, hl_opcode) || match_hl(HINS_mod_b, hl_opcode)  ) {

        // Move one into the other, then add using r10 as a temp variable
        ll_iseq->append(new Instruction(mov_opcode, src_operand, temp));
        ll_iseq->append(new Instruction(HL_TO_LL.at(hl_opcode), src_second_operand, temp));
        ll_iseq->append(new Instruction(mov_opcode, temp, dest_operand));
        return;
    }

    // div
    if (match_hl(HINS_div_b, hl_opcode)) {

        // Move one into the other, then add using r10 as a temp variable
        ll_iseq->append(new Instruction(mov_opcode, src_operand, temp));
        if (hl_opcode == HINS_div_q) {
            ll_iseq->append(new Instruction(MINS_IDIVQ, src_second_operand, temp));

        } else {
            ll_iseq->append(new Instruction(MINS_IDIVL, src_second_operand, temp));

        }
        ll_iseq->append(new Instruction(mov_opcode, temp, dest_operand));
        return;
    }


    // cmp
    if (match_hl(HINS_cmplt_b, hl_opcode)|| match_hl(HINS_cmplte_b, hl_opcode)
        || match_hl(HINS_cmpgt_b, hl_opcode) || match_hl(HINS_cmpgte_b, hl_opcode)
        || match_hl(HINS_cmpeq_b, hl_opcode) || match_hl(HINS_cmpneq_b, hl_opcode)) {

        // compare the two
        LowLevelOpcode compare = select_ll_opcode(MINS_CMPB, src_second_size);
        ll_iseq->append(new Instruction(mov_opcode, src_operand, temp));
        ll_iseq->append(new Instruction(compare, src_second_operand, temp));

        // Set appropriate flag
        Operand lowest(Operand::MREG8, MREG_R10);
        ll_iseq->append(new Instruction(HL_TO_LL.at(hl_opcode), lowest));

        if (dest_size == 1) {
            ll_iseq->append(new Instruction(mov_opcode, lowest, dest_operand));
        } else {
            Operand temp_2(select_mreg_kind(dest_size), MREG_R11);
            LowLevelOpcode movz;
            switch (dest_size) {
                case 2:
                    movz = MINS_MOVZBW;
                    break;
                case 4:
                    movz = MINS_MOVZBL;
                    break;
                case 8:
                    movz = MINS_MOVZBQ;
                    break;
                default:
                    RuntimeError::raise("Invalid size passed in");
            }
            ll_iseq->append(new Instruction(movz, lowest, temp_2));
            ll_iseq->append(new Instruction(mov_opcode, temp_2, dest_operand));
        }
        return;
    }



    RuntimeError::raise("high level opcode %d not handled", int(hl_opcode));
}

Operand
LowLevelCodeGen::get_ll_operand(Operand hl_operand, int size, const std::shared_ptr<InstructionSequence> &ll_iseq) {
    if (hl_operand.is_imm_ival() || hl_operand.is_imm_label() || hl_operand.is_label()) {
        return hl_operand;
    } else if (hl_operand.get_base_reg() < 11) {
        // VREG is actually a predefined register
        Operand::Kind kind = select_mreg_kind(size);
        if (hl_operand.is_memref()) {
            kind = Operand::MREG64_MEM;
        }
        switch (hl_operand.get_base_reg()) {
            case 0:
                return {kind, MREG_RAX};
            case 1:
                return {kind, MREG_RDI};
            case 2:
                return {kind, MREG_RSI};
            case 3:
                return {kind, MREG_RDX};
            case 4:
                return {kind, MREG_RCX};
            case 5:
                return {kind, MREG_R8};
            case 6:
                return {kind, MREG_R9};
            case 7:
                return {kind, MREG_R12};
            case 8:
                return {kind, MREG_R13};
            case 9:
                return {kind, MREG_R14};
            case 10:
                return {kind, MREG_R15};

        }
    } else {
        Operand ll = Operand(Operand::MREG64_MEM_OFF, MREG_RBP, get_offset(hl_operand.get_base_reg()));
        if (hl_operand.is_memref()) {
            Operand reg_op(select_mreg_kind(8), MREG_R11);

            // always 64 bit
            ll_iseq->append(new Instruction(MINS_MOVQ, ll, reg_op));
            return {Operand::MREG64_MEM, MREG_R11};
        }
        return ll;
    }
    RuntimeError::raise("Low level Operand cannot be established");
}


/**
 * Get the offset for each particular numbered vreg
 * @param vreg the vreg number
 * @return the actual memory offset
 */
long LowLevelCodeGen::get_offset(int vreg) const {
    return -1 * (vreg_boundary - ((vreg - 11) * 8));
}
