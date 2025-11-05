#include "chibicc.h"
#include "chibicc_events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Tokenize Event Implementation
 */
EventResult tokenize_event_execute(EventContext *context, void *user_data) {
    (void)user_data;
    
    void *input_file_ptr = NULL;
    EventChainErrorCode err = event_context_get(context, "input_file", &input_file_ptr);
    
    if (err != EC_SUCCESS || !input_file_ptr) {
        return event_result_failure(
            "Tokenize: input_file not found in context",
            EC_ERROR_NOT_FOUND,
            ERROR_DETAIL_FULL
        );
    }
    
    char *input_file = (char *)input_file_ptr;
    
    Token *tok = tokenize_file(input_file);
    if (!tok) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Tokenize: failed to tokenize file: %s", input_file);
        return event_result_failure(error_msg, EC_ERROR_EVENT_EXECUTION_FAILED, ERROR_DETAIL_FULL);
    }
    
    err = event_context_set(context, "tokens", tok);
    if (err != EC_SUCCESS) {
        return event_result_failure(
            "Tokenize: failed to store tokens in context",
            err,
            ERROR_DETAIL_FULL
        );
    }
    
    return event_result_success();
}

/**
 * Preprocess Event Implementation
 */
EventResult preprocess_event_execute(EventContext *context, void *user_data) {
    (void)user_data;
    
    void *tokens_ptr = NULL;
    EventChainErrorCode err = event_context_get(context, "tokens", &tokens_ptr);
    
    if (err != EC_SUCCESS || !tokens_ptr) {
        return event_result_failure(
            "Preprocess: tokens not found in context",
            EC_ERROR_NOT_FOUND,
            ERROR_DETAIL_FULL
        );
    }
    
    Token *tok = (Token *)tokens_ptr;
    Token *preprocessed = preprocess(tok);
    
    if (!preprocessed) {
        return event_result_failure(
            "Preprocess: preprocessing failed",
            EC_ERROR_EVENT_EXECUTION_FAILED,
            ERROR_DETAIL_FULL
        );
    }
    
    err = event_context_set(context, "preprocessed_tokens", preprocessed);
    if (err != EC_SUCCESS) {
        return event_result_failure(
            "Preprocess: failed to store preprocessed tokens in context",
            err,
            ERROR_DETAIL_FULL
        );
    }
    
    return event_result_success();
}

/**
 * Parse Event Implementation
 */
EventResult parse_event_execute(EventContext *context, void *user_data) {
    (void)user_data;
    
    void *tokens_ptr = NULL;
    EventChainErrorCode err = event_context_get(context, "preprocessed_tokens", &tokens_ptr);
    
    if (err != EC_SUCCESS || !tokens_ptr) {
        return event_result_failure(
            "Parse: preprocessed_tokens not found in context",
            EC_ERROR_NOT_FOUND,
            ERROR_DETAIL_FULL
        );
    }
    
    Token *tok = (Token *)tokens_ptr;
    Obj *prog = parse(tok);
    
    if (!prog) {
        return event_result_failure(
            "Parse: parsing failed",
            EC_ERROR_EVENT_EXECUTION_FAILED,
            ERROR_DETAIL_FULL
        );
    }
    
    err = event_context_set(context, "ast", prog);
    if (err != EC_SUCCESS) {
        return event_result_failure(
            "Parse: failed to store AST in context",
            err,
            ERROR_DETAIL_FULL
        );
    }
    
    return event_result_success();
}

/**
 * Codegen Event Implementation
 */
EventResult codegen_event_execute(EventContext *context, void *user_data) {
    (void)user_data;
    
    void *ast_ptr = NULL;
    EventChainErrorCode err = event_context_get(context, "ast", &ast_ptr);
    
    if (err != EC_SUCCESS || !ast_ptr) {
        return event_result_failure(
            "Codegen: AST not found in context",
            EC_ERROR_NOT_FOUND,
            ERROR_DETAIL_FULL
        );
    }
    
    Obj *prog = (Obj *)ast_ptr;
    
    char *buf = NULL;
    size_t buflen = 0;
    FILE *output_buf = open_memstream(&buf, &buflen);
    
    if (!output_buf) {
        return event_result_failure(
            "Codegen: failed to create output buffer",
            EC_ERROR_OUT_OF_MEMORY,
            ERROR_DETAIL_FULL
        );
    }
    
    codegen(prog, output_buf);
    fclose(output_buf);
    
    if (!buf) {
        return event_result_failure(
            "Codegen: code generation produced no output",
            EC_ERROR_EVENT_EXECUTION_FAILED,
            ERROR_DETAIL_FULL
        );
    }
    
    err = event_context_set(context, "assembly", buf);
    if (err != EC_SUCCESS) {
        free(buf);
        return event_result_failure(
            "Codegen: failed to store assembly in context",
            err,
            ERROR_DETAIL_FULL
        );
    }
    
    size_t *buflen_ptr = malloc(sizeof(size_t));
    if (buflen_ptr) {
        *buflen_ptr = buflen;
        event_context_set(context, "assembly_length", buflen_ptr);
    }
    
    return event_result_success();
}
