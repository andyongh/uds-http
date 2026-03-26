#include "json_handler.h"
#include "yyjson.h"
#include <string.h>
#include <stdlib.h>

char* json_handler_process(const char *body, size_t body_len, size_t *out_len) {
    // 1. Read incoming JSON
    yyjson_doc *doc = NULL;
    if (body && body_len > 0) {
        doc = yyjson_read(body, body_len, 0);
    }
    
    // 2. Build Response JSON
    yyjson_mut_doc *mut_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(mut_doc);
    yyjson_mut_doc_set_root(mut_doc, root);
    
    yyjson_mut_obj_add_str(mut_doc, root, "status", "success");
    yyjson_mut_obj_add_str(mut_doc, root, "message", "Request processed by UDS Server");
    
    if (doc) {
        yyjson_val *v_root = yyjson_doc_get_root(doc);
        if (yyjson_is_obj(v_root)) {
            // Echo back "received_keys" count or simply a copy
            yyjson_mut_val *received = yyjson_val_mut_copy(mut_doc, v_root);
            yyjson_mut_obj_add_val(mut_doc, root, "echo", received);
        }
        yyjson_doc_free(doc);
    } else {
        yyjson_mut_obj_add_str(mut_doc, root, "error", "Invalid or missing JSON payload");
    }
    
    // 3. Serialize
    char *json_str = yyjson_mut_write(mut_doc, 0, out_len);
    yyjson_mut_doc_free(mut_doc);
    
    // yyjson_mut_write uses malloc under the hood (with default allocator),
    // so returning it allows the caller to `free()` it.
    return json_str;
}
