#ifndef JSMN_HELPER_H_
#define JSMN_HELPER_H_

#include "jsmn.h"

/**
 * This header contains declaration of helper functions for jsmn
 * The user is encouraged to aid himself with this stuff in order
 * to simplify the initialization of his agents and regions
 */

// This typedef is useful in avoiding accidental token modifications
typedef const jsmntok_t c_jsmntok_t;

// This struct is given for the use in conjunction with next_get_token function().
struct _gnt_closure_t{
	int a;
	unsigned b;
};

// struct _gnt_closure_t iterator = GNT_CLOSURE_INITIALIZER; // easy huh?
#define GNT_CLOSURE_INITIALIZER {0, 0}

/**
 * This is a useful function which iterates over the children tokens
 * of the json object or array represented by token t.
 * Keep in mind that in case t is an object, this function will iterate over keys
 * (corresponding values can be obtained by taking the key+1 token)
 * @param base_t the root token of the jsmn parse
 * @param t the json array or object token we want to iterate over
 * @param closure the struct needed to keep state of the iteration, the definition and the initializer are given above
 * @return the next value token in case of an array, the next key token in case of an object, NULL at iteration end
 */
c_jsmntok_t* 	get_next_token			(c_jsmntok_t *base_t, c_jsmntok_t *t, struct _gnt_closure_t *closure);

/**
 * This function directly retrieves from the object token t_obj the value token associated with the supplied key.
 * @param base_t the root token of the jsmn parse
 * @param base the start of the base string (the json document)
 * @param t_obj the json object token
 * @param key the key
 * @return the value token associated in t_obj associated with the given key, NULL in case of failure
 */
c_jsmntok_t* 	get_value_token_by_key	(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key);
// TODO DOCUMENT
int 			parse_double_token		(const char *base, c_jsmntok_t *t, double *result);
int 			parse_unsigned_token	(const char *base, c_jsmntok_t *t, unsigned *result);
int 			strcmp_token			(const char *base, c_jsmntok_t *t, const char* str);

#endif /* JSMN_HELPER_H_ */
