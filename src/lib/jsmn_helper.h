#ifndef JSMN_HELPER_H_
#define JSMN_HELPER_H_

#include <lib/jsmn.h>
#include <stdbool.h>

// This typedef is useful in avoiding accidental token modifications
typedef const jsmntok_t c_jsmntok_t;

int load_and_parse_json_file(const char *file_name, char **file_str_p, jsmntok_t **tokens_p);

//! This struct is given for the use in conjunction with next_get_token function().
//! Treat it as an opaque data type.
struct _gnt_closure_t{
	int a;
	unsigned b;
};

//! use example: struct _gnt_closure_t iterator = GNT_CLOSURE_INITIALIZER;
#define GNT_CLOSURE_INITIALIZER {0, 0}

/// Alternatively this function can be used
void		init_gnt_closure	(struct _gnt_closure_t *closure);

/**
 * This is a useful function which iterates over the children tokens
 * of the JSON object or array represented by token t.
 * Keep in mind that in case t is an object, this function will iterate over keys
 * (corresponding values can be obtained by taking the key+1 token)
 * @param base_t the root token of the JSMN parse
 * @param t the JSON array or object token we want to iterate over
 * @param closure the struct needed to keep state of the iteration, the definition and the initializer are given above
 * @return the next value token in case of an array, the next key token in case of an object, NULL at iteration end
 */
c_jsmntok_t* 	get_next_token		(c_jsmntok_t *base_t, c_jsmntok_t *t, struct _gnt_closure_t *closure);

/**
 * This function directly retrieves from the object token t_obj the value token associated with the supplied key.
 * @param base_t the root token of the JSMN parse
 * @param base the start of the base string (the JSON document)
 * @param t_obj the JSON object token
 * @param key the key
 * @return the value token associated in t_obj associated with the given key, NULL in case of failure
 */
c_jsmntok_t* 	get_value_token_by_key	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key);

/**
 * This function retrieves from the token t the number of child tokens. In case t is a JSON object token, it returns
 * the number of key-value pairs.
 * @param base_t the root token of the JSMN parse
 * @param t the JSON object/array token
 * @return the child count of t, 0 in case of errors
 */
size_t		children_count_token	(c_jsmntok_t *base_t, c_jsmntok_t *t);

/**
 * This function retrieves from the token t his i-th child, in case t is an object the i-th key token is returned
 * @param base_t the root token of the JSMN parse
 * @param t the JSON object/array token
 * @param i the index of the desired child token
 * @return the token representing the i-th child of t, NULL in case of failure
 */
c_jsmntok_t* 	get_at_token		(c_jsmntok_t *base_t, c_jsmntok_t *t, size_t i);

/**
 * This function parses a floating point double value from a JSMN token
 * @param base the start of the base string (the JSON document)
 * @param t the JSON number token to parse
 * @param result a pointer to a valid double variable which will hold the parsed value
 * @return 0 in case of success, -1 for failure
 */
int 		parse_double_token	(const char *base, c_jsmntok_t *t, double *result);

/**
 * This function parses a unsigned int value from a JSMN token
 * @param base the start of the base string (the JSON document)
 * @param t the JSON number token to parse
 * @param result a pointer to a valid unsigned int variable which will hold the parsed value
 * @return 0 in case of success, -1 for failure
 */
int 		parse_unsigned_token	(const char *base, c_jsmntok_t *t, unsigned *result);

/**
 * This function behaves like standard strcmp() with the difference that the first argument is a JSMN token
 * @param base the start of the base string (the JSON document)
 * @param t the JSON string token to compare
 * @param str the string to compare against the string token
 * @return a positive, zero or negative value if the string token is respectively, more than, equal
 * or less than the string @p str.
 */
int 		strcmp_token		(const char *base, c_jsmntok_t *t, const char* str);

int		parse_unsigned_by_key	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key, unsigned *result);
int		parse_double_by_key	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key, double *result);
int		parse_boolean_by_key	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key, bool *result);

int		parse_double_array	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_arr, unsigned expected_count, double *result);
unsigned	parse_string_choice	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_str, unsigned cnt, const char *choices[cnt]);


#endif /* JSMN_HELPER_H_ */
