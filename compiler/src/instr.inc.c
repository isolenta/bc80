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

static char *MnemonicStrings[] = {
  GEN_INSTR_LIST(FN_STRING)
};

#define NARGS_0 0x1
#define NARGS_1 0x2
#define NARGS_2 0x3

static int NUM_ARGS[] = {
  NARGS_2,            // ADC
  NARGS_2,            // ADD
  NARGS_1,            // AND
  NARGS_2,            // BIT
  NARGS_1 | NARGS_2,  // CALL
  NARGS_0,            // CCF
  NARGS_1,            // CP
  NARGS_0,            // CPD
  NARGS_0,            // CPDR
  NARGS_0,            // CPI
  NARGS_0,            // CPIR
  NARGS_0,            // CPL
  NARGS_0,            // DAA
  NARGS_1,            // DEC
  NARGS_0,            // DI
  NARGS_1,            // DJNZ
  NARGS_0,            // EI
  NARGS_2,            // EX
  NARGS_0,            // EXX
  NARGS_0,            // HALT
  NARGS_1,            // IM
  NARGS_2,            // IN
  NARGS_1,            // INC
  NARGS_0,            // IND
  NARGS_0,            // INDR
  NARGS_0,            // INI
  NARGS_0,            // INIR
  NARGS_1 | NARGS_2,  // JP
  NARGS_1 | NARGS_2,  // JR
  NARGS_2,            // LD
  NARGS_0,            // LDD
  NARGS_0,            // LDDR
  NARGS_0,            // LDI
  NARGS_0,            // LDIR
  NARGS_0,            // NEG
  NARGS_0,            // NOP
  NARGS_1,            // OR
  NARGS_2,            // OUT
  NARGS_0,            // OUTD
  NARGS_0,            // OTDR
  NARGS_0,            // OUTI
  NARGS_0,            // OTIR
  NARGS_1,            // POP
  NARGS_1,            // PUSH
  NARGS_2,            // RES
  NARGS_0 | NARGS_1,  // RET
  NARGS_0,            // RETI
  NARGS_0,            // RETN
  NARGS_0,            // RLA
  NARGS_1,            // RL
  NARGS_0,            // RLCA
  NARGS_1,            // RLC
  NARGS_0,            // RLD
  NARGS_0,            // RRA
  NARGS_1,            // RR
  NARGS_0,            // RRCA
  NARGS_1,            // RRC
  NARGS_0,            // RRD
  NARGS_1,            // RST
  NARGS_2,            // SBC
  NARGS_0,            // SCF
  NARGS_2,            // SET
  NARGS_1,            // SLA
  NARGS_1,            // SRA
  NARGS_1,            // SLL
  NARGS_1,            // SRL
  NARGS_1,            // SUB
  NARGS_1,            // XOR
};

_Static_assert((sizeof(NUM_ARGS) / sizeof(NUM_ARGS[0])) == MAX_MNEMONIC_ID, "NUM_ARGS must have same size as MnemonicEnum");

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
#define COND_NZ   0x0
#define COND_Z    0x1
#define COND_NC   0x2
#define COND_C    0x3
#define COND_PO   0x4
#define COND_PE   0x5
#define COND_P    0x6
#define COND_M    0x7

static char *RESERVED_IDENTS[] = {
  "A", "B", "C", "D", "E", "H", "L", "F", "I", "R",
  "BC", "DE", "HL", "AF", "AF'", "SP",
  "IX", "IY", "IXH", "IXL", "IYH", "IYL",
  "NZ", "Z", "NC", "C", "PO", "PE", "P", "M", NULL};
