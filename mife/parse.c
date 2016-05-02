#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "parse.h"

bool jsmn_parse_f2_elem(const char *const json_string, const jsmntok_t **const json_tokens, bool *const elem) {
	if((*json_tokens)->type != JSMN_PRIMITIVE) return false;

	const int pos = (*json_tokens)->start;
	const char c = json_string[pos];

	/* for numbers, die if there are multiple digits */
	if((c == '0' || c == '1') && (*json_tokens)->end - pos > 1) {
		fprintf(stderr, "at position %d\nexpecting 0 or 1, but found a longer number\n", (*json_tokens)->start);
		return false;
	}

	switch(c) {
		case '0':
		case 'f':
			*elem = false;
			break;
		case '1':
		case 't':
			*elem = true;
			break;
		default:
			fprintf(stderr, "at position %d\nexpecting 0, 1, true, or false, found token starting with '%c'\n", (*json_tokens)->start, json_string[(*json_tokens)->start]);
			return false;
	}
	return true;
}

/* The next three functions are almost identical. I miss polymorphism. =( */
int jsmn_parse_f2_row(const char *const json_string, const jsmntok_t **const json_tokens, bool **const row, int expected_num_cols) {
	/* demand an array of elements of the appropriate length */
	if((*json_tokens)->type != JSMN_ARRAY) {
		fprintf(stderr, "at position %d\nexpecting matrix row, found non-array\n", (*json_tokens)->start);
		return -1;
	}
	if(expected_num_cols < 0) expected_num_cols = (*json_tokens)->size;
	if(expected_num_cols != (*json_tokens)->size) {
		fprintf(stderr, "at position %d\nexpecting row of length %d, found row of length %d\n",
		        (*json_tokens)->start, expected_num_cols, (*json_tokens)->size);
		return -1;
	}

	/* reserve some memory */
	if(ALLOC_FAILS(*row, expected_num_cols)) {
		fprintf(stderr, "out of memory in jsmn_parse_f2_row\n");
		return -1;
	}

	/* parse each element */
	int i;
	for(i = 0; i < expected_num_cols; i++) {
		++(*json_tokens);
		if(!jsmn_parse_f2_elem(json_string, json_tokens, *row+i)) {
			free(*row);
			return -1;
		}
	}

	return expected_num_cols;
}

bool jsmn_parse_f2_matrix(const char *const json_string, const jsmntok_t **const json_tokens, f2_matrix *const matrix) {
	/* demand an array of rows */
	matrix->num_rows = (*json_tokens)->size;
	if((*json_tokens)->type != JSMN_ARRAY) {
		fprintf(stderr, "at position %d\nexpecting matrix, found non-array\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some memory */
	if(ALLOC_FAILS(matrix->elems, matrix->num_rows)) {
		fprintf(stderr, "out of memory in jsmn_parse_f2_matrix\n");
		return false;
	}

	/* parse each row */
	int num_cols = -1;
	int i;
	for(i = 0; i < matrix->num_rows; i++) {
		++(*json_tokens);
		if(0 > (num_cols = jsmn_parse_f2_row(json_string, json_tokens, matrix->elems+i, num_cols))) {
			int j;
			for(j = 0; j < i; j++)
				free(matrix->elems[j]);
			free(matrix->elems);
			return false;
		}
	}
	matrix->num_cols = num_cols;

	return true;
}

bool jsmn_parse_f2_mbp(const char *const json_string, const jsmntok_t **const json_tokens, f2_mbp *const mbp) {
	/* demand an array of at least one matrix */
	mbp->matrices_len = (*json_tokens)->size;
	if((*json_tokens)->type != JSMN_ARRAY) {
		fprintf(stderr, "at position %d\nexpecting matrix branching program, found non-array\n", (*json_tokens)->start);
		return false;
	}
	if(mbp->matrices_len < 1) return false;

	/* reserve some memory */
	if(ALLOC_FAILS(mbp->matrices, mbp->matrices_len)) {
		fprintf(stderr, "out of memory in jsmn_parse_f2_mbp\n");
		return false;
	}

	/* parse each matrix */
	int i;
	for(i = 0; i < mbp->matrices_len; i++) {
		++(*json_tokens);
		if(!jsmn_parse_f2_matrix(json_string, json_tokens, mbp->matrices+i)) {
			int j;
			for(j = 0; j < i; j++)
				f2_matrix_free(mbp->matrices[j]);
			free(mbp->matrices);
			return false;
		}
	}

	/* check that the dimensions match up */
	for(i = 0; i < mbp->matrices_len-1; i++) {
		if(mbp->matrices[i].num_cols != mbp->matrices[i+1].num_rows) {
			fprintf(stderr, "matrices %d and %d have incompatible shapes\n", i, i+1);
			f2_mbp_free(*mbp);
			return false;
		}
	}

	return true;
}

int  jsmn_parse_string_row(const char *const json_string, const jsmntok_t **const json_tokens, char ***const row, int expected_num_cols) {
	/* demand an array of elements of the appropriate length */
	if((*json_tokens)->type != JSMN_ARRAY) {
		fprintf(stderr, "at position %d\nexpecting matrix row, found non-array\n", (*json_tokens)->start);
		return -1;
	}
	if(expected_num_cols < 0) expected_num_cols = (*json_tokens)->size;
	if(expected_num_cols != (*json_tokens)->size) {
		fprintf(stderr, "at position %d\nexpecting row of length %d, found row of length %d\n",
		        (*json_tokens)->start, expected_num_cols, (*json_tokens)->size);
		return -1;
	}

	/* reserve some memory */
	if(ALLOC_FAILS(*row, expected_num_cols)) {
		fprintf(stderr, "out of memory in jsmn_parse_string_row\n");
		return -1;
	}

	/* parse each element */
	int i;
	for(i = 0; i < expected_num_cols; i++) {
		++(*json_tokens);
		if(!jsmn_parse_string(json_string, json_tokens, *row+i)) {
			int j;
			for(j = 0; j < i; j++) free((*row)[j]);
			free(*row);
			return -1;
		}
	}

	return expected_num_cols;
}

bool jsmn_parse_string_matrix(const char *const json_string, const jsmntok_t **const json_tokens, string_matrix *const matrix) {
	/* demand an array of rows */
	matrix->num_rows = (*json_tokens)->size;
	if((*json_tokens)->type != JSMN_ARRAY) {
		fprintf(stderr, "at position %d\nexpecting matrix, found non-array\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some memory */
	if(ALLOC_FAILS(matrix->elems, matrix->num_rows)) {
		fprintf(stderr, "out of memory in jsmn_parse_string_matrix\n");
		return false;
	}

	/* parse each row */
	int num_cols = -1;
	int i;
	for(i = 0; i < matrix->num_rows; i++) {
		++(*json_tokens);
		if(0 > (num_cols = jsmn_parse_string_row(json_string, json_tokens, matrix->elems+i, num_cols))) {
			matrix->num_rows = i;
			string_matrix_free(*matrix);
			return false;
		}
		matrix->num_cols = num_cols;
	}

	return true;
}

bool jsmn_parse_mbp_step(const char *const json_string, const jsmntok_t **const json_tokens, mbp_step *const step) {
	int i;

	/* demand an object with at least two parts */
	step->symbols_len = (*json_tokens)->size - 1;
	if((*json_tokens)->type != JSMN_OBJECT || step->symbols_len < 1) {
		fprintf(stderr, "at position %d\nexpecting step (JSON object with key \"position\" and one key per input symbol), found non-object\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some memory; reserve one extra slot in case the user forgot to
	 * specify a position */
	step->position = NULL;
	step->symbols  = NULL;
	step->matrix   = NULL;
	if(ALLOC_FAILS(step->symbols, step->symbols_len+1)) {
		fprintf(stderr, "out of memory in jsmn_parse_mbp_step\n");
		return false;
	}
	for(i = 0; i < step->symbols_len; i++) step->symbols[i] = NULL;
	if(ALLOC_FAILS(step->matrix, step->symbols_len+1)) {
		fprintf(stderr, "out of memory in jsmn_parse_mbp_step\n");
		mbp_step_free(*step);
		return false;
	}

	/* parse each key-value pair */
	int next_symbol = 0;
	for(i = 0; i < step->symbols_len+1; i++) {
		++(*json_tokens);
		char *key;
		if(!jsmn_parse_string(json_string, json_tokens, &key)) {
			mbp_step_free(*step);
			return false;
		}

		++(*json_tokens);
		if(strcmp(key, "position") == 0) {
			free(key);
			if(step->position != NULL) {
				fprintf(stderr, "at position %d\nduplicate position information\n", (*json_tokens)->start);
				mbp_step_free(*step);
				return false;
			}
			if(!jsmn_parse_string(json_string, json_tokens, &step->position)) {
				mbp_step_free(*step);
				return false;
			}
		}
		else {
			step->symbols[next_symbol] = key;
			if(!jsmn_parse_f2_matrix(json_string, json_tokens, step->matrix + next_symbol)) {
				free(key);
				step->symbols[next_symbol] = NULL;
				mbp_step_free(*step);
				return false;
			}
			++next_symbol;
		}
	}

	/* make sure we saw a "position" key */
	if(next_symbol == step->symbols_len+1) {
		fprintf(stderr, "at position %d\nnever saw a \"position\" key in this object\n", (*json_tokens)->start);
		++step->symbols_len;
		mbp_step_free(*step);
		return false;
	}

	/* check that all the matrices have the same size */
	for(i = 1; i < step->symbols_len; i++) {
		if(step->matrix[i].num_rows != step->matrix[0].num_rows ||
		   step->matrix[i].num_cols != step->matrix[0].num_cols) {
			fprintf(stderr, "before position %d\ndimension mismatch in matrices 0 and %d\n", (*json_tokens)->end, i);
			mbp_step_free(*step);
			return false;
		}
	}

	return true;
}

bool jsmn_parse_mbp_steps(const char *const json_string, const jsmntok_t **const json_tokens, mbp_template *const template) {
	/* demand an array with at least one step in it */
	template->steps_len = (*json_tokens)->size;
	if((*json_tokens)->type != JSMN_ARRAY || template->steps_len <= 1) {
		fprintf(stderr, "at position %d\nexpecting array of steps\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some space */
	if(ALLOC_FAILS(template->steps, template->steps_len)) {
		fprintf(stderr, "out of memory in jsmn_parse_mbp_steps\n");
		return false;
	}

	/* parse each step */
	int i;
	for(i = 0; i < template->steps_len; i++) {
		++(*json_tokens);
		if(!jsmn_parse_mbp_step(json_string, json_tokens, template->steps+i)) {
			/* caller is responsible for cleaning up; mbp_template_free will
			 * inspect steps_len to decide how many steps to free */
			template->steps_len = i;
			return false;
		}
	}

	/* check that the steps' dimensions mesh */
	for(i = 1; i < template->steps_len; i++) {
		if(template->steps[i-1].matrix[0].num_cols != template->steps[i].matrix[0].num_rows) {
			fprintf(stderr, "before position %d\ndimension mismatch in steps %d and %d\n", (*json_tokens)->end, i-1, i);
			return false;
		}
	}

	return true;
}

bool jsmn_parse_string(const char *const json_string, const jsmntok_t **const json_tokens, char **const string) {
	/* demand a string */
	if((*json_tokens)->type != JSMN_STRING) {
		fprintf(stderr, "at position %d\nexpecting string\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some space */
	const unsigned int string_len = (*json_tokens)->end - (*json_tokens)->start;
	if(ALLOC_FAILS(*string, string_len+1)) {
		fprintf(stderr, "out of memory in jsmn_parse_string\n");
		return false;
	}

	/* copy */
	memcpy(*string, json_string+(*json_tokens)->start, string_len);
	(*string)[string_len] = '\0';
	return true;
}

bool jsmn_parse_mbp_template(const char *const json_string, const jsmntok_t **const json_tokens, mbp_template *const template) {
	int i;

	/* demand an object with exactly two parts */
	if((*json_tokens)->type != JSMN_OBJECT || (*json_tokens)->size != 2) {
		fprintf(stderr, "at position %d\nexpecting template (JSON object with keys \"steps\" and \"outputs\"), found non-object\n", (*json_tokens)->start);
		return false;
	}

	template->steps   = NULL;
	template->outputs = (string_matrix) { .num_rows = 0, .num_cols = 0, .elems = NULL };

	for(i = 0; i < 2; i++) {
		char *key;
		++(*json_tokens);
		switch(json_string[(*json_tokens)->start]) {
			case 's': /* steps */
				++(*json_tokens);
				if(!jsmn_parse_mbp_steps(json_string, json_tokens, template)) {
					mbp_template_free(*template);
					return false;
				}
				break;

			case 'o': /* outputs */
				++(*json_tokens);
				if(!jsmn_parse_string_matrix(json_string, json_tokens, &template->outputs)) {
					template->outputs.elems = NULL;
					mbp_template_free(*template);
					return false;
				}
				break;

			default:
				if(jsmn_parse_string(json_string, json_tokens, &key)) {
					fprintf(stderr, "at position %d\nexpecting \"steps\" or \"outputs\", found %s\n", (*json_tokens)->start, key);
					free(key);
				}
				mbp_template_free(*template);
				return false;
		}
	}

	if(NULL == template->steps) {
		fprintf(stderr, "before position %d\nmissing key \"steps\"\n", (*json_tokens)->end);
		mbp_template_free(*template);
		return false;
	}
	if(NULL == template->outputs.elems) {
		fprintf(stderr, "before position %d\nmissing key \"outputs\"\n", (*json_tokens)->end);
		mbp_template_free(*template);
		return false;
	}
	const     f2_matrix *const start_m = &template->steps[0].matrix[0];
	const     f2_matrix *const   end_m = &template->steps[template->steps_len-1].matrix[0];
	const string_matrix *const   out_m = &template->outputs;
	if(start_m->num_rows != out_m->num_rows ||
	     end_m->num_cols != out_m->num_cols) {
		fprintf(stderr, "before position %d\ndimension mismatch between template dimensions (%dx%d) and outputs (%dx%d)\n",
			(*json_tokens)->end, start_m->num_rows, end_m->num_cols, out_m->num_rows, out_m->num_cols);
		mbp_template_free(*template);
		return false;
	}

	return true;
}

bool jsmn_parse_mbp_plaintext(const char *const json_string, const jsmntok_t **const json_tokens, mbp_plaintext *const plaintext) {
	/* demand an array */
	plaintext->symbols_len = (*json_tokens)->size;
	if((*json_tokens)->type != JSMN_ARRAY) {
		fprintf(stderr, "at position %d\nexpecting plaintext, found non-array\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some memory */
	if(ALLOC_FAILS(plaintext->symbols, plaintext->symbols_len)) {
		fprintf(stderr, "out of memory in jsmn_parse_mbp_plaintext\n");
		return false;
	}

	/* parse each symbol */
	int i;
	for(i = 0; i < plaintext->symbols_len; i++) {
		++(*json_tokens);
		if(!jsmn_parse_string(json_string, json_tokens, plaintext->symbols+i)) {
			plaintext->symbols_len = i;
			mbp_plaintext_free(*plaintext);
			return false;
		}
	}

	return true;
}

bool jsmn_parse_ciphertext_mapping(const char *const json_string, const jsmntok_t **const json_tokens, ciphertext_mapping *const mapping) {
	int i;

	/* demand an object */
	mapping->positions_len = (*json_tokens)->size;
	if((*json_tokens)->type != JSMN_OBJECT) {
		fprintf(stderr, "at position %d\nexpecting ciphertext mapping (JSON object with one key per input position and string values), found non-object\n", (*json_tokens)->start);
		return false;
	}

	/* reserve some memory */
	mapping->positions = NULL;
	mapping->uids      = NULL;
	if(ALLOC_FAILS(mapping->positions, mapping->positions_len)) {
		fprintf(stderr, "out of memory in jsmn_parse_ciphertext_mapping\n");
		return false;
	}
	if(ALLOC_FAILS(mapping->uids, mapping->positions_len)) {
		fprintf(stderr, "out of memory in jsmn_parse_ciphertext_mapping\n");
		free(mapping->positions);
		return false;
	}

	/* parse each key-value pair */
	for(i = 0; i < mapping->positions_len; i++) {
		++(*json_tokens);
		if(!jsmn_parse_string(json_string, json_tokens, mapping->positions + i)) {
			mapping->positions_len = i;
			ciphertext_mapping_free(*mapping);
			return false;
		}

		++(*json_tokens);
		if(!jsmn_parse_string(json_string, json_tokens, mapping->uids + i)) {
			free(mapping->positions[i]);
			mapping->positions_len = i;
			ciphertext_mapping_free(*mapping);
			return false;
		}
	}

	return true;
}

/* a helper function for the top-level wrappers that allocates the appropriate
 * amount of space for tokens and populates them; the caller is then
 * responsible for figuring out what those tokens mean
 *
 * not part of the public interface */
bool jsmn_parse_setup(const char *const json_string, jsmntok_t **const json_tokens) {
	jsmn_parser parser;
	const size_t json_string_len = strlen(json_string);

	/* first pass: figure out how much stuff is in the string */
	jsmn_init(&parser);
	const int json_tokens_len = jsmn_parse(&parser, json_string, json_string_len, NULL, 0);
	if(json_tokens_len < 1) {
		fprintf(stderr, "found no JSON tokens\n");
		goto fail_none;
	}
	if(ALLOC_FAILS(*json_tokens, json_tokens_len)) {
		fprintf(stderr, "out of memory when allocating tokens in jsmn_parse_setup\n");
		goto fail_none;
	}

	/* second pass: record the results of parsing */
	jsmn_init(&parser);
	const int tokens_parsed = jsmn_parse(&parser, json_string, json_string_len, *json_tokens, json_tokens_len);
	if(tokens_parsed != json_tokens_len) {
		fprintf(stderr, "The impossible happened: parsed the same string twice and got two\ndifferent token counts (%d first time, %d second).\n", json_tokens_len, tokens_parsed);
		goto fail_free_tokens;
	}

	return true;

fail_free_tokens:
	free(*json_tokens);
fail_none:
	return false;
}

bool jsmn_parse_mbp_template_location(const location loc, mbp_template *const template) {
	int fd;
	struct stat fd_stat;
	char *json_string;
	jsmntok_t *json_tokens;
	bool status = false;

	/* read plaintext */
	if(-1 == (fd = open(loc.path, O_RDONLY, 0))) {
		fprintf(stderr, "could not open matrix branching program '%s'\n", loc.path);
		goto fail_none;
	}
	if(-1 == fstat(fd, &fd_stat)) {
		fprintf(stderr, "could not stat matrix branching program '%s'\n", loc.path);
		goto fail_close;
	}
	if(NULL == (json_string = mmap(NULL, fd_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0))) {
		fprintf(stderr, "mmap failed, out of address space?\n");
		goto fail_close;
	}

	if(jsmn_parse_setup(json_string, &json_tokens)) {
		const jsmntok_t *state = json_tokens;
		status = jsmn_parse_mbp_template(json_string, &state, template);
		free(json_tokens);
	}

fail_unmap:
	munmap(json_string, fd_stat.st_size);
fail_close:
	close(fd);
fail_none:
	return status;
}

bool jsmn_parse_mbp_plaintext_string(const char *const json_string, mbp_plaintext *const plaintext) {
	jsmntok_t *json_tokens;
	bool status = jsmn_parse_setup(json_string, &json_tokens);
	if(status) {
		const jsmntok_t *state = json_tokens;
		status = jsmn_parse_mbp_plaintext(json_string, &state, plaintext);
		free(json_tokens);
	}
	return status;
}

bool jsmn_parse_ciphertext_mapping_string(const char *const json_string, ciphertext_mapping *const mapping) {
	jsmntok_t *json_tokens;
	bool status = jsmn_parse_setup(json_string, &json_tokens);
	if(status) {
		const jsmntok_t *state = json_tokens;
		status = jsmn_parse_ciphertext_mapping(json_string, &state, mapping);
		free(json_tokens);
	}
	return status;
}
