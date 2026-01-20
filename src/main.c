/**
 * ============================================================================
 * Calculadora de Punto Flotante - Monitor 6502 + TM1638
 * ============================================================================
 * Calculadora de punto flotante usando las rutinas de MSBasic.
 * 
 * Teclado del TM1638 (QYF-TM1638):
 *   Layout deseado:          Hardware:
 *   [1] [2] [3] [+]           [ 1] [ 2] [ 3] [ 4]
 *   [4] [5] [6] [-]           [ 5] [ 6] [ 7] [ 8]
 *   [7] [8] [9] [*]           [ 9] [10] [11] [12]
 *   [.] [0] [=] [/]           [13] [14] [15] [16]
 * 
 * Nota: Mantener presionado [.] (tecla 13) por 1 segundo hace Clear (C)
 * 
 * Uso:
 *   LOAD CALC 0800
 *   R 0800
 * 
 * ROM API utilizada:
 *   - rom_uart_putc()   - Enviar caracteres
 *   - rom_delay_ms()    - Delays en milisegundos
 * ============================================================================
 */

#include <stdint.h>
#include <string.h>
#include "romapi.h"
#include "msbasic_float.h"
#include "../../libs/tm1638-6502-cc65/include/tm1638.h"

/* ============================================================================
 * HARDWARE
 * ============================================================================ */
#define LEDS            (*(volatile uint8_t *)0xC001)   /* LEDs (lógica negativa) */

/* ============================================================================
 * CONSTANTES DE LA CALCULADORA
 * ============================================================================ */
#define MAX_INPUT_LEN   12      /* Máximo número de dígitos en entrada */
#define KEY_DEBOUNCE_MS 150     /* Tiempo de anti-rebote */
#define LONG_PRESS_MS   1000    /* Tiempo para presión larga (1 segundo) */
#define SHORT_PRESS_MS  100     /* Tiempo mínimo para detectar punto (100ms) */

/* Versión del programa */
#define VERSION_MAJOR   1
#define VERSION_MINOR   2
#define VERSION_PATCH   0
#define VERSION_STR     "1.2.0"      /* Para UART */
#define VERSION_DISPLAY "CAL  1.2"   /* Para TM1638 (8 chars max) */

/* Estados de la calculadora */
typedef enum {
    STATE_FIRST_NUMBER,         /* Ingresando primer número */
    STATE_OPERATION,            /* Operación seleccionada */
    STATE_SECOND_NUMBER,        /* Ingresando segundo número */
    STATE_RESULT                /* Mostrando resultado */
} calc_state_t;

/* Operaciones */
typedef enum {
    OP_NONE = 0,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV
} operation_t;

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */
static char input_buffer[MAX_INPUT_LEN + 1];    /* Buffer de entrada + null */
static uint8_t input_len = 0;                    /* Longitud actual */
static calc_state_t state = STATE_FIRST_NUMBER;
static operation_t current_op = OP_NONE;
static msbasic_float_t operand1;                 /* Primer operando */
static msbasic_float_t operand2;                 /* Segundo operando */
static msbasic_float_t result;                   /* Resultado */
static uint8_t decimal_entered = 0;              /* Flag para punto decimal */
static uint8_t last_key = 0;                     /* Última tecla presionada */
static uint8_t key_processed = 0;                /* Flag para indicar que tecla ya se procesó */

/* ============================================================================
 * FUNCIONES AUXILIARES
 * ============================================================================ */

/* Función para enviar strings usando ROM API */
void uart_print(const char *s) {
    while (*s) rom_uart_putc(*s++);
}

/* Imprimir byte en hexadecimal */
void uart_print_hex(uint8_t b) {
    const char hex[] = "0123456789ABCDEF";
    rom_uart_putc(hex[b >> 4]);
    rom_uart_putc(hex[b & 0x0F]);
}

/* Inicializar buffer de entrada */
void clear_input(void) {
    input_len = 0;
    input_buffer[0] = '0';
    input_buffer[1] = '\0';
    decimal_entered = 0;
}

/* Actualizar display con el contenido del buffer */
/* Para entrada: alinear a la derecha (123 -> "     123")
 * Para resultado (más de 8 chars): alinear a la izquierda, truncar decimales
 */
void update_display(void) {
    char display[9];
    uint8_t i, j;
    
    /* Preparar texto con espacios */
    for (i = 0; i < 8; i++) display[i] = ' ';
    display[8] = '\0';
    
    /* Si buffer vacío, mostrar "0" */
    if (input_len == 0) {
        display[7] = '0';
    } else if (input_len <= 8) {
        /* Cabe en el display: alinear a la derecha */
        j = 8;
        for (i = input_len; i > 0 && j > 0; i--) {
            j--;
            display[j] = input_buffer[i-1];
        }
    } else {
        /* No cabe: copiar primeros 8 caracteres (izquierda a derecha) */
        for (i = 0; i < 8; i++) {
            display[i] = input_buffer[i];
        }
    }
    
    tm1638_show_text(display);
}

/* Agregar dígito al buffer */
void add_digit(char digit) {
    /* Si estamos mostrando resultado, limpiar e iniciar nuevo número */
    if (state == STATE_RESULT) {
        clear_input();
        state = STATE_FIRST_NUMBER;
        current_op = OP_NONE;
    }
    
    if (input_len < MAX_INPUT_LEN) {
        /* Remover '0' inicial si es el primer dígito */
        if (input_len == 1 && input_buffer[0] == '0' && !decimal_entered) {
            input_len = 0;
        }
        
        input_buffer[input_len++] = digit;
        input_buffer[input_len] = '\0';
        update_display();
    }
}

/* Agregar punto decimal */
void add_decimal(void) {
    /* Si estamos mostrando resultado, limpiar e iniciar nuevo número */
    if (state == STATE_RESULT) {
        clear_input();
        state = STATE_FIRST_NUMBER;
        current_op = OP_NONE;
    }
    
    if (!decimal_entered && input_len < MAX_INPUT_LEN) {
        if (input_len == 0) {
            input_buffer[input_len++] = '0';
        }
        input_buffer[input_len++] = '.';
        input_buffer[input_len] = '\0';
        decimal_entered = 1;
        update_display();
    }
}

/* Debug: imprimir FAC desde assembly */
void debug_print_fac(void) {
    extern uint8_t FAC[];
    uart_print("FAC=[");
    uart_print_hex(FAC[0]);
    uart_print(",");
    uart_print_hex(FAC[1]);
    uart_print(",");
    uart_print_hex(FAC[2]);
    uart_print("] ");
}

/* Debug: imprimir ARG desde assembly */
void debug_print_arg(void) {
    extern uint8_t ARG[];
    uart_print("ARG=[");
    uart_print_hex(ARG[0]);
    uart_print(",");
    uart_print_hex(ARG[1]);
    uart_print(",");
    uart_print_hex(ARG[2]);
    uart_print("] ");
}

/* Debug: imprimir buffer temporal */
void debug_print_temp(const uint8_t *ptr) {
    uart_print("TEMP=[");
    uart_print_hex(ptr[0]);
    uart_print(",");
    uart_print_hex(ptr[1]);
    uart_print(",");
    uart_print_hex(ptr[2]);
    uart_print("] ");
}

/* Tabla de constantes pre-calculadas en formato MSBasic 
 * Generadas con Microsoft BASIC real 
 * Formato: [exp][mantisa_byte1][mantisa_byte2][mantisa_byte3][signo]
 */
const msbasic_float_t float_constants[] = {
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
    {{0x84, 0x20, 0x00, 0x00, 0x00}},  /* 10 */
    {{0x84, 0x30, 0x00, 0x00, 0x00}},  /* 11 */
    {{0x84, 0x40, 0x00, 0x00, 0x00}},  /* 12 */
    {{0x84, 0x50, 0x00, 0x00, 0x00}},  /* 13 */
    {{0x84, 0x60, 0x00, 0x00, 0x00}},  /* 14 */
    {{0x84, 0x70, 0x00, 0x00, 0x00}},  /* 15 */
};

/* Constante para 10.0 */
const msbasic_float_t float_ten = {{0x84, 0x20, 0x00, 0x00, 0x00}};

/* Guardar el input actual como número flotante */
void save_current_input(msbasic_float_t *dest) {
    uint8_t i;
    int16_t parte_entera = 0;
    uint8_t encontro_punto = 0;
    
    uart_print("\r\nDEBUG save_current_input: buffer='");
    uart_print(input_buffer);
    uart_print("'");
    
    /* Parsear para obtener parte entera (solo para debug y optimización) */
    for (i = 0; input_buffer[i] != '\0'; i++) {
        if (input_buffer[i] == '.') {
            encontro_punto = 1;
        } else if (input_buffer[i] >= '0' && input_buffer[i] <= '9' && !encontro_punto) {
            if (parte_entera < 3000) {
                parte_entera = parte_entera * 10 + (input_buffer[i] - '0');
            }
        }
    }
    
    uart_print(" parte_entera=");
    uart_print_hex((parte_entera >> 8) & 0xFF);
    uart_print_hex(parte_entera & 0xFF);
    
    /* Usar tabla de constantes solo para enteros simples 0-15 */
    if (parte_entera >= 0 && parte_entera <= 15 && !encontro_punto) {
        *dest = float_constants[parte_entera];
        uart_print(" usando tabla[");
        uart_print_hex(parte_entera);
        uart_print("]\r\n");
        return;
    }
    
    /* Para todos los demás casos, usar fp_string_to_float */
    fp_string_to_float(input_buffer, dest);
    
    uart_print(" usando fp_string_to_float: ");
    uart_print_hex(dest->bytes[0]);
    uart_print(" ");
    uart_print_hex(dest->bytes[1]);
    uart_print("\r\n");
}

/* Mostrar número flotante en el display */
void display_float(const msbasic_float_t *value) {
    fp_float_to_string(value, input_buffer, MAX_INPUT_LEN);
    
    /* Actualizar longitud */
    input_len = 0;
    while (input_buffer[input_len] != '\0') input_len++;
    
    update_display();
}

/* Ejecutar operación */
void execute_operation(void) {
    uart_print("\r\nDEBUG execute_operation: op=");
    uart_print_hex(current_op);
    uart_print("\r\n  op1: ");
    uart_print_hex(operand1.bytes[0]);
    uart_print(" ");
    uart_print_hex(operand1.bytes[1]);
    uart_print(" ");
    uart_print_hex(operand1.bytes[2]);
    uart_print(" ");
    uart_print_hex(operand1.bytes[3]);
    uart_print(" ");
    uart_print_hex(operand1.bytes[4]);
    uart_print("\r\n  op2: ");
    uart_print_hex(operand2.bytes[0]);
    uart_print(" ");
    uart_print_hex(operand2.bytes[1]);
    uart_print(" ");
    uart_print_hex(operand2.bytes[2]);
    uart_print(" ");
    uart_print_hex(operand2.bytes[3]);
    uart_print(" ");
    uart_print_hex(operand2.bytes[4]);
    
    switch (current_op) {
        case OP_ADD:
            fp_add(&operand1, &operand2, &result);
            break;
        case OP_SUB:
            fp_sub(&operand1, &operand2, &result);
            break;
        case OP_MUL:
            fp_mul(&operand1, &operand2, &result);
            break;
        case OP_DIV:
            fp_div(&operand1, &operand2, &result);
            break;
        default:
            /* Sin operación, copiar operando1 al resultado */
            result = operand1;
            break;
    }
    
    uart_print("\r\n  res: ");
    uart_print_hex(result.bytes[0]);
    uart_print(" ");
    uart_print_hex(result.bytes[1]);
    uart_print(" ");
    uart_print_hex(result.bytes[2]);
    uart_print(" ");
    uart_print_hex(result.bytes[3]);
    uart_print(" ");
    uart_print_hex(result.bytes[4]);
    uart_print("\r\n");
}

/* Ejecutar limpieza (Clear) */
void do_clear(void) {
    clear_input();
    state = STATE_FIRST_NUMBER;
    current_op = OP_NONE;
    update_display();
}

/* Procesar tecla presionada */
void process_key(uint8_t key) {
    /* Mapeo de teclas según el layout deseado:
     * Hardware QYF-TM1638:     Layout deseado:
     *  [ 1] [ 2] [ 3] [ 4]     [1] [2] [3] [+]
     *  [ 5] [ 6] [ 7] [ 8]     [4] [5] [6] [-]
     *  [ 9] [10] [11] [12]     [7] [8] [9] [*]
     *  [13] [14] [15] [16]     [.] [0] [=] [/]
     */
    
    switch (key) {
        case 1:  add_digit('1'); break;
        case 2:  add_digit('2'); break;
        case 3:  add_digit('3'); break;
        case 5:  add_digit('4'); break;
        case 6:  add_digit('5'); break;
        case 7:  add_digit('6'); break;
        case 9:  add_digit('7'); break;
        case 10: add_digit('8'); break;
        case 11: add_digit('9'); break;
        case 14: add_digit('0'); break;
        
        case 13: /* Tecla . (Punto decimal) */
            add_decimal();
            break;
        
        case 4: /* Tecla + */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
            } else if (state == STATE_RESULT) {
                /* operand1 ya tiene el resultado */
            }
            current_op = OP_ADD;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 8: /* Tecla - */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
            } else if (state == STATE_RESULT) {
                /* operand1 ya tiene el resultado */
            }
            current_op = OP_SUB;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 12: /* Tecla * */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
            } else if (state == STATE_RESULT) {
                /* operand1 ya tiene el resultado */
            }
            current_op = OP_MUL;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 16: /* Tecla / */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
            } else if (state == STATE_RESULT) {
                /* operand1 ya tiene el resultado */
            }
            current_op = OP_DIV;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 15: /* Tecla = */
            if (state == STATE_SECOND_NUMBER) {
                save_current_input(&operand2);
                execute_operation();
                display_float(&result);
                state = STATE_RESULT;
                
                /* Preparar operand1 con el resultado para operaciones encadenadas */
                operand1 = result;
                current_op = OP_NONE;
            }
            break;
    }
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void) {
    uint8_t key;
    
    /* Banner por UART */
    uart_print("\r\n");
    uart_print("================================================\r\n");
    uart_print("  Calculadora Punto Flotante - Monitor 6502\r\n");
    uart_print("  Version ");
    uart_print(VERSION_STR);
    uart_print("\r\n");
    uart_print("================================================\r\n");
    uart_print("Usando rutinas de MSBasic\r\n");
    uart_print("Teclado TM1638:\r\n");
    uart_print("  [1] [2] [3] [+]\r\n");
    uart_print("  [4] [5] [6] [-]\r\n");
    uart_print("  [7] [8] [9] [*]\r\n");
    uart_print("  [.] [0] [=] [/]\r\n");
    uart_print("Mantener [.] 1seg = Clear\r\n\r\n");
    
    /* Apagar LEDs del hardware (si existen) */
    LEDS = 0xFF;
    
    /* Inicializar TM1638 */
    tm1638_init();
    tm1638_set_brightness(4);  /* Brillo medio (0-7) */
    
    /* Banner de versión en display */
    tm1638_show_text(VERSION_DISPLAY);
    rom_delay_ms(2000);  /* 2 segundos */
    
    /* Inicializar calculadora */
    clear_input();
    fp_clear_fac();
    fp_save_fac(&operand1);
    fp_save_fac(&operand2);
    fp_save_fac(&result);
    
    /* Mostrar "0" inicial */
    update_display();
    
    /* Loop principal */
    while (1) {
        uint16_t dot_hold_count = 0;  /* Contador para presión del punto */
        
        /* Leer tecla presionada */
        key = tm1638_get_key_pressed();
        
        if (key > 0) {
            /* Si es una nueva tecla diferente */
            if (key != last_key) {
                last_key = key;
                key_processed = 0;
                
                /* Procesar inmediatamente teclas que no son el punto */
                if (key != 13) {
                    process_key(key);
                    key_processed = 1;
                }
            }
            /* Si es el punto y sigue presionado */
            else if (key == 13 && !key_processed) {
                /* Contar tiempo de presión */
                dot_hold_count = 0;
                while (tm1638_get_key_pressed() == 13) {
                    rom_delay_ms(10);
                    dot_hold_count++;
                    
                    /* Si pasó 1 segundo (100 * 10ms), hacer Clear */
                    if (dot_hold_count >= 100) {
                        do_clear();
                        key_processed = 2;
                        break;
                    }
                }
                
                /* Si soltó antes de 1 segundo, agregar punto */
                if (key_processed != 2 && dot_hold_count >= 5) {
                    process_key(13);  /* Agregar punto decimal */
                    key_processed = 1;
                }
            }
            
            /* Pequeño delay para no saturar el bus */
            rom_delay_ms(10);
            
        } else {
            /* Tecla liberada - resetear estado */
            last_key = 0;
            key_processed = 0;
            
            rom_delay_ms(10);  /* Pequeño delay para no saturar el bus */
        }
    }
    
    return 0;
}
