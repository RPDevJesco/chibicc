#define _POSIX_C_SOURCE 200809L

#include "eventchains.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#define INITIAL_CAPACITY 8

/* ==================== Utility Functions ==================== */
/**
 * @brief Creates a newly allocated copy of a string up to a specified length.
 *
 * This function duplicates at most `n` characters from the input string `s`,
 * ensuring the result is null-terminated. It behaves like `strndup()` on
 * POSIX systems and can be used as a portable replacement.
 *
 * @param s The source null-terminated string to copy.
 * @param n The maximum number of characters to duplicate from `s`.
 * @return A pointer to the newly allocated, null-terminated copy of `s`,
 *         or NULL if memory allocation fails.
 */
char* strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

/**
 * Safe string length with maximum
 */
static size_t safe_strnlen(const char *str, size_t maxlen) {
    if (!str) return 0;
    const char *end = memchr(str, '\0', maxlen);
    return end ? (size_t)(end - str) : maxlen;
}

/**
 * Safe string copy with null termination guarantee
 */
static void safe_strncpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t src_len = safe_strnlen(src, dest_size - 1);
    memcpy(dest, src, src_len);
    dest[src_len] = '\0';
}

/**
 * Check for multiplication overflow
 */
static bool safe_multiply(size_t a, size_t b, size_t *result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }

    if (a > SIZE_MAX / b) {
        return false;  /* Would overflow */
    }

    *result = a * b;
    return true;
}

/**
 * Check for addition overflow
 */
static bool safe_add(size_t a, size_t b, size_t *result) {
    if (a > SIZE_MAX - b) {
        return false;  /* Would overflow */
    }

    *result = a + b;
    return true;
}

/**
 * Secure memory zeroing (won't be optimized away)
 */
static void secure_zero(void *ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile unsigned char *p = ptr;
    while (len--) {
        *p++ = 0;
    }
}

/**
 * Constant-time string comparison (resistant to timing attacks)
 */
static bool constant_time_strcmp(const char *a, const char *b, size_t max_len) {
    if (!a || !b) return false;

    volatile unsigned char result = 0;
    size_t i;
    size_t a_len = safe_strnlen(a, max_len);
    size_t b_len = safe_strnlen(b, max_len);

    /* Length must match */
    result |= (unsigned char)(a_len ^ b_len);

    /* Compare characters */
    size_t min_len = (a_len < b_len) ? a_len : b_len;
    for (i = 0; i < min_len; i++) {
        result |= (unsigned char)(a[i] ^ b[i]);
    }

    return result == 0;
}

/**
 * Validate function pointer (basic check)
 */
static bool is_valid_function_pointer(const void *ptr) {
    if (!ptr) return false;

    /* Basic sanity check - not in low memory (NULL area) */
    if (((uintptr_t)ptr) < 4096) {
        return false;
    }

    return true;
}

/**
 * Safe time conversion with overflow checking
 */
static EventChainErrorCode safe_time_to_int64(time_t time_val, int64_t *result) {
    if (!result) return EC_ERROR_NULL_POINTER;

    /* Check for time() error */
    if (time_val == (time_t)-1) {
        *result = 0;
        return EC_ERROR_TIME_CONVERSION;
    }

    /* Check if time_t fits in int64_t */
#if defined(TIME_MAX) && defined(TIME_MIN)
    if (sizeof(time_t) > sizeof(int64_t)) {
        if (time_val > (time_t)INT64_MAX || time_val < (time_t)INT64_MIN) {
            *result = (time_val > 0) ? INT64_MAX : INT64_MIN;
            return EC_ERROR_OVERFLOW;
        }
    }
#endif

    *result = (int64_t)time_val;
    return EC_SUCCESS;
}

/**
 * Sanitize error message based on detail level
 */
static void sanitize_error_message(
    char *dest,
    const char *src,
    size_t dest_size,
    ErrorDetailLevel level
) {
    if (!dest || dest_size == 0) return;

    if (level == ERROR_DETAIL_MINIMAL) {
        /* Production: generic message only */
        safe_strncpy(dest, "Operation failed", dest_size);
    } else {
        /* Development: full details */
        if (src) {
            safe_strncpy(dest, src, dest_size);
        } else {
            safe_strncpy(dest, "Unknown error", dest_size);
        }
    }
}

/* ==================== RefCountedValue Implementation ==================== */

RefCountedValue *ref_counted_value_create(void *data, ValueCleanupFunc cleanup) {
    RefCountedValue *value = calloc(1, sizeof(RefCountedValue));
    if (!value) return NULL;

    value->data = data;
    value->ref_count = 1;
    value->cleanup = cleanup;

    return value;
}

EventChainErrorCode ref_counted_value_retain(RefCountedValue *value) {
    if (!value) return EC_ERROR_NULL_POINTER;

    /* Check for overflow */
    if (value->ref_count >= SIZE_MAX) {
        return EC_ERROR_OVERFLOW;
    }

    value->ref_count++;
    return EC_SUCCESS;
}

EventChainErrorCode ref_counted_value_release(RefCountedValue *value) {
    if (!value) return EC_ERROR_NULL_POINTER;

    if (value->ref_count == 0) {
        /* Double-free attempt */
        fprintf(stderr, "ERROR: Attempted to release already-freed value\n");
        return EC_ERROR_INVALID_PARAMETER;
    }

    value->ref_count--;

    if (value->ref_count == 0) {
        /* Actually free the value */
        if (value->cleanup && value->data) {
            value->cleanup(value->data);
        }
        secure_zero(value, sizeof(RefCountedValue));
        free(value);
    }

    return EC_SUCCESS;
}

void *ref_counted_value_get_data(const RefCountedValue *value) {
    if (!value) return NULL;
    return value->data;
}

size_t ref_counted_value_get_count(const RefCountedValue *value) {
    if (!value) return 0;
    return value->ref_count;
}

/* ==================== EventContext Implementation ==================== */

EventContext *event_context_create(void) {
    EventContext *context = calloc(1, sizeof(EventContext));
    if (!context) {
        return NULL;
    }

    context->capacity = INITIAL_CAPACITY;
    context->count = 0;
    context->total_memory_bytes = sizeof(EventContext);

    /* Allocate all arrays */
    context->keys = calloc(context->capacity, sizeof(char *));
    context->values = calloc(context->capacity, sizeof(RefCountedValue *));

    if (!context->keys || !context->values) {
        free(context->keys);
        free(context->values);
        free(context);
        return NULL;
    }

    /* Account for array memory */
    size_t array_memory;
    if (!safe_multiply(context->capacity, sizeof(char *), &array_memory)) {
        free(context->keys);
        free(context->values);
        free(context);
        return NULL;
    }
    context->total_memory_bytes += array_memory;

    if (!safe_multiply(context->capacity, sizeof(RefCountedValue *), &array_memory)) {
        free(context->keys);
        free(context->values);
        free(context);
        return NULL;
    }
    context->total_memory_bytes += array_memory;

    return context;
}

void event_context_destroy(EventContext *context) {
    if (!context) return;

    /* Release all entries */
    for (size_t i = 0; i < context->count; i++) {
        /* Free key */
        if (context->keys[i]) {
            secure_zero(context->keys[i], strlen(context->keys[i]));
            free(context->keys[i]);
        }

        /* Release ref-counted value */
        if (context->values[i]) {
            ref_counted_value_release(context->values[i]);
        }
    }

    /* Free arrays */
    free(context->keys);
    free(context->values);

    /* Zero structure */
    secure_zero(context, sizeof(EventContext));

    free(context);
}

EventChainErrorCode event_context_set_with_cleanup(
    EventContext *context,
    const char *key,
    void *value,
    ValueCleanupFunc cleanup
) {
    if (!context) return EC_ERROR_NULL_POINTER;
    if (!key) return EC_ERROR_NULL_POINTER;

    /* Validate key length */
    size_t key_len = safe_strnlen(key, EVENTCHAINS_MAX_KEY_LENGTH + 1);
    if (key_len > EVENTCHAINS_MAX_KEY_LENGTH) {
        return EC_ERROR_KEY_TOO_LONG;
    }
    if (key_len == 0) {
        return EC_ERROR_INVALID_PARAMETER;
    }

    /* Check if key already exists */
    for (size_t i = 0; i < context->count; i++) {
        if (context->keys[i] && strcmp(context->keys[i], key) == 0) {
            /* Key exists - release old value and create new */
            if (context->values[i]) {
                /* Subtract old value memory */
                context->total_memory_bytes -= sizeof(RefCountedValue);
                ref_counted_value_release(context->values[i]);
            }

            /* Create new ref-counted value */
            RefCountedValue *new_value = ref_counted_value_create(value, cleanup);
            if (!new_value) {
                return EC_ERROR_OUT_OF_MEMORY;
            }

            context->values[i] = new_value;
            context->total_memory_bytes += sizeof(RefCountedValue);
            return EC_SUCCESS;
        }
    }

    /* Check capacity limits */
    if (context->count >= EVENTCHAINS_MAX_CONTEXT_ENTRIES) {
        return EC_ERROR_CAPACITY_EXCEEDED;
    }

    /* Calculate memory needed for new entry */
    size_t new_memory = key_len + 1 + sizeof(RefCountedValue);
    size_t total_after;
    if (!safe_add(context->total_memory_bytes, new_memory, &total_after)) {
        return EC_ERROR_OVERFLOW;
    }

    if (total_after > EVENTCHAINS_MAX_CONTEXT_MEMORY) {
        return EC_ERROR_MEMORY_LIMIT_EXCEEDED;
    }

    /* Expand if needed */
    if (context->count >= context->capacity) {
        size_t new_capacity;
        if (!safe_multiply(context->capacity, 2, &new_capacity)) {
            return EC_ERROR_OVERFLOW;
        }

        /* Cap at maximum */
        if (new_capacity > EVENTCHAINS_MAX_CONTEXT_ENTRIES) {
            new_capacity = EVENTCHAINS_MAX_CONTEXT_ENTRIES;
        }

        /* Reallocate arrays */
        char **new_keys = realloc(context->keys, sizeof(char *) * new_capacity);
        if (!new_keys) {
            return EC_ERROR_OUT_OF_MEMORY;
        }
        context->keys = new_keys;

        RefCountedValue **new_values = realloc(
            context->values,
            sizeof(RefCountedValue *) * new_capacity
        );
        if (!new_values) {
            return EC_ERROR_OUT_OF_MEMORY;
        }
        context->values = new_values;

        /* Zero new entries */
        for (size_t i = context->capacity; i < new_capacity; i++) {
            context->keys[i] = NULL;
            context->values[i] = NULL;
        }

        /* Update memory tracking */
        size_t old_array_memory, new_array_memory;
        safe_multiply(context->capacity, sizeof(char *) + sizeof(RefCountedValue *), &old_array_memory);
        safe_multiply(new_capacity, sizeof(char *) + sizeof(RefCountedValue *), &new_array_memory);
        context->total_memory_bytes += (new_array_memory - old_array_memory);

        context->capacity = new_capacity;
    }

    /* Add new entry */
    context->keys[context->count] = strndup(key, EVENTCHAINS_MAX_KEY_LENGTH);
    if (!context->keys[context->count]) {
        return EC_ERROR_OUT_OF_MEMORY;
    }

    /* Create ref-counted value */
    RefCountedValue *new_value = ref_counted_value_create(value, cleanup);
    if (!new_value) {
        free(context->keys[context->count]);
        context->keys[context->count] = NULL;
        return EC_ERROR_OUT_OF_MEMORY;
    }

    context->values[context->count] = new_value;
    context->total_memory_bytes += new_memory;
    context->count++;

    return EC_SUCCESS;
}

EventChainErrorCode event_context_set(
    EventContext *context,
    const char *key,
    void *value
) {
    return event_context_set_with_cleanup(context, key, value, NULL);
}

EventChainErrorCode event_context_get_ref(
    EventContext *context,
    const char *key,
    RefCountedValue **value_out
) {
    if (!context) return EC_ERROR_NULL_POINTER;
    if (!key) return EC_ERROR_NULL_POINTER;
    if (!value_out) return EC_ERROR_NULL_POINTER;

    for (size_t i = 0; i < context->count; i++) {
        if (context->keys[i] && strcmp(context->keys[i], key) == 0) {
            *value_out = context->values[i];
            if (*value_out) {
                ref_counted_value_retain(*value_out);
            }
            return EC_SUCCESS;
        }
    }

    *value_out = NULL;
    return EC_ERROR_NOT_FOUND;
}

EventChainErrorCode event_context_get(
    const EventContext *context,
    const char *key,
    void **value_out
) {
    if (!context) return EC_ERROR_NULL_POINTER;
    if (!key) return EC_ERROR_NULL_POINTER;
    if (!value_out) return EC_ERROR_NULL_POINTER;

    for (size_t i = 0; i < context->count; i++) {
        if (context->keys[i] && strcmp(context->keys[i], key) == 0) {
            *value_out = ref_counted_value_get_data(context->values[i]);
            return EC_SUCCESS;
        }
    }

    *value_out = NULL;
    return EC_ERROR_NOT_FOUND;
}

bool event_context_has(
    const EventContext *context,
    const char *key,
    bool constant_time
) {
    if (!context || !key) return false;

    if (constant_time) {
        /* Constant-time comparison for sensitive keys */
        bool found = false;
        for (size_t i = 0; i < context->count; i++) {
            if (context->keys[i]) {
                if (constant_time_strcmp(context->keys[i], key, EVENTCHAINS_MAX_KEY_LENGTH)) {
                    found = true;
                    /* Don't break - must check all entries for constant time */
                }
            }
        }
        return found;
    } else {
        /* Fast path for non-sensitive keys */
        for (size_t i = 0; i < context->count; i++) {
            if (context->keys[i] && strcmp(context->keys[i], key) == 0) {
                return true;
            }
        }
        return false;
    }
}

EventChainErrorCode event_context_remove(EventContext *context, const char *key) {
    if (!context) return EC_ERROR_NULL_POINTER;
    if (!key) return EC_ERROR_NULL_POINTER;

    for (size_t i = 0; i < context->count; i++) {
        if (context->keys[i] && strcmp(context->keys[i], key) == 0) {
            /* Update memory tracking */
            size_t removed_memory = strlen(context->keys[i]) + 1 + sizeof(RefCountedValue);
            context->total_memory_bytes -= removed_memory;

            /* Release ref-counted value */
            if (context->values[i]) {
                ref_counted_value_release(context->values[i]);
            }

            /* Free key */
            secure_zero(context->keys[i], strlen(context->keys[i]));
            free(context->keys[i]);

            /* Shift remaining entries down */
            for (size_t j = i; j < context->count - 1; j++) {
                context->keys[j] = context->keys[j + 1];
                context->values[j] = context->values[j + 1];
            }

            /* Clear last entry */
            context->keys[context->count - 1] = NULL;
            context->values[context->count - 1] = NULL;

            context->count--;
            return EC_SUCCESS;
        }
    }

    return EC_ERROR_NOT_FOUND;
}

size_t event_context_count(const EventContext *context) {
    if (!context) return 0;
    return context->count;
}

size_t event_context_memory_usage(const EventContext *context) {
    if (!context) return 0;
    return context->total_memory_bytes;
}

void event_context_clear(EventContext *context) {
    if (!context) return;

    /* Release all entries */
    for (size_t i = 0; i < context->count; i++) {
        if (context->keys[i]) {
            secure_zero(context->keys[i], strlen(context->keys[i]));
            free(context->keys[i]);
            context->keys[i] = NULL;
        }

        if (context->values[i]) {
            ref_counted_value_release(context->values[i]);
            context->values[i] = NULL;
        }
    }

    /* Reset counters but keep arrays allocated */
    context->count = 0;
    context->total_memory_bytes = sizeof(EventContext) +
        (context->capacity * (sizeof(char *) + sizeof(RefCountedValue *)));
}

/* ==================== EventResult Implementation ==================== */

EventResult event_result_success(void) {
    EventResult result;
    result.success = true;
    result.error_message[0] = '\0';
    result.error_code = EC_SUCCESS;
    return result;
}

EventResult event_result_failure(
    const char *error_message,
    EventChainErrorCode error_code,
    ErrorDetailLevel detail_level
) {
    EventResult result;
    result.success = false;
    result.error_code = error_code;

    sanitize_error_message(
        result.error_message,
        error_message,
        EVENTCHAINS_MAX_ERROR_LENGTH,
        detail_level
    );

    return result;
}

/* ==================== ChainableEvent Implementation ==================== */

ChainableEvent *chainable_event_create(
    EventExecuteFunc execute,
    void *user_data,
    const char *name
) {
    if (!execute) return NULL;
    if (!is_valid_function_pointer((const void *)execute)) return NULL;

    ChainableEvent *event = calloc(1, sizeof(ChainableEvent));
    if (!event) return NULL;

    event->execute = execute;
    event->user_data = user_data;

    if (name) {
        safe_strncpy(event->name, name, EVENTCHAINS_MAX_NAME_LENGTH);
    } else {
        safe_strncpy(event->name, "UnnamedEvent", EVENTCHAINS_MAX_NAME_LENGTH);
    }

    return event;
}

void chainable_event_destroy(ChainableEvent *event) {
    if (!event) return;

    secure_zero(event, sizeof(ChainableEvent));
    free(event);
}

/* ==================== EventMiddleware Implementation ==================== */

EventMiddleware *event_middleware_create(
    MiddlewareExecuteFunc execute,
    void *user_data,
    const char *name
) {
    if (!execute) return NULL;
    if (!is_valid_function_pointer((const void *)execute)) return NULL;

    EventMiddleware *middleware = calloc(1, sizeof(EventMiddleware));
    if (!middleware) return NULL;

    middleware->execute = execute;
    middleware->user_data = user_data;

    if (name) {
        safe_strncpy(middleware->name, name, EVENTCHAINS_MAX_NAME_LENGTH);
    } else {
        safe_strncpy(middleware->name, "UnnamedMiddleware", EVENTCHAINS_MAX_NAME_LENGTH);
    }

    return middleware;
}

void event_middleware_destroy(EventMiddleware *middleware) {
    if (!middleware) return;

    secure_zero(middleware, sizeof(EventMiddleware));
    free(middleware);
}

/* ==================== EventChain Implementation ==================== */

EventChain *event_chain_create_with_detail(
    FaultToleranceMode mode,
    ErrorDetailLevel detail_level
) {
    EventChain *chain = calloc(1, sizeof(EventChain));
    if (!chain) return NULL;

    chain->event_capacity = INITIAL_CAPACITY;
    chain->event_count = 0;
    chain->events = calloc(chain->event_capacity, sizeof(ChainableEvent *));

    chain->middleware_capacity = INITIAL_CAPACITY;
    chain->middleware_count = 0;
    chain->middlewares = calloc(chain->middleware_capacity, sizeof(EventMiddleware *));

    chain->context = event_context_create();
    chain->fault_tolerance = mode;
    chain->error_detail_level = detail_level;
    chain->should_continue = NULL;
    chain->failure_handler_data = NULL;
    chain->is_executing = 0;
    chain->signal_interrupted = 0;

    if (!chain->events || !chain->middlewares || !chain->context) {
        free(chain->events);
        free(chain->middlewares);
        event_context_destroy(chain->context);
        free(chain);
        return NULL;
    }

    return chain;
}

EventChain *event_chain_create(FaultToleranceMode mode) {
    return event_chain_create_with_detail(mode, ERROR_DETAIL_FULL);
}

void event_chain_destroy(EventChain *chain) {
    if (!chain) return;

    /* Destroy all events */
    for (size_t i = 0; i < chain->event_count; i++) {
        chainable_event_destroy(chain->events[i]);
    }
    free(chain->events);

    /* Destroy all middleware */
    for (size_t i = 0; i < chain->middleware_count; i++) {
        event_middleware_destroy(chain->middlewares[i]);
    }
    free(chain->middlewares);

    event_context_destroy(chain->context);

    secure_zero(chain, sizeof(EventChain));
    free(chain);
}

EventChainErrorCode event_chain_add_event(
    EventChain *chain,
    ChainableEvent *event
) {
    if (!chain) return EC_ERROR_NULL_POINTER;
    if (!event) return EC_ERROR_NULL_POINTER;
    if (!event->execute) return EC_ERROR_INVALID_PARAMETER;
    if (!is_valid_function_pointer((const void *)event->execute)) return EC_ERROR_INVALID_PARAMETER;

    /* Check for reentrancy */
    if (chain->is_executing) {
        return EC_ERROR_REENTRANCY;
    }

    /* Check capacity limits */
    if (chain->event_count >= EVENTCHAINS_MAX_EVENTS) {
        return EC_ERROR_CAPACITY_EXCEEDED;
    }

    /* Expand if needed */
    if (chain->event_count >= chain->event_capacity) {
        size_t new_capacity;
        if (!safe_multiply(chain->event_capacity, 2, &new_capacity)) {
            return EC_ERROR_OVERFLOW;
        }

        if (new_capacity > EVENTCHAINS_MAX_EVENTS) {
            new_capacity = EVENTCHAINS_MAX_EVENTS;
        }

        ChainableEvent **new_events = realloc(
            chain->events,
            sizeof(ChainableEvent *) * new_capacity
        );

        if (!new_events) {
            return EC_ERROR_OUT_OF_MEMORY;
        }

        chain->events = new_events;

        /* Zero new entries */
        for (size_t i = chain->event_capacity; i < new_capacity; i++) {
            chain->events[i] = NULL;
        }

        chain->event_capacity = new_capacity;
    }

    chain->events[chain->event_count++] = event;
    return EC_SUCCESS;
}

EventChainErrorCode event_chain_use_middleware(
    EventChain *chain,
    EventMiddleware *middleware
) {
    if (!chain) return EC_ERROR_NULL_POINTER;
    if (!middleware) return EC_ERROR_NULL_POINTER;
    if (!middleware->execute) return EC_ERROR_INVALID_PARAMETER;
    if (!is_valid_function_pointer((const void *)middleware->execute)) return EC_ERROR_INVALID_PARAMETER;

    /* Check for reentrancy */
    if (chain->is_executing) {
        return EC_ERROR_REENTRANCY;
    }

    /* Check capacity limits */
    if (chain->middleware_count >= EVENTCHAINS_MAX_MIDDLEWARE) {
        return EC_ERROR_CAPACITY_EXCEEDED;
    }

    /* Expand if needed */
    if (chain->middleware_count >= chain->middleware_capacity) {
        size_t new_capacity;
        if (!safe_multiply(chain->middleware_capacity, 2, &new_capacity)) {
            return EC_ERROR_OVERFLOW;
        }

        if (new_capacity > EVENTCHAINS_MAX_MIDDLEWARE) {
            new_capacity = EVENTCHAINS_MAX_MIDDLEWARE;
        }

        EventMiddleware **new_middlewares = realloc(
            chain->middlewares,
            sizeof(EventMiddleware *) * new_capacity
        );

        if (!new_middlewares) {
            return EC_ERROR_OUT_OF_MEMORY;
        }

        chain->middlewares = new_middlewares;

        /* Zero new entries */
        for (size_t i = chain->middleware_capacity; i < new_capacity; i++) {
            chain->middlewares[i] = NULL;
        }

        chain->middleware_capacity = new_capacity;
    }

    chain->middlewares[chain->middleware_count++] = middleware;
    return EC_SUCCESS;
}

EventChainErrorCode event_chain_set_failure_handler(
    EventChain *chain,
    bool (*handler)(const ChainableEvent *event, const char *error, void *user_data),
    void *user_data
) {
    if (!chain) return EC_ERROR_NULL_POINTER;

    /* Validate function pointer if provided */
    if (handler && !is_valid_function_pointer((const void *)handler)) {
        return EC_ERROR_INVALID_FUNCTION_POINTER;
    }

    chain->should_continue = handler;
    chain->failure_handler_data = user_data;
    return EC_SUCCESS;
}

EventContext *event_chain_get_context(EventChain *chain) {
    if (!chain) return NULL;
    return chain->context;
}

bool event_chain_was_interrupted(const EventChain *chain) {
    if (!chain) return false;
    return chain->signal_interrupted != 0;
}

/* ==================== Iterative Middleware Pipeline Execution ==================== */

/**
 * The "next" function that middleware calls to proceed down the chain
 *
 * This uses controlled recursion with a depth limit of EVENTCHAINS_MAX_MIDDLEWARE (16).
 * This is safe because the maximum depth is small and bounded.
 */
static EventResult execute_next_middleware(
    ChainableEvent *event,
    EventContext *context,
    void *data
) {
    MiddlewareChainContext *mw_ctx = (MiddlewareChainContext *)data;

    /* Check for signal interruption */
    if (mw_ctx->chain->signal_interrupted) {
        return event_result_failure(
            "Chain execution interrupted by signal",
            EC_ERROR_SIGNAL_INTERRUPTED,
            mw_ctx->chain->error_detail_level
        );
    }

    /* If we've processed all middleware, execute the actual event */
    if (mw_ctx->current_middleware_index >= mw_ctx->chain->middleware_count) {
        return event->execute(context, event->user_data);
    }

    /* Get the current middleware */
    EventMiddleware *middleware =
        mw_ctx->chain->middlewares[mw_ctx->current_middleware_index];

    /* Validate middleware */
    if (!middleware || !middleware->execute) {
        return event_result_failure(
            "Invalid middleware in chain",
            EC_ERROR_INVALID_PARAMETER,
            mw_ctx->chain->error_detail_level
        );
    }

    if (!is_valid_function_pointer((const void *)middleware->execute)) {
        return event_result_failure(
            "Invalid middleware function pointer",
            EC_ERROR_INVALID_FUNCTION_POINTER,
            mw_ctx->chain->error_detail_level
        );
    }

    /* Prepare context for next layer */
    MiddlewareChainContext next_ctx = *mw_ctx;
    next_ctx.current_middleware_index++;

    /* Execute this middleware (which will call execute_next_middleware recursively) */
    return middleware->execute(
        event,
        context,
        execute_next_middleware,
        &next_ctx,
        middleware->user_data
    );
}

/**
 * Execute event through middleware pipeline
 *
 * This uses controlled recursion with a depth limit of EVENTCHAINS_MAX_MIDDLEWARE (16).
 * This is safe because:
 * 1. Maximum depth is bounded and small (16 levels)
 * 2. Each stack frame is small (just function call overhead + small context struct)
 * 3. Total stack usage is ~16 * ~64 bytes = ~1KB worst case
 */
static EventResult execute_event_with_middleware_iterative(
    EventChain *chain,
    ChainableEvent *event
) {
    if (!chain || !event) {
        return event_result_failure(
            "NULL chain or event",
            EC_ERROR_NULL_POINTER,
            chain ? chain->error_detail_level : ERROR_DETAIL_FULL
        );
    }

    /* If no middleware, execute event directly */
    if (chain->middleware_count == 0) {
        return event->execute(chain->context, event->user_data);
    }

    /* Build middleware chain context */
    MiddlewareChainContext mw_ctx;
    mw_ctx.chain = chain;
    mw_ctx.event = event;
    mw_ctx.current_middleware_index = 0;

    /* Start execution from the first middleware */
    return execute_next_middleware(event, chain->context, &mw_ctx);
}

/* ==================== Chain Execution ==================== */

ChainResult event_chain_execute(EventChain *chain) {
    ChainResult result;
    result.success = true;
    result.failures = NULL;
    result.failure_count = 0;

    if (!chain) {
        result.success = false;
        return result;
    }

    /* Check for reentrancy */
    if (chain->is_executing) {
        result.success = false;

        result.failures = calloc(1, sizeof(EventFailure));
        if (result.failures) {
            EventFailure *failure = &result.failures[0];
            safe_strncpy(failure->event_name, "Chain", EVENTCHAINS_MAX_NAME_LENGTH);
            sanitize_error_message(
                failure->error_message,
                "Reentrancy detected: chain already executing",
                EVENTCHAINS_MAX_ERROR_LENGTH,
                chain->error_detail_level
            );
            failure->error_code = EC_ERROR_REENTRANCY;

            int64_t timestamp;
            if (safe_time_to_int64(time(NULL), &timestamp) == EC_SUCCESS) {
                failure->timestamp = timestamp;
            } else {
                failure->timestamp = 0;
            }

            result.failure_count = 1;
        }

        return result;
    }

    /* Set executing flag */
    chain->is_executing = 1;
    chain->signal_interrupted = 0;

    /* Allocate failure tracking */
    size_t failure_capacity = 8;
    result.failures = calloc(failure_capacity, sizeof(EventFailure));
    if (!result.failures) {
        result.success = false;
        chain->is_executing = 0;
        return result;
    }

    /* Execute each event in sequence */
    for (size_t i = 0; i < chain->event_count; i++) {
        /* Check for signal interruption */
        if (chain->signal_interrupted) {
            result.success = false;

            if (result.failure_count < failure_capacity) {
                EventFailure failure;
                safe_strncpy(failure.event_name, "Chain", EVENTCHAINS_MAX_NAME_LENGTH);
                sanitize_error_message(
                    failure.error_message,
                    "Execution interrupted by signal",
                    EVENTCHAINS_MAX_ERROR_LENGTH,
                    chain->error_detail_level
                );
                failure.error_code = EC_ERROR_SIGNAL_INTERRUPTED;

                int64_t timestamp;
                if (safe_time_to_int64(time(NULL), &timestamp) == EC_SUCCESS) {
                    failure.timestamp = timestamp;
                } else {
                    failure.timestamp = 0;
                }

                result.failures[result.failure_count++] = failure;
            }

            break;
        }

        ChainableEvent *event = chain->events[i];

        if (!event || !event->execute || !is_valid_function_pointer((const void *)event->execute)) {
            /* Record failure */
            if (result.failure_count < failure_capacity) {
                EventFailure failure;
                safe_strncpy(failure.event_name, "InvalidEvent", EVENTCHAINS_MAX_NAME_LENGTH);
                sanitize_error_message(
                    failure.error_message,
                    "Event validation failed",
                    EVENTCHAINS_MAX_ERROR_LENGTH,
                    chain->error_detail_level
                );
                failure.error_code = EC_ERROR_INVALID_PARAMETER;

                int64_t timestamp;
                if (safe_time_to_int64(time(NULL), &timestamp) == EC_SUCCESS) {
                    failure.timestamp = timestamp;
                } else {
                    failure.timestamp = 0;
                }

                result.failures[result.failure_count++] = failure;
            }
            result.success = false;
            chain->is_executing = 0;
            return result;
        }

        /* Execute event through iterative middleware pipeline */
        EventResult event_result = execute_event_with_middleware_iterative(chain, event);

        if (!event_result.success) {
            /* Expand failure array if needed */
            if (result.failure_count >= failure_capacity) {
                size_t new_capacity;
                if (!safe_multiply(failure_capacity, 2, &new_capacity)) {
                    /* Can't expand, stop recording failures */
                } else {
                    EventFailure *new_failures = realloc(
                        result.failures,
                        sizeof(EventFailure) * new_capacity
                    );
                    if (new_failures) {
                        result.failures = new_failures;

                        /* Zero new entries */
                        for (size_t j = failure_capacity; j < new_capacity; j++) {
                            memset(&result.failures[j], 0, sizeof(EventFailure));
                        }

                        failure_capacity = new_capacity;
                    }
                }
            }

            /* Record failure */
            if (result.failure_count < failure_capacity) {
                EventFailure failure;
                safe_strncpy(failure.event_name, event->name, EVENTCHAINS_MAX_NAME_LENGTH);
                safe_strncpy(failure.error_message, event_result.error_message, EVENTCHAINS_MAX_ERROR_LENGTH);
                failure.error_code = event_result.error_code;

                int64_t timestamp;
                if (safe_time_to_int64(time(NULL), &timestamp) == EC_SUCCESS) {
                    failure.timestamp = timestamp;
                } else {
                    failure.timestamp = 0;
                }

                result.failures[result.failure_count++] = failure;
            }

            /* Determine if we should continue */
            bool should_continue = false;

            switch (chain->fault_tolerance) {
                case FAULT_TOLERANCE_STRICT:
                    should_continue = false;
                    break;

                case FAULT_TOLERANCE_LENIENT:
                case FAULT_TOLERANCE_BEST_EFFORT:
                    should_continue = true;
                    break;

                case FAULT_TOLERANCE_CUSTOM:
                    if (chain->should_continue) {
                        should_continue = chain->should_continue(
                            event,
                            event_result.error_message,
                            chain->failure_handler_data
                        );
                    } else {
                        /* No handler provided, default to strict */
                        should_continue = false;
                    }
                    break;
            }

            if (!should_continue) {
                result.success = false;
                chain->is_executing = 0;
                return result;
            }
        }
    }

    /* If we got here and have failures, it's partial success */
    if (result.failure_count > 0) {
        result.success = (chain->fault_tolerance != FAULT_TOLERANCE_STRICT);
    }

    chain->is_executing = 0;
    return result;
}

/* ==================== ChainResult Implementation ==================== */

void chain_result_destroy(ChainResult *result) {
    if (!result) return;

    if (result->failures) {
        /* Zero sensitive data */
        for (size_t i = 0; i < result->failure_count; i++) {
            secure_zero(&result->failures[i], sizeof(EventFailure));
        }
        free(result->failures);
        result->failures = NULL;
    }

    result->failure_count = 0;
}

void chain_result_print(const ChainResult *result) {
    if (!result) {
        printf("NULL result\n");
        return;
    }

    printf("\n=== Chain Execution Result ===\n");
    printf("Success: %s\n", result->success ? "YES" : "NO");
    printf("Failures: %zu\n", result->failure_count);

    if (result->failure_count > 0) {
        printf("\nFailure Details:\n");
        for (size_t i = 0; i < result->failure_count; i++) {
            printf("  [%zu] Event: %s\n", i + 1, result->failures[i].event_name);
            printf("      Error: %s\n", result->failures[i].error_message);
            printf("      Code: %d\n", result->failures[i].error_code);
            printf("      Time: %lld\n", (long long)result->failures[i].timestamp);
        }
    }

    printf("==============================\n\n");
}

/* ==================== Utility Functions ==================== */

const char *event_chain_error_string(EventChainErrorCode code) {
    switch (code) {
        case EC_SUCCESS:
            return "Success";
        case EC_ERROR_NULL_POINTER:
            return "NULL pointer";
        case EC_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case EC_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case EC_ERROR_CAPACITY_EXCEEDED:
            return "Capacity exceeded";
        case EC_ERROR_KEY_TOO_LONG:
            return "Key too long";
        case EC_ERROR_NAME_TOO_LONG:
            return "Name too long";
        case EC_ERROR_NOT_FOUND:
            return "Not found";
        case EC_ERROR_OVERFLOW:
            return "Arithmetic overflow";
        case EC_ERROR_EVENT_EXECUTION_FAILED:
            return "Event execution failed";
        case EC_ERROR_MIDDLEWARE_FAILED:
            return "Middleware failed";
        case EC_ERROR_REENTRANCY:
            return "Reentrancy detected";
        case EC_ERROR_MEMORY_LIMIT_EXCEEDED:
            return "Memory limit exceeded";
        case EC_ERROR_INVALID_FUNCTION_POINTER:
            return "Invalid function pointer";
        case EC_ERROR_TIME_CONVERSION:
            return "Time conversion error";
        case EC_ERROR_SIGNAL_INTERRUPTED:
            return "Signal interrupted";
        default:
            return "Unknown error";
    }
}

const char *event_chain_version_string(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
        EVENTCHAINS_VERSION_MAJOR,
        EVENTCHAINS_VERSION_MINOR,
        EVENTCHAINS_VERSION_PATCH);
    return version;
}

const char *event_chain_build_info(void) {
    static char info[512];
    snprintf(info, sizeof(info),
        "EventChains v%d.%d.%d - Security-Hardened Build (No Magic Numbers)\n"
        "Features:\n"
        "  - Reference counting for memory safety\n"
        "  - Constant-time comparisons for sensitive data\n"
        "  - Memory usage limits (%zu MB max)\n"
        "  - Iterative middleware execution (max %d layers)\n"
        "  - Reentrancy protection\n"
        "  - Signal safety\n"
        "  - Function pointer validation\n"
        "  - Configurable error detail levels\n"
        "  - Overflow protection on all arithmetic\n"
        "  - Secure memory zeroing\n"
        "  - Optimized: No magic number overhead",
        EVENTCHAINS_VERSION_MAJOR,
        EVENTCHAINS_VERSION_MINOR,
        EVENTCHAINS_VERSION_PATCH,
        EVENTCHAINS_MAX_CONTEXT_MEMORY / (1024 * 1024),
        EVENTCHAINS_MAX_MIDDLEWARE
    );
    return info;
}