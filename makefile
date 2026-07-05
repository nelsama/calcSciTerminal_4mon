# ============================================================================
# Makefile - Calculadora Científica (Terminal UART)
# ============================================================================
# Calculadora de punto flotante con funciones trigonométricas y parser de
# expresiones. Usa las rutinas de MSBasic para las operaciones matemáticas.
#
# Uso:
#   make        - Compilar el programa
#   make clean  - Limpiar archivos generados
#   make info   - Ver tamaño del binario
#   make map    - Ver mapa de memoria
# ============================================================================

# Configuración CC65 - Ajustar ruta si es necesario
CC65_HOME = D:\cc65

# Herramientas
CC = cl65
CA65 = $(CC65_HOME)\bin\ca65.exe
LD = $(CC65_HOME)\bin\ld65.exe

# Directorios
SRC_DIR = src
INCLUDE_DIR = include
CONFIG_DIR = config
BUILD_DIR = build
OUTPUT_DIR = output
LIB_DIR = ../../libs

# Configuración del linker
LD_CONFIG = $(CONFIG_DIR)\programa.cfg

# ============================================
# LIBRERÍAS
# ============================================
MSBASIC_FLOAT_DIR = libs\msbasic-float
MSBASIC_DIR = $(MSBASIC_FLOAT_DIR)\msbasic

INCLUDES = -I$(INCLUDE_DIR) -I$(MSBASIC_FLOAT_DIR)\include

# Nombre del programa
PROGRAM_NAME = calc-sci

# Versión (se pasa al código C)
VERSION_MAJOR = 1
VERSION_MINOR = 0
VERSION_PATCH = 0

# Archivos de salida
PROGRAM = $(OUTPUT_DIR)\$(PROGRAM_NAME).bin
MAP_FILE = $(OUTPUT_DIR)\$(PROGRAM_NAME).map

# Archivos fuente
C_SOURCES = $(SRC_DIR)\main.c $(SRC_DIR)\parser.c $(MSBASIC_FLOAT_DIR)\src\float_convert.c
ASM_SOURCES = $(SRC_DIR)\startup.s $(MSBASIC_FLOAT_DIR)\src\msbasic_wrapper.s \
              $(MSBASIC_FLOAT_DIR)\src\msbasic_float_only.s \
              $(MSBASIC_FLOAT_DIR)\src\msbasic_trig.s

# Archivos objeto
C_OBJECTS = $(BUILD_DIR)\main.o $(BUILD_DIR)\parser.o $(BUILD_DIR)\float_convert.o
ASM_OBJECTS = $(BUILD_DIR)\startup.o $(BUILD_DIR)\msbasic_wrapper.o \
              $(BUILD_DIR)\msbasic_float_only.o $(BUILD_DIR)\msbasic_trig.o

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Flags del compilador C
CFLAGS = -t none $(INCLUDES) -O --cpu 6502 -I $(SRC_DIR) \
         -DVERSION_MAJOR=$(VERSION_MAJOR) \
         -DVERSION_MINOR=$(VERSION_MINOR) \
         -DVERSION_PATCH=$(VERSION_PATCH)

# Flags del ensamblador
ASFLAGS = -t none --cpu 6502

# Flags para MSBasic (requiere definir la configuración)
MSBASIC_ASFLAGS = --cpu 6502 \
                  --feature force_range \
                  -D CONFIG_2 \
                  -I$(MSBASIC_DIR)

# Flags del linker
LDFLAGS = -C $(LD_CONFIG) -m $(MAP_FILE)

SHELL = cmd.exe

# ============================================================================
# REGLAS PRINCIPALES
# ============================================================================

all: dirs $(PROGRAM)
	@echo.
	@echo ========================================
	@echo Programa generado: $(PROGRAM)
	@for %%I in ($(PROGRAM)) do @echo Tamano: %%~zI bytes
	@echo ========================================
	@echo Para usar:
	@echo   1. Copiar a SD como CALC
	@echo   2. En el monitor:
	@echo      LOAD CALC 0800
	@echo      R 0800
	@echo ========================================

dirs:
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
	@if not exist "$(OUTPUT_DIR)" mkdir "$(OUTPUT_DIR)"

# Compilar C
$(BUILD_DIR)\main.o: $(SRC_DIR)\main.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)\parser.o: $(SRC_DIR)\parser.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)\float_convert.o: $(MSBASIC_FLOAT_DIR)\src\float_convert.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Ensamblar startup
$(BUILD_DIR)\startup.o: $(SRC_DIR)\startup.s
	$(CA65) $(ASFLAGS) -o $@ $<

# Ensamblar wrapper MSBasic
$(BUILD_DIR)\msbasic_wrapper.o: $(MSBASIC_FLOAT_DIR)\src\msbasic_wrapper.s
	$(CA65) $(ASFLAGS) -I$(MSBASIC_DIR) -o $@ $<

# Ensamblar módulo float MSBasic completo (con rutinas aritméticas)
$(BUILD_DIR)\msbasic_float_only.o: $(MSBASIC_FLOAT_DIR)\src\msbasic_float_only.s
	$(CA65) $(MSBASIC_ASFLAGS) -o $@ $<

# Ensamblar módulo trigonométrico MSBasic (SIN, COS, TAN, ATN)
$(BUILD_DIR)\msbasic_trig.o: $(MSBASIC_FLOAT_DIR)\src\msbasic_trig.s
	$(CA65) $(MSBASIC_ASFLAGS) -o $@ $<

# Linkar
$(PROGRAM): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(CC65_HOME)\lib\none.lib

# ============================================================================
# UTILIDADES
# ============================================================================

info:
	@echo ========================================
	@echo Informacion del programa
	@echo ========================================
	@if exist $(PROGRAM) (for %%I in ($(PROGRAM)) do @echo Tamano: %%~zI bytes) else @echo Error: Programa no compilado

map:
	@if exist $(MAP_FILE) (type $(MAP_FILE)) else @echo Error: Archivo de mapa no encontrado. Compilar primero.

clean:
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	@if exist $(OUTPUT_DIR) rmdir /s /q $(OUTPUT_DIR)
	@echo Limpieza completa

# ============================================================================
# AYUDA
# ============================================================================

help:
	@echo Uso del makefile:
	@echo   make        - Compilar el programa
	@echo   make clean  - Limpiar archivos generados
	@echo   make info   - Ver informacion del binario
	@echo   make map    - Ver mapa de memoria

.PHONY: all dirs clean info map help
