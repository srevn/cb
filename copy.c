#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static const char base64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

char *base64_encode(const char *input, size_t length);
char *base64_decode(const char *input, size_t *output_length);
char *read_stream(FILE *stream, size_t *length);
char *read_paste(FILE *stream, size_t *length);
char *parse_response(char *response, size_t length, size_t *base64_out);

// =====================================================
// Base64 encoding/decoding functions
// =====================================================

char *base64_encode(const char *input, size_t length) {
	if (length == 0) {
		char *result = malloc(1);
		if (result) result[0] = '\0';
		return result;
	}

	size_t encoded_length = ((length + 2) / 3) * 4;
	char *encoded = malloc(encoded_length + 1);
	if (!encoded) return NULL;

	size_t i, j;
	for (i = 0, j = 0; i < length; i += 3, j += 4) {
		unsigned char byte1 = input[i];
		unsigned char byte2 = (i + 1 < length) ? input[i + 1] : 0;
		unsigned char byte3 = (i + 2 < length) ? input[i + 2] : 0;

		unsigned int triple = (byte1 << 16) | (byte2 << 8) | byte3;

		encoded[j] = base64_chars[(triple >> 18) & 0x3F];
		encoded[j + 1] = base64_chars[(triple >> 12) & 0x3F];
		encoded[j + 2] = (i + 1 < length) ? base64_chars[(triple >> 6) & 0x3F] : '=';
		encoded[j + 3] = (i + 2 < length) ? base64_chars[triple & 0x3F] : '=';
	}

	encoded[encoded_length] = '\0';
	return encoded;
}

static int decoding_table[256];
static int table_built = 0;

static void build_table() {
	if (table_built) return;
	for (int i = 0; i < 256; i++)
		decoding_table[i] = -1;
	for (int i = 0; i < 64; i++)
		decoding_table[(unsigned char) base64_chars[i]] = i;
	table_built = 1;
}

char *base64_decode(const char *input, size_t *output_length) {
	build_table();

	size_t input_length = strlen(input);
	if (input_length % 4 != 0) return NULL;

	*output_length = input_length / 4 * 3;
	if (input[input_length - 1] == '=')
		(*output_length)--;
	if (input[input_length - 2] == '=')
		(*output_length)--;

	char *decoded = malloc(*output_length + 1);
	if (!decoded) return NULL;

	for (size_t i = 0, j = 0; i < input_length; i += 4, j += 3) {
		int val1 = decoding_table[(unsigned char) input[i]];
		int val2 = decoding_table[(unsigned char) input[i + 1]];
		int val3, val4;

		if (val1 == -1 || val2 == -1) {
			free(decoded);
			return NULL;
		}

		if (input[i + 2] == '=') {
			val3 = -1;
			if (input[i + 3] != '=') {
				free(decoded);
				return NULL;
			}
			val4 = -1;
		} else {
			val3 = decoding_table[(unsigned char) input[i + 2]];
			if (val3 == -1) {
				free(decoded);
				return NULL;
			}
			if (input[i + 3] == '=') {
				val4 = -1;
			} else {
				val4 = decoding_table[(unsigned char) input[i + 3]];
				if (val4 == -1) {
					free(decoded);
					return NULL;
				}
			}
		}

		unsigned int triple = (val1 << 18) | (val2 << 12);
		if (val3 != -1)
			triple |= (val3 << 6);
		if (val4 != -1)
			triple |= val4;

		if (j < *output_length)
			decoded[j] = (triple >> 16) & 0xFF;
		if (j + 1 < *output_length)
			decoded[j + 1] = (triple >> 8) & 0xFF;
		if (j + 2 < *output_length)
			decoded[j + 2] = triple & 0xFF;
	}

	decoded[*output_length] = '\0';
	return decoded;
}

// =====================================================
// I/O and utility functions
// =====================================================

char *read_stream(FILE *stream, size_t *length) {
	size_t capacity = 8192;
	char *buffer = malloc(capacity);
	if (!buffer) {
		perror("malloc");
		return NULL;
	}

	size_t size = 0;
	size_t bytes_read;
	while ((bytes_read = fread(buffer + size, 1, capacity - size, stream)) > 0) {
		size += bytes_read;
		if (size == capacity) {
			capacity *= 2;
			char *new_buffer = realloc(buffer, capacity);
			if (!new_buffer) {
				perror("realloc");
				free(buffer);
				return NULL;
			}
			buffer = new_buffer;
		}
	}

	if (ferror(stream)) {
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

char *read_paste(FILE *stream, size_t *length) {
	size_t capacity = 256;
	char *buffer = malloc(capacity);
	if (!buffer) return NULL;

	size_t size = 0;
	int c;
	while ((c = fgetc(stream)) != EOF) {
		if (size >= capacity - 1) {
			capacity *= 2;
			char *new_buffer = realloc(buffer, capacity);
			if (!new_buffer) {
				free(buffer);
				return NULL;
			}
			buffer = new_buffer;
		}
		buffer[size++] = c;

		if (c == '\a')
			break;
		if (c == '\\' && size > 1 && buffer[size - 2] == '\x1b')
			break;
	}

	buffer[size] = '\0';
	*length = size;
	return buffer;
}

char *parse_response(char *response, size_t length, size_t *base64_out) {
	const char *prefix = "\033]52;c;";
	size_t prefix_len = strlen(prefix);

	if (length < prefix_len + 1) return NULL;
	if (strncmp(response, prefix, prefix_len) != 0) return NULL;

	char *base64_start = response + prefix_len;
	size_t base64_len;

	if (response[length - 1] == '\a') {
		base64_len = length - prefix_len - 1;
	} else if (length > 1 && response[length - 1] == '\\' &&
			   response[length - 2] == '\x1b') {
		base64_len = length - prefix_len - 2;
	} else {
		return NULL;
	}

	*base64_out = base64_len;
	return base64_start;
}

// =====================================================
// Main program
// =====================================================

int main(int argc, char *argv[]) {
	FILE *stream;

	if (argc == 1) {
		if (isatty(STDIN_FILENO)) {
			struct termios orig_termios;
			if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
				perror("tcgetattr");
				return 1;
			}
			struct termios raw_termios = orig_termios;
			raw_termios.c_lflag &= ~(ICANON | ECHO);
			if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios) == -1) {
				perror("tcsetattr");
				return 1;
			}
			FILE *term_out = isatty(STDOUT_FILENO) ? stdout : stderr;
			fprintf(term_out, "\033]52;c;?\a");
			fflush(term_out);

			size_t response_len;
			char *response = read_paste(stdin, &response_len);
			tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
			if (!response)
				return 1;

			size_t base64_len;
			char *base64_data_ptr = parse_response(response, response_len, &base64_len);
			if (!base64_data_ptr) {
				free(response);
				fprintf(stderr, "Invalid clipboard response\n");
				return 1;
			}

			char *base64_data = malloc(base64_len + 1);
			if (!base64_data) {
				free(response);
				perror("malloc");
				return 1;
			}
			memcpy(base64_data, base64_data_ptr, base64_len);
			base64_data[base64_len] = '\0';

			size_t decoded_len;
			char *decoded_data = base64_decode(base64_data, &decoded_len);
			free(base64_data);

			if (!decoded_data) {
				free(response);
				fprintf(stderr, "Failed to decode clipboard content\n");
				return 1;
			}

			fwrite(decoded_data, 1, decoded_len, stdout);

			free(decoded_data);
			free(response);
			return 0;
		}
		stream = stdin;
	} else if (argc == 2) {
		stream = fopen(argv[1], "rb");
		if (!stream) {
			perror(argv[1]);
			return 1;
		}
	} else {
		fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
		return 1;
	}

	size_t input_length;
	char *input = read_stream(stream, &input_length);

	if (stream != stdin) fclose(stream);
	if (!input) return 1;

	char *encoded = base64_encode(input, input_length);
	if (!encoded) {
		free(input);
		return 1;
	}

	if (isatty(STDOUT_FILENO)) {
		printf("\033]52;c;%s\a", encoded);
		fflush(stdout);
	} else {
		fwrite(input, 1, input_length, stdout);
	}

	free(input);
	free(encoded);
	return 0;
}
