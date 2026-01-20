/**
 * ============================================================================
 * float_convert.c - Conversiones decimal <-> float MSBasic sin usar strings
 * ============================================================================
 * Implementa conversiones eficientes usando solo operaciones matemáticas.
 * No requiere rutinas de string/print de MSBasic.
 * ============================================================================
 */

#include <stdint.h>
#include "msbasic_float.h"

/* Funciones externas para debug */
extern void uart_print(const char *str);
extern void uart_print_hex(uint8_t b);

/* Constantes pre-calculadas en formato MSBasic */

/* 10.0 en formato MSBasic */
static const msbasic_float_t CONST_10 = {{0x84, 0x20, 0x00, 0x00, 0x00}};

/* 1.0 en formato MSBasic */
static const msbasic_float_t CONST_1 = {{0x81, 0x00, 0x00, 0x00, 0x00}};

/* 0.5 en formato MSBasic (exacto en binario) */
static const msbasic_float_t CONST_0_5 = {{0x80, 0x00, 0x00, 0x00, 0x00}};

/* Tabla de constantes para dígitos 0-9 en formato MSBasic */
static const msbasic_float_t DIGIT_TABLE[10] = {
    {{0x00, 0x00, 0x00, 0x00, 0x00}},  /* 0 */
    {{0x81, 0x00, 0x00, 0x00, 0x00}},  /* 1 */
    {{0x82, 0x00, 0x00, 0x00, 0x00}},  /* 2 */
    {{0x82, 0x40, 0x00, 0x00, 0x00}},  /* 3 */
    {{0x83, 0x00, 0x00, 0x00, 0x00}},  /* 4 */
    {{0x83, 0x20, 0x00, 0x00, 0x00}},  /* 5 */
    {{0x83, 0x40, 0x00, 0x00, 0x00}},  /* 6 */
    {{0x83, 0x60, 0x00, 0x00, 0x00}},  /* 7 */
    {{0x84, 0x00, 0x00, 0x00, 0x00}},  /* 8 */
    {{0x84, 0x10, 0x00, 0x00, 0x00}},  /* 9 */
};

/**
 * Convierte string decimal a float MSBasic
 * Soporta números grandes usando acumulación en float
 * "1000000" -> se construye dígito a dígito usando operaciones float
 */
void fp_string_to_float(const char *str, msbasic_float_t *result) {
    uint8_t i = 0;
    uint8_t negative = 0;
    uint8_t decimal_places = 0;
    uint8_t found_dot = 0;
    uint8_t j;
    msbasic_float_t digit_float;
    
    /* Inicializar resultado a cero */
    result->bytes[0] = 0;
    result->bytes[1] = 0;
    result->bytes[2] = 0;
    result->bytes[3] = 0;
    result->bytes[4] = 0;
    
    /* Verificar signo negativo */
    if (str[i] == '-') {
        negative = 1;
        i++;
    }
    
    /* Construir número dígito a dígito usando operaciones float */
    /* result = result * 10 + dígito */
    while (str[i] != '\0') {
        if (str[i] == '.') {
            found_dot = 1;
        } else if (str[i] >= '0' && str[i] <= '9') {
            uint8_t digit = str[i] - '0';
            
            /* result = result * 10 */
            if (result->bytes[0] != 0) {
                fp_mul(result, &CONST_10, result);
            }
            
            /* result = result + dígito */
            if (digit > 0) {
                digit_float = DIGIT_TABLE[digit];
                fp_add(result, &digit_float, result);
            }
            
            if (found_dot) {
                decimal_places++;
            }
        }
        i++;
    }
    
    /* Dividir por 10^decimal_places */
    for (j = 0; j < decimal_places; j++) {
        fp_div(result, &CONST_10, result);
    }
    
    /* Aplicar signo negativo */
    if (negative && result->bytes[0] != 0) {
        result->bytes[1] |= 0x80;
    }
}

/**
 * Convierte float MSBasic a string decimal
 * Usa las rutinas de punto flotante para mayor precisión
 */
void fp_float_to_string(const msbasic_float_t *value, char *buffer, uint8_t max_len) {
    msbasic_float_t temp, frac;
    uint8_t pos = 0;
    uint8_t negative = 0;
    uint8_t exp;
    int32_t parte_entera;
    uint32_t mantisa;
    uint8_t i;
    char int_buf[12];
    uint8_t int_len = 0;
    uint8_t decimal_count = 0;
    uint8_t max_decimals;
    
    /* Verificar si es cero */
    if (value->bytes[0] == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    /* Verificar signo negativo */
    negative = (value->bytes[1] & 0x80) != 0;
    
    /* Hacer copia del valor absoluto */
    temp = *value;
    temp.bytes[1] &= 0x7F;  /* Quitar signo */
    
    exp = temp.bytes[0];
    
    /* Overflow check */
    if (exp > 0x98) {
        buffer[0] = 'E';
        buffer[1] = 'R';
        buffer[2] = 'R';
        buffer[3] = '\0';
        return;
    }
    
    /* Extraer parte entera */
    if (exp < 0x81) {
        parte_entera = 0;
    } else {
        mantisa = 0x00800000UL;
        mantisa |= ((uint32_t)(temp.bytes[1] & 0x7F) << 16);
        mantisa |= ((uint32_t)temp.bytes[2] << 8);
        mantisa |= (uint32_t)temp.bytes[3];
        i = exp - 0x80;
        parte_entera = (int32_t)(mantisa >> (24 - i));
    }
    
    /* Convertir parte entera a string (en reversa) */
    if (parte_entera == 0) {
        int_buf[int_len++] = '0';
    } else {
        int32_t n = parte_entera;
        while (n > 0 && int_len < 10) {
            int_buf[int_len++] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    /* Calcular decimales disponibles */
    max_decimals = max_len - 1 - (negative ? 1 : 0) - int_len - 1;
    if (max_decimals > 6) max_decimals = 6;
    
    /* Construir string: signo */
    if (negative) {
        buffer[pos++] = '-';
    }
    
    /* Copiar parte entera en orden correcto */
    while (int_len > 0) {
        buffer[pos++] = int_buf[--int_len];
    }
    
    /* Calcular parte decimal usando multiplicación por 10 en float */
    /* Primero, restar la parte entera para obtener solo la fracción */
    {
        msbasic_float_t int_float;
        
        /* Convertir parte_entera a float */
        if (parte_entera == 0) {
            int_float.bytes[0] = 0;
            int_float.bytes[1] = 0;
            int_float.bytes[2] = 0;
            int_float.bytes[3] = 0;
            int_float.bytes[4] = 0;
        } else if (parte_entera <= 9) {
            /* Usar tabla de constantes para dígitos 1-9 */
            int_float = DIGIT_TABLE[parte_entera];
        } else {
            /* Para números mayores, construir el float manualmente */
            uint32_t val = (uint32_t)parte_entera;
            uint8_t e = 0x80 + 24;  /* Empezar asumiendo 24 bits */
            
            /* Normalizar: desplazar hasta que el bit 23 esté en 1 */
            /* y el bit 24 esté en 0 */
            while (val < 0x800000UL && e > 0x81) {
                val <<= 1;
                e--;
            }
            while (val >= 0x1000000UL) {
                val >>= 1;
                e++;
            }
            
            /* val ahora tiene el bit 23 implícito, quitarlo para almacenar */
            int_float.bytes[0] = e;
            int_float.bytes[1] = (val >> 16) & 0x7F;  /* Bits 22-16, sin bit 23 */
            int_float.bytes[2] = (val >> 8) & 0xFF;   /* Bits 15-8 */
            int_float.bytes[3] = val & 0xFF;          /* Bits 7-0 */
            int_float.bytes[4] = 0;
        }
        
        /* frac = temp - int_float (parte fraccionaria) */
        fp_sub(&temp, &int_float, &frac);
    }
    
    /* Agregar punto decimal si hay decimales */
    if (max_decimals > 0 && frac.bytes[0] != 0) {
        buffer[pos++] = '.';
        
        /* Extraer dígitos decimales multiplicando por 10 */
        while (decimal_count < max_decimals && frac.bytes[0] != 0) {
            uint8_t digit;
            msbasic_float_t digit_float;
            
            /* frac = frac * 10 */
            fp_mul(&frac, &CONST_10, &frac);
            
            /* Extraer parte entera (dígito) */
            if (frac.bytes[0] >= 0x81) {
                mantisa = 0x00800000UL;
                mantisa |= ((uint32_t)(frac.bytes[1] & 0x7F) << 16);
                mantisa |= ((uint32_t)frac.bytes[2] << 8);
                mantisa |= (uint32_t)frac.bytes[3];
                i = frac.bytes[0] - 0x80;
                if (i <= 24) {
                    digit = (uint8_t)(mantisa >> (24 - i));
                } else {
                    digit = 0;
                }
            } else {
                digit = 0;
            }
            
            if (digit > 9) digit = 9;  /* Sanity check */
            
            buffer[pos++] = '0' + digit;
            decimal_count++;
            
            /* Restar el dígito de frac */
            if (digit > 0 && digit <= 9) {
                digit_float = DIGIT_TABLE[digit];
                fp_sub(&frac, &digit_float, &frac);
            }
            
            /* Si frac se volvió negativo (error de redondeo), terminar */
            if (frac.bytes[1] & 0x80) {
                break;
            }
        }
        
        /* Eliminar ceros finales */
        while (pos > 0 && buffer[pos-1] == '0') {
            pos--;
        }
        /* Si quedó solo el punto, quitarlo también */
        if (pos > 0 && buffer[pos-1] == '.') {
            pos--;
        }
    }
    
    buffer[pos] = '\0';
}
