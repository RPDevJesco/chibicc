/* ==================== MIDDLEWARE: Logging ==================== */

#include <stdio.h>

#include "../src/eventchain/eventchains.h"

EventResult logging_middleware(
    ChainableEvent *event,
    EventContext *context,
    MiddlewareNextFunc next,
    void *next_data,
    void *user_data
) {
    (void)user_data;

    fprintf(stderr, "[Logging] === Entering: %s ===\n", event->name);
    fprintf(stderr, "[Logging] Context entries: %zu\n", event_context_count(context));

    EventResult result = next(event, context, next_data);

    if (result.success) {
        fprintf(stderr, "[Logging] === Completed: %s (SUCCESS) ===\n", event->name);
    } else {
        fprintf(stderr, "[Logging] === Completed: %s (FAILED: %s) ===\n",
               event->name, result.error_message);
    }

    return result;
}
