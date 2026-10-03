#ifdef TEXT_LINE_IMPLEMENTATION

#include "text.h"

#include "utils/string.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct text {
	char *file_path;
	struct string *lines;
	uint32_t line_count;
};

struct text *text_load_from_file(const char *file_path) {
	/**!
	 * warning: `getline` is a UNIX only function, so can't
	 * use that on Windows.
	 */
#ifdef _WIN32
	/**!
	 * fixme: strlen here can be avoided by returning a struct
	 * from ReadFileContents, or by passing an out variable for
	 * the data and returning the length.
	 */
	char *data = string_read_file_contents(file_path);
	if (!data)
		return NULL;

	struct text *text = text_load_from_data(data, (uint32_t) strlen(data));
	text->file_path = string_duplicate(file_path);

	free(data);
	return text;

#else
	FILE *file = NULL;
	if (!(file = fopen(file_path, "r"))) {
		fprintf(stderr, "failed to open file: %s", file_path);
		return NULL;
	}

	struct text *text = calloc(1, sizeof(struct text));

	char *line = NULL;
	uint64_t line_capacity = 0;
	int32_t line_length = 0;
	while ((line_length = (int32_t) getline(&line, &line_capacity, file)) != -1) {
		if (line_length == 0)
			continue;

		text->lines = realloc(text->lines, (sizeof(struct string)) * (text->line_count + 1));
		text->lines[text->line_count++] = (struct string) {
			.data = line,
			.count = (uint32_t) line_length,
		};
		line = NULL;
	}

	fclose(file);

	// note: The last getline returns an empty string "" at the end of the file.
	// So we should call free on that, or that leads to memory leaks.
	free(line);
	return text;
#endif
}

struct text *text_load_from_data(const char *data, uint32_t data_length) {
	if (data == NULL)
		perror("got null data");

	uint32_t index = 0;
	uint32_t last_index = 0;

	struct text *text = calloc(1, sizeof(struct text));
	if (!text) perror("failed to allcoate Text");

	while (index < data_length) {
		if ((data[index++] != '\n') && index != data_length)
			continue;

		uint32_t length = index - last_index;

		char *buffer = calloc(length + 1, sizeof(char));
		if (!buffer)
			perror("failed to allocate buffer\n");

		buffer[length] = '\0';
		buffer = memcpy(buffer, data + last_index, length);
		last_index = index;

		text->lines = realloc(text->lines, (text->line_count + 1) * sizeof(struct string));
		text->lines[text->line_count++] = (struct string) {
			.data = buffer,
			.count = length,
		};
	}

	return text;
}

bool text_write_to_file(const char *file_path) {
	(void) file_path;
	perror("todo");
	return true;
}

uint32_t text_get_line_count(struct text *text) {
	return text->line_count;
}

char *text_get_line_utf8(struct text *text, uint32_t line) {
	if (text->line_count <= line)
		return NULL;
	return text->lines[line].data;
}

struct string_view text_get_line_utf8_at_offset(struct text *text, uint32_t line, uint32_t offset) {
	uint32_t length = text_get_line_length(text, line);
	if (offset < length) {
		return (struct string_view) {
			.data = &text->lines[line].data[offset],
			.count = length - offset,
			.source = &text->lines[line],
		};
	}

	return (struct string_view) {};
}

void text_destroy(struct text *text) {
	for (uint32_t idx = 0; idx < text->line_count; ++idx)
		free(text->lines[idx].data);
	free(text->lines);
	free(text->file_path);
}

uint32_t text_get_line_length(struct text *text, uint32_t line_index) {
	if (line_index <= text->line_count - 1)
		return text->lines[line_index].count;
	return 0;
}

#endif
