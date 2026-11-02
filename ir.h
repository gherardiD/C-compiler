#ifndef IR_H
#define IR_H

#include "parser.h"

// Three-address code used by the IR generator.

typedef enum {
    OPERAND_NONE,      // Unused operand slot in an instruction
    OPERAND_VAR,       // Program variable identifier (e.g., "counter", "x")
    OPERAND_TEMP,      // Compiler-generated temporary (e.g., "t0", "t1")
    OPERAND_LITERAL,   // Literal constant (e.g., "15", "3.14", "\"Hello\"")
    OPERAND_LABEL      // Branch target label (e.g., "L0", "L1")
} IROperandKind;

// An operand in a Three-Address Code instruction.
typedef struct {
    IROperandKind kind;
    char *name;        // String representation of the operand
} IROperand;

typedef enum {
    // Control flow & labels
    IR_LABEL,           // Label definition: L0:
    IR_JUMP,            // Unconditional jump: goto target
    IR_JUMP_IF,         // Branch if true: if cond goto target
    IR_JUMP_IF_FALSE,   // Branch if false: ifFalse cond goto target

    // Assignments and data movement
    IR_ASSIGN,          // dest = src1

    // Arithmetic binary operations
    IR_ADD,             // dest = src1 + src2
    IR_SUB,             // dest = src1 - src2
    IR_MUL,             // dest = src1 * src2
    IR_DIV,             // dest = src1 / src2
    IR_MOD,             // dest = src1 % src2

    // Relational / Comparison operations
    IR_EQ,              // dest = src1 == src2
    IR_NE,              // dest = src1 != src2
    IR_LT,              // dest = src1 < src2
    IR_LE,              // dest = src1 <= src2
    IR_GT,              // dest = src1 > src2
    IR_GE,              // dest = src1 >= src2

    // Logical operations
    IR_AND,             // dest = src1 && src2
    IR_OR,              // dest = src1 || src2

    // Unary operations
    IR_NEG,             // dest = -src1
    IR_NOT,             // dest = !src1
    IR_BNOT,            // dest = ~src1
    IR_ADDR_OF,         // dest = &src1
    IR_DEREF_LOAD,      // dest = *src1
    IR_DEREF_STORE,     // *dest = src1

    // Array operations
    IR_ARRAY_LOAD,      // dest = src1[src2]
    IR_ARRAY_STORE,     // dest[src1] = src2
    IR_ALLOC,           // alloc dest, src1 (allocate array of size src1)

    // Functions and calls
    IR_FUNC_START,      // func begin <name>
    IR_FUNC_END,        // func end <name>
    IR_PARAM,           // param <name>
    IR_ARG,             // arg <operand>
    IR_CALL,            // dest = call <name>, <argCount> (or call <name>, <argCount>)
    IR_RETURN           // return <operand> (or return)
} IROpcode;

// One instruction in the IR list.
typedef struct IRInstruction {
    IROpcode opcode;
    IROperand dest;
    IROperand src1;
    IROperand src2;
    struct IRInstruction *next;
} IRInstruction;

// A complete IR program containing an instruction sequence.
typedef struct {
    IRInstruction *head;
    IRInstruction *tail;
} IRProgram;

// Generator context maintaining counters and control flow stacks.
typedef struct {
    IRProgram *program;
    int tempCount;             // Counter for compiler temporaries (t0, t1, ...)
    int labelCount;            // Counter for labels (L0, L1, ...)
    char *breakLabels[128];    // Loop break targets for nested loops
    char *continueLabels[128]; // Loop continue targets for nested loops
    int loopDepth;             // Current nesting depth
} IRGenerator;

IRProgram *createIRProgram(void);

void freeIRProgram(IRProgram *program);

IRGenerator *createIRGenerator(void);

void freeIRGenerator(IRGenerator *gen);

// Operand constructors
IROperand irOperandNone(void);
IROperand irOperandVar(const char *name);
IROperand irOperandTemp(const char *name);
IROperand irOperandLiteral(const char *value);
IROperand irOperandLabel(const char *label);
IROperand irOperandCopy(const IROperand *op);
void freeOperand(IROperand *op);

IRInstruction *emitIR(IRProgram *program, IROpcode opcode, IROperand dest, IROperand src1, IROperand src2);

// Temporary and label generators
char *newTemp(IRGenerator *gen);
IROperand newTempOperand(IRGenerator *gen);
char *newLabel(IRGenerator *gen);
IROperand newLabelOperand(IRGenerator *gen);

IRProgram *generateIR(ASTNode *ast);

void printIR(const IRProgram *program);

#endif // IR_H
