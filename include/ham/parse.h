#ifndef HAM_PARSE_H
#define HAM_PARSE_H 1

/**
 * @defgroup HAM_PARSE Expression parsing
 * @ingroup HAM
 * @{
 */

#include "lex.h"

HAM_C_API_BEGIN

typedef enum ham_expr_kind{
	HAM_EXPR_REF,

	HAM_EXPR_KIND_COUNT,
	HAM_EXPR_KIND_ERROR = HAM_EXPR_KIND_COUNT,
} ham_expr_kind;

#define HAM_PARSE_CONTEXT_UTF_(n) ham_parse_context_utf##n
#define HAM_PARSE_CONTEXT_UTF(n) HAM_PARSE_CONTEXT_UTF_(n)

#define HAM_EXPR_UTF_(n, name) ham_expr_##name##_utf##n
#define HAM_EXPR_UTF(n, name) HAM_EXPR_UTF_(n, name)

#define HAM_PARSE_X_UTF 8
#include "parse.x.h"

#define HAM_PARSE_X_UTF 16
#include "parse.x.h"

#define HAM_PARSE_X_UTF 32
#include "parse.x.h"

ham_api ham_parse_context_utf8  *ham_parse_context_create_utf8();
ham_api ham_parse_context_utf16 *ham_parse_context_create_utf16();
ham_api ham_parse_context_utf32 *ham_parse_context_create_utf32();

ham_api void ham_parse_context_destroy_utf8 (ham_parse_context_utf8  *ctx);
ham_api void ham_parse_context_destroy_utf16(ham_parse_context_utf16 *ctx);
ham_api void ham_parse_context_destroy_utf32(ham_parse_context_utf32 *ctx);

#define HAM_PARSE_CONTEXT_CREATE_UTF_(n) ham_parse_context_create_utf##n
#define HAM_PARSE_CONTEXT_CREATE_UTF(n) HAM_PARSE_CONTEXT_CREATE_UTF_(n)

#define HAM_PARSE_CONTEXT_DESTROY_UTF_(n) ham_parse_context_destroy_utf##n
#define HAM_PARSE_CONTEXT_DESTROY_UTF(n) HAM_PARSE_CONTEXT_DESTROY_UTF_(n)

ham_api bool ham_parse_utf8 (const ham_token_utf8  *tok_beg, const ham_token_utf8  *tok_end, ham_expr_u_utf8  *ret);
ham_api bool ham_parse_utf16(const ham_token_utf16 *tok_beg, const ham_token_utf16 *tok_end, ham_expr_u_utf16 *ret);
ham_api bool ham_parse_utf32(const ham_token_utf32 *tok_beg, const ham_token_utf32 *tok_end, ham_expr_u_utf32 *ret);

#define HAM_PARSE_UTF_(n) ham_parse_utf##n
#define HAM_PARSE_UTF(n) HAM_PARSE_UTF_(n)

typedef HAM_PARSE_CONTEXT_UTF(HAM_UTF) ham_parse_context;

typedef HAM_EXPR_UTF(HAM_UTF, base)    ham_expr;
typedef HAM_EXPR_UTF(HAM_UTF, binding) ham_expr_binding;
typedef HAM_EXPR_UTF(HAM_UTF, ref)     ham_expr_ref;

#define ham_parse_context_create HAM_PARSE_CONTEXT_CREATE_UTF(HAM_UTF)
#define ham_parse_context_destroy HAM_PARSE_CONTEXT_DESTROY_UTF(HAM_UTF)

#define ham_parse HAM_PARSE_UTF(HAM_UTF)

HAM_C_API_END

/**
 * @}
 */

#endif // !HAM_PARSE_H
