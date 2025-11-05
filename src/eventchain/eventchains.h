//
// EventChains - Security-Hardened C99 Implementation (No Magic Numbers)
// Created by Jesse Glover on 10/15/25.
// Enhanced with comprehensive security mitigations
//

#ifndef EVENTCHAINS_HARDENED_H
#define EVENTCHAINS_HARDENED_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>

/**
 * EventChains Design Pattern - Security-Hardened C99 Implementation
 *
 * This implementation protects against:
 * - Memory safety issues (buffer overflows, use-after-free, double-free)
 * - Integer overflows in all arithmetic operations
 * - Resource exhaustion (memory, stack depth)
 * - Denial of service attacks
 * - Timing attacks (when handling sensitive data)
 * - Format string vulnerabilities
 * - Signal safety issues
 * - Reentrancy problems
 * - Side-channel information leakage
 * - Function pointer corruption
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define EVENTCHAINS_VERSION_MAJOR 3
#define EVENTCHAINS_VERSION_MINOR 1
#define EVENTCHAINS_VERSION_PATCH 0

/* Security-focused configuration limits */
#ifndef EVENTCHAINS_MAX_EVENTS
#define EVENTCHAINS_MAX_EVENTS 1024
#endif

#ifndef EVENTCHAINS_MAX_MIDDLEWARE
#define EVENTCHAINS_MAX_MIDDLEWARE 16  /* Reduced from 64 to prevent deep recursion */
#endif

#ifndef EVENTCHAINS_MAX_CONTEXT_ENTRIES
#define EVENTCHAINS_MAX_CONTEXT_ENTRIES 512
#endif

#ifndef EVENTCHAINS_MAX_CONTEXT_MEMORY
#define EVENTCHAINS_MAX_CONTEXT_MEMORY (10 * 1024 * 1024)  /* 10MB total context memory */
#endif

#ifndef EVENTCHAINS_MAX_KEY_LENGTH
#define EVENTCHAINS_MAX_KEY_LENGTH 256
#endif

#ifndef EVENTCHAINS_MAX_NAME_LENGTH
#define EVENTCHAINS_MAX_NAME_LENGTH 256
#endif

#ifndef EVENTCHAINS_MAX_ERROR_LENGTH
#define EVENTCHAINS_MAX_ERROR_LENGTH 1024
#endif

/* Forward declarations */
typedef struct EventContext EventContext;
typedef struct EventResult EventResult;
typedef struct ChainableEvent ChainableEvent;
typedef struct EventMiddleware EventMiddleware;
typedef struct EventChain EventChain;
typedef struct ChainResult ChainResult;
typedef struct RefCountedValue RefCountedValue;

/**
 * Error codes for operations
 */
typedef enum {
    EC_SUCCESS = 0,
    EC_ERROR_NULL_POINTER,
    EC_ERROR_INVALID_PARAMETER,
    EC_ERROR_OUT_OF_MEMORY,
    EC_ERROR_CAPACITY_EXCEEDED,
    EC_ERROR_KEY_TOO_LONG,
    EC_ERROR_NAME_TOO_LONG,
    EC_ERROR_NOT_FOUND,
    EC_ERROR_OVERFLOW,
    EC_ERROR_EVENT_EXECUTION_FAILED,
    EC_ERROR_MIDDLEWARE_FAILED,
    EC_ERROR_REENTRANCY,
    EC_ERROR_MEMORY_LIMIT_EXCEEDED,
    EC_ERROR_INVALID_FUNCTION_POINTER,
    EC_ERROR_TIME_CONVERSION,
    EC_ERROR_SIGNAL_INTERRUPTED
} EventChainErrorCode;

/**
 * FaultToleranceMode - Defines how the chain handles failures
 */
typedef enum {
    FAULT_TOLERANCE_STRICT,      /* Any failure stops the chain */
    FAULT_TOLERANCE_LENIENT,     /* Non-critical failures continue */
    FAULT_TOLERANCE_BEST_EFFORT, /* All events attempted */
    FAULT_TOLERANCE_CUSTOM       /* User-defined failure handling */
} FaultToleranceMode;

/**
 * ErrorDetailLevel - Controls how much information is in error messages
 */
typedef enum {
    ERROR_DETAIL_FULL,      /* Development: detailed error messages */
    ERROR_DETAIL_MINIMAL    /* Production: sanitized generic messages */
} ErrorDetailLevel;

/**
 * ValueCleanupFunc - Callback to clean up context values
 *
 * When set, this function is called to free value memory.
 * If NULL, the value is not freed (caller retains ownership).
 */
typedef void (*ValueCleanupFunc)(void *value);

/**
 * RefCountedValue - Reference-counted wrapper for context values
 * Prevents use-after-free when values are shared
 */
struct RefCountedValue {
    void *data;                 /* The actual data */
    size_t ref_count;           /* Reference count */
    ValueCleanupFunc cleanup;   /* Cleanup function */
};

/**
 * EventContext - Shared state container with proper ownership and limits
 *
 * Thread-safety: NOT thread-safe. External synchronization required.
 */
struct EventContext {
    char **keys;                /* Array of string keys (owned) */
    RefCountedValue **values;   /* Array of ref-counted values */
    size_t count;               /* Number of entries */
    size_t capacity;            /* Allocated capacity */
    size_t total_memory_bytes;  /* Total memory used (for limits) */
};

/**
 * EventResult - Represents the outcome of an event execution
 */
struct EventResult {
    bool success;
    char error_message[EVENTCHAINS_MAX_ERROR_LENGTH];
    EventChainErrorCode error_code;
};

/**
 * EventExecuteFunc - Function signature for event execution
 *
 * @param context - The event context (read/write)
 * @param user_data - Event-specific data
 * @return EventResult indicating success or failure
 *
 * Thread-safety: Implementation must be thread-safe if chain is shared
 */
typedef EventResult (*EventExecuteFunc)(EventContext *context, void *user_data);

/**
 * ChainableEvent - A unit of work in the workflow
 */
struct ChainableEvent {
    EventExecuteFunc execute;
    void *user_data;          /* Event-specific data (not owned) */
    char name[EVENTCHAINS_MAX_NAME_LENGTH];
};

/**
 * MiddlewareNextFunc - Function to call the next middleware or event
 */
typedef EventResult (*MiddlewareNextFunc)(
    ChainableEvent *event,
    EventContext *context,
    void *middleware_chain_data
);

/**
 * Context for building middleware chain
 */
typedef struct MiddlewareChainContext {
    EventChain *chain;
    ChainableEvent *event;
    size_t current_middleware_index;
} MiddlewareChainContext;

/**
 * MiddlewareExecuteFunc - Function signature for middleware execution
 *
 * @param event - The event being executed
 * @param context - The event context
 * @param next - Function to call the next layer
 * @param next_data - Data for the next layer
 * @param user_data - Middleware-specific data
 * @return EventResult from the wrapped execution
 */
typedef EventResult (*MiddlewareExecuteFunc)(
    ChainableEvent *event,
    EventContext *context,
    MiddlewareNextFunc next,
    void *next_data,
    void *user_data
);

/**
 * EventMiddleware - Wraps event execution with cross-cutting concerns
 */
struct EventMiddleware {
    MiddlewareExecuteFunc execute;
    void *user_data;          /* Middleware-specific data (not owned) */
    char name[EVENTCHAINS_MAX_NAME_LENGTH];
};

/**
 * EventChain - Orchestrates execution of events through middleware
 *
 * Thread-safety: NOT thread-safe. Do not share across threads.
 */
struct EventChain {
    ChainableEvent **events;
    size_t event_count;
    size_t event_capacity;

    EventMiddleware **middlewares;
    size_t middleware_count;
    size_t middleware_capacity;

    EventContext *context;
    FaultToleranceMode fault_tolerance;
    ErrorDetailLevel error_detail_level;

    /* Custom failure handler for CUSTOM mode */
    bool (*should_continue)(
        const ChainableEvent *event,
        const char *error,
        void *user_data
    );
    void *failure_handler_data;

    /* Reentrancy and signal safety */
    volatile sig_atomic_t is_executing;
    volatile sig_atomic_t signal_interrupted;
};

/**
 * EventFailure - Records a single event failure
 */
typedef struct {
    char event_name[EVENTCHAINS_MAX_NAME_LENGTH];
    char error_message[EVENTCHAINS_MAX_ERROR_LENGTH];
    EventChainErrorCode error_code;
    int64_t timestamp;        /* Unix timestamp */
} EventFailure;

/**
 * ChainResult - Final result of chain execution
 */
struct ChainResult {
    bool success;
    EventFailure *failures;   /* Array of failures (owned) */
    size_t failure_count;
};

/* ==================== RefCountedValue Functions ==================== */

/**
 * Create a new reference-counted value
 *
 * @param data - The data to wrap
 * @param cleanup - Cleanup function (or NULL)
 * @return Pointer to ref-counted value, or NULL on failure
 *
 * Thread-safety: Safe to call from any thread
 */
RefCountedValue *ref_counted_value_create(void *data, ValueCleanupFunc cleanup);

/**
 * Increment reference count
 *
 * @param value - The ref-counted value
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: NOT thread-safe. Caller must synchronize.
 */
EventChainErrorCode ref_counted_value_retain(RefCountedValue *value);

/**
 * Decrement reference count and free if zero
 *
 * @param value - The ref-counted value
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: NOT thread-safe. Caller must synchronize.
 */
EventChainErrorCode ref_counted_value_release(RefCountedValue *value);

/**
 * Get the data from a ref-counted value
 *
 * @param value - The ref-counted value
 * @return The wrapped data, or NULL if invalid
 *
 * Thread-safety: Safe if no concurrent modifications
 */
void *ref_counted_value_get_data(const RefCountedValue *value);

/**
 * Get current reference count
 *
 * @param value - The ref-counted value
 * @return Reference count, or 0 if invalid
 *
 * Thread-safety: Safe if no concurrent modifications
 */
size_t ref_counted_value_get_count(const RefCountedValue *value);

/* ==================== EventContext Functions ==================== */

/**
 * Create a new EventContext
 *
 * @return Pointer to new context, or NULL on failure
 *
 * Thread-safety: Safe to call from any thread
 */
EventContext *event_context_create(void);

/**
 * Destroy an EventContext and free its memory
 *
 * This will release references to all values.
 *
 * @param context - Context to destroy (may be NULL)
 *
 * Thread-safety: Not thread-safe. Caller must ensure exclusive access.
 */
void event_context_destroy(EventContext *context);

/**
 * Set a value in the context with ownership transfer (ref-counted)
 *
 * @param context - The context
 * @param key - Key name (copied, max 256 chars)
 * @param value - Value pointer
 * @param cleanup - Function to free value, or NULL if caller retains ownership
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe. Caller must synchronize.
 */
EventChainErrorCode event_context_set_with_cleanup(
    EventContext *context,
    const char *key,
    void *value,
    ValueCleanupFunc cleanup
);

/**
 * Set a value in the context (caller retains ownership)
 *
 * @param context - The context
 * @param key - Key name (copied, max 256 chars)
 * @param value - Value pointer (not owned by context)
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe. Caller must synchronize.
 */
EventChainErrorCode event_context_set(
    EventContext *context,
    const char *key,
    void *value
);

/**
 * Get a value from the context (increments ref count)
 *
 * Caller must call ref_counted_value_release() when done.
 *
 * @param context - The context
 * @param key - Key name
 * @param value_out - Output pointer for the ref-counted value
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe for writes. Multiple readers OK.
 */
EventChainErrorCode event_context_get_ref(
    EventContext *context,
    const char *key,
    RefCountedValue **value_out
);

/**
 * Get a value from the context (without incrementing ref count)
 *
 * WARNING: Use only if you won't store the pointer. Prefer get_ref().
 *
 * @param context - The context
 * @param key - Key name
 * @param value_out - Output pointer for the data
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe for writes. Multiple readers OK.
 */
EventChainErrorCode event_context_get(
    const EventContext *context,
    const char *key,
    void **value_out
);

/**
 * Check if a key exists in the context (constant-time for sensitive keys)
 *
 * @param context - The context
 * @param key - Key name
 * @param constant_time - If true, use timing-attack resistant comparison
 * @return true if key exists, false otherwise
 *
 * Thread-safety: Not thread-safe for writes. Multiple readers OK.
 */
bool event_context_has(
    const EventContext *context,
    const char *key,
    bool constant_time
);

/**
 * Remove a value from the context
 *
 * Releases the reference to the value.
 *
 * @param context - The context
 * @param key - Key name
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe. Caller must synchronize.
 */
EventChainErrorCode event_context_remove(EventContext *context, const char *key);

/**
 * Get the number of entries in the context
 *
 * @param context - The context
 * @return Number of entries, or 0 if context is NULL
 *
 * Thread-safety: Not thread-safe for concurrent modifications.
 */
size_t event_context_count(const EventContext *context);

/**
 * Get current memory usage in bytes
 *
 * @param context - The context
 * @return Memory usage in bytes, or 0 if context is NULL
 *
 * Thread-safety: Not thread-safe for concurrent modifications.
 */
size_t event_context_memory_usage(const EventContext *context);

/**
 * Clear all entries from the context
 *
 * Releases references to all values.
 *
 * @param context - The context
 *
 * Thread-safety: Not thread-safe. Caller must synchronize.
 */
void event_context_clear(EventContext *context);

/* ==================== EventResult Functions ==================== */

/**
 * Create a success result
 *
 * @return Success result
 *
 * Thread-safety: Safe to call from any thread
 */
EventResult event_result_success(void);

/**
 * Create a failure result with error message
 *
 * @param error_message - Error description (truncated to max length)
 * @param error_code - Specific error code
 * @param detail_level - Amount of detail to include
 * @return Failure result
 *
 * Thread-safety: Safe to call from any thread
 */
EventResult event_result_failure(
    const char *error_message,
    EventChainErrorCode error_code,
    ErrorDetailLevel detail_level
);

/* ==================== ChainableEvent Functions ==================== */

/**
 * Create a new ChainableEvent
 *
 * @param execute - The function that implements the event logic
 * @param user_data - Optional user data (not owned by event)
 * @param name - Name of the event (truncated to max length)
 * @return Pointer to new event, or NULL on failure
 *
 * Thread-safety: Safe to call from any thread
 */
ChainableEvent *chainable_event_create(
    EventExecuteFunc execute,
    void *user_data,
    const char *name
);

/**
 * Destroy a ChainableEvent
 *
 * @param event - Event to destroy (may be NULL)
 *
 * Thread-safety: Not thread-safe. Caller must ensure exclusive access.
 */
void chainable_event_destroy(ChainableEvent *event);

/* ==================== EventMiddleware Functions ==================== */

/**
 * Create a new EventMiddleware
 *
 * @param execute - The middleware execution function
 * @param user_data - Optional user data (not owned by middleware)
 * @param name - Name of the middleware (truncated to max length)
 * @return Pointer to new middleware, or NULL on failure
 *
 * Thread-safety: Safe to call from any thread
 */
EventMiddleware *event_middleware_create(
    MiddlewareExecuteFunc execute,
    void *user_data,
    const char *name
);

/**
 * Destroy an EventMiddleware
 *
 * @param middleware - Middleware to destroy (may be NULL)
 *
 * Thread-safety: Not thread-safe. Caller must ensure exclusive access.
 */
void event_middleware_destroy(EventMiddleware *middleware);

/* ==================== EventChain Functions ==================== */

/**
 * Create a new EventChain with specified fault tolerance and error detail
 *
 * @param mode - Fault tolerance mode
 * @param detail_level - Error message detail level
 * @return Pointer to new chain, or NULL on failure
 *
 * Thread-safety: Safe to call from any thread
 */
EventChain *event_chain_create_with_detail(
    FaultToleranceMode mode,
    ErrorDetailLevel detail_level
);

/**
 * Create a new EventChain with specified fault tolerance (full detail)
 *
 * @param mode - Fault tolerance mode
 * @return Pointer to new chain, or NULL on failure
 *
 * Thread-safety: Safe to call from any thread
 */
EventChain *event_chain_create(FaultToleranceMode mode);

/**
 * Destroy an EventChain and free all resources
 *
 * This destroys all events and middleware owned by the chain.
 *
 * @param chain - Chain to destroy (may be NULL)
 *
 * Thread-safety: Not thread-safe. Caller must ensure exclusive access.
 */
void event_chain_destroy(EventChain *chain);

/**
 * Add an event to the chain
 *
 * The chain takes ownership of the event.
 *
 * @param chain - The chain
 * @param event - Event to add (ownership transferred)
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe. Do not call during execution.
 */
EventChainErrorCode event_chain_add_event(
    EventChain *chain,
    ChainableEvent *event
);

/**
 * Add middleware to the chain
 *
 * The chain takes ownership of the middleware.
 * Middleware is executed in LIFO order (last added wraps first).
 *
 * @param chain - The chain
 * @param middleware - Middleware to add (ownership transferred)
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe. Do not call during execution.
 */
EventChainErrorCode event_chain_use_middleware(
    EventChain *chain,
    EventMiddleware *middleware
);

/**
 * Set custom failure handler for CUSTOM fault tolerance mode
 *
 * @param chain - The chain
 * @param handler - Callback to determine if execution should continue
 * @param user_data - Data passed to handler (not owned)
 * @return EC_SUCCESS or error code
 *
 * Thread-safety: Not thread-safe. Set before execution.
 */
EventChainErrorCode event_chain_set_failure_handler(
    EventChain *chain,
    bool (*handler)(const ChainableEvent *event, const char *error, void *user_data),
    void *user_data
);

/**
 * Get the context from the chain
 *
 * Use this to set initial values before execution.
 *
 * @param chain - The chain
 * @return Pointer to context, or NULL if chain is invalid
 *
 * Thread-safety: Not thread-safe for concurrent modifications.
 */
EventContext *event_chain_get_context(EventChain *chain);

/**
 * Execute the entire chain with signal safety
 *
 * @param chain - The chain to execute
 * @return ChainResult that must be freed with chain_result_destroy()
 *
 * Thread-safety: Not thread-safe. Do not share chain across threads.
 */
ChainResult event_chain_execute(EventChain *chain);

/**
 * Check if chain was interrupted by signal
 *
 * @param chain - The chain
 * @return true if interrupted, false otherwise
 *
 * Thread-safety: Safe to call from any thread
 */
bool event_chain_was_interrupted(const EventChain *chain);

/* ==================== ChainResult Functions ==================== */

/**
 * Destroy a ChainResult and free its memory
 *
 * @param result - Result to destroy (may be NULL)
 *
 * Thread-safety: Not thread-safe. Caller must ensure exclusive access.
 */
void chain_result_destroy(ChainResult *result);

/**
 * Print chain result to stdout
 *
 * @param result - Result to print
 *
 * Thread-safety: Not thread-safe with concurrent output.
 */
void chain_result_print(const ChainResult *result);

/* ==================== Factory Functions ==================== */

/**
 * Create a STRICT fault tolerance chain (production defaults)
 */
static inline EventChain *event_chain_create_strict(void) {
    return event_chain_create_with_detail(FAULT_TOLERANCE_STRICT, ERROR_DETAIL_MINIMAL);
}

/**
 * Create a STRICT fault tolerance chain (development defaults)
 */
static inline EventChain *event_chain_create_strict_dev(void) {
    return event_chain_create_with_detail(FAULT_TOLERANCE_STRICT, ERROR_DETAIL_FULL);
}

/**
 * Create a LENIENT fault tolerance chain
 */
static inline EventChain *event_chain_create_lenient(void) {
    return event_chain_create(FAULT_TOLERANCE_LENIENT);
}

/**
 * Create a BEST_EFFORT fault tolerance chain
 */
static inline EventChain *event_chain_create_best_effort(void) {
    return event_chain_create(FAULT_TOLERANCE_BEST_EFFORT);
}

/**
 * Create a CUSTOM fault tolerance chain
 */
static inline EventChain *event_chain_create_custom(void) {
    return event_chain_create(FAULT_TOLERANCE_CUSTOM);
}

/* ==================== Utility Functions ==================== */

/**
 * Get error message for an error code
 *
 * @param code - Error code
 * @return Human-readable error message (never NULL)
 *
 * Thread-safety: Safe to call from any thread
 */
const char *event_chain_error_string(EventChainErrorCode code);

/**
 * Get library version string
 *
 * @return Version string (never NULL)
 *
 * Thread-safety: Safe to call from any thread
 */
const char *event_chain_version_string(void);

/**
 * Get build information including security features
 *
 * @return Build info string (never NULL)
 *
 * Thread-safety: Safe to call from any thread
 */
const char *event_chain_build_info(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENTCHAINS_HARDENED_H */