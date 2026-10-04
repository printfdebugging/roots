#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "engine/unicode/unicode.h"

static uint8_t *utf8_invalid_byte_sequences[] = {
	/** @note Proudly stolen from Go source ;) */
	(uint8_t *) "\xed\xa0\x80\x80",	// surrogate min
	(uint8_t *) "\xed\xbf\xbf\x80",	// surrogate max

	// xx
	(uint8_t *) "\x91\x80\x80\x80",

	// s1
	(uint8_t *) "\xC2\x7F\x80\x80",
	(uint8_t *) "\xC2\xC0\x80\x80",
	(uint8_t *) "\xDF\x7F\x80\x80",
	(uint8_t *) "\xDF\xC0\x80\x80",

	// s2
	(uint8_t *) "\xE0\x9F\xBF\x80",
	(uint8_t *) "\xE0\xA0\x7F\x80",
	(uint8_t *) "\xE0\xBF\xC0\x80",
	(uint8_t *) "\xE0\xC0\x80\x80",

	// s3
	(uint8_t *) "\xE1\x7F\xBF\x80",
	(uint8_t *) "\xE1\x80\x7F\x80",
	(uint8_t *) "\xE1\xBF\xC0\x80",
	(uint8_t *) "\xE1\xC0\x80\x80",

	// s4
	(uint8_t *) "\xED\x7F\xBF\x80",
	(uint8_t *) "\xED\x80\x7F\x80",
	(uint8_t *) "\xED\x9F\xC0\x80",
	(uint8_t *) "\xED\xA0\x80\x80",

	// s5
	(uint8_t *) "\xF0\x8F\xBF\xBF",
	(uint8_t *) "\xF0\x90\x7F\xBF",
	(uint8_t *) "\xF0\x90\x80\x7F",
	(uint8_t *) "\xF0\xBF\xBF\xC0",
	(uint8_t *) "\xF0\xBF\xC0\x80",
	(uint8_t *) "\xF0\xC0\x80\x80",

	// s6
	(uint8_t *) "\xF1\x7F\xBF\xBF",
	(uint8_t *) "\xF1\x80\x7F\xBF",
	(uint8_t *) "\xF1\x80\x80\x7F",
	(uint8_t *) "\xF1\xBF\xBF\xC0",
	(uint8_t *) "\xF1\xBF\xC0\x80",
	(uint8_t *) "\xF1\xC0\x80\x80",

	// s7
	(uint8_t *) "\xF4\x7F\xBF\xBF",
	(uint8_t *) "\xF4\x80\x7F\xBF",
	(uint8_t *) "\xF4\x80\x80\x7F",
	(uint8_t *) "\xF4\x8F\xBF\xC0",
	(uint8_t *) "\xF4\x8F\xC0\x80",
	(uint8_t *) "\xF4\x90\x80\x80",

	/** @note Proudly stolen from https://github.com/Daniel-Abrecht/dpa-utils/blob/master/test/utf8.c#L147-L160  ;) */
	(uint8_t *) "\xFF",
	(uint8_t *) "\xC0\x80",
	(uint8_t *) "\xC1\xBF",
	(uint8_t *) "\xE0\x9F\xBF",
	(uint8_t *) "\xF0\x8F\xBF\xBF",
	(uint8_t *) "\xF8\x87\xBF\xBF\xBF",
	(uint8_t *) "\xFC\x83\xBF\xBF\xBF\xBF",
	(uint8_t *) "\xFE\x81\xBF\xBF\xBF\xBF\xBF",
	(uint8_t *) "\xFE\x84\x80\x80\x80\x80\x80",
	(uint8_t *) "\xFF\xBF\xBF\xBF\xBF\xBF\xBF\xBF",

	(uint8_t *) "\x80",
	(uint8_t *) "\xBF",
	(uint8_t *) "\x99\xae\x81\xb4\x99\xa2\x90\x85\xa0\x89\xaf\x88\x98\xa2\x90\x98\xad\x82\xbd\xb4",

	/* Same as above, but with a continuation uint8_t following */
	(uint8_t *) "\xFF\x80",
	(uint8_t *) "\xC0\x80\x80",
	(uint8_t *) "\xC1\xBF\x80",
	(uint8_t *) "\xE0\x9F\xBF\x80",
	(uint8_t *) "\xF0\x8F\xBF\xBF\x80",
	(uint8_t *) "\xF8\x87\xBF\xBF\xBF\x80",
	(uint8_t *) "\xFC\x83\xBF\xBF\xBF\xBF\x80",
	(uint8_t *) "\xFE\x81\xBF\xBF\xBF\xBF\xBF\x80",
	(uint8_t *) "\xFE\x84\x80\x80\x80\x80\x80\x80",
	(uint8_t *) "\xFF\xBF\xBF\xBF\xBF\xBF\xBF\xBF\x80",

	/* Same as above, but with a null uint8_t following */
	(uint8_t *) "\xFF\x00",
	(uint8_t *) "\xC0\x80\x00",
	(uint8_t *) "\xC1\xBF\x00",
	(uint8_t *) "\xE0\x9F\xBF\x00",
	(uint8_t *) "\xF0\x8F\xBF\xBF\x00",
	(uint8_t *) "\xF8\x87\xBF\xBF\xBF\x00",
	(uint8_t *) "\xFC\x83\xBF\xBF\xBF\xBF\x00",
	(uint8_t *) "\xFE\x81\xBF\xBF\xBF\xBF\xBF\x00",
	(uint8_t *) "\xFE\x84\x80\x80\x80\x80\x80\x00",
	(uint8_t *) "\xFF\xBF\xBF\xBF\xBF\xBF\xBF\xBF\x00",

	/* Same as above, but with another 2 uint8_t sequence following  */
	(uint8_t *) "\xFF\xC2\x80",
	(uint8_t *) "\xC0\x80\xC2\x80",
	(uint8_t *) "\xC1\xBF\xC2\x80",
	(uint8_t *) "\xE0\x9F\xBF\xC2\x80",
	(uint8_t *) "\xF0\x8F\xBF\xBF\xC2\x80",
	(uint8_t *) "\xF8\x87\xBF\xBF\xBF\xC2\x80",
	(uint8_t *) "\xFC\x83\xBF\xBF\xBF\xBF\xC2\x80",
	(uint8_t *) "\xFE\x81\xBF\xBF\xBF\xBF\xBF\xC2\x80",
	(uint8_t *) "\xFE\x84\x80\x80\x80\x80\x80\xC2\x80",
	(uint8_t *) "\xFF\xBF\xBF\xBF\xBF\xBF\xBF\xBF\xC2\x80",

	/* Truncated UTF-8 uint8_t sequence */
	(uint8_t *) "\xf0\x93\x83",  //  { "\xf0\x93\x83\x92", "𓃒" }
};

int unicode_test_utf8_invalid_byte_sequences(int argc, char *argv[]) {
	uint32_t data_length = sizeof(utf8_invalid_byte_sequences) / sizeof(struct uint8_t *);
	for (uint32_t dataidx = 0; dataidx < data_length; ++dataidx) {
		uint8_t *bytes = utf8_invalid_byte_sequences[dataidx];
		if (uc_valid_utf8_stream(bytes, strlen((char *) bytes))) {
			fprintf(stderr, "valid_utf8 falsely reported '%s' as valid utf8\n", bytes);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
