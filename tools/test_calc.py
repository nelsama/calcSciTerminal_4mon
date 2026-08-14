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
    def __init__(self, host, port, timeout=0.5):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeout)
        self.sock.connect((host, port))
        self.buf = b""

    def send(self, data):
        self.sock.sendall(data)

    def send_text(self, text, char_delay=0.05):
        """Envia texto char por char esperando el eco del monitor 6502.
        Esto garantiza que cada caracter llego antes de enviar el siguiente."""
        for ch in text:
            self.sock.sendall(ch.encode())
            # Esperar el eco del caracter (el monitor/calculadora lo reenvia)
            end = time.time() + 2.0
            echoed = False
            while time.time() < end:
                try:
                    data = self.sock.recv(1024)
                    if data:
                        # Ver si el ultimo byte recibido es el caracter enviado
                        if ch.encode() in data or (data and data[-1:] == ch.encode()):
                            echoed = True
                            break
                except socket.timeout:
                    pass
            if not echoed:
                # El monitor no eco este caracter, enviar igual (quizas es funcion)
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

    # Esperar 'C' (CRC) o NAK (checksum) del receptor
    print("  Esperando 'C'/NAK del monitor...")
    response = bridge.read_until(b"C", timeout=5.0)
    use_crc = False
    if response and b"C" in response:
        use_crc = True
        print("  Modo CRC (el monitor envio 'C')")
    else:
        bridge.read_until(b"\x15", timeout=2.0)
        print("  Modo Checksum (el monitor envio NAK)")

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

    # 0. Si la calculadora esta corriendo, enviar 'quit' para volver al monitor
    print("=== Enviando quit (por si la calculadora esta activa) ===")
    bridge.send_text("quit")
    time.sleep(1.0)
    resp = bridge.read_available(1.0)
    print(f"  Respuesta: {resp[:80]!r}")
    if b"Volviendo" in resp:
        print("  Calculadora terminada, de vuelta al monitor")
        # Esperar el banner completo del monitor y su prompt '>'
        end = time.time() + 5.0
        banner = b""
        while time.time() < end:
            chunk = bridge.read_available(0.5)
            banner += chunk
            if b">" in banner:
                break
        print(f"  Banner monitor: {banner[:100]!r}")
        time.sleep(0.5)
        bridge.read_available(0.5)  # limpiar resto
    else:
        print("  (No habia calculadora corriendo o ya estaba en el monitor)")
        # Ya estamos en el monitor, limpiar buffer
        bridge.read_available(0.5)

    # 1. Cargar por XMODEM
    print("=== XMODEM: enviando XRECV 0800 ===")
    bridge.send_text("XRECV 0800")
    time.sleep(0.5)
    resp = bridge.read_available(0.5)
    print(f"  Monitor: {resp[:80]!r}")
    if b"Listo para XMODEM" not in resp:
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
    time.sleep(1.0)
    resp = bridge.read_available(1.0)
    print(f"  Banner: {resp[:120]!r}")
    if b"Calculadora" not in resp:
        print("ADVERTENCIA: No se vio el banner de la calculadora")

    # 3. Ejecutar pruebas
    print("=== Ejecutando pruebas ===")
    passed = 0
    failed = 0
    failures = []

    for expr, expected, tol in TESTS:
        bridge.send_text(expr)
        time.sleep(0.4)
        resp = bridge.read_available(0.4)

        result, kind = parse_output(resp.decode(errors="replace"))
        if result is None:
            print(f"  [NO RESP] {expr!r} -> {resp[:50]!r}")
            failed += 1
            failures.append((expr, expected, "NO RESPONSE"))
            continue

        ok = compare(result, expected, tol)
        status = "OK " if ok else "FAIL"
        print(f"  [{status}] {expr!r} = {result!r} (esperado {expected!r})")
        if ok:
            passed += 1
        else:
            failed += 1
            failures.append((expr, expected, result))

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
    print(f"RESULTADO: {passed} OK, {failed} FAIL de {len(TESTS)} pruebas")
    if failures:
        print()
        print("FALLOS:")
        for expr, expected, actual in failures:
            print(f"  {expr}: esperado={expected!r}, obtenido={actual!r}")
    print("=" * 50)
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
