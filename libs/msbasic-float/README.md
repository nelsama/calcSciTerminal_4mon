# MSBasic Float Library for CC65

Librería de punto flotante basada en Microsoft BASIC para procesadores 6502.
Proporciona operaciones de punto flotante de 32 bits (4 bytes de mantisa + 1 byte de exponente).

## Características

- Operaciones aritméticas: suma, resta, multiplicación, división
- Conversión string ↔ float sin usar rutinas de string de MSBasic
- Compatible con CC65
- Formato de 5 bytes: `[exp][m1+sign][m2][m3][m4]`

## Estructura

```
msbasic-float/
├── include/
│   └── msbasic_float.h      # API C para usar la librería
├── src/
│   ├── float_convert.c      # Conversiones string ↔ float en C
│   ├── msbasic_wrapper.s    # Wrapper ASM para llamar desde C
│   └── msbasic_float_only.s # Rutinas float extraídas de MSBasic
└── msbasic/
    ├── defines.s            # Definiciones base MSBasic
    ├── macros.s             # Macros de ensamblador
    ├── zeropage.s           # Variables de página cero
    └── float.s              # Rutinas de punto flotante originales
```

## Uso

### Desde C

```c
#include "msbasic_float.h"

// Declarar variables de 5 bytes
unsigned char num1[5], num2[5], result[5];

// Convertir string a float
fp_string_to_float("123.456", num1);

// Operaciones
fp_add(num1, num2, result);   // result = num1 + num2
fp_sub(num1, num2, result);   // result = num1 - num2
fp_mult(num1, num2, result);  // result = num1 * num2
fp_div(num1, num2, result);   // result = num1 / num2

// Convertir float a string
char buffer[20];
fp_float_to_string(result, buffer);
```

### Funciones disponibles

| Función | Descripción |
|---------|-------------|
| `fp_string_to_float(str, dst)` | Convierte string a float (5 bytes) |
| `fp_float_to_string(src, str)` | Convierte float a string |
| `fp_add(a, b, result)` | Suma: result = a + b |
| `fp_sub(a, b, result)` | Resta: result = a - b |
| `fp_mult(a, b, result)` | Multiplicación: result = a * b |
| `fp_div(a, b, result)` | División: result = a / b |
| `fp_is_zero(num)` | Retorna 1 si num == 0 |

## Formato de Float MSBasic

5 bytes con el siguiente formato:
- **Byte 0**: Exponente (bias 0x80, 0x00 = cero)
- **Byte 1**: Mantisa high + bit de signo (bit 7 = signo)
- **Bytes 2-4**: Mantisa (3 bytes adicionales)

Ejemplos:
| Valor | Hex |
|-------|-----|
| 0 | `00 00 00 00 00` |
| 1 | `81 00 00 00 00` |
| -1 | `81 80 00 00 00` |
| 0.5 | `80 00 00 00 00` |
| 10 | `84 20 00 00 00` |

## Integración con Makefile

```makefile
MSBASIC_FLOAT_DIR = libs/msbasic-float
MSBASIC_DIR = $(MSBASIC_FLOAT_DIR)/msbasic

INCLUDES = -I$(MSBASIC_FLOAT_DIR)/include

# Compilar float_convert.c
$(BUILD_DIR)/float_convert.o: $(MSBASIC_FLOAT_DIR)/src/float_convert.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Compilar msbasic_wrapper.s
$(BUILD_DIR)/msbasic_wrapper.o: $(MSBASIC_FLOAT_DIR)/src/msbasic_wrapper.s
	$(CA65) $(ASFLAGS) -I$(MSBASIC_DIR) -o $@ $<

# Compilar msbasic_float_only.s (requiere flags especiales)
MSBASIC_ASFLAGS = --cpu 6502 --feature force_range -D CONFIG_2 -I$(MSBASIC_DIR)
$(BUILD_DIR)/msbasic_float_only.o: $(MSBASIC_FLOAT_DIR)/src/msbasic_float_only.s
	$(CA65) $(MSBASIC_ASFLAGS) -o $@ $<
```

## Zero Page

La librería usa las siguientes direcciones de página cero (configurables):
- `$55-$59`: FAC (Float ACumulator) - resultado de operaciones
- `$5A-$5E`: ARG (ARGument) - operando secundario
- Variables temporales adicionales en $50-$6D

## Dependencias

- CC65 toolchain
- Ninguna dependencia de runtime de CC65 (standalone)

## Créditos

Basado en el código de Microsoft BASIC para 6502.
Fuente original: [mist64/msbasic](https://github.com/mist64/msbasic)

## Licencia

El código original de MSBasic es propiedad de Microsoft.
Las modificaciones y wrappers son de dominio público.
