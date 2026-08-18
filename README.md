# Calculadora Científica - Monitor 6502 (Terminal UART)

Calculadora de punto flotante con funciones trigonométricas, logaritmos, exponenciales y parser de expresiones. Se comunica vía **terminal UART** — no requiere hardware adicional.

## Características

- ✅ **Operadores**: `+`, `-`, `*`, `/`, `^` (potencia)
- ✅ **Paréntesis** anidados para agrupar expresiones
- ✅ **Funciones trigonométricas**: `sin()`, `cos()`, `tan()`, `atan()` (radianes)
- ✅ **Logaritmo natural** `log()` y **exponencial** `exp()`
- ✅ **Raíz cuadrada** `sqrt()`/`sqr()` y **valor absoluto** `abs()`
- ✅ **Constante** `pi` y **resultado anterior** `ans` (reutilizable: `3*ans`)
- ✅ **Conversión grados/radianes**: `d2r()`, `r2d()`
- ✅ **Precisión**: ~6-7 dígitos significativos (formato MSBasic 5 bytes)
- ✅ **Usa rutinas de punto flotante de MSBasic**
- ✅ **Manejo de errores**: división por cero, raíz de negativo, log de ≤ 0
- ✅ **Comandos**: `quit`/`exit` (volver al monitor), `help` (ayuda)

### Ejemplos

```
> 2+3*4
= 14

> sin(pi/2)
= 1

> sin(d2r(45))
= 0.707107

> 2^3
= 8

> log(exp(5))
= 5

> 2+3
= 5

> ans*3
= 15

> ans^2
= 225

> sin(0.5)^2+cos(0.5)^2
= 1

> 1/0
ERR: Division by zero

> --5
= 5

> 2++3
= 5
```

## Estructura del Proyecto

```
CalculadoraPuntoFlotante-Terminal/
├── src/
│   ├── main.c                    # Terminal UART + comandos
│   ├── parser.c                  # Parser de expresiones
│   ├── startup.s                 # Código de inicio del runtime C
├── include/
│   ├── parser.h                  # Header del parser
│   └── romapi.h                  # Definiciones de ROM API
├── libs/
│   └── msbasic-float/            # Librería reusable de punto flotante
│       ├── include/
│       │   └── msbasic_float.h   # API completa (25+ funciones)
│       └── src/
│           ├── msbasic_float_only.s  # Aritmética MS Basic
│           ├── msbasic_trig.s        # Trigonometría MS Basic
│           ├── msbasic_wrapper.s     # Wrappers C
│           └── float_convert.c       # String ↔ float
├── config/
│   └── programa.cfg              # Configuración del linker
├── build/                        # Archivos objeto (generados)
├── output/                       # Binario final (generado)
├── makefile                      # Script de compilación
└── README.md                     # Este archivo
```

## Hardware Requerido

- **Tang Nano 9K** con Monitor 6502 v2.6.2
- **Conexión UART** (USB-TTL o similar) para la terminal

No requiere módulos adicionales — toda la interacción es por terminal.

## Software Requerido

- **CC65** instalado en `D:\cc65` (o ajustar ruta en makefile)
- **Monitor 6502 v2.6.2** con ROM API
- **Terminal serie** (PuTTY, screen, minicom, etc.) a 115200 baud

## Compilación

```bash
# Compilar el programa
make

# Limpiar archivos generados
make clean
```

El binario se genera en `output/calc-sci.bin` (~11.4KB)

## Instalación y Uso

### Vía SD Card
1. Copiar `output/calc-sci.bin` a la SD Card como `CALC`
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

## Funciones Disponibles

### Operadores (precedencia)
| Operador | Descripción | Asociatividad |
|----------|-------------|---------------|
| `()` | Paréntesis | Inner-first |
| `-`, `+` | Unario (negación, signo) — soporta anidados: `--5` = 5 | Right-to-left |
| `^` | Potencia | **Right-associative** |
| `*`, `/` | Multiplicación, División | Left-associative |
| `+`, `-` | Suma, Resta | Left-associative |

### Funciones
| Función | Descripción | Ejemplo |
|---------|-------------|---------|
| `sin(x)` | Seno (radianes) | `sin(pi/2)` = 1 |
| `cos(x)` | Coseno (radianes) | `cos(pi)` = -1 |
| `tan(x)` | Tangente (radianes) | `tan(pi/4)` = 1 |
| `atan(x)` | Arco tangente (radianes) | `atan(1)` = π/4 |
| `log(x)` | Logaritmo natural (x > 0) | `log(e)` = 1 |
| `exp(x)` | Exponencial (e^x) | `exp(1)` = e |
| `sqrt(x)` / `sqr(x)` | Raíz cuadrada (x ≥ 0) | `sqr(4)` = 2 |
| `abs(x)` | Valor absoluto | `abs(-5)` = 5 |
| `d2r(x)` | Grados → Radianes | `d2r(180)` = π |
| `r2d(x)` | Radianes → Grados | `r2d(pi)` = 180 |

### Constantes
| Constante | Valor |
|-----------|-------|
| `pi` | 3.1415926535... |
| `ans` | Último resultado exitoso |

### Reutilizar el resultado anterior (`ans`)

`ans` guarda el último resultado **exitoso** y se puede usar en cualquier
expresión igual que una constante: `3*ans`, `ans^2`, `sin(ans)`, `ans+1`...

```
> 2+3
= 5          ← ans = 5

> ans*3
= 15         ← 5 × 3

> ans^2
= 225        ← 15²

> ans/15
= 15

> sin(ans)
= 0.650287   ← funciona dentro de funciones
```

Comportamiento:
- Se actualiza con cada resultado **exitoso**.
- Un error **no** lo modifica: si `1/0` falla, `ans` conserva el último valor
  válido.
- Si aún no hay ningún resultado (recién iniciada la calculadora), `ans`
  devuelve `ERR: No previous result`.
- Evaluar `ans` a secas también actualiza `ans` (a sí mismo), sin efecto.

### Comandos
| Comando | Acción |
|---------|--------|
| `quit` / `exit` | Volver al monitor 6502 **sin reiniciarlo** (muestra `Retorno de $0800` y sigue el prompt) |
| `help` | Mostrar esta ayuda |

## Detalles Técnicos

### Formato de Punto Flotante MSBasic
- 5 bytes: `[exponente][mantisa1+signo][mantisa2][mantisa3][mantisa4]`
- Signo: bit 7 del byte de mantisa1
- Exponente: exceso 128 (0x80 = 2^0 = 1)
- Precisión: ~6-7 dígitos decimales
- Rango: ~±1.7×10^38

### Librería Reutilizable
La carpeta `libs/msbasic-float/` es una librería independiente que puede usarse en otros proyectos 6502 con CC65. Incluye:

- Operaciones aritméticas (+, -, ×, /)
- Funciones trigonométricas (sin, cos, tan, atan)
- Logaritmo natural y exponencial
- Raíz cuadrada, valor absoluto
- Conversión string ↔ float

### Tamaño
| Componente | Tamaño |
|------------|--------|
| Librería matemática | ~5.5 KB |
| Calculadora completa | ~11.4 KB |
| RAM disponible | ~13.8 KB |
| Espacio libre | ~2.9 KB |

## Diferencias con la Versión TM1638

Este proyecto empezó como una calculadora con display TM1638. La versión actual:

| Característica | TM1638 | Terminal UART |
|---------------|--------|---------------|
| Interfaz | Display 8 dígitos + 16 teclas | Terminal texto |
| Expresiones | Solo 2 operandos | Completas con paréntesis |
| Funciones | Solo +,-,×,/ | Trig, log, exp, sqr, etc. |
| Constantes | No | pi, ans |
| Conversión | No | d2r, r2d |
| Tamaño | ~12.8 KB | ~11.4 KB |

## 💖 Apóyame

Si disfrutas de este proyecto, considera apoyarme:

[![Support me on Ko-fi](https://img.shields.io/badge/Ko--fi-Apóyame-FF5E5B?logo=kofi&logoColor=white&style=for-the-badge)](https://ko-fi.com/nelsonfigueroa2k)

## Licencia

Este proyecto usa código de MSBasic (dominio público) y está disponible bajo licencia MIT.
