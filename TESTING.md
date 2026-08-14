# Pruebas Automatizadas - Calculadora Científica 6502

Este documento explica cómo ejecutar el paquete de pruebas automatizadas
contra el hardware real (Tang Nano 9K + Monitor 6502).

## Requisitos

- **Python 3** (usa solo librerías estándar: `socket`, `struct`, `time`)
- **Bridge UART→TCP** (ESP32-C3 o similar) conectado al monitor 6502
- **Compilar el programa primero**:

```bash
make
```

## Uso

```bash
python3 tools/test_calc.py
```

El script hace todo automáticamente:

```
1. Conecta al bridge (TCP raw)
2. Envía 'quit' (por si la calculadora sigue activa)
3. Espera el prompt del monitor
4. Envía 'XRECV 0800' → el monitor entra en modo XMODEM
5. Transfiere output/calc-sci.bin por XMODEM (checksum)
6. Envía 'R 0800' → ejecuta la calculadora
7. Envía 42 expresiones de prueba y valida resultados
8. Envía 'quit' → vuelve al monitor
```

## Configuración

Las constantes al inicio del script (`tools/test_calc.py`):

```python
HOST = "192.168.1.143"   # IP del bridge
PORT = 23                 # Puerto TCP raw (ESP32-C3 bridge)
BIN_FILE = "output/calc-sci.bin"
```

### Nota sobre el puerto

El puerto 22 puede no ser el correcto. El bridge ESP32-C3 de este proyecto
expone el UART por el **puerto 23** (telnet raw). Verifica con:

```bash
python3 -c "
import socket
for p in [22, 23, 80, 8080]:
    s = socket.socket(); s.settimeout(2)
    try:
        s.connect(('192.168.1.143', p)); print(f'{p}: ABIERTO')
    except: print(f'{p}: cerrado')
    s.close()
"
```

## Protocolo XMODEM

El monitor 6502 usa **XMODEM checksum** (NAK inicial), no CRC:

| Señal | Valor | Significado |
|-------|-------|-------------|
| NAK | `0x15` | Receptor pide **checksum** (modo usado) |
| 'C' | `0x43` | Receptor pide CRC16 |
| SOH | `0x01` | Inicio de bloque |
| ACK | `0x06` | Bloque aceptado |
| EOT | `0x04` | Fin de transferencia |
| CAN | `0x18` | Cancelar |

Formato de bloque (checksum):

```
[SOH][bloque#][255-bloque#][128 bytes datos][checksum mod 256]
```

## Comunicación con el 6502

El monitor 6502 (3.375 MHz) pierde caracteres si se envía la línea completa
de una vez. El script usa **eco-sync**: envía un carácter y espera a que el
monitor lo haga eco antes de enviar el siguiente. Esto garantiza que cada
carácter llega.

## Pruebas Incluidas (95)

### Operaciones básicas
```
2+2            = 4
10-3           = 7
6*7            = 42
10/4           = 2.5
0.1+0.2        = 0.3
```

### Precedencia y paréntesis
```
2+3*4          = 14
(2+3)*4        = 20
2*(3+4)        = 14
((2+3)*2)+1    = 11
2+3*4-6/2      = 11
```

### Números grandes
```
850*40000      = 34000000
100000*100     = 10000000
9999999+1      = 10000000
```

### Potencia
```
2^8            = 256
2^10           = 1024
3^2            = 9
10^5           = 100000 (±0.001)
4^0.5          = 2
2^2^3          = 256   (asociativa a derecha)
```

### Trigonometría (tolerancia ±1e-5 por precisión float)
```
sin(0)         = 0
cos(0)         ≈ 1     (0.999999 en el hardware)
sin(pi/2)      ≈ 1     (0.999999)
cos(pi)        ≈ -1    (-0.999999)
sin(0.5)^2+cos(0.5)^2 = 1
```

### d2r / r2d / pi
```
d2r(180)       = 3.141592
r2d(pi)        = 180
sin(d2r(90))   ≈ 1     (0.999999)
sin(d2r(45))   ≈ 0.707107 (0.707106)
pi             = 3.141592
```

### Log / Exp
```
log(1)         = 0
exp(0)         = 1
exp(1)         = 2.718281
log(exp(5))    = 5
exp(log(10))   = 10
```

### Raíz / Abs
```
sqr(4)         = 2
sqr(2)         = 1.414213
abs(-5)        = 5
abs(3.14)      = 3.139999 (precisión float)
```

### Errores
```
1/0            = ERR: Division by zero
sqr(-4)        = ERR: Math error
log(0)         = ERR: Math error
log(-5)        = ERR: Math error
```

### Casos de borde

#### División por cero (variantes)
```
0/0            = ERR: Division by zero
-5/0           = ERR: Division by zero
0/5            = 0
```

#### Fracciones periódicas
```
1/3            = 0.333333
1/7            = 0.142857
2/3            = 0.666666 (±1e-5)
```

#### Ceros
```
sqr(0)         = 0
abs(0)         = 0
0+5            = 5
0*5            = 0
5-5            = 0
```

#### Potencia con ceros
```
2^0            = 1
0^2            = 0
0^0            = 1
```

#### Números negativos
```
-5+3           = -2
-2*-3          = 6
2*-3           = -6
-2+3           = 1
abs(-3.5)      = 3.5
```

#### Negación anidada y unario +  (fix v1.0.2)
```
--5            = 5
---5           = -5
--5+3          = 8
2++3           = 5
+5             = 5
-2^2           = 4
(-2)^2         = 4
(-2)^3         = -8
```

> **Bug corregido**: `--5` daba `Syntax error`. El parser ahora usa
> recursión en `parse_unary` para negaciones anidadas y soporta unario `+`.

#### Decimales extremos
```
.5             = 0.5
5.             = 5
0.000001       = 0.000001
999999.999     = 999999.999267 (±0.001)
```

#### Espacios
```
  2  +  3      = 5
```

#### Funciones de esquina
```
atan(1)        = 0.785398
atan(0)        = 0
atan(-1)       = -0.785398
tan(0)         = 0
sqr(100)       = 10
```

#### Errores de sintaxis
```
(2+3           = ERR: Expected ')'
2+3)           = ERR: Syntax error
2**3           = ERR: Syntax error
2+*3           = ERR: Syntax error
sin            = ERR: Expected '('
sin(           = ERR: Unexpected end of expression
2#3            = ERR: Syntax error
2+abc          = ERR: Unknown function
+              = ERR: Unexpected end of expression
*5             = ERR: Syntax error
(2+3))         = ERR: Syntax error
```

## Precisión Esperada

El formato float de MS Basic tiene **~6-7 dígitos decimales** de precisión.
Algunos valores pueden diferir en el último dígito:

| Expresión | Hardware real | Valor teórico |
|-----------|---------------|---------------|
| `cos(0)` | 0.999999 | 1 |
| `sin(pi/2)` | 0.999999 | 1 |
| `sin(d2r(45))` | 0.707106 | 0.707107 |
| `10^5` | 100000.000091 | 100000 |

Esto es normal y coincide con el comportamiento del MS BASIC original
(C64, Apple II). No es un bug.

## Resultado de la última ejecución

```
RESULTADO: 95 OK, 0 FAIL de 95 pruebas
```

## Bugs encontrados por las pruebas

| Bug | Síntoma | Fix | Versión |
|-----|---------|-----|---------|
| fp_pow usaba `a_ptr_zp` para ambos operandos | `2^8` = e^8 = 2980.95 | Copiar `b_ptr_zp` a `a_ptr_zp` antes de cargar FAC | 1.0.0 |
| FPWRT heredaba Z flag incorrecto | `2^8` = e^8 = 2980.95 | `lda FAC` antes de `jsr FPWRT` | 1.0.1 |
| Conversor string usaba 24 bits de mantisa | `850*40000` = ERR | 32 bits (bytes 1-4) y límite exp ≤ $A0 | 1.0.1 |
| Negación anidada no soportada | `--5` = Syntax error | Recursión en `parse_unary` + unario `+` | 1.0.2 |
| `quit` reiniciaba el monitor | Banner "Tang Nano 9K..." al salir | No tocar el SP hardware (RTS simple al monitor) | 1.0.3 |

## Comportamiento de quit/exit

El monitor 6502 llama a los programas con **JSR** (subrutina). Al presionar
`quit` o `exit`, la calculadora retorna con `RTS` y el monitor muestra
`Retorno de $0800` volviendo a su prompt **sin reiniciar**:

```
> quit
Volviendo al monitor...
Retorno de $0800
>          ← el monitor sigue vivo
```

Para lograrlo, `startup.s`:
- **NO resetea el stack hardware** (el contexto del monitor con sus return
  addresses está ahí; se usa el stack desde SP hacia abajo y se balancea)
- Guarda y restaura el software stack pointer (`sp` ZP) del monitor
- Hace `cli` al salir (re-habilita IRQ del monitor)
- Finaliza con `RTS` simple en lugar de `jmp $8000`

## Solución de problemas

| Problema | Causa | Solución |
|----------|-------|----------|
| `ERR: Syntax error` al enviar XRECV | La calculadora sigue corriendo | El script envía `quit` automáticamente |
| Caracteres perdidos (`2+34`) | Envío demasiado rápido | El script usa eco-sync (char por char) |
| `No ACK para bloque N` en XMODEM | Modo CRC vs checksum | El script detecta NAK y usa checksum |
| Timeout al conectar | Puerto equivocado | Verificar con el script de escaneo de puertos |

## Archivos

```
tools/test_calc.py   ← Script de pruebas
TESTING.md           ← Esta documentación
output/calc-sci.bin  ← Binario probado
```
