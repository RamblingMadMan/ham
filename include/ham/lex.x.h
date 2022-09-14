#ifndef HAM_LEX_X_H
#define HAM_LEX_X_H 1

#ifndef HAM_LEX_X_UTF
#	error "HAM_LEX_X_UTF was not defined before including lex.x.h"
#endif

#ifdef HAM_LEX_X_CHAR
#	undef HAM_LEX_X_CHAR
#endif

#ifdef HAM_LEX_X_STR
#	undef HAM_LEX_X_STR
#endif

#ifdef HAM_LEX_X_SOURCE_LOCATION
#	undef HAM_LEX_X_SOURCE_LOCATION
#endif

#ifdef HAM_LEX_X_TOKEN
#	undef HAM_LEX_X_TOKEN
#endif

#define HAM_LEX_X_CHAR HAM_CHAR_UTF(HAM_LEX_X_UTF)
#define HAM_LEX_X_STR HAM_STR_UTF(HAM_LEX_X_UTF)
#define HAM_LEX_X_SOURCE_LOCATION HAM_SOURCE_LOCATION_UTF(HAM_LEX_X_UTF)
#define HAM_LEX_X_TOKEN HAM_TOKEN_UTF(HAM_LEX_X_UTF)

#define HAM_LEX_X_TOKEN_KIND_STR HAM_TOKEN_KIND_STR_UTF(HAM_LEX_X_UTF)

#define HAM_LEX_X_LIT_C(lit) HAM_LIT_C_UTF(HAM_LEX_X_UTF, lit)

HAM_C_API_BEGIN

ham_constexpr static inline const HAM_LEX_X_CHAR *HAM_LEX_X_TOKEN_KIND_STR(ham_token_kind kind){
	switch(kind){
	#define HAM_CASE(val) case (val): return HAM_LEX_X_LIT_C(#val);

		HAM_CASE(HAM_TOKEN_EOF)
		HAM_CASE(HAM_TOKEN_ERROR)
		HAM_CASE(HAM_TOKEN_NEWLINE)
		HAM_CASE(HAM_TOKEN_SPACE)
		HAM_CASE(HAM_TOKEN_ID)
		HAM_CASE(HAM_TOKEN_NAT)
		HAM_CASE(HAM_TOKEN_REAL)
		HAM_CASE(HAM_TOKEN_STR)
		HAM_CASE(HAM_TOKEN_OP)
		HAM_CASE(HAM_TOKEN_BRACKET)

	#undef HAM_CASE

		default: return HAM_LEX_X_LIT_C("UNKNOWN");
	}
}

typedef struct HAM_LEX_X_SOURCE_LOCATION{
	HAM_LEX_X_STR source_name;
	ham_usize line, col;
} HAM_LEX_X_SOURCE_LOCATION;

typedef struct HAM_LEX_X_TOKEN{
	ham_token_kind kind;
	HAM_LEX_X_SOURCE_LOCATION loc;
	HAM_LEX_X_STR str;
} HAM_LEX_X_TOKEN;

HAM_C_API_END

#undef HAM_LEX_X_UTF
#undef HAM_LEX_X_H

#endif // !HAM_LEX_X_H
