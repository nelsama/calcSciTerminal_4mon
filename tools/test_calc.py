#!/usr/bin/env python3
"""
test_calc.py - Pruebas automatizadas para la Calculadora Cientifica 6502
Conecta al bridge TCP raw (192.168.1.143:22), carga el binario por XMODEM
y ejecuta una bateria de expresiones verificando los resultados.
"""
import socket
import time
import sys
import struct

HOST = "192.168.1.143"
PORT = 23  # Bridge ESP32-C3 (RAW serial)
BIN_FILE = "output/calc-sci.bin"

# ============================================================================
# Clase para manejar la conexion con timeout y lectura de datos
# ============================================================================
class UartBridge:
    def __init__(self, host, port, timeout=0.5, connect_timeout=5.0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # El connect usa un timeout amplio con reintentos: el bridge puede
        # tardar en aceptar si esta procesando la sesion anterior.
        self.sock.settimeout(connect_timeout)
        last_err = None
        for _ in range(3):
            try:
                self.sock.connect((host, port))
                break
            except (socket.timeout, ConnectionError) as e:
                last_err = e
                time.sleep(2.0)
        else:
            raise last_err
        # Las lecturas usan el timeout corto para no bloquear los bucles
        self.sock.settimeout(timeout)
        self.buf = b""

    def send(self, data):
        self.sock.sendall(data)

    def send_text(self, text, char_delay=0.05, echo_timeout=1.0):
        """Envia texto char por char esperando el eco del receptor (monitor
        o calculadora). Garantiza que cada caracter llego antes de enviar el
        siguiente y evita perdida de caracteres. Si el receptor no hace eco
        (p.ej. modo XMODEM o launcher), continua tras echo_timeout."""
        for ch in text:
            self.sock.sendall(ch.encode())
            end = time.time() + echo_timeout
            echoed = False
            while time.time() < end:
                try:
                    data = self.sock.recv(1024)
                    if data:
                        if ch.encode() in data or (data and data[-1:] == ch.encode()):
                            echoed = True
                            break
                except socket.timeout:
                    pass
            if not echoed:
                time.sleep(char_delay)
            else:
                time.sleep(0.01)  # pequeña pausa para procesamiento
        self.sock.sendall(b"\r")
        time.sleep(char_delay)

    def read_available(self, wait=0.3):
        """Lee todo lo disponible esperando 'wait' segundos"""
        time.sleep(wait)
        data = b""
        while True:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                data += chunk
            except socket.timeout:
                break
        return data

    def read_until(self, marker, timeout=5.0):
        """Lee hasta encontrar un marcador (bytes)"""
        end = time.time() + timeout
        data = b""
        while time.time() < end:
            try:
                chunk = self.sock.recv(1024)
                if chunk:
                    data += chunk
                    if marker in data:
                        return data
            except socket.timeout:
                pass
        return data

    def close(self):
        self.sock.close()

# ============================================================================
# Protocolo XMODEM (CRC mode)
# ============================================================================
def crc16(data):
    """CRC-16/XMODEM"""
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def checksum(data):
    """Suma de verificacion de 8 bits (modulo 256)"""
    return sum(data) & 0xFF

def xmodem_send(bridge, filepath):
    """Envia un archivo por XMODEM. El monitor es el receptor."""
    with open(filepath, "rb") as f:
        data = f.read()

    # Rellenar a multiplo de 128
    padding = (128 - (len(data) % 128)) % 128
    data += b"\x1A" * padding  # 0x1A = Ctrl-Z (pad)
    total_blocks = len(data) // 128
    print(f"  Archivo: {len(data)} bytes, {total_blocks} bloques")

    # Esperar el primer NAK (checksum) o 'C' (CRC) del receptor.
    # El monitor usa checksum (NAK), pero se acepta 'C' por compatibilidad.
    end = time.time() + 5.0
    response = b""
    use_crc = False
    while time.time() < end:
        response += bridge.read_available(0.3)
        if b"\x15" in response:
            print("  Modo Checksum (el monitor envio NAK)")
            break
        if b"C" in response:
            use_crc = True
            print("  Modo CRC (el monitor envio 'C')")
            break

    block_num = 1
    for i in range(total_blocks):
        block_data = data[i*128:(i+1)*128]
        if use_crc:
            sum_bytes = struct.pack(">H", crc16(block_data))  # CRC16 big-endian
        else:
            sum_bytes = bytes([checksum(block_data)])
        packet = bytes([0x01, block_num & 0xFF, (255 - block_num) & 0xFF]) + block_data + sum_bytes

        # Enviar con reintentos (hasta 8 intentos por bloque)
        ok = False
        for attempt in range(8):
            bridge.send(packet)
            resp = bridge.read_until(b"\x06", timeout=3.0)  # ACK
            if resp and b"\x06" in resp:
                ok = True
                break
            # Si recibimos NAK repetido, puede ser problema de modo: probar el otro
            if attempt == 3 and not use_crc:
                pass
        if not ok:
            print(f"  ERROR: No ACK para bloque {block_num}")
            return False
        block_num = (block_num + 1) & 0xFF

    # Enviar EOT
    bridge.send(b"\x04")  # EOT
    resp = bridge.read_until(b"\x06", timeout=3.0)  # ACK final
    ack_ok = resp and b"\x06" in resp
    print(f"  Transferencia completada: ACK={ack_ok}")
    return True

# ============================================================================
# Paquete de pruebas
# ============================================================================
TESTS = [
    # ans: sin resultado anterior debe dar error
    ("ans", "No previous result", None),

    # Operaciones basicas
    ("2+2",              "4",             None),
    ("10-3",             "7",            None),
    ("6*7",              "42",           None),
    ("10/4",             "2.5",          None),
    ("0.1+0.2",          "0.3",          None),

    # Precedencia
    ("2+3*4",            "14",           None),
    ("(2+3)*4",          "20",           None),
    ("2*(3+4)",          "14",           None),
    ("((2+3)*2)+1",      "11",           None),
    ("2+3*4-6/2",        "11",           None),

    # Numeros grandes
    ("850*40000",        "34000000",     None),
    ("100000*100",       "10000000",     None),
    ("9999999+1",        "10000000",     None),

    # Potencia
    ("2^8",              "256",          None),
    ("2^10",             "1024",         None),
    ("3^2",              "9",            None),
    ("10^5",             "100000",       0.001),
    ("4^0.5",            "2",            None),
    ("2^2^3",            "256",          None),

    # Trigonometria (tolerancia por precision float MSBasic ~6-7 digitos)
    ("sin(0)",           "0",            0.000001),
    ("cos(0)",           "1",            0.00001),
    ("sin(pi/2)",        "1",            0.00001),
    ("cos(pi)",          "-1",           0.00001),
    ("sin(0.5)^2+cos(0.5)^2", "1",      0.000001),

    # d2r / r2d / pi
    ("d2r(180)",         "3.14159",      0.001),
    ("r2d(pi)",          "180",          0.001),
    ("sin(d2r(90))",     "1",            0.00001),
    ("sin(d2r(45))",     "0.707107",     0.00001),
    ("pi",               "3.14159",      0.001),

    # Log / Exp
    ("log(1)",           "0",            0.000001),
    ("exp(0)",           "1",            0.000001),
    ("exp(1)",           "2.71828",      0.0001),
    ("log(exp(5))",      "5",            0.001),
    ("exp(log(10))",     "10",           0.001),

    # Raiz / Abs
    ("sqr(4)",           "2",            0.000001),
    ("sqr(2)",           "1.41421",      0.0001),
    ("abs(-5)",          "5",            None),
    ("abs(3.14)",        "3.14",         0.0001),

    # Errores
    ("1/0",              "Division by zero", None),
    ("sqr(-4)",          "Math error",   None),
    ("log(0)",           "Math error",   None),
    ("log(-5)",          "Math error",   None),

    # =========================================================================
    # CASOS DE BORDE
    # =========================================================================

    # Division por cero (variantes)
    ("0/0",              "Division by zero", None),
    ("-5/0",             "Division by zero", None),
    ("0/5",              "0",            0.000001),
    ("1/3",              "0.333333",     0.00001),
    ("1/7",              "0.142857",     0.00001),
    ("2/3",              "0.666667",     0.00001),

    # Ceros
    ("sqr(0)",           "0",            0.000001),
    ("abs(0)",           "0",            0.000001),
    ("0+5",              "5",            None),
    ("0*5",              "0",            0.000001),
    ("5-5",              "0",            0.000001),

    # Potencia con ceros
    ("2^0",              "1",            0.000001),
    ("0^2",              "0",            0.000001),
    ("0^0",              "1",            0.000001),

    # Numeros negativos
    ("-5+3",             "-2",           None),
    ("-2*-3",            "6",            None),
    ("2*-3",             "-6",           None),
    ("--5",              "5",            None),
    ("-(-5)",            "5",            None),
    ("-2+3",             "1",            None),
    ("abs(-3.5)",        "3.5",          0.0001),

    # Decimales extremos
    ("0.5",              "0.5",          0.000001),
    (".5",               "0.5",          0.000001),
    ("5.",               "5",            0.000001),
    ("0.000001",         "0.000001",     0.0000001),
    ("999999.999",       "999999.999",   0.001),

    # Espacios
    ("  2  +  3  ",      "5",            None),

    # Funciones de esquina
    ("atan(1)",          "0.785398",     0.00001),
    ("atan(0)",          "0",            0.000001),
    ("atan(-1)",         "-0.785398",    0.00001),
    ("exp(1)",           "2.718281",     0.0001),
    ("tan(0)",           "0",            0.000001),
    ("log(1)",           "0",            0.000001),
    ("sqr(100)",         "10",           0.000001),

    # Errores de sintaxis
    ("(2+3",             "Expected ')'", None),
    ("2+3)",             "Syntax error", None),
    ("2**3",             "Syntax error", None),
    ("2+*3",             "Syntax error", None),
    ("sin",              "Expected '('", None),
    ("sin(",             "Unexpected end of expression", None),
    ("2#3",              "Syntax error", None),
    ("2+abc",            "Unknown function", None),
    ("+",                "Unexpected end of expression", None),
    ("*5",               "Syntax error", None),
    ("(2+3))",           "Syntax error", None),

    # Negacion anidada y unario +
    ("--5",              "5",            None),
    ("---5",             "-5",           None),
    ("--5+3",            "8",            None),
    ("2++3",             "5",            None),
    ("+5",               "5",            None),
    ("-2^2",             "4",            None),
    ("(-2)^2",           "4",            0.000001),
    ("(-2)^3",           "-8",           0.001),

    # ans: reutilizacion del resultado anterior
    ("7*3",              "21",           None),          # ans = 21
    ("ans",              "21",           None),          # ans no cambia
    ("ans+1",            "22",           None),          # ans = 22
    ("ans*2",            "44",           None),          # ans = 44
    ("ans/4",            "11",           None),          # ans = 11
    ("ans^2",            "121",          None),          # ans = 121
    ("sqr(ans)",         "11",           0.000001),      # ans = 11
    ("1/0",              "Division by zero", None),      # error: ans NO cambia
    ("ans+1",            "12",           None),          # ans sigue 11
]

# ============================================================================
# Validacion de resultados
# ============================================================================
def parse_output(text):
    """Extrae el resultado de '= xxx' o el error de 'ERR: xxx'"""
    lines = text.split("\r\n")
    for line in lines:
        line = line.strip()
        if line.startswith("= "):
            return line[2:].strip(), "result"
        if line.startswith("ERR: "):
            return line[5:].strip(), "error"
    return None, None

def compare(actual, expected, tolerance):
    """Compara resultado con tolerancia"""
    if tolerance is None:
        return actual == expected
    try:
        a = float(actual)
        e = float(expected)
        return abs(a - e) <= tolerance
    except ValueError:
        return actual == expected

# ============================================================================
# MAIN
# ============================================================================
def main():
    print(f"=== Conectando a {HOST}:{PORT} ===")
    bridge = UartBridge(HOST, PORT)
    time.sleep(0.5)

    # Descartar datos previos
    bridge.read_available(0.3)
    print("Conectado OK")

    # =====================================================================
    # SINCRONIZACION: llegar al prompt del monitor.
    # El monitor v2.6.2 tiene 4 estados posibles:
    #   - Calculadora corriendo : 'quit' la termina ("Volviendo al monitor...")
    #   - Prompt del monitor    : 'quit' provoca RESET -> el monitor auto-
    #                             carga LAUNCH.BIN (APP LAUNCHER). 'Q' lo cierra
    #   - APP LAUNCHER          : pantalla estatica; 'Q' vuelve al monitor
    #   - Modo XMODEM atascado  : CAN CAN (0x18) lo aborta ("Error XMODEM")
    # =====================================================================

    # 0. CAN CAN: abortar sesion XMODEM atascada de un intento previo
    def at_prompt(data):
        tail = data.rstrip()
        return tail.endswith(b">") and (b"H=ayuda" in data or b"Retorno de" in data)

    # Sincronizacion con reintentos: el hardware puede quedar en un estado
    # raro de sesiones previas (XMODEM atascado, calculadora, launcher...).
    prompt_ok = False
    for attempt in range(3):
        bridge.send(b"\x18\x18")  # CAN CAN
        time.sleep(0.5)
        bridge.read_available(0.8)

        print("=== Enviando quit (por si la calculadora esta activa) ===")
        bridge.send_text("quit")
        time.sleep(1.2)
        resp = bridge.read_available(3.0)
        print(f"  Tras quit: {resp[:110]!r}")

        if b"LAUNCH" in resp or not resp:
            print("  APP LAUNCHER detectado; saliendo con Q...")
            bridge.send(b"Q")
            time.sleep(1.0)
            resp = bridge.read_available(3.0)
            print(f"  Tras Q: {resp[:110]!r}")

        end = time.time() + 6.0
        while time.time() < end:
            resp += bridge.read_available(0.4)
            if at_prompt(resp):
                break
        if at_prompt(resp):
            prompt_ok = True
            break
    if not prompt_ok:
        print("  ERROR: no se pudo llegar al prompt del monitor")
        bridge.close()
        sys.exit(1)
    bridge.read_available(0.5)  # limpiar resto

    # 5. Pedir modo XMODEM
    print("=== XMODEM: enviando XRECV 0800 ===")
    bridge.send_text("XRECV 0800")

    # Esperar confirmacion del modo XMODEM. El banner varia segun la version
    # del monitor: v2.2.0+ dice 'Listo para XMODEM', v2.6.2 usa 'CARGANDO Y
    # EJECUTANDO' + 'Inicie transferencia...' seguido de NAKs.
    def xmodem_ready(data):
        return (b"Listo para XMODEM" in data or
                b"Inicie transferencia" in data or
                b"XMODEM" in data or
                b"CARGANDO" in data or
                b"\x15" in data)

    print("  Esperando confirmacion XMODEM...")
    end = time.time() + 6.0
    resp = b""
    while time.time() < end:
        resp += bridge.read_available(0.3)
        if xmodem_ready(resp):
            break
    print(f"  Monitor: {resp[:100]!r}")
    if not xmodem_ready(resp):
        print("  ERROR: El monitor no entro en modo XMODEM")
        bridge.close()
        sys.exit(1)

    print("=== XMODEM: transfiriendo binario ===")
    if not xmodem_send(bridge, BIN_FILE):
        print("FALLO en XMODEM")
        bridge.close()
        sys.exit(1)

    time.sleep(0.5)
    resp = bridge.read_available(0.5)
    print(f"  Tras XMODEM: {resp[:80]!r}")

    # 2. Ejecutar
    print("=== Ejecutando R 0800 ===")
    bridge.send_text("R 0800")
    # Esperar el banner COMPLETO de la calculadora y su prompt '> '.
    # Si el primer test se envia antes, los caracteres se pierden durante
    # la impresion del banner (la linea llega vacia y da '= 0').
    banner = b""
    end = time.time() + 8.0
    while time.time() < end:
        chunk = bridge.read_available(0.3)
        banner += chunk
        if b"Calculadora" in banner and b"> " in banner:
            break
    print(f"  Banner (final): {banner[-90:]!r}")
    if b"Calculadora" not in banner:
        print("ADVERTENCIA: No se vio el banner de la calculadora")
    time.sleep(0.3)
    bridge.read_available(0.3)  # limpiar resto

    # Ping de sincronizacion: enviar una linea vacia y esperar que la
    # calculadora reimprima el prompt '> '. Confirma que esta lista en el
    # bucle antes del primer test (el primer envio tras el banner es fragil).
    bridge.send(b"\r")
    end = time.time() + 3.0
    while time.time() < end:
        chunk = bridge.read_available(0.3)
        if b"> " in chunk:
            break
    bridge.read_available(0.3)  # limpiar resto

    # 3. Ejecutar pruebas
    # Tests a ejecutar: si se pasan numeros como argumentos, solo esos
    # (1-based). Ej: python test_calc.py 1 5 20
    selected = [int(x) for x in sys.argv[1:]] if len(sys.argv) > 1 else None
    indices = selected if selected is not None else list(range(1, len(TESTS) + 1))

    print("=== Ejecutando pruebas ===")
    passed = 0
    failed = 0
    failures = []

    for idx in indices:
        expr, expected, tol = TESTS[idx - 1]
        bridge.send_text(expr)
        time.sleep(0.4)
        resp = bridge.read_available(0.4)
        result, kind = parse_output(resp.decode(errors="replace"))

        # Retry: el bridge puede perder el PRIMER envio (la linea llega vacia
        # y la calculadora responde '= 0' sin evaluar), o el CR final.
        # Se reintenta hasta 2 veces reenviando la expresion completa.
        def needs_retry():
            if result is None:
                return True
            # Caso 'ans' sin resultado previo: '0' significa linea vacia.
            if expected == "No previous result" and result == "0":
                return True
            return False

        attempts = 0
        while needs_retry() and attempts < 2:
            attempts += 1
            bridge.send(b"\r")           # descartar linea pendiente
            time.sleep(0.4)
            bridge.send_text(expr)        # reenviar expresion
            time.sleep(0.4)
            resp = bridge.read_available(0.4)
            result, kind = parse_output(resp.decode(errors="replace"))

        if result is None:
            print(f"  [NO RESP] {idx:>2}/{len(TESTS)} {expr!r} -> {resp[:50]!r}")
            failed += 1
            failures.append((idx, expr, expected, "NO RESPONSE"))
            continue

        ok = compare(result, expected, tol)
        status = "OK " if ok else "FAIL"
        print(f"  [{status}] {idx:>2}/{len(TESTS)} {expr!r} = {result!r} (esperado {expected!r})")
        if not ok:
            print(f"         raw: {resp[:200]!r}")
        if ok:
            passed += 1
        else:
            failed += 1
            failures.append((idx, expr, expected, result))

    # 4. Salir
    print("=== Enviando quit ===")
    bridge.send_text("quit")
    time.sleep(0.5)
    resp = bridge.read_available(0.5)
    print(f"  Respuesta: {resp[:60]!r}")

    bridge.close()

    # 5. Resumen
    print()
    print("=" * 50)
    print(f"RESULTADO: {passed} OK, {failed} FAIL de {len(indices)} pruebas")
    if failures:
        print()
        print("FALLOS:")
        for idx, expr, expected, actual in failures:
            print(f"  {idx}. {expr}: esperado={expected!r}, obtenido={actual!r}")
    print("=" * 50)
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
