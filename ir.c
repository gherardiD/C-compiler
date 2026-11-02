#define _POSIX_C_SOURCE 200809L
#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void generateStatement(IRGenerator *gen, ASTNode *node);
static IROperand generateExpression(IRGenerator *gen, ASTNode *node);

IROperand irOperandNone(void) {
    IROperand op;
    op.kind = OPERAND_NONE;
    op.name = NULL;
    return op;
}

IROperand irOperandVar(const char *name) {
    IROperand op;
    op.kind = OPERAND_VAR;
    op.name = name ? strdup(name) : NULL;
    return op;
}

IROperand irOperandTemp(const char *name) {
    IROperand op;
    op.kind = OPERAND_TEMP;
    op.name = name ? strdup(name) : NULL;
    return op;
}

IROperand irOperandLiteral(const char *value) {
    IROperand op;
    op.kind = OPERAND_LITERAL;
    op.name = value ? strdup(value) : NULL;
    return op;
}

IROperand irOperandLabel(const char *label) {
    IROperand op;
    op.kind = OPERAND_LABEL;
    op.name = label ? strdup(label) : NULL;
    return op;
}

IROperand irOperandCopy(const IROperand *op) {
    IROperand copy;
    copy.kind = op ? op->kind : OPERAND_NONE;
    copy.name = (op && op->name) ? strdup(op->name) : NULL;
    return copy;
}

void freeOperand(IROperand *op) {
    if (!op) return;
    if (op->name) {
        free(op->name);
        op->name = NULL;
    }
    op->kind = OPERAND_NONE;
}

IRProgram *createIRProgram(void) {
    IRProgram *program = (IRProgram *)malloc(sizeof(IRProgram));
    if (!program) {
        fprintf(stderr, "Error: Memory allocation failed for IRProgram\n");
        exit(1);
    }
    program->head = NULL;
    program->tail = NULL;
    return program;
}

void freeIRProgram(IRProgram *program) {
    if (!program) return;
    IRInstruction *curr = program->head;
    while (curr) {
        IRInstruction *next = curr->next;
        freeOperand(&curr->dest);
        freeOperand(&curr->src1);
        freeOperand(&curr->src2);
        free(curr);
        curr = next;
    }
    free(program);
}

IRInstruction *emitIR(IRProgram *program, IROpcode opcode, IROperand dest, IROperand src1, IROperand src2) {
    IRInstruction *instr = (IRInstruction *)malloc(sizeof(IRInstruction));
    if (!instr) {
        fprintf(stderr, "Error: Memory allocation failed for IRInstruction\n");
        exit(1);
    }
    instr->opcode = opcode;
    instr->dest = dest;
    instr->src1 = src1;
    instr->src2 = src2;
    instr->next = NULL;

    if (program->tail) {
        program->tail->next = instr;
    } else {
        program->head = instr;
    }
    program->tail = instr;

    return instr;
}

IRGenerator *createIRGenerator(void) {
    IRGenerator *gen = (IRGenerator *)malloc(sizeof(IRGenerator));
    if (!gen) {
        fprintf(stderr, "Error: Memory allocation failed for IRGenerator\n");
        exit(1);
    }
    gen->program = createIRProgram();
    gen->tempCount = 0;
    gen->labelCount = 0;
    gen->loopDepth = 0;
    for (int i = 0; i < 128; i++) {
        gen->breakLabels[i] = NULL;
        gen->continueLabels[i] = NULL;
    }
    return gen;
}

static void pushLoop(IRGenerator *gen, const char *breakLabel, const char *continueLabel) {
    if (gen->loopDepth < 128) {
        gen->breakLabels[gen->loopDepth] = strdup(breakLabel);
        gen->continueLabels[gen->loopDepth] = strdup(continueLabel);
        gen->loopDepth++;
    } else {
        fprintf(stderr, "Error: Maximum loop nesting depth exceeded in IR generator\n");
    }
}

static void popLoop(IRGenerator *gen) {
    if (gen->loopDepth > 0) {
        gen->loopDepth--;
        free(gen->breakLabels[gen->loopDepth]);
        free(gen->continueLabels[gen->loopDepth]);
        gen->breakLabels[gen->loopDepth] = NULL;
        gen->continueLabels[gen->loopDepth] = NULL;
    }
}

void freeIRGenerator(IRGenerator *gen) {
    if (!gen) return;
    while (gen->loopDepth > 0) {
        popLoop(gen);
    }
    free(gen);
}

char *newTemp(IRGenerator *gen) {
    char buf[32];
    snprintf(buf, sizeof(buf), "t%d", gen->tempCount++);
    return strdup(buf);
}

IROperand newTempOperand(IRGenerator *gen) {
    char *name = newTemp(gen);
    IROperand op = irOperandTemp(name);
    free(name);
    return op;
}

char *newLabel(IRGenerator *gen) {
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d", gen->labelCount++);
    return strdup(buf);
}

IROperand newLabelOperand(IRGenerator *gen) {
    char *name = newLabel(gen);
    IROperand op = irOperandLabel(name);
    free(name);
    return op;
}

// Decomposes nested AST expressions into 3-address instructions using temporaries.

static IROperand generateExpression(IRGenerator *gen, ASTNode *node) {
    if (!node) return irOperandNone();

    switch (node->type) {
        // --- Literals and Identifiers ---
        case NODE_INTEGER_LITERAL:
        case NODE_FLOAT_LITERAL:
            return irOperandLiteral(node->data.literal.value);

        case NODE_STRING_LITERAL: {
            size_t len = strlen(node->data.literal.value);
            char *quoted = (char *)malloc(len + 3);
            snprintf(quoted, len + 3, "\"%s\"", node->data.literal.value);
            IROperand op = irOperandLiteral(quoted);
            free(quoted);
            return op;
        }

        case NODE_IDENTIFIER:
            return irOperandVar(node->data.literal.value);

        // --- Array Access (R-Value): dest = arr[index] ---
        case NODE_ARRAY_ACCESS: {
            IROperand arrOp = generateExpression(gen, node->data.arrayAccess.array);
            IROperand idxOp = generateExpression(gen, node->data.arrayAccess.index);
            IROperand dest = newTempOperand(gen);
            emitIR(gen->program, IR_ARRAY_LOAD, dest, arrOp, idxOp);
            return irOperandCopy(&dest);
        }

        // --- Function Calls: arg 1, arg 2, ... dest = call func, N ---
        case NODE_FUNC_CALL: {
            const char *funcName = node->data.funCall.name;
            int argCount = node->data.funCall.argCount;

            // Evaluate each argument and emit an IR_ARG instruction
            for (int i = 0; i < argCount; i++) {
                IROperand argOp = generateExpression(gen, node->data.funCall.args[i]);
                emitIR(gen->program, IR_ARG, irOperandNone(), argOp, irOperandNone());
            }

            char argCountStr[16];
            snprintf(argCountStr, sizeof(argCountStr), "%d", argCount);

            IROperand dest = newTempOperand(gen);
            emitIR(gen->program, IR_CALL, dest, irOperandVar(funcName), irOperandLiteral(argCountStr));
            return irOperandCopy(&dest);
        }

        // --- Binary Operations: dest = left op right ---
        case NODE_BINARY_OP: {
            const char *op = node->data.binaryOp.op;

            // Logical AND (&&) with short-circuit evaluation:
            // if left is false, do not evaluate right; jump straight to falseLabel.
            if (strcmp(op, "&&") == 0) {
                char *falseLabel = newLabel(gen);
                char *endLabel = newLabel(gen);
                IROperand dest = newTempOperand(gen);

                IROperand left = generateExpression(gen, node->data.binaryOp.left);
                emitIR(gen->program, IR_JUMP_IF_FALSE, irOperandLabel(falseLabel), left, irOperandNone());

                IROperand right = generateExpression(gen, node->data.binaryOp.right);
                emitIR(gen->program, IR_JUMP_IF_FALSE, irOperandLabel(falseLabel), right, irOperandNone());

                // Both true: set result to 1
                emitIR(gen->program, IR_ASSIGN, irOperandCopy(&dest), irOperandLiteral("1"), irOperandNone());
                emitIR(gen->program, IR_JUMP, irOperandLabel(endLabel), irOperandNone(), irOperandNone());

                // Short-circuit false: set result to 0
                emitIR(gen->program, IR_LABEL, irOperandLabel(falseLabel), irOperandNone(), irOperandNone());
                emitIR(gen->program, IR_ASSIGN, irOperandCopy(&dest), irOperandLiteral("0"), irOperandNone());

                emitIR(gen->program, IR_LABEL, irOperandLabel(endLabel), irOperandNone(), irOperandNone());

                free(falseLabel);
                free(endLabel);
                return dest;
            }

            // Logical OR (||) with short-circuit evaluation:
            // if left is true, do not evaluate right; jump straight to trueLabel.
            if (strcmp(op, "||") == 0) {
                char *trueLabel = newLabel(gen);
                char *endLabel = newLabel(gen);
                IROperand dest = newTempOperand(gen);

                IROperand left = generateExpression(gen, node->data.binaryOp.left);
                emitIR(gen->program, IR_JUMP_IF, irOperandLabel(trueLabel), left, irOperandNone());

                IROperand right = generateExpression(gen, node->data.binaryOp.right);
                emitIR(gen->program, IR_JUMP_IF, irOperandLabel(trueLabel), right, irOperandNone());

                // Both false: set result to 0
                emitIR(gen->program, IR_ASSIGN, irOperandCopy(&dest), irOperandLiteral("0"), irOperandNone());
                emitIR(gen->program, IR_JUMP, irOperandLabel(endLabel), irOperandNone(), irOperandNone());

                // Short-circuit true: set result to 1
                emitIR(gen->program, IR_LABEL, irOperandLabel(trueLabel), irOperandNone(), irOperandNone());
                emitIR(gen->program, IR_ASSIGN, irOperandCopy(&dest), irOperandLiteral("1"), irOperandNone());

                emitIR(gen->program, IR_LABEL, irOperandLabel(endLabel), irOperandNone(), irOperandNone());

                free(trueLabel);
                free(endLabel);
                return dest;
            }

            // Map operator string to Three-Address Code opcode
            IROpcode irOp;
            if (strcmp(op, "+") == 0) irOp = IR_ADD;
            else if (strcmp(op, "-") == 0) irOp = IR_SUB;
            else if (strcmp(op, "*") == 0) irOp = IR_MUL;
            else if (strcmp(op, "/") == 0) irOp = IR_DIV;
            else if (strcmp(op, "%") == 0) irOp = IR_MOD;
            else if (strcmp(op, "==") == 0) irOp = IR_EQ;
            else if (strcmp(op, "!=") == 0) irOp = IR_NE;
            else if (strcmp(op, "<") == 0) irOp = IR_LT;
            else if (strcmp(op, "<=") == 0) irOp = IR_LE;
            else if (strcmp(op, ">") == 0) irOp = IR_GT;
            else if (strcmp(op, ">=") == 0) irOp = IR_GE;
            else irOp = IR_ADD;

            IROperand left = generateExpression(gen, node->data.binaryOp.left);
            IROperand right = generateExpression(gen, node->data.binaryOp.right);
            IROperand dest = newTempOperand(gen);
            emitIR(gen->program, irOp, dest, left, right);
            return irOperandCopy(&dest);
        }

        // --- Assignments: =, +=, -=, *=, /= ---
        case NODE_ASSIGNMENT: {
            const char *op = node->data.binaryOp.op;
            ASTNode *left = node->data.binaryOp.left;
            ASTNode *right = node->data.binaryOp.right;
            IROperand rhs = generateExpression(gen, right);

            // Simple assignment: target = rhs
            if (strcmp(op, "=") == 0) {
                if (left->type == NODE_IDENTIFIER) {
                    IROperand target = irOperandVar(left->data.literal.value);
                    emitIR(gen->program, IR_ASSIGN, target, rhs, irOperandNone());
                    return irOperandVar(left->data.literal.value);
                } else if (left->type == NODE_ARRAY_ACCESS) {
                    IROperand arrOp = generateExpression(gen, left->data.arrayAccess.array);
                    IROperand idxOp = generateExpression(gen, left->data.arrayAccess.index);
                    emitIR(gen->program, IR_ARRAY_STORE, arrOp, idxOp, rhs);
                    return irOperandNone();
                } else if (left->type == NODE_UNARY_OP && strcmp(left->data.unaryOp.op, "*") == 0) {
                    // Dereference store: *ptr = rhs
                    IROperand ptrOp = generateExpression(gen, left->data.unaryOp.operand);
                    emitIR(gen->program, IR_DEREF_STORE, ptrOp, rhs, irOperandNone());
                    return irOperandNone();
                }
            } else {
                // Compound assignment: +=, -=, *=, /=
                // e.g., 'counter += 1' lowers to 't = counter + 1; counter = t'
                IROpcode binOp;
                if (strcmp(op, "+=") == 0) binOp = IR_ADD;
                else if (strcmp(op, "-=") == 0) binOp = IR_SUB;
                else if (strcmp(op, "*=") == 0) binOp = IR_MUL;
                else if (strcmp(op, "/=") == 0) binOp = IR_DIV;
                else binOp = IR_ADD;

                if (left->type == NODE_IDENTIFIER) {
                    const char *varName = left->data.literal.value;
                    IROperand curVal = irOperandVar(varName);
                    IROperand t = newTempOperand(gen);
                    emitIR(gen->program, binOp, t, curVal, rhs);
                    IROperand target = irOperandVar(varName);
                    emitIR(gen->program, IR_ASSIGN, target, irOperandCopy(&t), irOperandNone());
                    return irOperandVar(varName);
                } else if (left->type == NODE_ARRAY_ACCESS) {
                    IROperand arrOp = generateExpression(gen, left->data.arrayAccess.array);
                    IROperand idxOp = generateExpression(gen, left->data.arrayAccess.index);
                    IROperand curVal = newTempOperand(gen);
                    emitIR(gen->program, IR_ARRAY_LOAD, curVal, irOperandCopy(&arrOp), irOperandCopy(&idxOp));
                    IROperand t = newTempOperand(gen);
                    emitIR(gen->program, binOp, t, irOperandCopy(&curVal), rhs);
                    emitIR(gen->program, IR_ARRAY_STORE, arrOp, idxOp, irOperandCopy(&t));
                    return irOperandNone();
                }
            }
            freeOperand(&rhs);
            return irOperandNone();
        }

        // --- Unary Operations: -, +, !, ~, &, *, ++, -- ---
        case NODE_UNARY_OP: {
            const char *op = node->data.unaryOp.op;
            ASTNode *operand = node->data.unaryOp.operand;

            if (strcmp(op, "-") == 0) {
                IROperand opnd = generateExpression(gen, operand);
                IROperand dest = newTempOperand(gen);
                emitIR(gen->program, IR_NEG, dest, opnd, irOperandNone());
                return irOperandCopy(&dest);
            } else if (strcmp(op, "+") == 0) {
                return generateExpression(gen, operand);
            } else if (strcmp(op, "!") == 0) {
                IROperand opnd = generateExpression(gen, operand);
                IROperand dest = newTempOperand(gen);
                emitIR(gen->program, IR_NOT, dest, opnd, irOperandNone());
                return irOperandCopy(&dest);
            } else if (strcmp(op, "~") == 0) {
                IROperand opnd = generateExpression(gen, operand);
                IROperand dest = newTempOperand(gen);
                emitIR(gen->program, IR_BNOT, dest, opnd, irOperandNone());
                return irOperandCopy(&dest);
            } else if (strcmp(op, "&") == 0) {
                IROperand opnd = generateExpression(gen, operand);
                IROperand dest = newTempOperand(gen);
                emitIR(gen->program, IR_ADDR_OF, dest, opnd, irOperandNone());
                return irOperandCopy(&dest);
            } else if (strcmp(op, "*") == 0) {
                IROperand opnd = generateExpression(gen, operand);
                IROperand dest = newTempOperand(gen);
                emitIR(gen->program, IR_DEREF_LOAD, dest, opnd, irOperandNone());
                return irOperandCopy(&dest);
            } else if (strcmp(op, "++") == 0 || strcmp(op, "--") == 0) {
                IROpcode binOp = (strcmp(op, "++") == 0) ? IR_ADD : IR_SUB;
                if (operand->type == NODE_IDENTIFIER) {
                    const char *idName = operand->data.literal.value;
                    IROperand curVal = irOperandVar(idName);
                    IROperand t = newTempOperand(gen);
                    emitIR(gen->program, binOp, t, curVal, irOperandLiteral("1"));
                    IROperand target = irOperandVar(idName);
                    emitIR(gen->program, IR_ASSIGN, target, irOperandCopy(&t), irOperandNone());
                    return irOperandVar(idName);
                }
            }
            return irOperandNone();
        }

        default:
            return irOperandNone();
    }
}

// Lowers high-level statements into sequential Three-Address Code instructions.

static void generateStatement(IRGenerator *gen, ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        // --- Program Root ---
        case NODE_PROGRAM: {
            for (int i = 0; i < node->data.program.count; i++) {
                generateStatement(gen, node->data.program.declarations[i]);
            }
            break;
        }

        // --- Compound Statement { ... } ---
        case NODE_COMPOUND_STMT: {
            for (int i = 0; i < node->data.compoundStmt.count; i++) {
                generateStatement(gen, node->data.compoundStmt.statements[i]);
            }
            break;
        }

        // --- Function Declaration & Body ---
        case NODE_FUNC_DECL: {
            const char *name = node->data.funDecl.name;
            // Reset temporary variable counter for each function for clean t0, t1, ...
            gen->tempCount = 0;

            emitIR(gen->program, IR_FUNC_START, irOperandVar(name), irOperandNone(), irOperandNone());

            // Emit function parameters
            for (int i = 0; i < node->data.funDecl.paramCount; i++) {
                const char *pName = node->data.funDecl.params[i]->data.varDecl.name;
                emitIR(gen->program, IR_PARAM, irOperandVar(pName), irOperandNone(), irOperandNone());
            }

            // Generate function body
            if (node->data.funDecl.body) {
                generateStatement(gen, node->data.funDecl.body);
            }

            emitIR(gen->program, IR_FUNC_END, irOperandVar(name), irOperandNone(), irOperandNone());
            break;
        }

        // --- Variable Declaration (optional init) ---
        case NODE_VAR_DECL: {
            const char *name = node->data.varDecl.name;
            if (node->data.varDecl.init) {
                IROperand initVal = generateExpression(gen, node->data.varDecl.init);
                emitIR(gen->program, IR_ASSIGN, irOperandVar(name), initVal, irOperandNone());
            }
            break;
        }

        // --- Array Declaration: alloc arr, size ---
        case NODE_ARRAY_DECL: {
            const char *name = node->data.arrayDecl.name;
            IROperand sizeOp;
            if (node->data.arrayDecl.size) {
                sizeOp = generateExpression(gen, node->data.arrayDecl.size);
            } else {
                sizeOp = irOperandLiteral("0");
            }
            emitIR(gen->program, IR_ALLOC, irOperandVar(name), sizeOp, irOperandNone());
            break;
        }

        // --- Expression Statement (e.g., function call or assignment) ---
        case NODE_EXPRESSION_STMT: {
            if (node->data.exprStmt.expr) {
                // If it's a function call at statement level, call without capturing return value
                if (node->data.exprStmt.expr->type == NODE_FUNC_CALL) {
                    ASTNode *callNode = node->data.exprStmt.expr;
                    const char *funcName = callNode->data.funCall.name;
                    int argCount = callNode->data.funCall.argCount;
                    for (int i = 0; i < argCount; i++) {
                        IROperand argOp = generateExpression(gen, callNode->data.funCall.args[i]);
                        emitIR(gen->program, IR_ARG, irOperandNone(), argOp, irOperandNone());
                    }
                    char argCountStr[16];
                    snprintf(argCountStr, sizeof(argCountStr), "%d", argCount);
                    emitIR(gen->program, IR_CALL, irOperandNone(), irOperandVar(funcName), irOperandLiteral(argCountStr));
                } else {
                    IROperand res = generateExpression(gen, node->data.exprStmt.expr);
                    freeOperand(&res);
                }
            }
            break;
        }

        // --- If-Else Statement ---
        // if (condition) thenBranch [else elseBranch]
        //
        // Pattern with else:
        //   cond = evaluate(condition)
        //   ifFalse cond goto L_else
        //   <thenBranch>
        //   goto L_end
        // L_else:
        //   <elseBranch>
        // L_end:
        //
        // Pattern without else:
        //   cond = evaluate(condition)
        //   ifFalse cond goto L_end
        //   <thenBranch>
        // L_end:
        case NODE_IF_STMT: {
            char *elseLabel = node->data.ifStmt.elseBranch ? newLabel(gen) : NULL;
            char *endLabel = newLabel(gen);

            IROperand cond = generateExpression(gen, node->data.ifStmt.condition);
            char *branchTarget = elseLabel ? elseLabel : endLabel;
            emitIR(gen->program, IR_JUMP_IF_FALSE, irOperandLabel(branchTarget), cond, irOperandNone());

            generateStatement(gen, node->data.ifStmt.thenBranch);

            if (elseLabel) {
                emitIR(gen->program, IR_JUMP, irOperandLabel(endLabel), irOperandNone(), irOperandNone());
                emitIR(gen->program, IR_LABEL, irOperandLabel(elseLabel), irOperandNone(), irOperandNone());
                generateStatement(gen, node->data.ifStmt.elseBranch);
                free(elseLabel);
            }

            emitIR(gen->program, IR_LABEL, irOperandLabel(endLabel), irOperandNone(), irOperandNone());
            free(endLabel);
            break;
        }

        // --- While Loop ---
        // while (condition) body
        //
        // Pattern:
        // L_start:
        //   cond = evaluate(condition)
        //   ifFalse cond goto L_end
        //   <body>
        //   goto L_start
        // L_end:
        case NODE_WHILE_STMT: {
            char *startLabel = newLabel(gen);
            char *endLabel = newLabel(gen);

            // In while loop, continue jumps to startLabel (condition check),
            // break jumps to endLabel (exit loop).
            pushLoop(gen, endLabel, startLabel);

            emitIR(gen->program, IR_LABEL, irOperandLabel(startLabel), irOperandNone(), irOperandNone());

            IROperand cond = generateExpression(gen, node->data.loop.condition);
            emitIR(gen->program, IR_JUMP_IF_FALSE, irOperandLabel(endLabel), cond, irOperandNone());

            generateStatement(gen, node->data.loop.body);

            emitIR(gen->program, IR_JUMP, irOperandLabel(startLabel), irOperandNone(), irOperandNone());
            emitIR(gen->program, IR_LABEL, irOperandLabel(endLabel), irOperandNone(), irOperandNone());

            popLoop(gen);
            free(startLabel);
            free(endLabel);
            break;
        }

        // --- For Loop ---
        // for (init; condition; increment) body
        //
        // Pattern:
        //   <init>
        // L_cond:
        //   cond = evaluate(condition)
        //   ifFalse cond goto L_end
        //   <body>
        // L_incr:
        //   <increment>
        //   goto L_cond
        // L_end:
        case NODE_FOR_STMT: {
            char *condLabel = newLabel(gen);
            char *incrLabel = newLabel(gen);
            char *endLabel = newLabel(gen);

            // Execute loop initialization
            if (node->data.loop.init) {
                generateStatement(gen, node->data.loop.init);
            }

            // In for loop, continue jumps to incrLabel (executes increment before condition),
            // break jumps to endLabel (exit loop).
            pushLoop(gen, endLabel, incrLabel);

            emitIR(gen->program, IR_LABEL, irOperandLabel(condLabel), irOperandNone(), irOperandNone());

            if (node->data.loop.condition) {
                IROperand cond = generateExpression(gen, node->data.loop.condition);
                emitIR(gen->program, IR_JUMP_IF_FALSE, irOperandLabel(endLabel), cond, irOperandNone());
            }

            generateStatement(gen, node->data.loop.body);

            // Increment step (continue target)
            emitIR(gen->program, IR_LABEL, irOperandLabel(incrLabel), irOperandNone(), irOperandNone());
            if (node->data.loop.increment) {
                IROperand incRes = generateExpression(gen, node->data.loop.increment);
                freeOperand(&incRes);
            }

            emitIR(gen->program, IR_JUMP, irOperandLabel(condLabel), irOperandNone(), irOperandNone());
            emitIR(gen->program, IR_LABEL, irOperandLabel(endLabel), irOperandNone(), irOperandNone());

            popLoop(gen);
            free(condLabel);
            free(incrLabel);
            free(endLabel);
            break;
        }

        // --- Jump Statements ---
        case NODE_RETURN_STMT: {
            if (node->data.returnStmt.expr) {
                IROperand val = generateExpression(gen, node->data.returnStmt.expr);
                emitIR(gen->program, IR_RETURN, irOperandNone(), val, irOperandNone());
            } else {
                emitIR(gen->program, IR_RETURN, irOperandNone(), irOperandNone(), irOperandNone());
            }
            break;
        }

        case NODE_BREAK_STMT: {
            if (gen->loopDepth > 0) {
                const char *target = gen->breakLabels[gen->loopDepth - 1];
                emitIR(gen->program, IR_JUMP, irOperandLabel(target), irOperandNone(), irOperandNone());
            } else {
                fprintf(stderr, "IR Error: 'break' outside of loop context\n");
            }
            break;
        }

        case NODE_CONTINUE_STMT: {
            if (gen->loopDepth > 0) {
                const char *target = gen->continueLabels[gen->loopDepth - 1];
                emitIR(gen->program, IR_JUMP, irOperandLabel(target), irOperandNone(), irOperandNone());
            } else {
                fprintf(stderr, "IR Error: 'continue' outside of loop context\n");
            }
            break;
        }

        // Standalone expressions used as statements
        case NODE_ASSIGNMENT:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_FUNC_CALL: {
            IROperand res = generateExpression(gen, node);
            freeOperand(&res);
            break;
        }

        default:
            break;
    }
}

IRProgram *generateIR(ASTNode *ast) {
    if (!ast) return NULL;

    IRGenerator *gen = createIRGenerator();
    generateStatement(gen, ast);

    IRProgram *program = gen->program;
    freeIRGenerator(gen);
    return program;
}

void printIR(const IRProgram *program) {
    if (!program || !program->head) {
        printf("(empty IR)\n");
        return;
    }

    IRInstruction *curr = program->head;
    while (curr) {
        switch (curr->opcode) {
            case IR_FUNC_START:
                printf("\nfunc begin %s\n", curr->dest.name ? curr->dest.name : "");
                break;

            case IR_FUNC_END:
                printf("func end %s\n", curr->dest.name ? curr->dest.name : "");
                break;

            case IR_LABEL:
                printf("%s:\n", curr->dest.name ? curr->dest.name : "");
                break;

            case IR_PARAM:
                printf("  param %s\n", curr->dest.name ? curr->dest.name : "");
                break;

            case IR_ARG:
                printf("  arg %s\n", curr->src1.name ? curr->src1.name : "");
                break;

            case IR_CALL:
                if (curr->dest.kind != OPERAND_NONE && curr->dest.name) {
                    printf("  %s = call %s, %s\n",
                           curr->dest.name,
                           curr->src1.name ? curr->src1.name : "",
                           curr->src2.name ? curr->src2.name : "0");
                } else {
                    printf("  call %s, %s\n",
                           curr->src1.name ? curr->src1.name : "",
                           curr->src2.name ? curr->src2.name : "0");
                }
                break;

            case IR_RETURN:
                if (curr->src1.kind != OPERAND_NONE && curr->src1.name) {
                    printf("  return %s\n", curr->src1.name);
                } else {
                    printf("  return\n");
                }
                break;

            case IR_JUMP:
                printf("  goto %s\n", curr->dest.name ? curr->dest.name : "");
                break;

            case IR_JUMP_IF:
                printf("  if %s goto %s\n",
                       curr->src1.name ? curr->src1.name : "",
                       curr->dest.name ? curr->dest.name : "");
                break;

            case IR_JUMP_IF_FALSE:
                printf("  ifFalse %s goto %s\n",
                       curr->src1.name ? curr->src1.name : "",
                       curr->dest.name ? curr->dest.name : "");
                break;

            case IR_ASSIGN:
                printf("  %s = %s\n",
                       curr->dest.name ? curr->dest.name : "",
                       curr->src1.name ? curr->src1.name : "");
                break;

            case IR_ADD:
                printf("  %s = %s + %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_SUB:
                printf("  %s = %s - %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_MUL:
                printf("  %s = %s * %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_DIV:
                printf("  %s = %s / %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_MOD:
                printf("  %s = %s %% %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;

            case IR_EQ:
                printf("  %s = %s == %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_NE:
                printf("  %s = %s != %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_LT:
                printf("  %s = %s < %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_LE:
                printf("  %s = %s <= %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_GT:
                printf("  %s = %s > %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_GE:
                printf("  %s = %s >= %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;

            case IR_AND:
                printf("  %s = %s && %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_OR:
                printf("  %s = %s || %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;

            case IR_NEG:
                printf("  %s = -%s\n", curr->dest.name, curr->src1.name);
                break;
            case IR_NOT:
                printf("  %s = !%s\n", curr->dest.name, curr->src1.name);
                break;
            case IR_BNOT:
                printf("  %s = ~%s\n", curr->dest.name, curr->src1.name);
                break;

            case IR_ADDR_OF:
                printf("  %s = &%s\n", curr->dest.name, curr->src1.name);
                break;
            case IR_DEREF_LOAD:
                printf("  %s = *%s\n", curr->dest.name, curr->src1.name);
                break;
            case IR_DEREF_STORE:
                printf("  *%s = %s\n", curr->dest.name, curr->src1.name);
                break;

            case IR_ARRAY_LOAD:
                printf("  %s = %s[%s]\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;
            case IR_ARRAY_STORE:
                printf("  %s[%s] = %s\n", curr->dest.name, curr->src1.name, curr->src2.name);
                break;

            case IR_ALLOC:
                printf("  alloc %s, %s\n", curr->dest.name, curr->src1.name);
                break;
        }
        curr = curr->next;
    }
}
