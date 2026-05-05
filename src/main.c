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
 * Nota: 
 *   Mantener presionado [.] (tecla 13) por 1 segundo = Clear (C)
 *   Mantener presionado [-] (tecla 8) por 1 segundo = Cambiar signo (+/-)
 *   Presionar [-] rapidamente = operador resta
 * 
 * UART:
 *   Escriba "quit" o "q" + Enter para volver al monitor 6502.
 *   Cada operación se registra en la consola (ej: 1.2 + 3.4 = 4.6)
 * 
 * Uso:
 *   LOAD CALC 0800
 *   R 0800
 * 
 * ROM API utilizada:
 *   - rom_uart_putc()   - Enviar caracteres
 *   - rom_uart_getc()   - Recibir caracteres
 *   - rom_uart_rx_ready() - Verificar si hay datos disponibles
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
#define SHORT_PRESS_MS  100     /* Tiempo mínimo para detectar presión corta (100ms) */
#define UART_BUF_LEN    16      /* Buffer para comandos UART */

/* Versión del programa */
#define VERSION_MAJOR   1
#define VERSION_MINOR   4
#define VERSION_PATCH   1
#define VERSION_STR     "1.4.1"      /* Para UART */
#define VERSION_DISPLAY "CAL  1.4"   /* Para TM1638 (8 chars max) */

/* IDs de teclas del TM1638 */
#define KEY_DOT     13
#define KEY_MINUS   8

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

/* Símbolos de operación para mostrar en UART */
static const char * const op_symbols[] = {
    "",    /* OP_NONE */
    " + ", /* OP_ADD */
    " - ", /* OP_SUB */
    " * ", /* OP_MUL */
    " / "  /* OP_DIV */
};

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
static operation_t last_op = OP_NONE;             /* Última operación para repetición con = */

/* Buffer para registro UART de la operación actual */
static char uart_op1_str[MAX_INPUT_LEN + 1];     /* String del primer operando */
static char uart_op2_str[MAX_INPUT_LEN + 1];     /* String del segundo operando */

/* Prototipos de funciones */
uint8_t is_negative(void);
void toggle_sign(void);

/* ============================================================================
 * FUNCIONES AUXILIARES
 * ============================================================================ */

/* Función para enviar strings usando ROM API */
void uart_print(const char *s) {
    while (*s) rom_uart_putc(*s++);
}

/* Enviar un caracter por UART */
void uart_putc(char c) {
    rom_uart_putc(c);
}

/* Inicializar buffer de entrada */
void clear_input(void) {
    input_len = 0;
    input_buffer[0] = '0';
    input_buffer[1] = '\0';
    decimal_entered = 0;
}

/* Convertir float a string para UART */
void float_to_str(const msbasic_float_t *value, char *buf, uint8_t maxlen) {
    fp_float_to_string(value, buf, maxlen);
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
        /* Remover '0' inicial si es el primer dígito (solo si no hay signo -) */
        if (!is_negative() && input_len == 1 && input_buffer[0] == '0' && !decimal_entered) {
            input_len = 0;
        }
        /* Con signo negativo: " -0" -> remover el 0 */
        if (is_negative() && input_len == 2 && input_buffer[1] == '0' && !decimal_entered) {
            input_len = 1;  /* Deja solo el '-' */
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
        } else if (input_len == 1 && input_buffer[0] == '-') {
            /* Si solo tenemos el signo negativo, agregar "0" antes del punto */
            input_buffer[input_len++] = '0';
        }
        input_buffer[input_len++] = '.';
        input_buffer[input_len] = '\0';
        decimal_entered = 1;
        update_display();
    }
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

/* Guardar el input actual como número flotante */
void save_current_input(msbasic_float_t *dest) {
    fp_string_to_float(input_buffer, dest);
}

/* Mostrar número flotante en el display */
void display_float(const msbasic_float_t *value) {
    fp_float_to_string(value, input_buffer, MAX_INPUT_LEN);
    
    /* Actualizar longitud */
    input_len = 0;
    while (input_buffer[input_len] != '\0') input_len++;
    
    update_display();
}

/* Mostrar mensaje de error en el display (máx 8 caracteres) */
void show_error(const char *msg) {
    uint8_t i;
    for (i = 0; i < 8 && msg[i] != '\0'; i++) {
        input_buffer[i] = msg[i];
    }
    input_buffer[i] = '\0';
    input_len = i;
    tm1638_show_text(input_buffer);
}

/* Verificar si un número MSBasic es cero */
uint8_t fp_is_zero(const msbasic_float_t *val) {
    return val->bytes[0] == 0;
}

/* Guardar string del operando actual para registro UART */
void uart_save_operand_str(char *dest) {
    uint8_t i;
    for (i = 0; i <= input_len && i < MAX_INPUT_LEN; i++) {
        dest[i] = input_buffer[i];
    }
    dest[MAX_INPUT_LEN] = '\0';
}

/* Registrar la operación completa en UART: "1.2 + 3.4 = 4.6\r\n" */
void uart_log_operation(void) {
    char res_str[MAX_INPUT_LEN + 1];
    
    /* Convertir resultado a string */
    float_to_str(&result, res_str, MAX_INPUT_LEN);
    
    /* Imprimir: operand1 + op + operand2 = resultado */
    uart_print(uart_op1_str);
    uart_print(op_symbols[current_op]);
    uart_print(uart_op2_str);
    uart_print(" = ");
    uart_print(res_str);
    uart_print("\r\n");
}

/* Registrar error en UART: "1.2 / 0 = DIV/0\r\n" */
void uart_log_error(void) {
    uart_print(uart_op1_str);
    uart_print(op_symbols[current_op]);
    uart_print(uart_op2_str);
    uart_print(" = DIV/0\r\n");
}

/* Ejecutar operación */
/* Retorna 0 si OK, 1 si hubo error (ej: división por cero) */
uint8_t execute_operation(void) {
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
            if (fp_is_zero(&operand2)) {
                show_error("DIV/0   ");
                return 1;  /* Error */
            }
            fp_div(&operand1, &operand2, &result);
            break;
        default:
            /* Sin operación, copiar operando1 al resultado */
            result = operand1;
            break;
    }
    return 0;  /* OK */
}

/* Ejecutar limpieza (Clear) */
void do_clear(void) {
    clear_input();
    state = STATE_FIRST_NUMBER;
    current_op = OP_NONE;
    last_op = OP_NONE;
    update_display();
    /* Registrar clear en UART */
    uart_print("CLEAR\r\n");
}

/* Procesar comando UART */
/* Retorna 1 si se debe salir (quit), 0 si no */
uint8_t process_uart_command(const char *cmd, uint8_t len) {
    /* Comando: "quit" o "q" */
    if (len == 1 && cmd[0] == 'q') return 1;
    if (len == 4 && cmd[0] == 'q' && cmd[1] == 'u' && cmd[2] == 'i' && cmd[3] == 't') return 1;
    return 0;
}

/* Leer y procesar un comando UART */
/* Retorna 1 si se debe salir, 0 si no */
uint8_t check_uart_command(void) {
    static char uart_buf[UART_BUF_LEN];
    static uint8_t uart_pos = 0;
    char c;
    
    while (rom_uart_rx_ready()) {
        c = rom_uart_getc();
        
        if (c == '\r' || c == '\n') {
            /* Fin de línea - procesar comando */
            uart_buf[uart_pos] = '\0';
            if (uart_pos > 0) {
                if (process_uart_command(uart_buf, uart_pos)) {
                    uart_pos = 0;
                    return 1;  /* Quit */
                }
            }
            uart_pos = 0;
        } else if (c == '\b' || c == 127) {
            /* Backspace */
            if (uart_pos > 0) uart_pos--;
        } else if (uart_pos < UART_BUF_LEN - 1) {
            uart_buf[uart_pos++] = c;
        }
    }
    return 0;  /* No quit */
}

/* Guardar string del resultado actual en uart_op1_str para operaciones encadenadas */
void uart_save_result_as_op1(void) {
    /* input_buffer contiene el string del resultado (display_float lo escribió ahí) */
    uint8_t i;
    for (i = 0; i <= input_len && i < MAX_INPUT_LEN; i++) {
        uart_op1_str[i] = input_buffer[i];
    }
    uart_op1_str[MAX_INPUT_LEN] = '\0';
}

/* Verificar si el buffer actual tiene signo negativo */
uint8_t is_negative(void) {
    return (input_len > 0 && input_buffer[0] == '-');
}

/* Cambiar signo del numero actual (+/-) */
void toggle_sign(void) {
    uint8_t i;
    
    /* Si el buffer es "0" (y solo "0"), no cambiar signo */
    if (input_len == 1 && input_buffer[0] == '0') return;
    if (input_len == 2 && input_buffer[0] == '-' && input_buffer[1] == '0') return;
    
    if (is_negative()) {
        /* Quitar signo negativo: mover todo un lugar a la izquierda */
        for (i = 1; i <= input_len; i++) {
            input_buffer[i - 1] = input_buffer[i];
        }
        input_len--;
    } else {
        /* Agregar signo negativo: mover todo un lugar a la derecha */
        if (input_len < MAX_INPUT_LEN) {
            for (i = input_len; i > 0; i--) {
                input_buffer[i] = input_buffer[i - 1];
            }
            input_buffer[0] = '-';
            input_len++;
            input_buffer[input_len] = '\0';
        }
    }
    
    /* Si estamos en STATE_RESULT, actualizar operand1 con el nuevo signo */
    if (state == STATE_RESULT) {
        save_current_input(&operand1);
        uart_save_result_as_op1();
    }
    
    update_display();
}

/* Procesar tecla presionada del TM1638 (solo presión corta) */
void process_key(uint8_t key) {
    /* Mapeo de teclas según el layout deseado:
     * Hardware QYF-TM1638:     Layout deseado:
     *  [ 1] [ 2] [ 3] [ 4]     [1] [2] [3] [+]
     *  [ 5] [ 6] [ 7] [ 8]     [4] [5] [6] [-]
     *  [ 9] [10] [11] [12]     [7] [8] [9] [*]
     *  [13] [14] [15] [16]     [.] [0] [=] [/]
     * 
     * Nota: Las teclas 8 (-) y 13 (.) se procesan en el loop principal
     *       para distinguir presión corta de presión larga.
     *       Aquí solo llegan cuando ya se determinó que fue presión corta.
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
        
        case 13: /* Tecla . (Punto decimal) - presión corta */
            add_decimal();
            break;
        
        case 8: /* Tecla - (Resta) - presión corta */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
                uart_save_operand_str(uart_op1_str);
            } else if (state == STATE_RESULT) {
                /* En STATE_RESULT, operand1 ya tiene el resultado.
                 * input_buffer tiene el string visible. Lo guardamos como op1_str. */
                uart_save_result_as_op1();
            }
            current_op = OP_SUB;
            last_op = OP_SUB;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 4: /* Tecla + */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
                uart_save_operand_str(uart_op1_str);
            } else if (state == STATE_RESULT) {
                uart_save_result_as_op1();
            }
            current_op = OP_ADD;
            last_op = OP_ADD;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 12: /* Tecla * */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
                uart_save_operand_str(uart_op1_str);
            } else if (state == STATE_RESULT) {
                uart_save_result_as_op1();
            }
            current_op = OP_MUL;
            last_op = OP_MUL;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 16: /* Tecla / */
            if (state == STATE_FIRST_NUMBER) {
                save_current_input(&operand1);
                uart_save_operand_str(uart_op1_str);
            } else if (state == STATE_RESULT) {
                uart_save_result_as_op1();
            }
            current_op = OP_DIV;
            last_op = OP_DIV;
            clear_input();
            state = STATE_SECOND_NUMBER;
            break;
        
        case 15: /* Tecla = */
            if (state == STATE_SECOND_NUMBER || state == STATE_RESULT) {
                /* En STATE_RESULT: repetir la ultima operacion (last_op) con
                 * operand1 = resultado anterior y operand2 = el mismo de antes.
                 * En STATE_SECOND_NUMBER: guardar el segundo operando ingresado. */
                if (state == STATE_SECOND_NUMBER) {
                    save_current_input(&operand2);
                    uart_save_operand_str(uart_op2_str);
                } else {
                    /* STATE_RESULT sin nuevo operador: restaurar ultima operacion */
                    current_op = last_op;
                }
                if (execute_operation() == 0) {
                    display_float(&result);
                    /* Registrar operacion en UART */
                    uart_log_operation();
                    /* Preparar operand1 con el resultado para reutilizarlo */
                    operand1 = result;
                    /* Guardar string del resultado como op1_str para la prox operacion */
                    uart_save_result_as_op1();
                    state = STATE_RESULT;
                } else {
                    /* Error (ej: division por cero) - registrar en UART */
                    uart_log_error();
                }
                current_op = OP_NONE;
            }
            break;
    }
}

/* Esperar a que se suelte una tecla, midiendo el tiempo de presión */
/* Retorna: 0 = presión corta, 1 = presión larga (>= LONG_PRESS_MS) */
uint8_t wait_key_release_with_timeout(uint8_t expected_key, uint16_t *hold_count_out) {
    uint16_t count = 0;
    while (tm1638_get_key_pressed() == expected_key) {
        rom_delay_ms(10);
        count++;
        
        /* Si pasó LONG_PRESS_MS (50 * 10ms = 500ms) */
        if (count >= 50) {
            *hold_count_out = count;
            return 1;  /* Presión larga */
        }
    }
    *hold_count_out = count;
    /* Si se soltó antes de 50ms, es rebote */
    if (count < 5) return 2;  /* Rebote */
    return 0;  /* Presión corta */
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void) {
    uint8_t key;
    uint16_t hold_count;
    uint8_t hold_result;
    
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
    uart_print("Mantener [.] 500ms = Clear\r\n");
    uart_print("Mantener [-] 500ms = Cambiar signo (+/-)\r\n");
    uart_print("Escriba 'quit' o 'q' + Enter para salir\r\n\r\n");
    uart_print("-- Inicio de sesion --\r\n");
    
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
        /* Verificar si hay comando UART (quit) */
        if (check_uart_command()) {
            uart_print("\r\n-- Fin de sesion --\r\n");
            /* Apagar display TM1638 */
            tm1638_show_text("        ");  /* 8 espacios = display vacio */
            tm1638_set_brightness(0);         /* Brillo minimo */
            /* Apagar LEDs del hardware */
            LEDS = 0x00;
            uart_print("Volviendo al monitor 6502...\r\n");
            break;  /* Sale del while y retorna al monitor */
        }
        
        /* Leer tecla presionada del TM1638 */
        key = tm1638_get_key_pressed();
        
        if (key > 0) {
            /* Si es una nueva tecla diferente */
            if (key != last_key) {
                last_key = key;
                key_processed = 0;
            }
            
            /* Si es una tecla con doble función (presión larga vs corta) */
            if (!key_processed && (key == KEY_DOT || key == KEY_MINUS)) {
                /* Esperar a que se suelte para medir el tiempo */
                hold_result = wait_key_release_with_timeout(key, &hold_count);
                
                if (hold_result == 0) {
                    /* Presión corta */
                    if (key == KEY_DOT) {
                        add_decimal();
                    } else { /* KEY_MINUS */
                        /* Presión corta de -: operador resta */
                        if (state == STATE_FIRST_NUMBER) {
                            save_current_input(&operand1);
                            uart_save_operand_str(uart_op1_str);
                        } else if (state == STATE_RESULT) {
                            uart_save_result_as_op1();
                        }
                        current_op = OP_SUB;
                        last_op = OP_SUB;
                        clear_input();
                        state = STATE_SECOND_NUMBER;
                    }
                    key_processed = 1;
                } else if (hold_result == 1) {
                    /* Presión larga */
                    if (key == KEY_DOT) {
                        do_clear();
                    } else { /* KEY_MINUS */
                        toggle_sign();
                    }
                    key_processed = 2;
                }
                /* Si es rebote (hold_result == 2), ignorar */
                
                /* La tecla ya se soltó. Marcamos key=0 para que no se reprocese
                 * en esta misma iteración. Los flags se resetearán en el else
                 * cuando no haya tecla presionada. */
                key = 0;
                
            } else if (!key_processed) {
                /* Teclas normales (dígitos y operadores +, *, /, =) */
                process_key(key);
                key_processed = 1;
            }
            
        } else {
            /* Tecla liberada - resetear estado */
            last_key = 0;
            key_processed = 0;
        }
        
        /* Pequeño delay para no saturar el bus TM1638 */
        rom_delay_ms(10);
    }
    
    return 0;
}
