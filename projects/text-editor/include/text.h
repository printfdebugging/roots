#ifndef TEXT_H
#define TEXT_H

#include "engine/utils/string.h"

#include <stdint.h>

/* note: todo: the text api has to change */
/* return string views into sub portions of the text */

struct text;

struct text *text_load_from_file(const char *filepath);

struct text *text_load_from_data(const char *data, uint32_t data_length);

bool text_write_to_file(const char *filepath);

uint32_t text_get_line_count(struct text *text);

char *text_get_line_utf8(struct text *text, uint32_t line);

struct string_view text_get_line_utf8_at_offset(struct text *text, uint32_t line, uint32_t offset);

void text_destroy(struct text *text);

uint32_t text_get_line_length(struct text *text, uint32_t line_index);

#endif
