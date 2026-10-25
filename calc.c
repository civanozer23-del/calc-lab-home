// Civan Ozer 251ADB130
// https://github.com/civanozer23-del/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (OpenAI), conversation history link:https://chatgpt.com/share/6ade4c0b-2300-83eb-9c69-a804d91e11ec 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
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
    char *text;
    size_t pos;
    size_t end;
    size_t error_pos;
    int has_error;
} Parser;

/*
 * AI (ChatGPT), prompt:
 * "Help me implement the RTU Arithmetic Parser lab using recursive
 * descent parsing, exact file positions, integer/float arithmetic,
 * parentheses and right-associative exponentiation."
 *
 * Parser positions are indexes in the complete input file.
 * The assignment reports positions starting from 1.
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

static Value make_int(long long x) {
    Value v;
    v.is_float = 0;
    v.i = x;
    v.f = (double)x;
    return v;
}

static Value make_float(double x) {
    Value v;
    v.is_float = 1;
    v.i = 0;
    v.f = x;
    return v;
}

static double as_double(Value v) {
    if (v.is_float) {
        return v.f;
    }

    return (double)v.i;
}

static int is_zero(Value v) {
    if (v.is_float) {
        return v.f == 0.0;
    }

    return v.i == 0;
}

static Value parse_expr(Parser *p);

/*
 * A valid float must have digits on both sides of '.'.
 * Therefore .5 and 5. are rejected at the '.'.
 *
 * After locating a valid decimal token, its ending character is
 * temporarily replaced by '\0' so strtod can convert exactly without
 * allocating and copying every number in a large input file.
 */
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

    size_t start = p->pos;
    long long integer_value = 0;

    while (p->pos < p->end &&
           isdigit((unsigned char)p->text[p->pos])) {
        integer_value =
            integer_value * 10 +
            (p->text[p->pos] - '0');

        p->pos++;
    }

    if (p->pos < p->end &&
        p->text[p->pos] == '.') {

        size_t dot_pos = p->pos;
        p->pos++;

        /*
         * "5." is invalid and the error belongs to '.'.
         */
        if (p->pos >= p->end ||
            !isdigit((unsigned char)p->text[p->pos])) {
            set_error(p, dot_pos);
            return make_int(0);
        }

        while (p->pos < p->end &&
               isdigit((unsigned char)p->text[p->pos])) {
            p->pos++;
        }

        size_t token_end = p->pos;
        char saved = p->text[token_end];

        p->text[token_end] = '\0';

        errno = 0;
        double value = strtod(p->text + start, NULL);

        p->text[token_end] = saved;

        /*
         * Extremely large floating literals are rejected rather
         * than allowing an infinite value into later arithmetic.
         */
        if (errno == ERANGE || !isfinite(value)) {
            set_error(p, start);
            return make_float(0.0);
        }

        return make_float(value);
    }

    return make_int(integer_value);
}

/*
 * primary := NUMBER | '(' expr ')'
 *
 * Calling expr() again inside parentheses naturally supports
 * nested parentheses while keeping the same precedence rules.
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

static long long integer_power(
    long long base,
    long long exponent
) {
    long long result = 1;

    while (exponent > 0) {
        if (exponent % 2 == 1) {
            result *= base;
        }

        exponent /= 2;

        if (exponent > 0) {
            base *= base;
        }
    }

    return result;
}

/*
 * power := primary [ '**' power ]
 *
 * AI (ChatGPT), prompt:
 * "How do I make ** right-associative in recursive descent?"
 *
 * The right side calls parse_power() recursively, so:
 * 2 ** 3 ** 2
 * groups as:
 * 2 ** (3 ** 2)
 *
 * If both operands are integers, exponentiation stays integer.
 * A negative integer exponent is an error at the exponent's start.
 * If either operand is floating-point, pow() is used.
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

        size_t operator_pos = p->pos;

        p->pos += 2;
        skip_spaces(p);

        size_t exponent_start = p->pos;

        Value right = parse_power(p);

        if (p->has_error) {
            return left;
        }

        if (!left.is_float &&
            !right.is_float) {

            if (right.i < 0) {
                set_error(p, exponent_start);
                return left;
            }

            left =
                make_int(
                    integer_power(left.i, right.i)
                );

        } else {

            double result =
                pow(
                    as_double(left),
                    as_double(right)
                );

            /*
             * NaN or infinity from floating-point exponentiation
             * is reported at the ** operator.
             */
            if (!isfinite(result)) {
                set_error(p, operator_pos);
                return left;
            }

            left = make_float(result);
        }
    }

    return left;
}

/*
 * term := power { ('*' | '/') power }
 *
 * Multiplication/division are below ** but above +/-.
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
         * Leave ** for parse_power().
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

        size_t operator_pos = p->pos;

        p->pos++;
        skip_spaces(p);

        size_t right_start = p->pos;

        Value right = parse_power(p);

        if (p->has_error) {
            return result;
        }

        if (op == '/' && is_zero(right)) {
            set_error(p, right_start);
            return result;
        }

        if (!result.is_float &&
            !right.is_float) {

            if (op == '*') {
                result.i *= right.i;
            } else {
                /*
                 * C signed integer division truncates toward zero.
                 */
                result.i /= right.i;
            }

            result.f = (double)result.i;

        } else {

            double a = as_double(result);
            double b = as_double(right);
            double value;

            if (op == '*') {
                value = a * b;
            } else {
                value = a / b;
            }

            if (!isfinite(value)) {
                set_error(p, operator_pos);
                return result;
            }

            result = make_float(value);
        }
    }

    return result;
}

/*
 * expr := term { ('+' | '-') term }
 *
 * Full precedence:
 *
 * primary -> **
 * **      -> * /
 * * /     -> + -
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

        size_t operator_pos = p->pos;

        p->pos++;

        Value right = parse_term(p);

        if (p->has_error) {
            return result;
        }

        if (!result.is_float &&
            !right.is_float) {

            if (op == '+') {
                result.i += right.i;
            } else {
                result.i -= right.i;
            }

            result.f = (double)result.i;

        } else {

            double a = as_double(result);
            double b = as_double(right);
            double value;

            if (op == '+') {
                value = a + b;
            } else {
                value = a - b;
            }

            if (!isfinite(value)) {
                set_error(p, operator_pos);
                return result;
            }

            result = make_float(value);
        }
    }

    return result;
}

/*
 * AI (ChatGPT), prompt:
 * "How can I read arbitrarily long input efficiently in C?"
 *
 * The buffer capacity doubles only when full. This keeps reading
 * linear in the input size and avoids fixed line-length limits.
 */
static char *read_entire_file(
    FILE *file,
    size_t *length
) {
    size_t capacity = 8192;
    size_t used = 0;

    char *data = malloc(capacity + 1);

    if (data == NULL) {
        return NULL;
    }

    while (1) {
        if (used == capacity) {
            size_t new_capacity =
                capacity * 2;

            char *new_data =
                realloc(
                    data,
                    new_capacity + 1
                );

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

static char *copy_string(const char *text) {
    size_t length = strlen(text);

    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);

    return copy;
}

static int ensure_directory(const char *path) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }

    if (errno == EEXIST) {
        struct stat st;

        if (stat(path, &st) == 0 &&
            S_ISDIR(st.st_mode)) {
            return 1;
        }
    }

    return 0;
}

static int is_regular_file(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISREG(st.st_mode);
}

static int is_directory_path(const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

/*
 * Return the final component of a path.
 * Trailing slashes are ignored.
 */
static char *path_name(const char *path) {
    size_t length = strlen(path);

    while (length > 1 &&
           path[length - 1] == '/') {
        length--;
    }

    size_t start = length;

    while (start > 0 &&
           path[start - 1] != '/') {
        start--;
    }

    size_t name_length = length - start;

    char *name = malloc(name_length + 1);

    if (name == NULL) {
        return NULL;
    }

    memcpy(
        name,
        path + start,
        name_length
    );

    name[name_length] = '\0';

    return name;
}

static char *remove_txt_extension(
    const char *name
) {
    size_t length = strlen(name);
    size_t base_length = length;

    if (length >= 4 &&
        strcmp(
            name + length - 4,
            ".txt"
        ) == 0) {

        base_length = length - 4;
    }

    char *base =
        malloc(base_length + 1);

    if (base == NULL) {
        return NULL;
    }

    memcpy(
        base,
        name,
        base_length
    );

    base[base_length] = '\0';

    return base;
}

static int has_txt_extension(
    const char *name
) {
    size_t length = strlen(name);

    return length >= 4 &&
           strcmp(
               name + length - 4,
               ".txt"
           ) == 0;
}

static char *join_path(
    const char *left,
    const char *right
) {
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);

    int need_slash =
        left_length > 0 &&
        left[left_length - 1] != '/';

    size_t total =
        left_length +
        right_length +
        (need_slash ? 1 : 0) +
        1;

    char *result = malloc(total);

    if (result == NULL) {
        return NULL;
    }

    snprintf(
        result,
        total,
        need_slash
            ? "%s/%s"
            : "%s%s",
        left,
        right
    );

    return result;
}

static char *make_default_output_dir(
    const char *base
) {
    size_t length =
        strlen(base) +
        strlen(STUDENT_NAME) +
        strlen(STUDENT_ID) +
        3;

    char *result = malloc(length);

    if (result == NULL) {
        return NULL;
    }

    snprintf(
        result,
        length,
        "%s_%s_%s",
        base,
        STUDENT_NAME,
        STUDENT_ID
    );

    return result;
}

static char *make_output_filename(
    const char *input_name
) {
    char *base =
        remove_txt_extension(input_name);

    if (base == NULL) {
        return NULL;
    }

    size_t length =
        strlen(base) +
        strlen(STUDENT_NAME) +
        strlen(STUDENT_LASTNAME) +
        strlen(STUDENT_ID) +
        8;

    char *result = malloc(length);

    if (result == NULL) {
        free(base);
        return NULL;
    }

    snprintf(
        result,
        length,
        "%s_%s_%s_%s.txt",
        base,
        STUDENT_NAME,
        STUDENT_LASTNAME,
        STUDENT_ID
    );

    free(base);

    return result;
}

static void print_value(
    FILE *out,
    Value value
) {
    if (value.is_float) {
        fprintf(
            out,
            "%.15g\n",
            value.f
        );
    } else {
        fprintf(
            out,
            "%lld\n",
            value.i
        );
    }
}

/*
 * Evaluate one input file.
 *
 * ERROR:<pos> is a normal calculator result and therefore returns 0.
 * File-system or operating errors return a non-zero status.
 */
static int process_file(
    const char *input_path,
    const char *outdir
) {
    FILE *in =
        fopen(input_path, "rb");

    if (in == NULL) {
        fprintf(
            stderr,
            "Cannot open input file: %s\n",
            input_path
        );

        return 1;
    }

    size_t file_length = 0;

    char *file_data =
        read_entire_file(
            in,
            &file_length
        );

    fclose(in);

    if (file_data == NULL) {
        fprintf(
            stderr,
            "Cannot read input file: %s\n",
            input_path
        );

        return 1;
    }

    char *input_name =
        path_name(input_path);

    if (input_name == NULL) {
        fprintf(
            stderr,
            "Memory allocation failed.\n"
        );

        free(file_data);
        return 1;
    }

    char *output_name =
        make_output_filename(
            input_name
        );

    free(input_name);

    if (output_name == NULL) {
        fprintf(
            stderr,
            "Memory allocation failed.\n"
        );

        free(file_data);
        return 1;
    }

    char *output_path =
        join_path(
            outdir,
            output_name
        );

    free(output_name);

    if (output_path == NULL) {
        fprintf(
            stderr,
            "Memory allocation failed.\n"
        );

        free(file_data);
        return 1;
    }

    FILE *out =
        fopen(output_path, "w");

    if (out == NULL) {
        fprintf(
            stderr,
            "Cannot write output file: %s\n",
            output_path
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
         * Blank lines produce no result.
         * '#' starts a comment only when it is the first
         * non-blank character on the line.
         */
        if (p.pos < p.end &&
            file_data[p.pos] != '#') {

            Value result =
                parse_expr(&p);

            skip_spaces(&p);

            /*
             * Anything left after a complete expression is the
             * first invalid token. This also catches e/E in
             * unsupported scientific notation.
             */
            if (!p.has_error &&
                p.pos < p.end) {

                set_error(
                    &p,
                    p.pos
                );
            }

            if (p.has_error) {
                fclose(out);

                /*
                 * Any error anywhere in the input removes all
                 * partial results already written.
                 */
                out =
                    fopen(
                        output_path,
                        "w"
                    );

                if (out == NULL) {
                    fprintf(
                        stderr,
                        "Cannot write output file: %s\n",
                        output_path
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

            print_value(
                out,
                result
            );
        }

        if (line_end < file_length &&
            file_data[line_end] == '\n') {

            line_start =
                line_end + 1;

        } else {

            line_start =
                line_end;
        }
    }

    fclose(out);
    free(output_path);
    free(file_data);

    return 0;
}

/*
 * Directory entries are collected before processing.
 * This prevents output files from becoming new inputs if the
 * output directory happens to be the same as the input directory.
 */
static int process_directory(
    const char *dir_path,
    const char *outdir
) {
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        fprintf(
            stderr,
            "Cannot open input directory: %s\n",
            dir_path
        );

        return 1;
    }

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    struct dirent *entry;

    while ((entry = readdir(dir))
           != NULL) {

        if (!has_txt_extension(
                entry->d_name)) {
            continue;
        }

        char *full_path =
            join_path(
                dir_path,
                entry->d_name
            );

        if (full_path == NULL) {
            fprintf(
                stderr,
                "Memory allocation failed.\n"
            );

            closedir(dir);

            for (size_t i = 0;
                 i < count;
                 i++) {
                free(names[i]);
            }

            free(names);

            return 1;
        }

        int regular =
            is_regular_file(
                full_path
            );

        free(full_path);

        if (!regular) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity =
                capacity == 0
                    ? 8
                    : capacity * 2;

            char **new_names =
                realloc(
                    names,
                    new_capacity *
                    sizeof(*new_names)
                );

            if (new_names == NULL) {
                fprintf(
                    stderr,
                    "Memory allocation failed.\n"
                );

                closedir(dir);

                for (size_t i = 0;
                     i < count;
                     i++) {
                    free(names[i]);
                }

                free(names);

                return 1;
            }

            names = new_names;
            capacity = new_capacity;
        }

        names[count] =
            copy_string(
                entry->d_name
            );

        if (names[count] == NULL) {
            fprintf(
                stderr,
                "Memory allocation failed.\n"
            );

            closedir(dir);

            for (size_t i = 0;
                 i < count;
                 i++) {
                free(names[i]);
            }

            free(names);

            return 1;
        }

        count++;
    }

    closedir(dir);

    for (size_t i = 0;
         i < count;
         i++) {

        char *input_path =
            join_path(
                dir_path,
                names[i]
            );

        if (input_path == NULL) {
            fprintf(
                stderr,
                "Memory allocation failed.\n"
            );

            for (size_t j = 0;
                 j < count;
                 j++) {
                free(names[j]);
            }

            free(names);

            return 1;
        }

        int status =
            process_file(
                input_path,
                outdir
            );

        free(input_path);

        if (status != 0) {
            for (size_t j = 0;
                 j < count;
                 j++) {
                free(names[j]);
            }

            free(names);

            return status;
        }
    }

    for (size_t i = 0;
         i < count;
         i++) {
        free(names[i]);
    }

    free(names);

    return 0;
}

static void print_usage(void) {
    fprintf(
        stderr,
        "Usage: calc [-d DIR | --dir DIR] "
        "[-o OUTDIR | --output-dir OUTDIR] "
        "[input.txt]\n"
    );
}

int main(int argc, char *argv[]) {
    const char *dir_path = NULL;
    const char *input_file = NULL;
    const char *requested_outdir = NULL;

    /*
     * AI (ChatGPT), prompt:
     * "Implement the assignment CLI with -d/--dir,
     * -o/--output-dir and an optional input file."
     *
     * Options may appear in either order, but conflicting
     * input modes or unknown options are rejected.
     */
    for (int i = 1;
         i < argc;
         i++) {

        if (strcmp(
                argv[i],
                "-d"
            ) == 0 ||
            strcmp(
                argv[i],
                "--dir"
            ) == 0) {

            if (i + 1 >= argc ||
                dir_path != NULL ||
                input_file != NULL) {

                print_usage();
                return 1;
            }

            dir_path =
                argv[++i];

        } else if (
            strcmp(
                argv[i],
                "-o"
            ) == 0 ||
            strcmp(
                argv[i],
                "--output-dir"
            ) == 0
        ) {

            if (i + 1 >= argc ||
                requested_outdir != NULL) {

                print_usage();
                return 1;
            }

            requested_outdir =
                argv[++i];

        } else if (
            argv[i][0] == '-'
        ) {

            fprintf(
                stderr,
                "Unknown option: %s\n",
                argv[i]
            );

            return 1;

        } else {

            if (input_file != NULL ||
                dir_path != NULL) {

                print_usage();
                return 1;
            }

            input_file =
                argv[i];
        }
    }

    if (input_file == NULL &&
        dir_path == NULL) {

        print_usage();
        return 1;
    }

    if (input_file != NULL &&
        !is_regular_file(
            input_file
        )) {

        fprintf(
            stderr,
            "Input file does not exist: %s\n",
            input_file
        );

        return 1;
    }

    if (dir_path != NULL &&
        !is_directory_path(
            dir_path
        )) {

        fprintf(
            stderr,
            "Input directory does not exist: %s\n",
            dir_path
        );

        return 1;
    }

    char *default_outdir = NULL;
    const char *outdir =
        requested_outdir;

    /*
     * Default folder:
     *
     * task1.txt -> task1_civan_251ADB130
     * labs      -> labs_civan_251ADB130
     */
    if (outdir == NULL) {
        char *base;

        if (input_file != NULL) {
            char *name =
                path_name(
                    input_file
                );

            if (name == NULL) {
                fprintf(
                    stderr,
                    "Memory allocation failed.\n"
                );

                return 1;
            }

            base =
                remove_txt_extension(
                    name
                );

            free(name);

        } else {

            base =
                path_name(
                    dir_path
                );
        }

        if (base == NULL) {
            fprintf(
                stderr,
                "Memory allocation failed.\n"
            );

            return 1;
        }

        default_outdir =
            make_default_output_dir(
                base
            );

        free(base);

        if (default_outdir == NULL) {
            fprintf(
                stderr,
                "Memory allocation failed.\n"
            );

            return 1;
        }

        outdir =
            default_outdir;
    }

    if (!ensure_directory(outdir)) {
        fprintf(
            stderr,
            "Cannot create output directory: %s\n",
            outdir
        );

        free(default_outdir);

        return 1;
    }

    int status;

    if (input_file != NULL) {
        status =
            process_file(
                input_file,
                outdir
            );
    } else {
        status =
            process_directory(
                dir_path,
                outdir
            );
    }

    free(default_outdir);

    return status;
}