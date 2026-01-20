# ============================================================================
# Makefile - Calculadora de Punto Flotante para Monitor 6502
# ============================================================================
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
CONFIG_DIR = config
BUILD_DIR = build
OUTPUT_DIR = output
LIB_DIR = ../../libs

# Configuración del linker
LD_CONFIG = $(CONFIG_DIR)\programa.cfg

# ============================================
# LIBRERÍAS (agregar más aquí)
# ============================================
TM1638_DIR = $(LIB_DIR)/tm1638-6502-cc65
MSBASIC_FLOAT_DIR = libs/msbasic-float
MSBASIC_DIR = $(MSBASIC_FLOAT_DIR)/msbasic

INCLUDES = -I$(TM1638_DIR) -I$(MSBASIC_FLOAT_DIR)/include

# Nombre del programa
PROGRAM_NAME = calc-float

# Archivos de salida
PROGRAM = $(OUTPUT_DIR)\$(PROGRAM_NAME).bin
MAP_FILE = $(OUTPUT_DIR)\$(PROGRAM_NAME).map

# Archivos fuente
C_SOURCES = $(SRC_DIR)\main.c $(MSBASIC_FLOAT_DIR)\src\float_convert.c
ASM_SOURCES = $(SRC_DIR)\startup.s $(MSBASIC_FLOAT_DIR)\src\msbasic_wrapper.s $(MSBASIC_FLOAT_DIR)\src\msbasic_float_only.s

# Archivos objeto
C_OBJECTS = $(BUILD_DIR)\main.o $(BUILD_DIR)\float_convert.o
ASM_OBJECTS = $(BUILD_DIR)\startup.o $(BUILD_DIR)\msbasic_wrapper.o $(BUILD_DIR)\msbasic_float_only.o
TM1638_OBJ = $(BUILD_DIR)\tm1638.o

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS) $(TM1638_OBJ)

# Flags del compilador C
CFLAGS = -t none $(INCLUDES) -O --cpu 6502 -I $(SRC_DIR) -I include

# Flags del ensamblador
ASFLAGS = -t none --cpu 6502

# Flags para MSBasic (requiere definir la configuración)
MSBASIC_ASFLAGS = --cpu 6502 \
                  --feature force_range \
                  -D CONFIG_2 \
                  -I$(MSBASIC_DIR)

# Flags del linker
LDFLAGS = -C $(LD_CONFIG) -m $(MAP_FILE)

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

$(BUILD_DIR)\float_convert.o: $(MSBASIC_FLOAT_DIR)\src\float_convert.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Ensamblar startup
$(BUILD_DIR)\startup.o: $(SRC_DIR)\startup.s
	$(CA65) $(ASFLAGS) -o $@ $<

# Ensamblar wrapper MSBasic
$(BUILD_DIR)\msbasic_wrapper.o: $(MSBASIC_FLOAT_DIR)\src\msbasic_wrapper.s
	$(CA65) $(ASFLAGS) -I$(MSBASIC_DIR) -o $@ $<

# Ensamblar módulo MSBasic completo
$(BUILD_DIR)\msbasic_float_only.o: $(MSBASIC_FLOAT_DIR)\src\msbasic_float_only.s
	$(CA65) $(MSBASIC_ASFLAGS) -o $@ $<

# Linkar
$(PROGRAM): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(CC65_HOME)\lib\none.lib

# Compilar TM1638
$(TM1638_OBJ): $(TM1638_DIR)/src/tm1638.c
	$(CC) -c $(CFLAGS) -o $@ $<

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
