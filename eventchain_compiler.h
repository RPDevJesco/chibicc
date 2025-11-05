#ifndef EVENTCHAIN_COMPILER_H
#define EVENTCHAIN_COMPILER_H

/**
 * EventChain-based Compilation Orchestration
 * 
 * This module provides the main compilation function using the EventChains
 * pattern, mirroring the structure of expression_compiler's compile_expression().
 * 
 * The compilation pipeline consists of four stages:
 * 1. Tokenize - Convert source file to token stream
 * 2. Preprocess - Expand macros and process directives
 * 3. Parse - Build AST from tokens
 * 4. Codegen - Generate assembly from AST
 * 
 * Each stage is implemented as a ChainableEvent that communicates through
 * EventContext. Middleware (timing, logging, memory monitoring) wraps all
 * stages for observability.
 */

/**
 * Compile a C source file using EventChains pattern
 * 
 * This is the main compilation orchestration function that:
 * - Creates an EventChain
 * - Sets up the compilation context
 * - Registers all compilation stages as events
 * - Adds middleware for observability
 * - Executes the chain
 * - Writes the output
 * 
 * @param input_file Path to input C source file
 * @param output_file Path to output assembly file
 * @return 0 on success, non-zero on failure
 */
int compile_with_eventchains(const char *input_file, const char *output_file, bool preprocess_only);

/**
 * Get preprocessed tokens from the last compilation
 * Only valid after compile_with_eventchains() with preprocess_only=true
 */
Token *get_preprocessed_tokens(void);

/**
 * Cleanup the stored EventContext
 */
void cleanup_eventchain_context(void);

#endif /* EVENTCHAIN_COMPILER_H */
