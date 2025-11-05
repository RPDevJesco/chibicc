/* ==================== MIDDLEWARE: Memory Monitor ==================== */

#include <stdio.h>

#include "../src/eventchain/eventchains.h"

EventResult memory_monitor_middleware(
    ChainableEvent *event,
    EventContext *context,
    MiddlewareNextFunc next,
    void *next_data,
    void *user_data
) {
    (void)user_data;

    size_t before = event_context_memory_usage(context);

    EventResult result = next(event, context, next_data);

    size_t after = event_context_memory_usage(context);
    long delta = (long)after - (long)before;

    printf("[MemoryMonitor] %s: %+ld bytes (total: %zu bytes)\n",
           event->name, delta, after);

    return result;
}
