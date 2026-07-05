# Arquitectura de msbasic-float

Este documento explica cómo se relacionan los archivos fuente de la librería.

## Diagrama de Dependencias

```
┌─────────────────────────────────────────────────────────────────────┐
│                         APLICACIÓN (C)                              │
│                           main.c                                    │
│                              │                                      │
│                    #include "msbasic_float.h"                       │
│                              │                                      │
│        fp_add(), fp_sin(), fp_log(), fp_pow(), etc.                 │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      CAPA C (float_convert.c)                       │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  fp_string_to_float()  - Convierte "123.45" → float 5 bytes │    │
│  │  fp_float_to_string()  - Convierte float 5 bytes → string   │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                              │                                      │
│              Llama a las funciones C del wrapper                    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   CAPA WRAPPER (msbasic_wrapper.s)                  │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Exporta ~25 símbolos para C:                               │    │
│  │    _fp_add, _fp_sub, _fp_mul, _fp_div, _fp_pow              │    │
│  │    _fp_sin, _fp_cos, _fp_tan, _fp_atn                       │    │
│  │    _fp_log, _fp_exp, _fp_sqr, _fp_abs, _fp_int              │    │
│  │    _fp_load_fac, _fp_save_fac, _fp_clear_fac                │    │
│  │    _FAC, _ARG                                                │    │
│  │                                                             │    │
│  │  Cada función:                                              │    │
│  │    1. Carga operandos en FAC/ARG                            │    │
│  │    2. Llama a rutina MSBasic (FADD, SIN, LOG, etc.)         │    │
│  │    3. Guarda resultado de FAC al destino                    │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                              │                                      │
│            Llama a: FADD, FSUB, FMULT, FDIV, SIN, COS,             │
│                     TAN, ATN, LOG, EXP, SQR, ABS, INT, FPWRT       │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    ▼                     ▼
┌──────────────────────────────┐ ┌──────────────────────────────┐
│  msbasic_float_only.s        │ │  msbasic_trig.s              │
│  ┌──────────────────────────┐│ │  ┌──────────────────────────┐│
│  │ 1. Configura Zero Page   ││ │  │ CONSTANTES:              ││
│  │    ZP_START1 = $20       ││ │  │  CON_PI_HALF (π/2)       ││
│  │    ZP_START2 = $2A       ││ │  │  CON_PI_DOUB (2π)        ││
│  │    ZP_START3 = $35       ││ │  │  POLY_SIN, POLY_ATN      ││
│  │    ZP_START4 = $80       ││ │  │                          ││
│  │                          ││ │  │ FUNCIONES:               ││
│  │ 2. Stubs: ERROR, STROUT  ││ │  │  SIN() - seno            ││
│  │                          ││ │  │  COS() - coseno          ││
│  │ 3. Incluye:              ││ │  │  TAN() - tangente        ││
│  │    defines.s             ││ │  │  ATN() - arcotangente    ││
│  │    macros.s              ││ │  └──────────────────────────┘│
│  │    zeropage.s            ││ └──────────────────────────────┘
│  │    float.s ← RUTINAS!    ││
│  └──────────────────────────┘│
└──────────────────────────────┘
         │              │
         ▼              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    MSBASIC ORIGINAL (msbasic/)                      │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  defines.s  - Constantes y configuración de MSBasic         │    │
│  │  macros.s   - Macros de ensamblador                         │    │
│  │  zeropage.s - Definiciones de FAC, ARG, variables ZP        │    │
│  │  float.s    - RUTINAS MATEMÁTICAS:                          │    │
│  │               FADD, FSUB, FMULT, FDIV, DIV                  │    │
│  │               LOG, EXP, SQR, FPWRT, INT, ABS, SGN           │    │
│  │               NORMALIZE, ROUND, POLYNOMIAL, POLYNOMIAL_ODD  │    │
│  │               QINT, GIVAYF, SIGN                            │    │
│  │               (~1800 líneas de código 6502)                 │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

## Flujo de una Operación: `a + b`

```
C: fp_add(a, b, result)
         │
         ▼
msbasic_wrapper.s: _fp_add → carga FAC=a, ARG=b, JSR FADD
         │
         ▼
msbasic/float.s: FADD → suma FAC + ARG, resultado en FAC
         │
         ▼
msbasic_wrapper.s: guarda FAC en 'result' (5 bytes)
         │
         ▼
C: 'result' contiene a + b
```

## Flujo de una Operación: `sin(x)`

```
C: fp_sin(x, result)
         │
         ▼
msbasic_wrapper.s: _fp_sin → carga FAC=x, JSR SIN
         │
         ▼
msbasic_trig.s: SIN → reduce rango a [0, 2π],
                pliega a [0, π/4], evalúa POLYNOMIAL_ODD
         │
         ▼
msbasic/float.s: POLYNOMIAL_ODD usa FMULT, FADD internamente
         │
         ▼
msbasic_wrapper.s: guarda FAC en 'result' (5 bytes)
         │
         ▼
C: 'result' contiene sin(x)
```

## Archivos y su Propósito

| Archivo | Capa | Propósito |
|---------|------|-----------|
| `msbasic_float.h` | API | Declaraciones C (~25 funciones) |
| `float_convert.c` | C | Conversiones string↔float |
| `msbasic_wrapper.s` | ASM/C | Puente entre C y rutinas MSBasic |
| `msbasic_float_only.s` | ASM | Configuración ZP + stubs + float.s |
| `msbasic_trig.s` | ASM | SIN, COS, TAN, ATN desde trig.s |
| `msbasic/defines.s` | MSBasic | Configuración original MSBasic |
| `msbasic/macros.s` | MSBasic | Macros de ensamblador |
| `msbasic/zeropage.s` | MSBasic | Variables en página cero |
| `msbasic/float.s` | MSBasic | **Las rutinas matemáticas reales** |

## ¿Por qué esta estructura?

### 1. Separación de responsabilidades

- **float.s** es código de Microsoft de los años 70, probado y optimizado
- **msbasic_float_only.s** adapta float.s a nuestro entorno (ZP, stubs)
- **msbasic_trig.s** aísla las funciones trigonométricas (se ensambla aparte)
- **msbasic_wrapper.s** hace todo llamable desde C (convención CC65)
- **float_convert.c** añade funcionalidad que MSBasic no tiene

### 2. Reutilización

El código original de MSBasic (`msbasic/`) está intacto. float.s y trig.s
(proyecto completo) son archivos separados que se incluyen sin modificar.

### 3. Módulos separados

`msbasic_float_only.s` y `msbasic_trig.s` se ensamblan por separado y se
linkean independientemente. Si no necesitas trigonometría, simplemente no
incluyas `msbasic_trig.o` en el link.

## Funciones Disponibles en Cada Capa

### float.s (msbasic_float_only.o)

| Función | Descripción |
|---------|-------------|
| FADD | Suma FAC = FAC + (Y,A) |
| FSUB | Resta FAC = (Y,A) - FAC |
| FMULT | Multiplicación FAC = FAC × (Y,A) |
| FDIV | División FAC = (Y,A) / FAC |
| FPWRT | Potencia ARG ^ FAC |
| LOG | Logaritmo natural |
| EXP | Exponencial (e^x) |
| SQR | Raíz cuadrada |
| INT | Floor (mayor entero ≤ FAC) |
| ABS | Valor absoluto |
| SGN | Signo (-1, 0, 1) |
| NEGOP | Negar FAC |
| POLYNOMIAL | Evaluación de polinomios |
| POLYNOMIAL_ODD | Evaluación de polinomios impares |

### trig.s (msbasic_trig.o)

| Función | Descripción |
|---------|-------------|
| SIN | Seno (radianes) |
| COS | Coseno (radianes) |
| TAN | Tangente (radianes) |
| ATN | Arcotangente (radianes) |

## Zero Page

La librería usa estas direcciones (configurables en `msbasic_float_only.s`):

```
$20-$29: Variables temporales (ZP_START1)
$2A-$34: Variables temporales (ZP_START2)
$35-$3F: CHARAC, CPRMASK, flags (ZP_START3)
$80-$B7: FAC, ARG, temp vars (ZP_START4)
         ├─ $80: TEMPPT
         ├─ $90-$94: RESULT (5 bytes)
         ├─ $B8-$B9: TEMP3 (usado por TAN)
         └─ $CB-$CF: FAC (Float Accumulator)
         └─ $D0-$D4: ARG (Argument Register)
```

## Formato Float MSBasic (5 bytes)

```
Byte:   [0]      [1]         [2]    [3]    [4]
        EXP    MANT1+SIGN   MANT2  MANT3  MANT4

EXP: Exponente con bias 0x80 (0x00 = número es cero)
MANT1: Bits 22-16 de mantisa, bit 7 = signo (1=negativo)
MANT2: Bits 15-8 de mantisa
MANT3: Bits 7-0 de mantisa
MANT4: Byte extra de precisión
```

Ejemplos:
```
  0     = 00 00 00 00 00
  1     = 81 00 00 00 00
  -1    = 81 80 00 00 00
  0.5   = 80 00 00 00 00
  10    = 84 20 00 00 00
  π     = 82 49 0F DA A2
```

## Compilación

```makefile
# 1. msbasic_float_only.s incluye los .s de msbasic/
$(BUILD_DIR)/msbasic_float_only.o: msbasic_float_only.s
    ca65 --cpu 6502 --feature force_range -D CONFIG_2 -I$(MSBASIC_DIR) -o $@ $<

# 2. msbasic_trig.s se ensambla con los mismos flags
$(BUILD_DIR)/msbasic_trig.o: msbasic_trig.s
    ca65 --cpu 6502 --feature force_range -D CONFIG_2 -I$(MSBASIC_DIR) -o $@ $<

# 3. msbasic_wrapper.s exporta símbolos para C
$(BUILD_DIR)/msbasic_wrapper.o: msbasic_wrapper.s
    ca65 --cpu 6502 -I$(MSBASIC_DIR) -o $@ $<

# 4. float_convert.c se compila normalmente
$(BUILD_DIR)/float_convert.o: float_convert.c
    cc65 -c -t none -I$(INCLUDES) -O --cpu 6502 -o $@ $<

# 5. El linker une todo
ld65 ... msbasic_float_only.o msbasic_trig.o msbasic_wrapper.o \
        float_convert.o ... D:\cc65\lib\none.lib
```

## Extensión

Para agregar nuevas funciones (si existieran en MSBasic):

1. Agregar wrapper en `msbasic_wrapper.s`
2. Agregar declaración en `msbasic_float.h`
3. Recompilar

Si la función no existe en MSBasic, implementarla en C en
`float_convert.c` usando las operaciones básicas disponibles.
