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
│                    fp_add(), fp_div(), etc.                         │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      CAPA C (float_convert.c)                       │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  fp_string_to_float()  - Convierte "123.45" → float 5 bytes │    │
│  │  fp_float_to_string()  - Convierte float 5 bytes → string   │    │
│  │  fp_add(), fp_sub()... - Funciones C que llaman a ASM       │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                              │                                      │
│              Llama a: _fp_add, _fp_div, _FAC, _ARG                  │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   CAPA WRAPPER (msbasic_wrapper.s)                  │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  Exporta símbolos para C:                                   │    │
│  │    .export _fp_add, _fp_sub, _fp_mult, _fp_div              │    │
│  │    .export _FAC, _ARG                                       │    │
│  │                                                             │    │
│  │  Cada función:                                              │    │
│  │    1. Carga operandos en FAC/ARG                            │    │
│  │    2. Llama a rutina MSBasic (FADD, FDIV, etc.)             │    │
│  │    3. Guarda resultado de FAC al destino                    │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                              │                                      │
│              Llama a: FADD, FSUB, FMULT, FDIV, etc.                 │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                CAPA ENSAMBLADOR (msbasic_float_only.s)              │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  1. Configura Zero Page:                                    │    │
│  │       ZP_START1 = $20  (variables temporales)               │    │
│  │       FAC = $55-$59    (acumulador flotante)                │    │
│  │       ARG = $5A-$5E    (argumento)                          │    │
│  │                                                             │    │
│  │  2. Define STUBS para símbolos que float.s necesita:        │    │
│  │       ERROR, STROUT, IQERR (nunca se llaman)                │    │
│  │                                                             │    │
│  │  3. Incluye el código MSBasic:                              │    │
│  │       .include "defines.s"                                  │    │
│  │       .include "macros.s"                                   │    │
│  │       .include "zeropage.s"                                 │    │
│  │       .include "float.s"   ← ¡AQUÍ ESTÁN LAS RUTINAS!       │    │
│  └─────────────────────────────────────────────────────────────┘    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    MSBASIC ORIGINAL (msbasic/)                      │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  defines.s  - Constantes y configuración de MSBasic         │    │
│  │  macros.s   - Macros de ensamblador                         │    │
│  │  zeropage.s - Definiciones de FAC, ARG, variables ZP        │    │
│  │  float.s    - RUTINAS MATEMÁTICAS REALES:                   │    │
│  │               FADD, FSUB, FMULT, FDIV                       │    │
│  │               NORMALIZE, ROUND, etc.                        │    │
│  │               (~1500 líneas de código 6502)                 │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

## Flujo de una Operación: `a + b`

```
C: fp_add(a, b, result)
         │
         ▼
float_convert.c: copia 'a' a FAC, 'b' a ARG, llama _fp_add
         │
         ▼
msbasic_wrapper.s: _fp_add → carga Y:X con puntero a ARG, JSR FADD
         │
         ▼
msbasic/float.s: FADD → suma FAC + ARG, resultado queda en FAC
         │
         ▼
msbasic_wrapper.s: guarda FAC en 'result' (5 bytes)
         │
         ▼
C: 'result' contiene a + b
```

## Archivos y su Propósito

| Archivo | Capa | Propósito |
|---------|------|-----------|
| `msbasic_float.h` | API | Declaraciones C para el usuario |
| `float_convert.c` | C | Conversiones string↔float, interfaz de alto nivel |
| `msbasic_wrapper.s` | ASM/C | Puente entre C y las rutinas MSBasic |
| `msbasic_float_only.s` | ASM | Configuración y ensamblaje de MSBasic |
| `msbasic/defines.s` | MSBasic | Configuración original MSBasic |
| `msbasic/macros.s` | MSBasic | Macros de ensamblador |
| `msbasic/zeropage.s` | MSBasic | Variables en página cero |
| `msbasic/float.s` | MSBasic | **Las rutinas matemáticas reales** |

## ¿Por qué esta estructura?

### 1. Separación de responsabilidades

- **float.s** es código de Microsoft de los años 70, probado y optimizado
- **msbasic_float_only.s** adapta ese código a nuestro entorno (configura ZP, stubs)
- **msbasic_wrapper.s** hace el código llamable desde C (convención de llamada CC65)
- **float_convert.c** añade funcionalidad que MSBasic no tiene (conversión sin strings)

### 2. Reutilización

El código original de MSBasic (`msbasic/`) está intacto. Si se actualiza el repo original, solo hay que copiar los archivos.

### 3. Portabilidad

Para usar en otro proyecto:
1. Copiar `libs/msbasic-float/`
2. Ajustar `ZP_START` en `msbasic_float_only.s` si hay conflictos
3. Incluir en el makefile

## Zero Page

La librería usa estas direcciones (configurables en `msbasic_float_only.s`):

```
$20-$29: Variables temporales (ZP_START1)
$2A-$34: Variables temporales (ZP_START2)  
$35-$4F: Variables temporales (ZP_START3)
$55-$59: FAC (Float ACcumulator) - resultado
$5A-$5E: ARG (ARGument) - operando
$80-$8F: Variables temporales (ZP_START4)
```

## Formato Float MSBasic (5 bytes)

```
Byte:   [0]      [1]         [2]    [3]    [4]
        EXP    MANT1+SIGN   MANT2  MANT3  MANT4
        
EXP: Exponente con bias 0x80 (0x00 = número es cero)
MANT1: Byte alto de mantisa, bit 7 = signo (1=negativo)
MANT2-4: Resto de mantisa (24 bits más)

Ejemplos:
  0     = 00 00 00 00 00
  1     = 81 00 00 00 00  (exp=1, mantisa implícita 1.0)
  -1    = 81 80 00 00 00  (igual pero bit 7 = 1)
  0.5   = 80 00 00 00 00  (exp=0 → 2^0 * 0.5 = 0.5)
  10    = 84 20 00 00 00  (exp=4 → 2^4 * 0.625 = 10)
  123.5 = 87 77 00 00 00
```

## Compilación

El makefile ensambla en este orden:

```makefile
# 1. msbasic_float_only.s incluye los .s de msbasic/
$(BUILD_DIR)/msbasic_float_only.o: msbasic_float_only.s
    ca65 --cpu 6502 -D CONFIG_2 -I$(MSBASIC_DIR) -o $@ $<

# 2. msbasic_wrapper.s exporta símbolos para C
$(BUILD_DIR)/msbasic_wrapper.o: msbasic_wrapper.s
    ca65 --cpu 6502 -I$(MSBASIC_DIR) -o $@ $<

# 3. float_convert.c se compila normalmente
$(BUILD_DIR)/float_convert.o: float_convert.c
    cc65 -O -o $@ $<

# 4. El linker une todo
ld65 ... msbasic_float_only.o msbasic_wrapper.o float_convert.o ...
```

## Extensión

Para agregar nuevas funciones:

1. **Si existe en MSBasic** (ej: SQR, SIN, COS):
   - Agregar wrapper en `msbasic_wrapper.s`
   - Agregar declaración en `msbasic_float.h`

2. **Si no existe**:
   - Implementar en `float_convert.c` usando las operaciones básicas
   - O escribir en ASM en `msbasic_wrapper.s`
