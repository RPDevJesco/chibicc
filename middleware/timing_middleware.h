/* ==================== MIDDLEWARE: Timing ==================== */

#include <stdio.h>
#include <time.h>

#include "../src/eventchain/eventchains.h"

EventResult timing_middleware(
    ChainableEvent *event,
    EventContext *context,
    MiddlewareNextFunc next,
    void *next_data,
    void *user_data
) {
    (void)user_data;

    clock_t start = clock();

    /* Execute the wrapped event */
    EventResult result = next(event, context, next_data);

    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

    printf("[Timing] %s took %.3f ms\n", event->name, elapsed);

    return result;
}
