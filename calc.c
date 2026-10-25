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
    long long i;
} Value;

typedef struct {
    const char *text;
    size_t pos;
    size_t end;
    size_t error_pos;
    int has_error;
} Parser;

/*
 * AI (ChatGPT): helped design the recursive-descent parser,
 * precedence structure and exact error-position tracking.
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
           (p->text[p->pos] == ' ' ||
            p->text[p->pos] == '\t')) {
        p->pos++;
    }
}

static Value make_int(long long value) {
    Value v;
    v.i = value;
    return v;
}

static Value parse_expr(Parser *p);

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

        value = value * 10 +
                (p->text[p->pos] - '0');

        p->pos++;
    }

    return make_int(value);
}

/*
 * primary := NUMBER | '(' expr ')'
 */
static Value parse_primary(Parser *p) {
    skip_spaces(p);

    if (p->pos >= p->end) {
        set_end_error(p);
        return make_int(0);
    }

    if (p->text[p->pos] == '(') {
        p->pos++;

        Value result = parse_expr(p);

        if (p->has_error) {
            return result;
        }

        skip_spaces(p);

        if (p->pos >= p->end) {
            set_end_error(p);
            return result;
        }

        if (p->text[p->pos] != ')') {
            set_error(p, p->pos);
            return result;
        }

        p->pos++;
        return result;
    }

    return parse_number(p);
}

/*
 * Integer exponentiation by repeated squaring.
 * The assignment states that integer tests do not overflow.
 */
static long long integer_power(long long base, long long exponent) {
    long long result = 1;

    while (exponent > 0) {
        if (exponent % 2 == 1) {
            result *= base;
        }

        base *= base;
        exponent /= 2;
    }

    return result;
}

/*
 * AI (ChatGPT): prompt about implementing right-associative **.
 *
 * power := primary [ '**' power ]
 *
 * power() calls itself for the exponent, therefore:
 * 2 ** 3 ** 2
 * becomes:
 * 2 ** (3 ** 2)
 *
 * rather than:
 * (2 ** 3) ** 2
 */
static Value parse_power(Parser *p) {
    Value left = parse_primary(p);

    if (p->has_error) {
        return left;
    }

    skip_spaces(p);

    if (p->pos + 1 < p->end &&
        p->text[p->pos] == '*' &&
        p->text[p->pos + 1] == '*') {

        p->pos += 2;

        skip_spaces(p);

        /*
         * Negative exponent errors are reported at the start
         * of the exponent expression.
         */
        size_t exponent_start = p->pos;

        Value right = parse_power(p);

        if (p->has_error) {
            return left;
        }

        if (right.i < 0) {
            set_error(p, exponent_start);
            return left;
        }

        left.i = integer_power(left.i, right.i);
    }

    return left;
}

/*
 * term handles ordinary * and /.
 * '**' is deliberately left for parse_power().
 */
static Value parse_term(Parser *p) {
    Value result = parse_power(p);

    if (p->has_error) {
        return result;
    }

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end) {
            break;
        }

        /*
         * Do not consume the first '*' of '**'.
         */
        if (p->pos + 1 < p->end &&
            p->text[p->pos] == '*' &&
            p->text[p->pos + 1] == '*') {
            break;
        }

        char op = p->text[p->pos];

        if (op != '*' && op != '/') {
            break;
        }

        p->pos++;

        skip_spaces(p);

        size_t right_start = p->pos;

        Value right = parse_power(p);

        if (p->has_error) {
            return result;
        }

        if (op == '*') {
            result.i *= right.i;
        } else {
            if (right.i == 0) {
                set_error(p, right_start);
                return result;
            }

            result.i /= right.i;
        }
    }

    return result;
}

/*
 * Full grammar through Grade 8:
 *
 * expr    := term  { ('+' | '-') term }
 * term    := power { ('*' | '/') power }
 * power   := primary [ '**' power ]
 * primary := NUMBER | '(' expr ')'
 */
static Value parse_expr(Parser *p) {
    Value result = parse_term(p);

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

        Value right = parse_term(p);

        if (p->has_error) {
            return result;
        }

        if (op == '+') {
            result.i += right.i;
        } else {
            result.i -= right.i;
        }
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
 * AI (ChatGPT): suggested a geometrically growing realloc buffer
 * so the input is not limited by a fixed line length.
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

            char *new_data =
                realloc(data, new_capacity + 1);

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
    if (argc != 4 ||
        strcmp(argv[1], "-o") != 0) {

        fprintf(
            stderr,
            "Usage: calc -o OUTDIR input.txt\n"
        );

        return 1;
    }

    const char *outdir = argv[2];
    const char *input_path = argv[3];

    FILE *in = fopen(input_path, "rb");

    if (in == NULL) {
        fprintf(
            stderr,
            "Cannot open input file.\n"
        );

        return 1;
    }

    size_t file_length = 0;

    char *file_data =
        read_entire_file(in, &file_length);

    fclose(in);

    if (file_data == NULL) {
        fprintf(
            stderr,
            "Cannot read input file.\n"
        );

        return 1;
    }

    if (!ensure_directory(outdir)) {
        fprintf(
            stderr,
            "Cannot create output directory.\n"
        );

        free(file_data);
        return 1;
    }

    const char *filename =
        base_name(input_path);

    size_t base_length =
        strlen(filename) + 1;

    char *input_base =
        malloc(base_length);

    if (input_base == NULL) {
        fprintf(
            stderr,
            "Memory allocation failed.\n"
        );

        free(file_data);
        return 1;
    }

    strcpy(input_base, filename);

    char *dot = strrchr(input_base, '.');

    if (dot != NULL &&
        strcmp(dot, ".txt") == 0) {
        *dot = '\0';
    }

    size_t output_length =
        strlen(outdir) +
        strlen(input_base) +
        strlen(STUDENT_NAME) +
        strlen(STUDENT_LASTNAME) +
        strlen(STUDENT_ID) +
        10;

    char *output_path =
        malloc(output_length);

    if (output_path == NULL) {
        fprintf(
            stderr,
            "Memory allocation failed.\n"
        );

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
        fprintf(
            stderr,
            "Cannot open output file.\n"
        );

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
         *
         * A line whose first non-blank character is '#'
         * is a comment and is ignored completely.
         */
        if (p.pos < p.end &&
            file_data[p.pos] != '#') {

            Value result = parse_expr(&p);

            skip_spaces(&p);

            /*
             * A '#' here is not a comment because it was not
             * the first non-blank character. It therefore becomes
             * an invalid token at its exact position.
             */
            if (!p.has_error &&
                p.pos < p.end) {
                set_error(&p, p.pos);
            }

            if (p.has_error) {
                fclose(out);

                out = fopen(output_path, "w");

                if (out == NULL) {
                    fprintf(
                        stderr,
                        "Cannot write output file.\n"
                    );

                    free(output_path);
                    free(file_data);

                    return 1;
                }

                fprintf(
                    out,
                    "ERROR:%zu\n",
                    p.error_pos
                );

                fclose(out);
                free(output_path);
                free(file_data);

                return 0;
            }

            fprintf(
                out,
                "%lld\n",
                result.i
            );
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