# Calculadora de Punto Flotante - Monitor 6502

Calculadora de punto flotante completa usando el módulo **TM1638** (display de 8 dígitos + 16 botones) con el Monitor 6502 en la Tang Nano 9K.

## Características

- ✅ Operaciones: suma (+), resta (-), multiplicación (*), división (/)
- ✅ Números grandes hasta ~16 millones
- ✅ Decimales pequeños (ej: 0.000123)
- ✅ Precisión: ~6-7 dígitos significativos
- ✅ Redondeo inteligente (122.9999 → 123)
- ✅ Números periódicos (1/3 = 0.333333)
- ✅ Operaciones encadenadas (resultado anterior como primer operando)
- ✅ Usa rutinas de punto flotante de **MSBasic**
- ✅ Display adapta decimales según espacio disponible
- ✅ Interfaz tipo calculadora estándar

## Distribución del Teclado

```
[1] [2] [3] [+]  ← Teclas 1,2,3,4
[4] [5] [6] [-]  ← Teclas 5,6,7,8  
[7] [8] [9] [*]  ← Teclas 9,10,11,12
[.] [0] [=] [/]  ← Teclas 13,14,15,16
```

- **.**: Punto decimal (permite ingresar números con decimales)
- **=**: Igual (ejecuta la operación)
- **Presión larga de [.]** (1 segundo): Clear - reinicia la calculadora

## Ejemplos de Operaciones

| Operación | Resultado |
|-----------|-----------|
| 1 / 2 | 0.5 |
| 1 / 3 | 0.333333 |
| 1 / 7 × 10000 | 1428.571 |
| 0.000123 × 1000000 | 123 |
| 55555 × 80 | 4444400 |
| 1.2 - 3 | -1.8 |

## Estructura del Proyecto

```
CalculadorapuntoFlotante-Tm1638/
├── src/
│   ├── main.c               # Código principal de la calculadora
│   ├── float_convert.c      # Conversiones string ↔ float
│   ├── msbasic_wrapper.s    # Wrapper ASM para rutinas MSBasic
│   ├── msbasic_float_only.s # Rutinas de punto flotante MSBasic
│   └── startup.s            # Código de inicio del runtime C
├── include/
│   ├── romapi.h             # Definiciones de ROM API
│   └── msbasic_float.h      # Header para punto flotante
├── config/
│   └── programa.cfg         # Configuración del linker
├── libs/
│   └── msbasic/             # Fuentes originales de MSBasic
├── build/                   # Archivos objeto (generados)
├── output/                  # Binario final (generado)
├── makefile                 # Script de compilación
└── README.md                # Este archivo
```

## Hardware Requerido

- **Tang Nano 9K** con Monitor 6502 v2.2.0+
- **Módulo TM1638** (QYF-TM1638: 8 dígitos, 16 botones)
- Conexiones TM1638:
  - STB (Strobe): Puerto configurable
  - CLK (Clock): Puerto configurable  
  - DIO (Data): Puerto configurable

## Software Requerido

- **CC65** instalado en `D:\cc65` (o ajustar ruta en makefile)
- **Monitor 6502 v2.2.0+** con ROM API
- **Librería TM1638** en `../../libs/tm1638-6502-cc65`

## Compilación

```bash
# Compilar el programa
make

# Limpiar archivos generados
make clean
```

El binario se genera en `output/calc-float.bin` (~13KB)

## Instalación y Uso

### Vía SD Card
1. Copiar `output/calc-float.bin` a la SD Card como `CALC`
2. En el monitor:
```
LOAD CALC 0800
R 0800
```

### Vía XMODEM
```
XRECV 0800
R 0800
```

## Funcionamiento

La calculadora funciona como una calculadora estándar:

1. **Ingresar primer número** (con dígitos y opcionalmente punto decimal)
2. **Presionar operación** (+, -, *, /)
3. **Ingresar segundo número**
4. **Presionar =** para ver el resultado
5. El resultado se puede usar como primer operando de la siguiente operación

### Clear (Limpiar)
Mantener presionada la tecla [.] por 1 segundo para reiniciar la calculadora.

## Detalles Técnicos

### Formato de Punto Flotante MSBasic
- 5 bytes: [exponente][mantisa1][mantisa2][mantisa3][mantisa4]
- Signo: bit 7 del byte de mantisa1
- Exponente: exceso 128 (0x80 = 2^0 = 1)
- Mantisa: 24 bits con bit implícito

### Conversiones Implementadas
- **String → Float**: Construye dígito a dígito usando operaciones float
- **Float → String**: Extrae parte entera y decimal, redondeo inteligente

### Limitaciones
- Números mayores a ~16 millones muestran "ERR"
- Display máximo de 8 caracteres (incluyendo signo y punto)
- División por cero no está validada

## Licencia

Este proyecto usa código de MSBasic (dominio público) y está disponible bajo licencia MIT.
