# Changelog - Calculadora Punto Flotante TM1638

Registro de cambios del proyecto.

## [1.4.0] - 2026-01-19

### Agregado
- **Cambio de signo (+/-)**: Mantener presionada la tecla `-` (tecla 8) por 1 segundo cambia el signo del número actual. Presión corta sigue siendo el operador resta
- **Función `wait_key_release_with_timeout()`**: Refactorizada la detección de presión larga/corta para las teclas `.` y `-` en una función reutilizable
- **Funciones `toggle_sign()` e `is_negative()`**: Para gestionar el signo negativo en el buffer de entrada
- **Ajuste en `add_digit()` y `add_decimal()`**: Ahora consideran correctamente el signo `-` al inicio del buffer

### Versión
- Actualizada a v1.4.0

## [1.3.0] - 2026-01-19

### Agregado
- **Comando UART "quit"**: Escriba `quit` o `q` + Enter en la terminal para volver al monitor 6502. Al salir, el display TM1638 se apaga (pantalla en blanco + brillo 0) y los LEDs se desactivan
- **Registro de operaciones en UART**: Cada operación se muestra en la consola en formato legible, incluyendo operaciones encadenadas y el comando CLEAR:
  ```
  10 + 5 = 15
  CLEAR            ← clear manual
  20 * 2 = 40
  40 * 2 = 80       ← repitiendo ultima operacion con =
  ```
- **Variable `last_op`**: Registra la última operación seleccionada para poder repetirla presionando `=` múltiples veces (comportamiento de calculadora de bolsillo)
- **Banner de inicio/fin de sesión**: `-- Inicio de sesion --` y `-- Fin de sesion --` para marcar el inicio y fin de la calculadora

### Corregido
- **División por cero**: Agregada validación que muestra "DIV/0" en el display y UART, evitando llamar a la rutina MSBasic (bucle infinito)
- **Eliminado debug UART permanente**: Se removieron todas las trazas de debug en `save_current_input()`, `execute_operation()`, y las funciones `uart_print_hex()`, `debug_print_fac()`, `debug_print_arg()`, `debug_print_temp()`
- **Eliminada función `fp_test_direct()`** del wrapper ASM (código de test no usado en producción)

### Optimizado
- **Wrapper ASM**: Las cuatro operaciones aritméticas ahora usan macros `copy_mem_to_fac` y `copy_mem_to_temp`, eliminando ~80 líneas de código duplicado
- **`save_current_input()`**: Simplificada para siempre usar `fp_string_to_float()`. Eliminada la optimización prematura con tabla de constantes que parseaba el buffer dos veces
- **Loop principal**: Movida la declaración de `dot_hold_count` dentro del bloque donde se usa. Eliminado un `rom_delay_ms(10)` redundante
- **Tecla `=`**: Ahora también funciona desde `STATE_RESULT`, permitiendo repetir la última operación presionando = múltiples veces
- **`fp_float_to_string()`**: Agregado safety check para `max_decimals` para evitar underflow

### Versión
- Actualizada a v1.3.0

## [1.2.0] - 2026-01-19

### Corregido
- **Normalización de parte entera en float→string**: Corregido bug donde números como `2*3.1415` mostraban `6.999999` en lugar de `6.283`
- Ahora usa tabla de constantes para dígitos 1-9 (valores exactos)
- Arreglada lógica de normalización para números >= 10

### Agregado
- Banner de versión en display TM1638 al iniciar ("CAL  1.2")
- Banner de versión en UART al iniciar
- Constantes de versión centralizadas (`VERSION_STR`, `VERSION_DISPLAY`)

### Casos de prueba verificados
- `2 * 3.1415 = 6.283` ✓

---

## [1.1.0] - 2026-01-19

### Corregido
- **Conversión float a string para números pequeños**: Reescrito el algoritmo `fp_float_to_string()` para manejar correctamente números menores que 0.1 (ej: 0.006452)
- El método anterior tenía problemas con `extra_shift > 31` bits causando resultados incorrectos
- Nuevo algoritmo usa multiplicación por 10 con las rutinas de MSBasic para extraer dígitos decimales
- Ahora muestra correctamente ceros después del punto decimal (ej: `-0.00645` en lugar de `-0.001`)

### Mejorado
- Reducido tamaño del binario de ~13.7 KB a ~12.6 KB
- Código más simple y mantenible en `float_convert.c`

### Casos de prueba verificados
- `63.45 * 0.345 = 21.8902` ✓
- `21.89 / 234 = 0.0935` ✓
- `0.093548 - 0.1 = -0.00645` ✓

---

## [1.0.0] - 2026-01-18

### Agregado
- Calculadora de punto flotante completa para Monitor 6502
- Soporte para módulo TM1638 (QYF-TM1638: 8 dígitos, 16 botones)
- Operaciones: suma (+), resta (-), multiplicación (*), división (/)
- Entrada de números decimales con punto
- Función Clear (mantener tecla [.] por 1 segundo)
- Operaciones encadenadas (el resultado se usa como primer operando)
- Debug por UART para diagnóstico

### Dependencias
- Librería MSBasic Float (rutinas de punto flotante de Microsoft BASIC)
- Librería TM1638 para 6502/CC65
- ROM API del Monitor 6502 (UART, delays)

### Layout del teclado
```
[1] [2] [3] [+]
[4] [5] [6] [-]
[7] [8] [9] [*]
[.] [0] [=] [/]
```

---

## Formato del archivo

El formato está basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/).

### Tipos de cambios
- **Agregado**: nuevas funcionalidades
- **Cambiado**: cambios en funcionalidades existentes
- **Obsoleto**: funcionalidades que serán eliminadas próximamente
- **Eliminado**: funcionalidades eliminadas
- **Corregido**: corrección de errores
- **Seguridad**: vulnerabilidades corregidas
- **Mejorado**: optimizaciones y mejoras de rendimiento
