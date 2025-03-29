#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GEN_INSTR_LIST(FN) \
FN(ADC) \
FN(ADD) \
FN(AND) \
FN(BIT) \
FN(CALL) \
FN(CCF) \
FN(CP) \
FN(CPD) \
FN(CPDR) \
FN(CPI) \
FN(CPIR) \
FN(CPL) \
FN(DAA) \
FN(DEC) \
FN(DI) \
FN(DJNZ) \
FN(EI) \
FN(EX) \
FN(EXX) \
FN(HALT) \
FN(IM) \
FN(IN) \
FN(INC) \
FN(IND) \
FN(INDR) \
FN(INI) \
FN(INIR) \
FN(JP) \
FN(JR) \
FN(LD) \
FN(LDD) \
FN(LDDR) \
FN(LDI) \
FN(LDIR) \
FN(NEG) \
FN(NOP) \
FN(OR) \
FN(OUT) \
FN(OUTD) \
FN(OTDR) \
FN(OUTI) \
FN(OTIR) \
FN(POP) \
FN(PUSH) \
FN(RES) \
FN(RET) \
FN(RETI) \
FN(RETN) \
FN(RLA) \
FN(RL) \
FN(RLCA) \
FN(RLC) \
FN(RLD) \
FN(RRA) \
FN(RR) \
FN(RRCA) \
FN(RRC) \
FN(RRD) \
FN(RST) \
FN(SBC) \
FN(SCF) \
FN(SET) \
FN(SLA) \
FN(SRA) \
FN(SLL) \
FN(SRL) \
FN(SUB) \
FN(XOR)

#define FN_ENUM(ENUM) ENUM,
#define FN_STRING(STRING) #STRING,

typedef enum {
  GEN_INSTR_LIST(FN_ENUM)
  MAX_MNEMONIC_ID
} MnemonicEnum;

#define REG_A     0x7
#define REG_B     0x0
#define REG_C     0x1
#define REG_D     0x2
#define REG_E     0x3
#define REG_H     0x4
#define REG_L     0x5
#define REG_BC    0x0
#define REG_DE    0x1
#define REG_HL    0x2
#define REG_IX_IY 0x2
#define REG_SP    0x3
#define REG_IX    0x0
#define REG_IY    0x1
#define REG_IXH   0x4
#define REG_IXL   0x5
#define REG_IYH   0x4
#define REG_IYL   0x5
#define COND_NZ   0x0
#define COND_Z    0x1
#define COND_NC   0x2
#define COND_C    0x3
#define COND_PO   0x4
#define COND_PE   0x5
#define COND_P    0x6
#define COND_M    0x7

typedef enum {
  ARG_16IMM,
  ARG_BITNUM,
  ARG_CONDITION,
  ARG_RELJUMP,
  ARG_INDEX,
  ARG_HALFINDEX,
  ARG_8IMM,
  ARG_REGPAIR_P,
  ARG_REGPAIR_Q,
  ARG_REGPAIR_I,
  ARG_8GPR,
  ARG_RST,
  ARG_INTERRUPT,
  ARG_REFRESH,
  ARG_AF_SHADOW,
} arg_kind;

typedef struct {
  arg_kind kind;
  bool is_ref;
  int value;  // argument value (depend on kind)
  int extra;  // extra value (for index/halfindex kinds)
} disas_arg_t;

struct disas_node;

typedef struct disas_node {
  MnemonicEnum instr;
  bool valid;
  int isize;
  int addr;
  int num_args;
  disas_arg_t args[2];
  int cycles;

  struct disas_node *next;
} disas_node_t;

#define ADDRSPC 65536

typedef struct {
  bool opt_addr;
  bool opt_source;
  bool opt_labels;
  bool opt_timings;
  uint16_t org;
  uint16_t curr_addr;
  uint8_t *binary;
  uint8_t labels_bmp[ADDRSPC / 8];
  disas_node_t *nodes;
  disas_node_t *last_node;

  char *out_str;
  uint32_t out_len;
  uint32_t out_capacity;
} disas_context_t;

#define ADD_LABEL(ctx, addr) ctx->labels_bmp[(addr) / 8] |= (1 << ((addr) % 8))
#define HAS_LABEL(ctx, addr) (ctx->labels_bmp[(addr) / 8] & (1 << ((addr) % 8)))

extern char *disassemble(char *data,
                  ssize_t size,
                  bool opt_addr,
                  bool opt_source,
                  bool opt_labels,
                  bool opt_timings,
                  int opt_org);
extern void disas_render_text(disas_context_t *ctx);

extern char *MnemonicStrings[];

// frequently used register shortcuts
#define mk_arg_A mk_arg(ARG_8GPR, REG_A, 0, false)
#define mk_arg_BC mk_arg(ARG_REGPAIR_P, REG_BC, 0, false)
#define mk_arg_DE mk_arg(ARG_REGPAIR_P, REG_DE, 0, false)
#define mk_arg_HL mk_arg(ARG_REGPAIR_P, REG_HL, 0, false)
#define mk_arg_SP mk_arg(ARG_REGPAIR_Q, REG_SP, 0, false)
#define mk_arg_HL_REF mk_arg(ARG_REGPAIR_P, REG_HL, 0, true)
#define mk_arg_I mk_arg(ARG_INTERRUPT, 0, 0, false)
#define mk_arg_R mk_arg(ARG_REFRESH, 0, 0, false)
#define mk_GPR(_gpr_) mk_arg(ARG_8GPR, _gpr_, 0, false)
#define mk_HALFINDEX(_r_, _pfx_) mk_arg(ARG_HALFINDEX, _r_, _pfx_, false)

extern char *disassemble(char *data,
                  ssize_t size,
                  bool opt_addr,
                  bool opt_source,
                  bool opt_labels,
                  bool opt_timings,
                  int opt_org);
