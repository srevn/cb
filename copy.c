#include <stdio.h>
#include <stdlib.h>

static const char base64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const char *input, size_t length);
char *read_stdin(size_t *length);

int main() {
	size_t input_length;
	char *input = read_stdin(&input_length);
	if (!input) {
		return 1;
	}

	char *encoded = base64_encode(input, input_length);
	if (!encoded) {
		free(input);
		return 1;
	}

	printf("\033]52;c;%s\a", encoded);

	free(input);
	free(encoded);
	return 0;
}

char *read_stdin(size_t *length) {
	size_t capacity = 8192; // Initial capacity
	char *buffer = malloc(capacity);
	if (!buffer) {
		perror("malloc");
		return NULL;
	}

	size_t size = 0;
	size_t bytes_read;
	while ((bytes_read = fread(buffer + size, 1, capacity - size, stdin)) > 0) {
		size += bytes_read;
		if (size == capacity) {
			capacity *= 2; // Double the capacity
			char *new_buffer = realloc(buffer, capacity);
			if (!new_buffer) {
				perror("realloc");
				free(buffer);
				return NULL;
			}
			buffer = new_buffer;
		}
	}

	if (ferror(stdin)) {
		perror("fread");
		free(buffer);
		return NULL;
	}

	char *final_buffer = realloc(buffer, size + 1);
	if (!final_buffer) {
		perror("realloc");
		free(buffer);
		return NULL;
	}

	final_buffer[size] = '\0';
	*length = size;
	return final_buffer;
}

char *base64_encode(const char *input, size_t length) {
	if (length == 0) {
		char *result = malloc(1);
		if (result) result[0] = '\0';
		return result;
	}

	size_t encoded_length = ((length + 2) / 3) * 4;
	char *encoded = malloc(encoded_length + 1);
	if (!encoded) {
		return NULL;
	}

	size_t i, j;
	for (i = 0, j = 0; i < length; i += 3, j += 4) {
		unsigned char a = input[i];
		unsigned char b = (i + 1 < length) ? input[i + 1] : 0;
		unsigned char c = (i + 2 < length) ? input[i + 2] : 0;

		unsigned int triple = (a << 16) | (b << 8) | c;

		encoded[j] = base64_chars[(triple >> 18) & 0x3F];
		encoded[j + 1] = base64_chars[(triple >> 12) & 0x3F];
		encoded[j + 2] = (i + 1 < length) ? base64_chars[(triple >> 6) & 0x3F] : '=';
		encoded[j + 3] = (i + 2 < length) ? base64_chars[triple & 0x3F] : '=';
	}

	encoded[encoded_length] = '\0';
	return encoded;
}
