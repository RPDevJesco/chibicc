#ifndef CHIBICC_EVENTS_H
#define CHIBICC_EVENTS_H

#include "src/eventchain/eventchains.h"

/**
 * ChiBiCC Compilation Events - EventChains Integration
 * 
 * This file provides wrapper events for chibicc's compilation stages,
 * allowing them to be orchestrated through the EventChains pattern.
 */

/**
 * Tokenize Event - Converts source file to tokens
 * 
 * Input (from context):
 *   - "input_file" (char*): Path to the input file
 * 
 * Output (to context):
 *   - "tokens" (Token*): Linked list of tokens
 */
EventResult tokenize_event_execute(EventContext *context, void *user_data);

/**
 * Preprocess Event - Expands macros and processes directives
 * 
 * Input (from context):
 *   - "tokens" (Token*): Input tokens
 * 
 * Output (to context):
 *   - "preprocessed_tokens" (Token*): Preprocessed tokens
 */
EventResult preprocess_event_execute(EventContext *context, void *user_data);

/**
 * Parse Event - Constructs AST from tokens
 * 
 * Input (from context):
 *   - "preprocessed_tokens" (Token*): Preprocessed tokens
 * 
 * Output (to context):
 *   - "ast" (Obj*): Abstract syntax tree (program)
 */
EventResult parse_event_execute(EventContext *context, void *user_data);

/**
 * Codegen Event - Generates assembly from AST
 * 
 * Input (from context):
 *   - "ast" (Obj*): Abstract syntax tree
 *   - "output_file" (char*): Path to output file
 * 
 * Output (to context):
 *   - "assembly" (char*): Generated assembly code
 */
EventResult codegen_event_execute(EventContext *context, void *user_data);

#endif /* CHIBICC_EVENTS_H */
