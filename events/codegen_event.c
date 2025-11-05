/* Codegen Event - Thin wrapper around codegen() */

#include "../chibicc.h"
#include "../src/eventchain/eventchains.h"
#include "codegen_event.h"
#include <stdio.h>

EventResult codegen_event_execute(EventContext *context, void *user_data) {
    (void)user_data;
    
    /* Get AST from context */
    void *ast_ptr = NULL;
    if (event_context_get(context, "ast", &ast_ptr) != EC_SUCCESS || !ast_ptr) {
        return event_result_failure("No AST", EC_ERROR_NOT_FOUND, ERROR_DETAIL_FULL);
    }
    
    Obj *prog = (Obj *)ast_ptr;
    
    /* Get output file from context */
    void *output_file_ptr = NULL;
    if (event_context_get(context, "output_file", &output_file_ptr) != EC_SUCCESS || !output_file_ptr) {
        return event_result_failure("No output file", EC_ERROR_NOT_FOUND, ERROR_DETAIL_FULL);
    }
    
    char *output_file = (char *)output_file_ptr;
    
    /* Generate code to memory buffer */
    char *buf = NULL;
    size_t buflen = 0;
    FILE *out = open_memstream(&buf, &buflen);
    if (!out) {
        return event_result_failure("Failed to open memory stream", EC_ERROR_OUT_OF_MEMORY, ERROR_DETAIL_FULL);
    }
    
    /* Call existing codegen function */
    codegen(prog, out);
    fclose(out);
    
    /* Write to output file (handle "-" as stdout) */
    FILE *outfile;
    if (!output_file || strcmp(output_file, "-") == 0) {
        outfile = stdout;
    } else {
        outfile = fopen(output_file, "w");
        if (!outfile) {
            free(buf);
            return event_result_failure("Failed to open output file", EC_ERROR_EVENT_EXECUTION_FAILED, ERROR_DETAIL_FULL);
        }
    }
    
    fwrite(buf, buflen, 1, outfile);
    
    /* Only close if it's not stdout */
    if (outfile != stdout) {
        fclose(outfile);
    }
    
    free(buf);
    
    return event_result_success();
}
