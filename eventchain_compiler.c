/* EventChain Compiler - Minimal orchestration */

#include "chibicc.h"
#include "eventchain_compiler.h"
#include "src/eventchain/eventchains.h"
#include "events/tokenize_event.h"
#include "events/preprocess_event.h"
#include "events/parse_event.h"
#include "events/codegen_event.h"
#include "middleware/timing_middleware.h"
#include "middleware/logging_middleware.h"
#include "middleware/memory_monitor_middleware.h"
#include <stdio.h>

/* Global to store the last EventContext for retrieving tokens */
static EventContext *last_context = NULL;

int compile_with_eventchains(const char *input_file, const char *output_file, bool preprocess_only) {
    /* Create event chain */
    EventChain *chain = event_chain_create(FAULT_TOLERANCE_LENIENT);
    if (!chain) {
        fprintf(stderr, "Failed to create EventChain\n");
        return 1;
    }
    
    /* Set up context */
    EventContext *ctx = event_chain_get_context(chain);
    event_context_set(ctx, "input_file", (void *)input_file);
    event_context_set(ctx, "output_file", (void *)output_file);
    
    /* Store context globally for token retrieval */
    last_context = ctx;
    
    /* Create events */
    ChainableEvent *tokenize = chainable_event_create(tokenize_event_execute, NULL, "Tokenize");
    ChainableEvent *preprocess = chainable_event_create(preprocess_event_execute, NULL, "Preprocess");
    
    /* Add tokenize and preprocess (always needed) */
    event_chain_add_event(chain, tokenize);
    event_chain_add_event(chain, preprocess);
    
    /* Add parse and codegen only if not in preprocess-only mode */
    if (!preprocess_only) {
        ChainableEvent *parse = chainable_event_create(parse_event_execute, NULL, "Parse");
        ChainableEvent *codegen = chainable_event_create(codegen_event_execute, NULL, "Codegen");
        event_chain_add_event(chain, parse);
        event_chain_add_event(chain, codegen);
    }
    
    /* Add middleware */
    EventMiddleware *timing = event_middleware_create(timing_middleware, NULL, "Timing");
    EventMiddleware *logging = event_middleware_create(logging_middleware, NULL, "Logging");
    EventMiddleware *memory = event_middleware_create(memory_monitor_middleware, NULL, "Memory");
    
    event_chain_use_middleware(chain, memory);
    event_chain_use_middleware(chain, timing);
    event_chain_use_middleware(chain, logging);
    
    /* Execute */
    ChainResult result = event_chain_execute(chain);
    
    int ret = result.success ? 0 : 1;
    
    /* Don't cleanup chain yet if preprocess_only - we need to retrieve tokens */
    if (!preprocess_only) {
        chain_result_destroy(&result);
        event_chain_destroy(chain);
        last_context = NULL;
    } else {
        chain_result_destroy(&result);
        /* Keep chain alive for token retrieval, will be cleaned up later */
    }
    
    return ret;
}

/* Get preprocessed tokens from the last compilation */
Token *get_preprocessed_tokens(void) {
    if (!last_context) {
        return NULL;
    }
    
    void *tokens_ptr = NULL;
    if (event_context_get(last_context, "tokens", &tokens_ptr) != EC_SUCCESS) {
        return NULL;
    }
    
    return (Token *)tokens_ptr;
}

/* Cleanup the stored context */
void cleanup_eventchain_context(void) {
    /* Note: The actual chain cleanup happens elsewhere */
    last_context = NULL;
}
