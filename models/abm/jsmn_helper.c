#include "jsmn_helper.h"

#include <string.h>
#include <stdlib.h>

c_jsmntok_t* get_next_token(c_jsmntok_t *base_t, c_jsmntok_t *t, struct _gnt_closure_t *closure) {

	// we expect t to be an object or an array
	if ((t->type != JSMN_OBJECT && t->type != JSMN_ARRAY ) || t->size <= closure->a)
		return NULL;

	closure->a++;
	closure->b++;

	while((t+closure->b)->parent != (t-base_t))
		closure->b++;

	return t+closure->b;
}

/**
 * Retrieves from the object token t the value token with specified key
 */
c_jsmntok_t* get_value_token_by_key(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t, const char *key) {

	c_jsmntok_t *t_aux;

	// we expect t to be an object
	if (t->type != JSMN_OBJECT)
		return NULL;

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

	while ((t_aux = get_next_token(base_t, t, &closure)) != NULL) {
		/**
		 *  this stuff is not greatly documented in jsmn, anyway turns out that
		 * 	strings acting as object keys have child count set to 1.
		 * 	This is just another not necessary check.
		 */
		if (t_aux->type == JSMN_STRING && t_aux->size == 1) {
			if (strcmp_token(base, t_aux, key) == 0)
				// this is token representing the value associated with the key we found
				return t_aux + 1;
		}
	}
	return NULL;
}

/**
 * Parses into result the double floating point number represented by primitive token t
 * Returns 0 on success, -1 on failure
 */
int parse_double_token(const char *base, c_jsmntok_t *t, double *result) {
	char buff[64];
	size_t size;
	double tmp;
	char *check;

	if (t->type != JSMN_PRIMITIVE)
		return -1;

	size = (size_t) (t->end - t->start);
	size = size < (64 - 1) ? size : (64 - 1);

	memcpy(buff, &base[t->start], size);
	buff[size] = '\0';

	tmp = strtod(buff, &check);
	if (buff == check)
		return -1;

	*result = tmp;
	return 0;
}

/**
 * Parses into result the unsigned int number represented by primitive token t
 * Returns 0 on success, -1 on failure
 */
int parse_unsigned_token(const char *base, c_jsmntok_t *t, unsigned *result) {
	double tmp;
	// XXX CHECK THIS CHECK
	if (parse_double_token(base, t, &tmp) < 0 || tmp != (unsigned) tmp)
		return -1;

	*result = (unsigned) tmp;
	return 0;
}

/**
 * Same semantic of standard strcmp but safer.
 *  For proper operations t is EXPECTED to have type JSMN_STRING
 */
int strcmp_token(const char *base, c_jsmntok_t *t, const char* str) {
	int t_len = t->end - t->start;
	int res = strncmp(&base[t->start], str, t_len);
	return res == 0 ? str[t_len] != '\0' : res;
}
