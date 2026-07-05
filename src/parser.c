/**
 * ============================================================================
 * parser.c - Recursive Descent Expression Parser
 * ============================================================================
 * Parses and evaluates arithmetic expressions using MSBasic floating point.
 * 
 * Grammar:
 *   expression := addsub
 *   addsub     := muldiv (('+' | '-') muldiv)*
 *   muldiv     := power (('*' | '/') power)*
 *   power      := unary ('^' power)?
 *   unary      := '-' atom | atom
 *   atom       := number | function | '(' expression ')'
 *   function   := func_name '(' expression ')'
 *   number     := digit+ ('.' digit*)? | '.' digit+
 * ============================================================================
 */

#include "parser.h"
#include <stdint.h>

/* Define NULL for pointer comparisons */
#ifndef NULL
#define NULL 0
#endif

/* ============================================================================
 * PARSER STATE
 * ============================================================================ */

/** Current position in the input string being parsed */
static const char *g_pos;

/** Error message from last failed parse (NULL if no error) */
static const char *g_error;

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static uint8_t parse_addsub(msbasic_float_t *result);
static uint8_t parse_muldiv(msbasic_float_t *result);
static uint8_t parse_power(msbasic_float_t *result);
static uint8_t parse_unary(msbasic_float_t *result);
static uint8_t parse_atom(msbasic_float_t *result);

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * Skip whitespace characters (space, tab) at current position.
 */
static void skip_ws(void) {
    while (*g_pos == ' ' || *g_pos == '\t') {
        g_pos++;
    }
}

/**
 * Check if character c is a digit (0-9).
 */
static uint8_t is_digit(char c) {
    return (c >= '0' && c <= '9');
}

/**
 * Check if character c is a lowercase letter (a-z).
 */
static uint8_t is_letter(char c) {
    return (c >= 'a' && c <= 'z');
}

/**
 * Negate a floating point value: x = 0 - x
 */
static void fp_negate(msbasic_float_t *x) {
    msbasic_float_t zero = FP_ZERO;
    fp_sub(&zero, x, x);
}

/* ============================================================================
 * NUMBER PARSING
 * ============================================================================ */

/**
 * Parse a number (digits and optional decimal point) into result.
 * Supports: "123", "3.14", ".5", "123."
 *
 * @param result Pointer to store the parsed float
 * @return 0 on success, 1 on error
 */
static uint8_t parse_number(msbasic_float_t *result) {
    char num_buf[24];  /* Buffer for the number string */
    uint8_t i = 0;
    uint8_t has_digits = 0;

    /* Copy digit sequence including optional decimal point */
    while (is_digit(*g_pos) || *g_pos == '.') {
        has_digits = 1;
        if (i < sizeof(num_buf) - 1) {
            num_buf[i++] = *g_pos;
        }
        g_pos++;
    }
    num_buf[i] = '\0';

    if (!has_digits) {
        g_error = "Expected number";
        return 1;
    }

    fp_string_to_float(num_buf, result);
    return 0;
}

/* ============================================================================
 * FUNCTION PARSING
 * ============================================================================ */

/**
 * Table of supported function names and their implementations.
 * Uses a simple linear search since there are only 9 functions.
 */

/** Function types */
typedef enum {
    FUNC_SIN,
    FUNC_COS,
    FUNC_TAN,
    FUNC_ATAN,
    FUNC_LOG,
    FUNC_EXP,
    FUNC_SQRT,
    FUNC_SQR,
    FUNC_ABS,
    FUNC_D2R,
    FUNC_R2D
} func_type_t;

/* Constantes en formato byte array (5 bytes float MSBasic) */
/* π = $82,$49,$0F,$DA,$A2 */
static const unsigned char PI_BYTES[5] = {0x82, 0x49, 0x0F, 0xDA, 0xA2};
/* 180 = $88,$34,$00,$00,$00 */
static const unsigned char C180_BYTES[5] = {0x88, 0x34, 0x00, 0x00, 0x00};

static void fp_set_pi(msbasic_float_t *f) {
    unsigned char i;
    for (i = 0; i < 5; i++) f->bytes[i] = PI_BYTES[i];
}

static void fp_set_180(msbasic_float_t *f) {
    unsigned char i;
    for (i = 0; i < 5; i++) f->bytes[i] = C180_BYTES[i];
}



/**
 * Apply a function to an argument and store the result.
 * @return 0 on success, 1 on error
 */
static uint8_t apply_function(func_type_t func, const msbasic_float_t *arg, msbasic_float_t *result) {
    msbasic_float_t temp;
    msbasic_float_t pi_val;
    msbasic_float_t c180_val;
    
    switch (func) {
        case FUNC_SIN:  fp_sin(arg, result);  break;
        case FUNC_COS:  fp_cos(arg, result);  break;
        case FUNC_TAN:  fp_tan(arg, result);  break;
        case FUNC_ATAN: fp_atn(arg, result);  break;
        case FUNC_LOG:
            if (arg->bytes[0] == 0 || (arg->bytes[1] & 0x80)) {
                g_error = "Math error";
                return 1;
            }
            fp_log(arg, result);
            break;
        case FUNC_EXP:  fp_exp(arg, result);  break;
        case FUNC_SQRT:
        case FUNC_SQR:
            if (arg->bytes[0] != 0 && (arg->bytes[1] & 0x80)) {
                g_error = "Math error";
                return 1;
            }
            fp_sqr(arg, result);
            break;
        case FUNC_ABS:  fp_abs(arg, result);  break;
        case FUNC_D2R:
            fp_set_pi(&pi_val);
            fp_set_180(&c180_val);
            fp_mul(arg, &pi_val, &temp);
            fp_div(&temp, &c180_val, result);
            break;
        case FUNC_R2D:
            fp_set_pi(&pi_val);
            fp_set_180(&c180_val);
            fp_mul(arg, &c180_val, &temp);
            fp_div(&temp, &pi_val, result);
            break;
    }
    return 0;
}

/**
 * Parse a function call: func_name '(' expression ')'
 *
 * @param result Pointer to store the result
 * @return 0 on success, 1 on error
 */
static uint8_t parse_function(msbasic_float_t *result) {
    func_type_t func;
    const char *func_start;
    uint8_t name_len;
    msbasic_float_t arg;
    
    func_start = g_pos;
    name_len = 0;

    /* Identify function name (letters and digits) */
    while (is_letter(*g_pos) || is_digit(*g_pos)) {
        g_pos++;
        name_len++;
    }

    /* Determine which function by comparing the name at func_start.
     * g_pos is now past the name, so we compare func_start directly. */
    if (name_len == 3) {
        /* Compare first 3 characters from func_start */
        if (func_start[0] == 's' && func_start[1] == 'i' && func_start[2] == 'n') {
            func = FUNC_SIN;
        } else if (func_start[0] == 'c' && func_start[1] == 'o' && func_start[2] == 's') {
            func = FUNC_COS;
        } else if (func_start[0] == 't' && func_start[1] == 'a' && func_start[2] == 'n') {
            func = FUNC_TAN;
        } else if (func_start[0] == 'l' && func_start[1] == 'o' && func_start[2] == 'g') {
            func = FUNC_LOG;
        } else if (func_start[0] == 'e' && func_start[1] == 'x' && func_start[2] == 'p') {
            func = FUNC_EXP;
        } else if (func_start[0] == 'a' && func_start[1] == 'b' && func_start[2] == 's') {
            func = FUNC_ABS;
        } else if (func_start[0] == 's' && func_start[1] == 'q' && func_start[2] == 'r') {
            func = FUNC_SQR;
        } else if (func_start[0] == 'd' && func_start[1] == '2' && func_start[2] == 'r') {
            func = FUNC_D2R;
        } else if (func_start[0] == 'r' && func_start[1] == '2' && func_start[2] == 'd') {
            func = FUNC_R2D;
        } else {
            g_error = "Unknown function";
            return 1;
        }
    } else if (name_len == 4) {
        if (func_start[0] == 's' && func_start[1] == 'q' && func_start[2] == 'r' && func_start[3] == 't') {
            func = FUNC_SQRT;
        } else if (func_start[0] == 'a' && func_start[1] == 't' && func_start[2] == 'a' && func_start[3] == 'n') {
            func = FUNC_ATAN;
        } else {
            g_error = "Unknown function";
            return 1;
        }
    } else {
        g_error = "Unknown function";
        return 1;
    }

    /* Expect opening parenthesis */
    skip_ws();
    if (*g_pos != '(') {
        g_error = "Expected '('";
        return 1;
    }
    g_pos++;  /* skip '(' */

    /* Parse argument expression */
    if (parse_addsub(&arg)) return 1;

    /* Expect closing parenthesis */
    skip_ws();
    if (*g_pos != ')') {
        g_error = "Expected ')'";
        return 1;
    }
    g_pos++;  /* skip ')' */

    /* Apply the function */
    if (apply_function(func, &arg, result)) return 1;
    return 0;
}

/* ============================================================================
 * ATOM PARSING
 * ============================================================================ */

/**
 * Parse an atom: number, function call, or parenthesized expression.
 * Grammar: atom := number | function | '(' expression ')'
 *
 * @param result Pointer to store the result
 * @return 0 on success, 1 on error
 */
static uint8_t parse_atom(msbasic_float_t *result) {
    skip_ws();

    if (*g_pos == '\0') {
        g_error = "Unexpected end of expression";
        return 1;
    }

    /* Parenthesized expression */
    if (*g_pos == '(') {
        g_pos++;  /* skip '(' */
        if (parse_addsub(result)) return 1;
        skip_ws();
        if (*g_pos != ')') {
            g_error = "Expected ')'";
            return 1;
        }
        g_pos++;  /* skip ')' */
        return 0;
    }

    /* Function call (starts with a letter) */
    if (is_letter(*g_pos)) {
        /* Check for 'pi' constant (2 letters, not followed by letter or digit) */
        if (g_pos[0] == 'p' && g_pos[1] == 'i' && !is_letter(g_pos[2]) && !is_digit(g_pos[2])) {
            fp_set_pi(result);
            g_pos += 2;
            return 0;
        }
        return parse_function(result);
    }

    /* Number */
    if (is_digit(*g_pos) || *g_pos == '.') {
        return parse_number(result);
    }

    g_error = "Syntax error";
    return 1;
}

/* ============================================================================
 * UNARY MINUS
 * ============================================================================ */

/**
 * Parse unary minus and atom.
 * Grammar: unary := '-' atom | atom
 *
 * @param result Pointer to store the result
 * @return 0 on success, 1 on error
 */
static uint8_t parse_unary(msbasic_float_t *result) {
    skip_ws();

    if (*g_pos == '-') {
        g_pos++;  /* skip '-' */
        if (parse_atom(result)) return 1;
        fp_negate(result);
        return 0;
    }

    return parse_atom(result);
}

/* ============================================================================
 * POWER (right-associative)
 * ============================================================================ */

/**
 * Parse power operator (right-associative).
 * Grammar: power := unary ('^' power)?
 *
 * @param result Pointer to store the result
 * @return 0 on success, 1 on error
 */
static uint8_t parse_power(msbasic_float_t *result) {
    msbasic_float_t left;
    msbasic_float_t right;
    
    if (parse_unary(&left)) return 1;

    skip_ws();

    if (*g_pos == '^') {
        g_pos++;  /* skip '^' */

        /* Recursive call for right-associativity: a^b^c = a^(b^c) */
        if (parse_power(&right)) return 1;

        fp_pow(&left, &right, result);
        return 0;
    }

    *result = left;
    return 0;
}

/* ============================================================================
 * MULTIPLY / DIVIDE (left-associative)
 * ============================================================================ */

/**
 * Parse multiply and divide operators (left-associative).
 * Grammar: muldiv := power (('*' | '/') power)*
 *
 * @param result Pointer to store the result
 * @return 0 on success, 1 on error
 */
static uint8_t parse_muldiv(msbasic_float_t *result) {
    msbasic_float_t left;
    msbasic_float_t right;
    
    if (parse_power(&left)) return 1;

    while (1) {
        skip_ws();

        if (*g_pos == '*') {
            g_pos++;  /* skip '*' */
            if (parse_power(&right)) return 1;
            fp_mul(&left, &right, &left);
        } else if (*g_pos == '/') {
            g_pos++;  /* skip '/' */
            if (parse_power(&right)) return 1;
            /* Check division by zero */
            if (right.bytes[0] == 0) {
                g_error = "Division by zero";
                return 1;
            }
            fp_div(&left, &right, &left);
        } else {
            break;
        }
    }

    *result = left;
    return 0;
}

/* ============================================================================
 * ADD / SUBTRACT (left-associative)
 * ============================================================================ */

/**
 * Parse add and subtract operators (left-associative).
 * Grammar: addsub := muldiv (('+' | '-') muldiv)*
 *
 * @param result Pointer to store the result
 * @return 0 on success, 1 on error
 */
static uint8_t parse_addsub(msbasic_float_t *result) {
    msbasic_float_t left;
    msbasic_float_t right;
    
    if (parse_muldiv(&left)) return 1;

    while (1) {
        skip_ws();

        if (*g_pos == '+') {
            g_pos++;  /* skip '+' */
            if (parse_muldiv(&right)) return 1;
            fp_add(&left, &right, &left);
        } else if (*g_pos == '-') {
            g_pos++;  /* skip '-' */
            if (parse_muldiv(&right)) return 1;
            fp_sub(&left, &right, &left);
        } else {
            break;
        }
    }

    *result = left;
    return 0;
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

/**
 * Parse and evaluate a mathematical expression.
 *
 * @param input  Null-terminated string with the expression
 * @param result Pointer to msbasic_float_t where result is stored
 * @return 0 on success, non-zero on error
 */
uint8_t parse_expression(const char *input, msbasic_float_t *result) {
    g_pos = input;
    g_error = NULL;
    
    skip_ws();

    /* Empty input: return zero */
    if (*g_pos == '\0') {
        result->bytes[0] = 0;
        result->bytes[1] = 0;
        result->bytes[2] = 0;
        result->bytes[3] = 0;
        result->bytes[4] = 0;
        return 0;
    }

    /* Parse the expression */
    if (parse_addsub(result)) {
        return 1;
    }

    /* After parsing, ensure no unexpected trailing characters */
    skip_ws();
    if (*g_pos != '\0') {
        g_error = "Syntax error";
        return 1;
    }

    return 0;
}

/**
 * Get the error message from the last failed parse.
 *
 * @return Pointer to a null-terminated error string, or empty string if no error
 */
const char* parser_get_error(void) {
    if (g_error) {
        return g_error;
    }
    return "";
}
