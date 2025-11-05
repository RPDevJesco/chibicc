/* Tokenize Event - Thin wrapper around tokenize_file() */

#include "../chibicc.h"
#include "../src/eventchain/eventchains.h"
#include "tokenize_event.h"

/* Helper to append tokens */
static Token *append_tokens(Token *tok1, Token *tok2) {
    if (!tok1 || tok1->kind == TK_EOF)
        return tok2;
    
    Token *t = tok1;
    while (t->next->kind != TK_EOF)
        t = t->next;
    t->next = tok2;
    return tok1;
}

EventResult tokenize_event_execute(EventContext *context, void *user_data) {
    (void)user_data;
    
    /* Get input file from context */
    void *input_file_ptr = NULL;
    if (event_context_get(context, "input_file", &input_file_ptr) != EC_SUCCESS || !input_file_ptr) {
        return event_result_failure("No input file", EC_ERROR_NOT_FOUND, ERROR_DETAIL_FULL);
    }
    
    char *input_file = (char *)input_file_ptr;
    
    /* Process -include files first (access global opt_include) */
    extern StringArray opt_include;
    Token *tok = NULL;
    
    for (int i = 0; i < opt_include.len; i++) {
        char *incl = opt_include.data[i];
        char *path;
        
        if (file_exists(incl)) {
            path = incl;
        } else {
            path = search_include_paths(incl);
            if (!path) {
                return event_result_failure("Cannot find -include file", EC_ERROR_NOT_FOUND, ERROR_DETAIL_FULL);
            }
        }
        
        Token *tok2 = tokenize_file(path);
        if (!tok2) {
            return event_result_failure("Failed to tokenize -include file", EC_ERROR_EVENT_EXECUTION_FAILED, ERROR_DETAIL_FULL);
        }
        tok = append_tokens(tok, tok2);
    }
    
    /* Tokenize main input file */
    Token *tok2 = tokenize_file(input_file);
    if (!tok2) {
        return event_result_failure("Tokenization failed", EC_ERROR_EVENT_EXECUTION_FAILED, ERROR_DETAIL_FULL);
    }
    tok = append_tokens(tok, tok2);
    
    /* Store result in context */
    if (event_context_set(context, "tokens", tok) != EC_SUCCESS) {
        return event_result_failure("Failed to store tokens", EC_ERROR_EVENT_EXECUTION_FAILED, ERROR_DETAIL_FULL);
    }
    
    return event_result_success();
}
