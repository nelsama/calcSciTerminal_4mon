/**
 * ============================================================================
 * Calculadora Científica - Monitor 6502 (Terminal UART)
 * ============================================================================
 * Calculadora de punto flotante con interfaz de terminal vía UART.
 * 
 * Características:
 *   - Expresiones aritméticas completas (+, -, *, /)
 *   - Paréntesis y anidamiento
 *   - Potencia (^)
 *   - Funciones: sin, cos, tan, atan, log, exp, sqrt, sqr, abs
 *   - Precedencia de operadores y asociatividad
 * 
 * Uso:
 *   LOAD CALC 0800
 *   R 0800
 * 
 * ROM API utilizada:
 *   - rom_uart_putc()    - Enviar un carácter
 *   - rom_uart_getc()    - Recibir un carácter (bloqueante)
 *   - rom_uart_rx_ready()- Verificar si hay datos disponibles
 *   - rom_uart_puts()    - Enviar cadena de texto
 *   - rom_delay_ms()     - Delay en milisegundos
 * 
 * Dependencias:
 *   - MSBasic Float Library (punto flotante)
 *   - parser.h / parser.c (evaluación de expresiones)
 * ============================================================================
 */

#include <stdint.h>
#include "romapi.h"
#include "msbasic_float.h"
#include "parser.h"

/* ============================================================================
 * CONSTANTES
 * ============================================================================ */

/** Versión del programa (definidas en makefile, con fallback) */
#ifndef VERSION_MAJOR
#define VERSION_MAJOR   1
#define VERSION_MINOR   0
#define VERSION_PATCH   0
#endif

/* Construir string de versión a partir de los números */
#define STR_HELPER(x)   #x
#define STR(x)          STR_HELPER(x)
#define VERSION_STR     STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH)

/** Tamaño máximo del buffer de entrada */
#define INPUT_BUF_SIZE  64

/** Tamaño máximo del buffer de salida para números flotantes */
#define OUTPUT_BUF_SIZE 40

/** Constantes de caracteres */
#define CHAR_BS         0x08    /* Backspace */
#define CHAR_DEL        0x7F    /* Delete */
#define CHAR_CR         0x0D    /* Carriage Return */
#define CHAR_LF         0x0A    /* Line Feed */
#define CHAR_SPACE      0x20    /* Space */

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */

/** Buffer de entrada para una línea de comando */
static char input_buffer[INPUT_BUF_SIZE];

/** Longitud actual del buffer de entrada */
static uint8_t input_len;

/* ============================================================================
 * FUNCIONES AUXILIARES
 * ============================================================================ */

/**
 * Enviar una cadena terminada en null por UART.
 */
static void uart_print(const char *s) {
    rom_uart_puts(s);
}

/**
 * Enviar un sólo carácter por UART.
 */
static void uart_putc(char c) {
    rom_uart_putc(c);
}

/**
 * Enviar secuencia de nueva línea (CR+LF).
 */
static void uart_newline(void) {
    uart_putc(CHAR_CR);
    uart_putc(CHAR_LF);
}

/**
 * Borrar el último carácter de la línea actual (backspace visual).
 */
static void uart_backspace(void) {
    uart_putc(CHAR_BS);
    uart_putc(' ');
    uart_putc(CHAR_BS);
}

/**
 * Inicializar el buffer de entrada.
 */
static void clear_input(void) {
    input_len = 0;
    input_buffer[0] = '\0';
}

/**
 * Agregar un carácter al buffer de entrada con eco en UART.
 */
static uint8_t input_add_char(char c) {
    if (input_len >= INPUT_BUF_SIZE - 1) {
        return 0;
    }
    input_buffer[input_len++] = c;
    input_buffer[input_len] = '\0';
    uart_putc(c);
    return 1;
}

/**
 * Eliminar el último carácter del buffer.
 */
static void input_delete_last(void) {
    if (input_len > 0) {
        input_len--;
        input_buffer[input_len] = '\0';
        uart_backspace();
    }
}

/**
 * Leer una línea completa desde UART con eco y soporte para backspace.
 */
static uint8_t read_line(void) {
    char c;

    clear_input();

    while (1) {
        while (!rom_uart_rx_ready()) {
            /* Spin-wait */
        }

        c = rom_uart_getc();

        if (c == CHAR_CR || c == CHAR_LF) {
            uart_newline();
            return 1;
        }

        if (c == CHAR_BS || c == CHAR_DEL) {
            input_delete_last();
            continue;
        }

        if (c < CHAR_SPACE) {
            continue;
        }

        input_add_char(c);
    }
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void) {
    msbasic_float_t result;
    char output_buffer[OUTPUT_BUF_SIZE];

    /* Banner de inicio */
    uart_newline();
    uart_print("================================================\r\n");
    uart_print("  Calculadora Cientifica - Monitor 6502\r\n");
    uart_print("  Version ");
    uart_print(VERSION_STR);
    uart_print("\r\n");
    uart_print("================================================\r\n");
    uart_print("  Usando rutinas MSBasic Float\r\n");
    uart_print("  Expresiones: + - * / ^ ( )\r\n");
    uart_print("  Funciones: sin cos tan atan log exp sqrt sqr abs\r\n");
    uart_print("            d2r r2d pi\r\n");
    uart_print("  Comandos: quit, exit, help\r\n");
    uart_print("================================================\r\n");
    uart_newline();

    rom_delay_ms(100);

    /* Bucle principal */
    while (1) {
        uart_print("> ");

        if (!read_line()) {
            continue;
        }

        if (input_len == 0) {
            continue;
        }

        /* Comprobar comandos */
        if (input_buffer[0] == 'q' && input_buffer[1] == 'u' &&
            input_buffer[2] == 'i' && input_buffer[3] == 't' &&
            input_buffer[4] == '\0') {
            uart_print("Volviendo al monitor...\r\n");
            rom_delay_ms(200);
            return 0;
        }
        if (input_buffer[0] == 'e' && input_buffer[1] == 'x' &&
            input_buffer[2] == 'i' && input_buffer[3] == 't' &&
            input_buffer[4] == '\0') {
            uart_print("Volviendo al monitor...\r\n");
            rom_delay_ms(200);
            return 0;
        }
        if (input_buffer[0] == 'h' && input_buffer[1] == 'e' &&
            input_buffer[2] == 'l' && input_buffer[3] == 'p' &&
            input_buffer[4] == '\0') {
            uart_print("\r\n");
            uart_print("  CALCULADORA CIENTIFICA - Ayuda\r\n");
            uart_print("  ===============================\r\n");
            uart_print("  Operadores: +  -  *  /  ^\r\n");
            uart_print("  Agrupacion: (  )\r\n");
            uart_print("  Constante:  pi\r\n");
            uart_print("  Funciones:\r\n");
            uart_print("    sin(x)   cos(x)   tan(x)    - Trig (radianes)\r\n");
            uart_print("    atan(x)                       - Arco tangente\r\n");
            uart_print("    log(x)   exp(x)             - Log/Exp naturales\r\n");
            uart_print("    sqrt(x)  sqr(x)   abs(x)    - Raiz/Abs\r\n");
            uart_print("    d2r(x)   r2d(x)            - Conv. grados/rad\r\n");
            uart_print("  Comandos: quit, exit, help\r\n");
            uart_print("  ===============================\r\n");
            uart_print("  Ejemplos:\r\n");
            uart_print("    2+3*4          = 14\r\n");
            uart_print("    sin(pi/2)      = 1\r\n");
            uart_print("    sin(d2r(45))   = 0.707107\r\n");
            uart_print("    log(exp(5))    = 5\r\n");
            uart_print("\r\n");
            continue;
        }

        /* Evaluar la expresión */
        if (parse_expression(input_buffer, &result) == 0) {
            fp_float_to_string(&result, output_buffer, OUTPUT_BUF_SIZE);
            uart_print("= ");
            uart_print(output_buffer);
            uart_newline();
        } else {
            uart_print("ERR: ");
            uart_print(parser_get_error());
            uart_newline();
        }
    }

    return 0;
}
