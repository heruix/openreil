#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <iostream>
#include <string>

// libasmir includes
#include "irtoir.h"

// OpenREIL includes
#include "libopenreil.h"
#include "reil_translator.h"

#define STR_ARG_EMPTY " "
#define STR_VAR(_name_, _t_) "(" + (_name_) + ", " + to_string_size((_t_)) + ")"
#define STR_CONST(_val_, _t_) "(" + to_string_constant((_val_), (_t_)) + ", " + to_string_size((_t_)) + ")"

typedef struct _reil_context
{
    CReilTranslator *translator;

} reil_context;

string to_string_constant(reil_const_t val, reil_size_t size)
{
    stringstream s;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;

    switch (size)
    {
    case U1: if (val == 0) s << "0"; else s << "1"; break;
    case U8: u8 = (uint8_t)val; s << dec << (int)u8; break;
    case U16: u16 = (uint16_t)val; s << u16; break;
    case U32: u32 = (uint32_t)val; s << u32; break;
    case U64: u64 = (uint64_t)val; s << u64; break;
    default: assert(0);
    }
    
    return s.str();
}

string to_string_size(reil_size_t size)
{
    switch (size)
    {
    case U1: return string("1");
    case U8: return string("8");
    case U16: return string("16");
    case U32: return string("32");
    case U64: return string("64");
    }

    assert(0);
}

string to_string_operand(reil_arg_t *a)
{
    switch (a->type)
    {
    case A_NONE: return string(STR_ARG_EMPTY);
    case A_REG: return STR_VAR(string(a->name), a->size);
    case A_TEMP: return STR_VAR(string(a->name), a->size);
    case A_CONST: return STR_CONST(a->val, a->size);
    }    

    assert(0);
}

// defined in reil_translator.cpp
extern const char *reil_inst_name[];

string to_string_inst_code(reil_op_t inst_code)
{
    return string(reil_inst_name[inst_code]);
}

void reil_inst_print(reil_inst_t *inst)
{
    printf("%.8llx.%.2x ", inst->raw_info.addr, inst->inum);  
    printf("%7s ", to_string_inst_code(inst->op).c_str());
    printf("%16s, ", to_string_operand(&inst->a).c_str());
    printf("%16s, ", to_string_operand(&inst->b).c_str());
    printf("%16s  ", to_string_operand(&inst->c).c_str());
    printf("\n");
}

reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context)
{
    VexArch guest;

    switch (arch)
    {
    case REIL_X86: guest = VexArchX86; break;
    default: assert(0);
    }

    // allocate translator context
    reil_context *c = (reil_context *)malloc(sizeof(reil_context));
    assert(c);

    // create new translator instance
    c->translator = new CReilTranslator(guest, handler, context);
    assert(c->translator);

    return c;
}

void reil_close(reil_t reil)
{
    reil_context *c = (reil_context *)reil;
    assert(c);

    assert(c->translator);
    delete c->translator;

    free(c);
}

int reil_translate_report_error(reil_addr_t addr, const char *reason)
{
    fprintf(stderr, "Eror while processing instruction at address 0x%llx\n", addr);
    fprintf(stderr, "Exception occurs: %s\n", reason);
    
    return REIL_ERROR;
}

int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len)
{
    int inst_len = 0;
    reil_context *c = (reil_context *)reil;
    assert(c);    

    try
    {
        inst_len = c->translator->process_inst(addr, buff, len);    
        assert(inst_len != 0 && inst_len != -1);
    }
    catch (CReilTranslatorException e)
    {
        // libopenreil exception
        return reil_translate_report_error(addr, e.reason.c_str());
    }
    catch (const char *e)
    {
        // libasmir exception
        return reil_translate_report_error(addr, e);
    }

    return inst_len;
}

int reil_translate(reil_t reil, reil_addr_t addr, unsigned char *buff, int len)
{
    int p = 0, translated = 0;    

    while (p < len)
    {
        uint8_t inst_buff[MAX_INST_LEN];
        int copy_len = min(MAX_INST_LEN, len - p), inst_len = 0;

        // copy one instruction into the buffer
        memset(inst_buff, 0, sizeof(inst_buff));
        memcpy(inst_buff, buff + p, copy_len);

        inst_len = reil_translate_insn(reil, addr + p, inst_buff, sizeof(inst_buff));
        if (inst_len == REIL_ERROR) return REIL_ERROR;

        p += inst_len;
        translated += 1;
    }    

    return translated;
}