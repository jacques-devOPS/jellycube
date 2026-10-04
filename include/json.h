#ifndef JC_JSON_H
#define JC_JSON_H
#include <stddef.h>
#define JSMN_PARENT_LINKS
#define JSMN_HEADER
#include "jsmn.h"
#define JSON_TOKENS 8192
typedef struct { const char *text; jsmntok_t tokens[JSON_TOKENS]; int count; } json_doc_t;
int json_parse(json_doc_t *doc, const char *text);
int json_member(const json_doc_t *doc, int object, const char *key);
int json_string(const json_doc_t *doc, int token, char *out, size_t size);
int json_equal(const json_doc_t *doc, int token, const char *value);
int json_escape(const char *in, char *out, size_t size);
#endif
