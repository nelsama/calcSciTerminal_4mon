/**
 * ============================================================================
 * parser.h - Expression Parser for Scientific Calculator
 * ============================================================================
 * Recursive descent parser for arithmetic expressions with functions.
 * Uses MSBasic floating point library for evaluation.
 * 
 * Supported operations (precedence highest to lowest):
 *   1. Parentheses / Functions: (), sin(), cos(), tan(), atan(), log(),
 *      exp(), sqrt(), sqr(), abs()
 *   2. Unary minus: -
 *   3. Power: ^ (right-associative)
 *   4. Multiply/Divide: *, /
 *   5. Add/Subtract: +, -
 * 
 * Usage:
 *   msbasic_float_t result;
 *   if (parse_expression("2+3*sin(0.5)", &result) == 0) {
 *       // result contains the evaluated value
 *   } else {
 *       const char *err = parser_get_error();
 *   }
 * ============================================================================
 */

#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include "msbasic_float.h"

/**
 * Parse and evaluate a mathematical expression.
 *
 * @param input  Null-terminated string with the expression
 * @param result Pointer to msbasic_float_t where result is stored
 * @return 0 on success, non-zero on error
 */
uint8_t parse_expression(const char *input, msbasic_float_t *result);

/**
 * Get the error message from the last failed parse.
 *
 * @return Pointer to a null-terminated error string
 */
const char* parser_get_error(void);

#endif /* PARSER_H */
