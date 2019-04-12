#include <lib/jsmn_helper.h>
#include <mm/dymelor.h>

#include <string.h>
#include <stdio.h>
#include <limits.h>


int load_and_parse_json_file(const char *file_name, char **file_str_p, jsmntok_t **tokens_p) {
	FILE *t_file;
	long int file_len;
	jsmn_parser parser;
	int tokens_cnt;
	char *buffer = NULL;		// we initialize those pointers so we can safely free them even if uninstantiated
	jsmntok_t *tokens = NULL;

	// by default we fail
	int ret = -1;

	// sanity check
	if(file_name == NULL || file_str_p ==  NULL || tokens_p ==  NULL)
		return -1;

	// open file
	t_file = fopen(file_name, "r");
	if(t_file == NULL)
		return -1;

	// compute file length
	// set file position at EOF
	if(fseek(t_file, 0L, SEEK_END))
		goto end;
	//get the actual length
	file_len = ftell(t_file);
	if(file_len < 0)
		goto end;
	// reset file position
	if(fseek(t_file, 0L, SEEK_SET))
		goto end;

	// allocate buffer
	buffer = rsalloc(file_len + 1);
	// Load the whole JSON file from disk
	if(!fread(buffer, (size_t) file_len, 1, t_file))
		goto end;

	// close file
	fclose(t_file);

	// terminate buffer with null character
	buffer[file_len] = '\0';

	// init parser
	jsmn_init(&parser);
	// compute tokens count
	tokens_cnt = jsmn_parse(&parser, buffer, file_len, NULL, 0);
	if(tokens_cnt <= 0)
		goto end;
	// allocate tokens buffer
	tokens = rsalloc(sizeof(jsmntok_t) * (size_t) tokens_cnt);

	// re init parser
	jsmn_init(&parser);
	// Parse the loaded JSON file
	if(jsmn_parse(&parser, buffer, file_len, tokens, tokens_cnt) != tokens_cnt)
		goto end;

	// returns the important values
	*file_str_p = buffer;
	*tokens_p = tokens;
	// set a correct return code
	ret = 0;
	end:

	// if we are failing free the buffers
	if(ret) {
		rsfree(buffer);
		rsfree(tokens);
	}

	return ret;
}

void init_gnt_closure(struct _gnt_closure_t *closure) {
	closure->a = 0;
	closure->b = 0;
}

c_jsmntok_t* get_next_token(c_jsmntok_t *base_t, c_jsmntok_t *t, struct _gnt_closure_t *closure) {

	// we expect t to be an object or an array
	if((t->type != JSMN_OBJECT && t->type != JSMN_ARRAY) || t->size <= closure->a)
		return NULL;

	closure->a++;
	closure->b++;

	while ((t + closure->b)->parent != (t - base_t))
		closure->b++;

	return t + closure->b;
}

c_jsmntok_t* get_value_token_by_key(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t, const char *key) {

	c_jsmntok_t *t_aux;

	// we expect t to be an object
	if(t->type != JSMN_OBJECT)
		return NULL;

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

	while ((t_aux = get_next_token(base_t, t, &closure)) != NULL) {
		/*
		 *  this stuff is not greatly documented in jsmn, anyway turns out that
		 * 	strings acting as object keys have child count set to 1.
		 * 	This is just another unnecessary check.
		 */
		if(t_aux->type == JSMN_STRING && t_aux->size == 1) {
			if(strcmp_token(base, t_aux, key) == 0)
				// this is token representing the value associated with the key we found
				return t_aux + 1;
		}
	}
	return NULL;
}

size_t children_count_token(c_jsmntok_t *base_t, c_jsmntok_t *t) {
	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;
	size_t ret = 0;

	while (get_next_token(base_t, t, &closure))
		ret++;

	return ret;
}

c_jsmntok_t* get_at_token(c_jsmntok_t *base_t, c_jsmntok_t *t, size_t i) {
	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;

	while (i--)
		if(!(get_next_token(base_t, t, &closure)))
			return NULL;

	return get_next_token(base_t, t, &closure);
}

int parse_double_token(const char *base, c_jsmntok_t *t, double *result) {
	char buff[64];
	size_t size;
	double tmp;
	char *check;

	if(t->type != JSMN_PRIMITIVE)
		return -1;

	size = (size_t) (t->end - t->start);
	size = size < (64 - 1) ? size : (64 - 1);

	memcpy(buff, &base[t->start], size);
	buff[size] = '\0';

	tmp = strtod(buff, &check);
	if(buff == check)
		return -1;

	*result = tmp;
	return 0;
}

int parse_unsigned_token(const char *base, c_jsmntok_t *t, unsigned *result) {
	double tmp;

	if(parse_double_token(base, t, &tmp) < 0 ||
			tmp < 0.0 		||
			tmp > UINT_MAX 		||
			tmp > (unsigned) tmp 	||
			tmp < (unsigned) tmp)
		return -1;

	*result = (unsigned) tmp;
	return 0;
}

int strcmp_token(const char *base, c_jsmntok_t *t, const char* str) {
	int t_len = t->end - t->start;
	int res = strncmp(&base[t->start], str, t_len);
	return res == 0 ? str[t_len] != '\0' : res;
}

int parse_double_by_key(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key, double *result) {
	// sanity checks
	if(!(base_t && base && t_obj && key && result))
		return -1;

	// parse requested double
	c_jsmntok_t *t = get_value_token_by_key(base_t, base, t_obj, key);
	if(t == NULL || parse_double_token(base, t, result) < 0) {
		return -1;
	}

	return 0;
}

int parse_unsigned_by_key(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key, unsigned *result) {
	// sanity checks
	if(!(base_t && base && t_obj && key && result))
		return -1;

	// parse requested unsigned
	c_jsmntok_t *t = get_value_token_by_key(base_t, base, t_obj, key);
	if(t == NULL || parse_unsigned_token(base, t, result) < 0) {
		return -1;
	}

	return 0;
}

int parse_boolean_by_key(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_obj, const char *key, bool *result) {
	// sanity checks
	if(!(base_t && base && t_obj && key && result))
		return -1;

	// find relevant token
	c_jsmntok_t *t = get_value_token_by_key(base_t, base, t_obj, key);
	if(t == NULL || t->type != JSMN_PRIMITIVE) {
		return -1;
	}

	// parse the boolean value
	if(!strcmp_token(base, t, "false")) {
		*result = false;
	} else if(!strcmp_token(base, t, "true")) {
		*result = true;
	} else {
		return -1;
	}

	return 0;
}

int parse_double_array(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_arr, unsigned expected_cnt, double *result) {
	// sanity checks
	if(!(base_t && base && t_arr && result))
		return -1;

	// we make sure the elements count is the same
	if(t_arr->type != JSMN_ARRAY || children_count_token(base_t, t_arr) != expected_cnt) {
		return -1;
	}

	struct _gnt_closure_t closure = GNT_CLOSURE_INITIALIZER;
	unsigned i;
	for (i = 0; i < expected_cnt; ++i) {
		if(parse_double_token(base, get_next_token(base_t, t_arr, &closure), &result[i]) < 0)
			return -1;
	}

	return 0;
}

unsigned parse_string_choice(c_jsmntok_t *base_t, const char *base, c_jsmntok_t *t_str, unsigned cnt, const char *choices[cnt]) {
	// sanity checks
	if(!(base_t && base && t_str && choices))
		return UINT_MAX;

	// look for the topology type
	if(t_str->type != JSMN_STRING) {
		return UINT_MAX;
	}

	unsigned i;
	for (i = 0; i < cnt; ++i) {
		if(!strcmp_token(base, t_str, choices[i]))
			return i;
	}
	return cnt;
}
