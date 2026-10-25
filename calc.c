// Civan Ozer 251ADB130
// https://github.com/civanozer23-del/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (OpenAI), conversation history link: ADD_BEFORE_SUBMISSION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#define STUDENT_NAME "civan"
#define STUDENT_LASTNAME "ozer"
#define STUDENT_ID "251ADB130"

typedef struct {
    int is_float;
    long long i;
    double f;
} Value;

typedef struct {
    const char *text;
    size_t pos;
    size_t end;
    size_t error_pos;
    int has_error;
} Parser;

/*
 * AI (ChatGPT), prompt:
 * "Help me implement the RTU Arithmetic Parser lab in C step by step,
 * using recursive descent parsing and following the assignment rules."
 *
 * Positions in the parser are zero-based indexes into the complete file.
 * The assignment requires one-based positions, so set_error adds 1.
 * If an operand is missing at the end of a line, p->end + 1 points to
 * the newline, or to file length + 1 when the last line has no newline.
 */
static void set_error(Parser *p, size_t index) {
    if (!p->has_error) {
        p->has_error = 1;
        p->error_pos = index + 1;
    }
}

static void set_end_error(Parser *p) {
    if (!p->has_error) {
        p->has_error = 1;
        p->error_pos = p->end + 1;
    }
}

static void skip_spaces(Parser *p) {
    while (p->pos < p->end &&
           (p->text[p->pos] == ' ' || p->text[p->pos] == '\t')) {
        p->pos++;
    }
}

static Value make_int(long long x) {
    Value v;

    v.is_float = 0;
    v.i = x;
    v.f = (double)x;

    return v;
}

static Value parse_number(Parser *p) {
    skip_spaces(p);

    if (p->pos >= p->end) {
        set_end_error(p);
        return make_int(0);
    }

    if (!isdigit((unsigned char)p->text[p->pos])) {
        set_error(p, p->pos);
        return make_int(0);
    }

    long long value = 0;

    while (p->pos < p->end &&
           isdigit((unsigned char)p->text[p->pos])) {
        value = value * 10 + (p->text[p->pos] - '0');
        p->pos++;
    }

    return make_int(value);
}

/*
 * At this stage the grammar is:
 *
 * expression := number { ('+' | '-') number }
 *
 * This is enough for grades 4 and 5. More precedence levels will be
 * added later for *, /, parentheses and **.
 */
static Value parse_expr(Parser *p) {
    Value result = parse_number(p);

    if (p->has_error) {
        return result;
    }

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end) {
            break;
        }

        char op = p->text[p->pos];

        if (op != '+' && op != '-') {
            break;
        }

        p->pos++;

        Value right = parse_number(p);

        if (p->has_error) {
            return result;
        }

        if (op == '+') {
            result.i += right.i;
        } else {
            result.i -= right.i;
        }

        result.f = (double)result.i;
    }

    return result;
}

static int ensure_directory(const char *path) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }

    if (errno == EEXIST) {
        return 1;
    }

    return 0;
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');

    if (slash != NULL) {
        return slash + 1;
    }

    return path;
}

/*
 * AI (ChatGPT), prompt:
 * "How can I read a whole file in C without assuming a fixed line length?"
 *
 * The buffer grows geometrically with realloc. This avoids a fixed
 * line-size limit and avoids repeatedly copying the whole file.
 */
static char *read_entire_file(FILE *file, size_t *length) {
    size_t capacity = 8192;
    size_t used = 0;

    char *data = malloc(capacity + 1);

    if (data == NULL) {
        return NULL;
    }

    while (1) {
        if (used == capacity) {
            size_t new_capacity = capacity * 2;
            char *new_data = realloc(data, new_capacity + 1);

            if (new_data == NULL) {
                free(data);
                return NULL;
            }

            data = new_data;
            capacity = new_capacity;
        }

        size_t count = fread(
            data + used,
            1,
            capacity - used,
            file
        );

        used += count;

        if (count == 0) {
            if (ferror(file)) {
                free(data);
                return NULL;
            }

            break;
        }
    }

    data[used] = '\0';
    *length = used;

    return data;
}

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Usage: calc -o OUTDIR input.txt\n");
        return 1;
    }

    const char *outdir = argv[2];
    const char *input_path = argv[3];

    FILE *in = fopen(input_path, "rb");

    if (in == NULL) {
        fprintf(stderr, "Cannot open input file.\n");
        return 1;
    }

    size_t file_length = 0;
    char *file_data = read_entire_file(in, &file_length);

    fclose(in);

    if (file_data == NULL) {
        fprintf(stderr, "Cannot read input file.\n");
        return 1;
    }

    if (!ensure_directory(outdir)) {
        fprintf(stderr, "Cannot create output directory.\n");
        free(file_data);
        return 1;
    }

    const char *filename = base_name(input_path);

    size_t base_length = strlen(filename) + 1;
    char *input_base = malloc(base_length);

    if (input_base == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(file_data);
        return 1;
    }

    strcpy(input_base, filename);

    char *dot = strrchr(input_base, '.');

    if (dot != NULL && strcmp(dot, ".txt") == 0) {
        *dot = '\0';
    }

    size_t output_length =
        strlen(outdir) +
        strlen(input_base) +
        strlen(STUDENT_NAME) +
        strlen(STUDENT_LASTNAME) +
        strlen(STUDENT_ID) +
        10;

    char *output_path = malloc(output_length);

    if (output_path == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(input_base);
        free(file_data);
        return 1;
    }

    snprintf(
        output_path,
        output_length,
        "%s/%s_%s_%s_%s.txt",
        outdir,
        input_base,
        STUDENT_NAME,
        STUDENT_LASTNAME,
        STUDENT_ID
    );

    free(input_base);

    FILE *out = fopen(output_path, "w");

    if (out == NULL) {
        fprintf(stderr, "Cannot open output file.\n");
        free(output_path);
        free(file_data);
        return 1;
    }

    size_t line_start = 0;

    while (line_start < file_length) {
        size_t line_end = line_start;

        while (line_end < file_length &&
               file_data[line_end] != '\n') {
            line_end++;
        }

        Parser p;

        p.text = file_data;
        p.pos = line_start;
        p.end = line_end;
        p.error_pos = 0;
        p.has_error = 0;

        skip_spaces(&p);

        /*
         * Blank lines produce no output.
         */
        if (p.pos < p.end) {
            Value result = parse_expr(&p);

            skip_spaces(&p);

            /*
             * If parsing stopped before the end, the current character is
             * the first unexpected token, so its position is the error.
             */
            if (!p.has_error && p.pos < p.end) {
                set_error(&p, p.pos);
            }

            if (p.has_error) {
                fclose(out);

                /*
                 * An error anywhere in the input invalidates every previous
                 * result, so reopen with "w" to erase partial output.
                 */
                out = fopen(output_path, "w");

                if (out == NULL) {
                    fprintf(stderr, "Cannot write output file.\n");
                    free(output_path);
                    free(file_data);
                    return 1;
                }

                fprintf(out, "ERROR:%zu\n", p.error_pos);

                fclose(out);
                free(output_path);
                free(file_data);

                return 0;
            }

            fprintf(out, "%lld\n", result.i);
        }

        if (line_end < file_length &&
            file_data[line_end] == '\n') {
            line_start = line_end + 1;
        } else {
            line_start = line_end;
        }
    }

    fclose(out);

    free(output_path);
    free(file_data);

    return 0;
}