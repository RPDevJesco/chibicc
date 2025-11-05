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

    printf("[Logging] === Entering: %s ===\n", event->name);
    printf("[Logging] Context entries: %zu\n", event_context_count(context));

    EventResult result = next(event, context, next_data);

    if (result.success) {
        printf("[Logging] === Completed: %s (SUCCESS) ===\n", event->name);
    } else {
        printf("[Logging] === Completed: %s (FAILED: %s) ===\n",
               event->name, result.error_message);
    }

    return result;
}
