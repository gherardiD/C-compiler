# C-compiler

Compiler C is a modular compiler written in C, designed as a learning and experimentation project for understanding how real-world compilers work — from lexical analysis to code generation.

Overview

The project aims to implement a complete compilation pipeline for a simple C-like language:
	1.	Lexical Analysis (Lexer) – Converts the raw source code into a stream of tokens such as keywords, identifiers, literals, operators, and punctuators.
	2.	Syntax Analysis (Parser) – Builds an Abstract Syntax Tree (AST) from the token stream, checking that the program follows the grammar rules.
	3.	Semantic Analysis – Validates types, variable declarations, and scope rules.
	4.	Intermediate Code Generation – Translates the AST into an intermediate representation (IR) suitable for optimization and code generation.
	5.	Code Optimization – (Planned) Performs basic optimizations such as constant folding and dead-code elimination.
	6.	Target Code Generation – (Planned) Produces executable assembly or machine code.

⸻

Current Status

Lexer – Functional
  •	Supports keywords, identifiers, integers, operators, and punctuators
  •	Handles whitespace and delimiters gracefully
  •	Outputs a structured list of tokens
