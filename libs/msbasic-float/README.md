# MSBasic Float Library for CC65

Librería de punto flotante extraída de **Microsoft BASIC 6502** para usar en proyectos CC65 sin necesidad de un BASIC en ROM.

Proporciona operaciones aritméticas, trigonométricas, logarítmicas y de conversión en formato float de 5 bytes.

## Características

- ✅ **Aritmética**: suma, resta, multiplicación, división
- ✅ **Trigonométricas**: seno, coseno, tangente, arcotangente (radianes)
- ✅ **Log/Exp**: logaritmo natural, exponencial (e^x)
- ✅ **Raíz/Abs**: raíz cuadrada, valor absoluto
- ✅ **Potencia**: a^b mediante exp(log(a)*b)
- ✅ **Conversión**: string ↔ float, entero ↔ float
- ✅ **Compatible con CC65** (`-t none` o cualquier target)
- ✅ **Sin dependencias externas** — solo código 6502 puro
- ✅ **Formato MSBasic**: 5 bytes, precisión ~6-7 dígitos
- ✅ **Código probado**:直接 de la ROM de Microsoft BASIC

## Estructura

```
msbasic-float/
├── include/
│   └── msbasic_float.h            # API completa en C
├── src/
│   ├── msbasic_float_only.s       # float.s original (aritmética)
│   ├── msbasic_trig.s             # trig.s original (sin, cos, tan, atan)
│   ├── msbasic_wrapper.s          # Wrappers ASM → C (25 funciones)
│   └── float_convert.c            # string ↔ float en C puro
├── msbasic/
│   ├── defines.s                  # Configuración MSBasic
│   ├── macros.s                   # Macros de ensamblador
│   ├── zeropage.s                 # Variables Zero Page
│   └── float.s                    # float.s original (sin modificar)
├── README.md
└── ARCHITECTURE.md
```

## API Completa

### Operaciones Aritméticas

```c
void fp_add(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);
void fp_sub(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);
void fp_mul(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);
void fp_div(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);
void fp_pow(const msbasic_float_t *a, const msbasic_float_t *b, msbasic_float_t *result);  // a^b
```

### Funciones Trigonométricas (radianes)

```c
void fp_sin(const msbasic_float_t *x, msbasic_float_t *result);
void fp_cos(const msbasic_float_t *x, msbasic_float_t *result);
void fp_tan(const msbasic_float_t *x, msbasic_float_t *result);
void fp_atn(const msbasic_float_t *x, msbasic_float_t *result);  // arcotangente
```

### Logaritmo y Exponencial

```c
void fp_log(const msbasic_float_t *x, msbasic_float_t *result);  // ln(x), x > 0
void fp_exp(const msbasic_float_t *x, msbasic_float_t *result);  // e^x
```

### Otras Funciones

```c
void fp_sqr(const msbasic_float_t *x, msbasic_float_t *result);  // sqrt(x), x >= 0
void fp_abs(const msbasic_float_t *x, msbasic_float_t *result);
void fp_int(const msbasic_float_t *x, msbasic_float_t *result);  // floor(x)
```

### Conversiones

```c
void fp_string_to_float(const char *str, msbasic_float_t *result);
void fp_float_to_string(const msbasic_float_t *value, char *buffer, uint8_t max_len);
void fp_int_to_fac(int16_t value);           // entero 16-bit → FAC interno
int16_t fp_fac_to_int(void);                 // FAC interno → entero 16-bit
```

### Manejo del Acumulador (FAC)

```c
void fp_load_fac(const msbasic_float_t *src);   // memoria → FAC
void fp_save_fac(msbasic_float_t *dest);         // FAC → memoria
void fp_clear_fac(void);                         // FAC = 0
```

### Constantes Útiles

```c
#define FP_ZERO {{0x00, 0x00, 0x00, 0x00, 0x00}}
#define FP_ONE  {{0x81, 0x00, 0x00, 0x00, 0x00}}
#define FP_TWO  {{0x82, 0x00, 0x00, 0x00, 0x00}}
#define FP_TEN  {{0x84, 0x20, 0x00, 0x00, 0x00}}
#define FP_HALF {{0x80, 0x00, 0x00, 0x00, 0x00}}
#define FP_PI   {{0x82, 0x49, 0x0F, 0xDA, 0xA2}}  // π
```

## Ejemplo de Uso

```c
#include "msbasic_float.h"

msbasic_float_t a, b, r;
char buf[20];

// 2 + 3
a = (msbasic_float_t)FP_TWO;
b = (msbasic_float_t){{0x82, 0x40, 0x00, 0x00, 0x00}};  // 3
fp_add(&a, &b, &r);
fp_float_to_string(&r, buf, 20);  // buf = "5"

// sin(π/2)
msbasic_float_t pi = (msbasic_float_t)FP_PI;  // π
fp_load_fac(&pi);                              // FAC = π
// Usar fp_div con constante 2... o usar fp_sin directamente

// Conversión desde string
fp_string_to_float("123.456", &a);
fp_float_to_string(&a, buf, 20);  // buf = "123.456"
```

## Formato Float MSBasic (5 bytes)

```
Byte:   [0]      [1]         [2]    [3]    [4]
        EXP    MANT1+SIGN   MANT2  MANT3  MANT4

EXP:  Exponente con bias 0x80 (0x00 = número es cero)
MANT1: Bits 22-16 de mantisa, bit 7 = signo (1 = negativo)
MANT2: Bits 15-8 de mantisa
MANT3: Bits 7-0 de mantisa
MANT4: Byte extra de precisión (bits 23-30 de mantisa, sin bit implícito)
```

### Ejemplos

| Valor | Hex (5 bytes) |
|-------|---------------|
| 0 | `00 00 00 00 00` |
| 1 | `81 00 00 00 00` |
| -1 | `81 80 00 00 00` |
| 0.5 | `80 00 00 00 00` |
| 10 | `84 20 00 00 00` |
| π | `82 49 0F DA A2` |

Rango: ±1.7×10³⁸, Precisión: ~6-7 dígitos decimales.

## Zero Page

La librería usa las siguientes direcciones de Zero Page (configurables en `msbasic_float_only.s`):

| Bloque | Dirección | Uso | Tamaño |
|--------|-----------|-----|--------|
| ZP_START1 | `$20` | Temporales (GORESTART, GOSTROUT, etc.) | 10 bytes |
| ZP_START2 | `$2A` | Temporales (Z15, POSX, LINNUM, etc.) | 11 bytes |
| ZP_START3 | `$35` | Parseo (CHARAC, CPRMASK, etc.) | 11 bytes |
| ZP_START4 | `$80` | FAC, ARG y variables float | 56 bytes |

**FAC** (Float Accumulator): `$xx` (dentro de ZP_START4)
**ARG** (Argument Register): `$xx+5` (dentro de ZP_START4)

Si estas direcciones entran en conflicto con tu sistema, redefíne los `ZP_START` en `msbasic_float_only.s`.

## Integración con Makefile

```makefile
MSBASIC_FLOAT_DIR = libs/msbasic-float
MSBASIC_DIR = $(MSBASIC_FLOAT_DIR)/msbasic

INCLUDES = -I$(MSBASIC_FLOAT_DIR)/include

# Flags especiales para MSBasic
MSBASIC_ASFLAGS = --cpu 6502 --feature force_range -D CONFIG_2 -I$(MSBASIC_DIR)

# Compilar módulos
$(BUILD_DIR)/msbasic_float_only.o: $(MSBASIC_FLOAT_DIR)/src/msbasic_float_only.s
	$(CA65) $(MSBASIC_ASFLAGS) -o $@ $<

$(BUILD_DIR)/msbasic_trig.o: $(MSBASIC_FLOAT_DIR)/src/msbasic_trig.s
	$(CA65) $(MSBASIC_ASFLAGS) -o $@ $<

$(BUILD_DIR)/msbasic_wrapper.o: $(MSBASIC_FLOAT_DIR)/src/msbasic_wrapper.s
	$(CA65) $(ASFLAGS) -I$(MSBASIC_DIR) -o $@ $<

$(BUILD_DIR)/float_convert.o: $(MSBASIC_FLOAT_DIR)/src/float_convert.c
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

# Linkar (agregar los .o a OBJECTS y none.lib)
```

## Compatibilidad

| Plataforma | Estado |
|------------|--------|
| **Monitor 6502** (Tang Nano 9K) | ✅ Probado |
| **Cualquier 6502 con CC65** | ✅ ZP configurable |
| **C64 / VIC-20 / PET** | ⚠️ Preferir ROM nativa (mismas rutinas) |
| **Apple II (Applesoft)** | ⚠️ Preferir ROM nativa (mismas rutinas) |
| **Atari 400/800** | ❌ Formato BCD incompatible |

## Dependencias

- **CC65 toolchain** (ca65, ld65, o cl65)
- Ninguna dependencia de runtime — funciona standalone
- Ninguna dependencia de I/O (UART, display, etc.)

## Tamaño

| Componente | Tamaño |
|------------|--------|
| float.s (aritmética + log/exp/sqr) | ~2.1 KB |
| trig.s (sin, cos, tan, atan) | ~0.3 KB |
| wrappers ASM (~25 funciones C) | ~1.2 KB |
| float_convert.c (string ↔ float) | ~2.0 KB |
| **Total** | **~5.5 KB** |

## Créditos

- Basado en el código de **Microsoft BASIC para 6502** (mist64/msbasic)
- Código original: [github.com/mist64/msbasic](https://github.com/mist64/msbasic)
- Wrappers y adaptación CC65: Nelson Figueroa

## Licencia

El código original de MSBasic es propiedad de Microsoft.
Las modificaciones, wrappers y adaptaciones son de dominio público.
