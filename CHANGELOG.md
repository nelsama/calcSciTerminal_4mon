# Changelog - Calculadora Punto Flotante TM1638

Registro de cambios del proyecto.

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
