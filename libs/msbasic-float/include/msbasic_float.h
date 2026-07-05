/**
 * ============================================================================
 * msbasic_float.h - API de Punto Flotante MSBasic para 6502
 * ============================================================================
 * Proporciona una interfaz en C para las rutinas de matemática de punto
 * flotante de Microsoft BASIC 6502.
 * 
 * FORMATO INTERNO: 5 bytes por número flotante
 *   [EXP][M1][M2][M3][SIGN]
 *   
 *   EXP  = Exponente con sesgo de 128 (valor 0x00 = número cero)
 *   M1   = Byte alto de mantisa (bit 7 implícito, NO almacenado)
 *   M2   = Byte medio de mantisa
 *   M3   = Byte bajo de mantisa
 *   SIGN = 0x00 = positivo, 0xFF = negativo
 * 
 * EJEMPLOS DE VALORES:
 *   0   = {0x00, 0x00, 0x00, 0x00, 0x00}
 *   1   = {0x81, 0x00, 0x00, 0x00, 0x00}
 *   2   = {0x82, 0x00, 0x00, 0x00, 0x00}
 *   3   = {0x82, 0x40, 0x00, 0x00, 0x00}
 *   4   = {0x83, 0x00, 0x00, 0x00, 0x00}
 *   5   = {0x83, 0x20, 0x00, 0x00, 0x00}
 *   10  = {0x84, 0x20, 0x00, 0x00, 0x00}
 *   0.5 = {0x80, 0x00, 0x00, 0x00, 0x00}
 *   -1  = {0x81, 0x00, 0x00, 0x00, 0xFF}
 * 
 * Rango aproximado: ±1.7E+38
 * Precisión: ~6-7 dígitos decimales
 * 
 * Ver docs/MSBASIC_FLOAT_API.md para documentación completa.
 * ============================================================================
 */

#ifndef MSBASIC_FLOAT_H
#define MSBASIC_FLOAT_H

#include <stdint.h>

/* ============================================================================
 * TIPOS
 * ============================================================================ */

/**
 * Tipo de dato de punto flotante de MSBasic (5 bytes)
 */
typedef struct {
    uint8_t bytes[5];
} msbasic_float_t;

/* ============================================================================
 * CONSTANTES ÚTILES
 * ============================================================================ */
#define FP_ZERO {{0x00, 0x00, 0x00, 0x00, 0x00}}
#define FP_ONE  {{0x81, 0x00, 0x00, 0x00, 0x00}}
#define FP_TWO  {{0x82, 0x00, 0x00, 0x00, 0x00}}
#define FP_TEN  {{0x84, 0x20, 0x00, 0x00, 0x00}}
#define FP_HALF {{0x80, 0x00, 0x00, 0x00, 0x00}}
#define FP_PI   {{0x82, 0x49, 0x0F, 0xDA, 0xA2}}

/* ============================================================================
 * OPERACIONES ARITMÉTICAS
 * ============================================================================ */

/**
 * Suma de punto flotante: result = a + b
 * 
 * @param a      Primer operando (puntero a 5 bytes)
 * @param b      Segundo operando (puntero a 5 bytes)
 * @param result Resultado (puntero a 5 bytes)
 * 
 * Ejemplo:
 *   msbasic_float_t a = {{0x82, 0x00, 0x00, 0x00, 0x00}};  // 2
 *   msbasic_float_t b = {{0x82, 0x40, 0x00, 0x00, 0x00}};  // 3
 *   msbasic_float_t r;
 *   fp_add(&a, &b, &r);  // r = 5 = {0x83, 0x20, 0x00, 0x00, 0x00}
 */
void fp_add(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);

/**
 * Resta de punto flotante: result = a - b
 * 
 * @param a      Minuendo (puntero a 5 bytes)
 * @param b      Sustraendo (puntero a 5 bytes)
 * @param result Resultado (puntero a 5 bytes)
 * 
 * Ejemplo:
 *   msbasic_float_t a = {{0x83, 0x20, 0x00, 0x00, 0x00}};  // 5
 *   msbasic_float_t b = {{0x82, 0x40, 0x00, 0x00, 0x00}};  // 3
 *   msbasic_float_t r;
 *   fp_sub(&a, &b, &r);  // r = 2 = {0x82, 0x00, 0x00, 0x00, 0x00}
 */
void fp_sub(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);

/**
 * Multiplicación de punto flotante: result = a * b
 * 
 * @param a      Primer factor (puntero a 5 bytes)
 * @param b      Segundo factor (puntero a 5 bytes)
 * @param result Resultado (puntero a 5 bytes)
 * 
 * Ejemplo:
 *   msbasic_float_t a = {{0x82, 0x00, 0x00, 0x00, 0x00}};  // 2
 *   msbasic_float_t b = {{0x83, 0x00, 0x00, 0x00, 0x00}};  // 4
 *   msbasic_float_t r;
 *   fp_mul(&a, &b, &r);  // r = 8 = {0x84, 0x00, 0x00, 0x00, 0x00}
 */
void fp_mul(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);

/**
 * División de punto flotante: result = a / b
 * 
 * @param a      Dividendo (puntero a 5 bytes)
 * @param b      Divisor (puntero a 5 bytes, NO PUEDE SER CERO)
 * @param result Resultado (puntero a 5 bytes)
 * 
 * Ejemplo:
 *   msbasic_float_t a = {{0x84, 0x20, 0x00, 0x00, 0x00}};  // 10
 *   msbasic_float_t b = {{0x82, 0x00, 0x00, 0x00, 0x00}};  // 2
 *   msbasic_float_t r;
 *   fp_div(&a, &b, &r);  // r = 5 = {0x83, 0x20, 0x00, 0x00, 0x00}
 */
void fp_div(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);

/* ============================================================================
 * CONVERSIONES
 * ============================================================================ */

/**
 * Convierte string decimal a float MSBasic
 * Soporta: enteros, decimales, negativos
 * 
 * @param str    String con número (ej: "123.45", "-0.5", "42")
 * @param result Estructura donde guardar el resultado
 */
void fp_string_to_float(const char *str, msbasic_float_t *result);

/**
 * Convierte float MSBasic a string decimal
 * 
 * @param value   Valor de punto flotante
 * @param buffer  Buffer de salida (debe tener espacio suficiente)
 * @param max_len Tamaño máximo del buffer
 */
void fp_float_to_string(const msbasic_float_t *value, char *buffer, uint8_t max_len);

/**
 * Convierte entero de 16 bits a FAC (acumulador interno)
 * 
 * @param value Valor entero (-32768 a 32767)
 */
void fp_int_to_fac(int16_t value);

/**
 * Convierte el acumulador FAC a entero de 16 bits (trunca decimales)
 * 
 * @return Valor entero (saturado si está fuera de rango)
 */
int16_t fp_fac_to_int(void);

/* ============================================================================
 * OPERACIONES CON EL ACUMULADOR (FAC)
 * ============================================================================ */

/**
 * Carga un valor de memoria al acumulador FAC
 * 
 * @param src Fuente del valor de 5 bytes
 */
void fp_load_fac(const msbasic_float_t *src);

/**
 * Guarda el acumulador FAC a memoria
 * 
 * @param dest Destino del valor de 5 bytes
 */
void fp_save_fac(msbasic_float_t *dest);

/**
 * Pone el acumulador FAC en cero
 */
void fp_clear_fac(void);

/* ============================================================================
 * TEST (solo para debug)
 * ============================================================================ */

/**
 * Test directo de suma 2+2=4 sin usar punteros
 * Útil para verificar que las rutinas MSBasic funcionan
 */
void fp_test_direct(void);

/* ============================================================================
 * FUNCIONES TRIGONOMÉTRICAS (radianes)
 * ============================================================================ */

/**
 * Seno: result = sin(x)
 * @param x      Ángulo en radianes
 * @param result Resultado
 */
void fp_sin(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Coseno: result = cos(x)
 * @param x      Ángulo en radianes
 * @param result Resultado
 */
void fp_cos(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Tangente: result = tan(x)
 * @param x      Ángulo en radianes
 * @param result Resultado
 */
void fp_tan(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Arcotangente: result = atan(x)
 * @param x      Valor
 * @param result Resultado en radianes
 */
void fp_atn(const msbasic_float_t *x, msbasic_float_t *result);

/* ============================================================================
 * FUNCIONES EXPONENCIALES Y LOGARITMO
 * ============================================================================ */

/**
 * Logaritmo natural: result = ln(x)
 * @param x      Valor (debe ser > 0)
 * @param result Resultado
 */
void fp_log(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Exponencial: result = e^x
 * @param x      Exponente
 * @param result Resultado
 */
void fp_exp(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Potencia: result = a ^ b
 * @param a      Base
 * @param b      Exponente
 * @param result Resultado
 */
void fp_pow(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);

/* ============================================================================
 * OTRAS FUNCIONES MATEMÁTICAS
 * ============================================================================ */

/**
 * Raíz cuadrada: result = sqrt(x)
 * @param x      Valor (debe ser >= 0)
 * @param result Resultado
 */
void fp_sqr(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Valor absoluto: result = abs(x)
 * @param x      Valor
 * @param result Resultado
 */
void fp_abs(const msbasic_float_t *x, msbasic_float_t *result);

/**
 * Parte entera (floor): result = floor(x)
 * @param x      Valor
 * @param result Resultado (mayor entero <= x)
 */
void fp_int(const msbasic_float_t *x, msbasic_float_t *result);

#endif /* MSBASIC_FLOAT_H */
