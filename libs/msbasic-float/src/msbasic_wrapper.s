; ==============================================================================
; msbasic_wrapper.s - Wrapper para usar rutinas de punto flotante de MSBasic
; ==============================================================================
; Proporciona una interfaz simple entre C y las rutinas de MSBasic.
;
; MSBasic usa dos registros principales:
;   - FAC (Floating Point Accumulator): 5 bytes en Zero Page ($CB-$CF)
;   - ARG (Argument Register): 5 bytes en Zero Page ($D1-$D5)
;
; Formato de 5 bytes: [exp][mantissa 0-3][sign]
; ==============================================================================

; Importar funciones de CC65 PRIMERO
.import popax
.import _debug_print_fac  ; Para debug

; Usar direcciones Zero Page FIJAS que NO use la ROM
; La ROM usa $F0-$FF, así que usamos $02-$09 que están libres
tmpptr          = $02    ; $02-$03
a_ptr_zp        = $04    ; $04-$05
b_ptr_zp        = $06    ; $06-$07
result_ptr_zp   = $08    ; $08-$09

; Definir áreas temporales en BSS
.segment "BSS"
temp_operand: .res 5    ; Almacenamiento temporal para operando

; Exportar funciones a C
.export _fp_add, _fp_sub, _fp_mul, _fp_div
.export _fp_int_to_fac, _fp_fac_to_int
.export _fp_load_fac, _fp_save_fac
.export _fp_clear_fac
.export _FAC, _ARG     ; Exportar FAC y ARG para debug

; Importar de MSBasic (solo operaciones matemáticas)
.import FADD, FSUB, FMULT, FDIV
.import GIVAYF, QINT
.import STORE_FAC_AT_YX_ROUNDED  ; Para guardar FAC correctamente
.importzp FAC, ARG, FACSIGN, ARGSIGN
.importzp FACEXTENSION, ARGEXTENSION

; Crear alias para exportar FAC y ARG
_FAC = FAC
_ARG = ARG

.segment "CODE"

; Debug: imprimir un byte hex via UART ROM
; Entrada: A = byte a imprimir
UART_PUTC = $BF18

debug_hex:
        pha
        lsr
        lsr
        lsr
        lsr
        jsr     print_nibble
        pla
        and     #$0F
        jsr     print_nibble
        lda     #' '
        jsr     UART_PUTC
        rts

print_nibble:
        cmp     #10
        bcc     @digit
        adc     #6              ; A-F
@digit:
        adc     #'0'
        jmp     UART_PUTC

debug_newline:
        lda     #13
        jsr     UART_PUTC
        lda     #10
        jmp     UART_PUTC

; ==============================================================================
; TEST DIRECTO - Sin punteros, valores hardcodeados
; void fp_test_direct(void) - Prueba 2+2=4 directamente
; ==============================================================================
.export _fp_test_direct
_fp_test_direct:
        ; Limpiar FAC completamente
        lda     #0
        sta     FAC
        sta     FAC+1
        sta     FAC+2
        sta     FAC+3
        sta     FAC+4
        sta     FACSIGN
        sta     FACEXTENSION
        sta     ARGSIGN
        sta     ARGEXTENSION

        ; Cargar valor 2 en FAC con formato CORRECTO (bit 7 explícito)
        ; Valor 2 = exp 0x82, mantisa 0x80 (bit implícito ahora explícito)
        lda     #$82
        sta     FAC
        lda     #$80            ; ¡BIT 7 ACTIVO!
        sta     FAC+1
        lda     #$00
        sta     FAC+2
        sta     FAC+3
        sta     FAC+4

        ; Cargar valor 2 en test_val2 (formato almacenamiento SIN bit explícito)
        ; FADD espera el segundo operando en formato almacenamiento
        lda     #$82
        sta     test_val2
        lda     #$00            ; Sin bit explícito para memoria
        sta     test_val2+1
        sta     test_val2+2
        sta     test_val2+3
        sta     test_val2+4

        ; Debug: Mostrar FAC antes de FADD
        lda     #'A'
        jsr     UART_PUTC
        lda     FAC
        jsr     debug_hex
        lda     FAC+1
        jsr     debug_hex
        jsr     debug_newline

        ; Llamar FADD: FAC = FAC + test_val2 (2+2=4)
        lda     #<test_val2
        ldy     #>test_val2
        jsr     FADD

        ; Debug: Mostrar FAC después de FADD
        lda     #'R'
        jsr     UART_PUTC
        lda     FAC
        jsr     debug_hex
        lda     FAC+1
        jsr     debug_hex
        lda     FAC+2
        jsr     debug_hex
        jsr     debug_newline

        rts

; Valor temporal para test
.segment "BSS"
test_val2: .res 5

.segment "CODE"

; ==============================================================================
; void fp_add(float *a, float *b, float *result)
; Suma: result = a + b
; FORMATO DE ALMACENAMIENTO: [EXP][M1+SIGN][M2][M3][M4]
;   - bit 7 de byte[1] = signo (1=negativo, 0=positivo)
;   - bytes 1-4 son mantisa sin bit implícito
; FORMATO FAC INTERNO: FACSIGN separado, FAC+1 siempre tiene bit 7 = 1
; ==============================================================================
_fp_add:
        ; Guardar parámetros: fp_add(a, b, result)
        ; CC65 pasa: A/X = result (último), pop = b, pop = a
        sta     result_ptr_zp
        stx     result_ptr_zp+1

        jsr     popax
        sta     b_ptr_zp
        stx     b_ptr_zp+1

        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1

        ; Limpiar extensiones
        lda     #0
        sta     FACEXTENSION
        sta     ARGEXTENSION

        ; Copiar 'a' a FAC extrayendo signo de bit 7 de byte[1]
        ldy     #0
        lda     (a_ptr_zp),y    ; Exponente
        sta     FAC
        iny
        lda     (a_ptr_zp),y    ; Mantisa alta con signo en bit 7
        tax                     ; Guardar en X
        and     #$80            ; Extraer bit de signo
        beq     @a_positive
        lda     #$FF            ; Negativo
        bne     @a_set_sign
@a_positive:
        lda     #$00            ; Positivo
@a_set_sign:
        sta     FACSIGN
        txa                     ; Recuperar byte[1]
        ora     #$80            ; Poner bit implícito
        sta     FAC+1
        iny
        lda     (a_ptr_zp),y
        sta     FAC+2
        iny
        lda     (a_ptr_zp),y
        sta     FAC+3
        iny
        lda     (a_ptr_zp),y
        sta     FAC+4

        ; Copiar 'b' a temp_operand (formato almacenamiento, sin modificar)
        ; FADD llamará a LOAD_ARG_FROM_YA que extrae el signo correctamente
        ldy     #0
        lda     (b_ptr_zp),y
        sta     temp_operand
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+1
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+2
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+3
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+4

        ; Llamar FADD con dirección de temp_operand
        lda     #<temp_operand
        ldy     #>temp_operand
        jsr     FADD

        ; Usar STORE_FAC_AT_YX_ROUNDED para guardar correctamente
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jsr     STORE_FAC_AT_YX_ROUNDED

        rts

.segment "BSS"
temp_sign: .res 1

.segment "CODE"

; ==============================================================================
; void fp_sub(float *a, float *b, float *result)
; Resta: result = a - b
; NOTA: FSUB hace FAC = (memoria) - FAC, así que cargamos b en FAC
;       y pasamos dirección de a para obtener a-b
; ==============================================================================
_fp_sub:
        ; Guardar parámetros: fp_sub(a, b, result)
        sta     result_ptr_zp
        stx     result_ptr_zp+1

        jsr     popax
        sta     b_ptr_zp
        stx     b_ptr_zp+1

        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1

        ; Limpiar extensiones
        lda     #0
        sta     FACEXTENSION
        sta     ARGEXTENSION

        ; FSUB hace: FAC = (memoria) - FAC
        ; Para a-b, cargar 'b' en FAC y 'a' en memoria

        ; Copiar 'b' a FAC extrayendo signo de bit 7 de byte[1]
        ldy     #0
        lda     (b_ptr_zp),y
        sta     FAC
        iny
        lda     (b_ptr_zp),y
        tax
        and     #$80
        beq     @sub_b_pos
        lda     #$FF
        bne     @sub_b_set
@sub_b_pos:
        lda     #$00
@sub_b_set:
        sta     FACSIGN
        txa
        ora     #$80
        sta     FAC+1
        iny
        lda     (b_ptr_zp),y
        sta     FAC+2
        iny
        lda     (b_ptr_zp),y
        sta     FAC+3
        iny
        lda     (b_ptr_zp),y
        sta     FAC+4

        ; Copiar 'a' a temp_operand (el minuendo)
        ldy     #0
        lda     (a_ptr_zp),y
        sta     temp_operand
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+1
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+2
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+3
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+4

        ; Llamar FSUB: FAC = temp_operand(a) - FAC(b) = a-b
        lda     #<temp_operand
        ldy     #>temp_operand
        jsr     FSUB

        ; Usar STORE_FAC_AT_YX_ROUNDED para guardar correctamente
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jsr     STORE_FAC_AT_YX_ROUNDED

        rts

; ==============================================================================
; void fp_mul(float *a, float *b, float *result)
; Multiplicación: result = a * b
; ==============================================================================
_fp_mul:
        ; Guardar parámetros
        sta     result_ptr_zp
        stx     result_ptr_zp+1

        jsr     popax
        sta     b_ptr_zp
        stx     b_ptr_zp+1

        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1

        ; Limpiar extensiones
        lda     #0
        sta     FACEXTENSION
        sta     ARGEXTENSION

        ; Copiar 'a' a FAC extrayendo signo de bit 7 de byte[1]
        ldy     #0
        lda     (a_ptr_zp),y
        sta     FAC
        iny
        lda     (a_ptr_zp),y
        tax
        and     #$80
        beq     @mul_a_pos
        lda     #$FF
        bne     @mul_a_set
@mul_a_pos:
        lda     #$00
@mul_a_set:
        sta     FACSIGN
        txa
        ora     #$80
        sta     FAC+1
        iny
        lda     (a_ptr_zp),y
        sta     FAC+2
        iny
        lda     (a_ptr_zp),y
        sta     FAC+3
        iny
        lda     (a_ptr_zp),y
        sta     FAC+4

        ; Copiar 'b' a temp_operand
        ldy     #0
        lda     (b_ptr_zp),y
        sta     temp_operand
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+1
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+2
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+3
        iny
        lda     (b_ptr_zp),y
        sta     temp_operand+4

        ; Llamar FMULT
        lda     #<temp_operand
        ldy     #>temp_operand
        jsr     FMULT

        ; Usar STORE_FAC_AT_YX_ROUNDED para guardar correctamente
        ; Esta función maneja el formato de signo correctamente
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jsr     STORE_FAC_AT_YX_ROUNDED

        rts

; ==============================================================================
; void fp_div(float *a, float *b, float *result)
; División: result = a / b
; NOTA: FDIV hace FAC = (memoria) / FAC, así que cargamos b en FAC
;       y pasamos dirección de a para obtener a/b
; ==============================================================================
_fp_div:
        ; Guardar parámetros
        sta     result_ptr_zp
        stx     result_ptr_zp+1

        jsr     popax
        sta     b_ptr_zp
        stx     b_ptr_zp+1

        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1

        ; Limpiar extensiones
        lda     #0
        sta     FACEXTENSION
        sta     ARGEXTENSION

        ; FDIV hace: FAC = (memoria) / FAC
        ; Para a/b, cargar 'b' en FAC y 'a' en memoria

        ; Copiar 'b' a FAC extrayendo signo de bit 7 de byte[1]
        ldy     #0
        lda     (b_ptr_zp),y
        sta     FAC
        iny
        lda     (b_ptr_zp),y
        tax
        and     #$80
        beq     @div_b_pos
        lda     #$FF
        bne     @div_b_set
@div_b_pos:
        lda     #$00
@div_b_set:
        sta     FACSIGN
        txa
        ora     #$80
        sta     FAC+1
        iny
        lda     (b_ptr_zp),y
        sta     FAC+2
        iny
        lda     (b_ptr_zp),y
        sta     FAC+3
        iny
        lda     (b_ptr_zp),y
        sta     FAC+4

        ; Copiar 'a' a temp_operand (el dividendo)
        ldy     #0
        lda     (a_ptr_zp),y
        sta     temp_operand
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+1
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+2
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+3
        iny
        lda     (a_ptr_zp),y
        sta     temp_operand+4

        ; Llamar FDIV: FAC = temp_operand(a) / FAC(b) = a/b
        lda     #<temp_operand
        ldy     #>temp_operand
        jsr     FDIV

        ; Usar STORE_FAC_AT_YX_ROUNDED para guardar correctamente
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jsr     STORE_FAC_AT_YX_ROUNDED

        rts

; ==============================================================================
; void fp_decimal_to_fac(int32_t mantissa, int8_t exponent)
; Convierte número decimal representado como mantissa * 10^exponent
; Parámetros: mantissa (4 bytes en stack), exponent (1 byte)
; Ejemplo: 123.45 = mantissa=12345, exponent=-2
; ==============================================================================
_fp_decimal_to_fac:
        ; Por ahora, implementación simplificada
        ; El código C hará la conversión usando operaciones básicas
        rts

; ==============================================================================
; void fp_fac_to_decimal(int32_t *mantissa, int8_t *exponent)
; Convierte FAC a representación decimal
; ==============================================================================
_fp_fac_to_decimal:
        ; Por ahora, implementación simplificada
        ; El código C hará la extracción usando divisiones
        rts

; ==============================================================================
; void fp_int_to_fac(int16_t value)
; Convierte entero de 16 bits (con signo) a FAC
; IMPLEMENTACIÓN PROPIA - No usa GIVAYF
;
; Formato MSBasic: [exp][m1][m2][m3][sign]
; - exp = 128 + posición del bit más alto + 1
; - mantisa normalizada: bit implícito = 0.5 (posición 23)
; ==============================================================================
_fp_int_to_fac:
        ; CC65 pasa: A=low, X=high
        sta     int_low
        stx     int_high

        ; Limpiar FAC
        lda     #0
        sta     FAC
        sta     FAC+1
        sta     FAC+2
        sta     FAC+3
        sta     FAC+4
        sta     FACSIGN
        sta     FACEXTENSION

        ; Verificar si es cero
        lda     int_low
        ora     int_high
        beq     @done           ; Si es 0, ya está listo

        ; Verificar signo
        lda     int_high
        bpl     @positive

        ; Número negativo: hacer complemento a 2
        lda     #$FF
        sta     FACSIGN         ; Marcar como negativo

        ; Negar el número: ~value + 1
        lda     int_low
        eor     #$FF
        clc
        adc     #1
        sta     int_low
        lda     int_high
        eor     #$FF
        adc     #0
        sta     int_high

@positive:
        ; Ahora convertir el valor absoluto
        ; Poner valor en FAC+1/FAC+2 (16 bits en posición alta)
        lda     int_high
        sta     FAC+1
        lda     int_low
        sta     FAC+2
        lda     #0
        sta     FAC+3
        sta     FAC+4

        ; Exponente inicial: 0x90 (144) = 128 + 16
        ; Porque el valor está en los bits 8-23 de la mantisa de 24 bits
        lda     #$90
        sta     FAC

        ; Normalizar: rotar izquierda hasta que bit 7 de FAC+1 esté set
@normalize:
        lda     FAC+1
        bmi     @done           ; Bit 7 set = normalizado

        ; Si FAC+1 es 0, mover bytes y ajustar exponente
        bne     @shift_one

        ; FAC+1 = 0, mover todo un byte
        lda     FAC+2
        sta     FAC+1
        lda     FAC+3
        sta     FAC+2
        lda     FAC+4
        sta     FAC+3
        lda     #0
        sta     FAC+4

        ; Restar 8 del exponente
        lda     FAC
        sec
        sbc     #8
        sta     FAC
        cmp     #$80            ; Si exponente < 0x80, underflow
        bcc     @zero_result
        jmp     @normalize

@zero_result:
        lda     #0
        sta     FAC
        sta     FAC+1
        rts

@shift_one:
        ; Rotar toda la mantisa un bit a la izquierda
        asl     FAC+4
        rol     FAC+3
        rol     FAC+2
        rol     FAC+1

        ; Decrementar exponente
        dec     FAC
        lda     FAC
        cmp     #$80            ; Si exponente < 0x80, underflow
        bcc     @zero_result
        jmp     @normalize

@done:
        rts

; Variables temporales para conversión
int_low:    .byte 0
int_high:   .byte 0

; ==============================================================================
; int16_t fp_fac_to_int(void)
; Convierte FAC a entero de 16 bits
; ==============================================================================
_fp_fac_to_int:
        jsr     QINT
        ; QINT deja resultado en FAC+3 (low) y FAC+4 (high)
        ; CC65 espera A=low, X=high
        lda     FAC+3
        ldx     FAC+4
        rts

; ==============================================================================
; void fp_load_fac(const float *src)
; Carga 5 bytes desde memoria a FAC
; NOTA: Agrega bit 7 a la mantisa (formato FAC vs formato almacenamiento)
; ==============================================================================
_fp_load_fac:
        sta     tmpptr
        stx     tmpptr+1

        ; Limpiar FACSIGN y FACEXTENSION antes de cargar
        lda     #0
        sta     FACSIGN
        sta     FACEXTENSION

        ; Cargar exponente
        ldy     #0
        lda     (tmpptr),y
        sta     FAC

        ; Cargar mantisa byte 1 CON bit 7
        iny
        lda     (tmpptr),y
        ora     #$80            ; Agregar bit 7 implícito
        sta     FAC+1

        ; Cargar resto de mantisa
        iny
        lda     (tmpptr),y
        sta     FAC+2
        iny
        lda     (tmpptr),y
        sta     FAC+3
        iny
        lda     (tmpptr),y
        sta     FAC+4
        rts

; ==============================================================================
; void fp_save_fac(float *dest)
; Guarda FAC a memoria
; NOTA: Quita bit 7 de la mantisa (formato almacenamiento vs formato FAC)
; ==============================================================================
_fp_save_fac:
        sta     tmpptr
        stx     tmpptr+1

        ; Guardar exponente
        ldy     #0
        lda     FAC
        sta     (tmpptr),y

        ; Guardar mantisa byte 1 SIN bit 7
        iny
        lda     FAC+1
        and     #$7F            ; Quitar bit 7 implícito
        sta     (tmpptr),y

        ; Guardar resto de mantisa
        iny
        lda     FAC+2
        sta     (tmpptr),y
        iny
        lda     FAC+3
        sta     (tmpptr),y
        iny
        lda     FAC+4
        sta     (tmpptr),y
        rts

; ==============================================================================
; void fp_clear_fac(void)
; Pone FAC = 0
; ==============================================================================
_fp_clear_fac:
        lda     #0
        sta     FAC
        sta     FAC+1
        sta     FAC+2
        sta     FAC+3
        sta     FAC+4
        rts

; ==============================================================================
; void fp_copy_fac_to_arg(void)
; Copia FAC a ARG
; ==============================================================================
_fp_copy_fac_to_arg:
        ldy     #4
@loop:
        lda     FAC,y
        sta     ARG,y
        dey
        bpl     @loop
        rts

; ==============================================================================
; WRAPPERS PARA FUNCIONES MATEMÁTICAS AVANZADAS
; ==============================================================================
; Estas funciones llaman a las rutinas de MS Basic desde C.
;
; Convención CC65 para fp_func(x, result):
;   - A/X = último parámetro (result)
;   - popax = penúltimo parámetro (x)
;
; La función MS Basic opera sobre FAC y deja resultado en FAC.
; Luego guardamos FAC en result.
; ==============================================================================

; Importar funciones de MS Basic
.import SIN, COS, TAN, ATN
.import LOG, EXP, SQR, ABS
.import INT, FPWRT

; ==============================================================================
; load_fac_from_ptr - Carga float desde puntero en a_ptr_zp a FAC
; ==============================================================================
load_fac_from_ptr:
        ldy     #0
        lda     (a_ptr_zp),y
        sta     FAC
        iny
        lda     (a_ptr_zp),y
        pha
        and     #$80
        beq     @pos
        lda     #$FF
        bne     @set_sign
@pos:
        lda     #$00
@set_sign:
        sta     FACSIGN
        pla
        ora     #$80
        sta     FAC+1
        iny
        lda     (a_ptr_zp),y
        sta     FAC+2
        iny
        lda     (a_ptr_zp),y
        sta     FAC+3
        iny
        lda     (a_ptr_zp),y
        sta     FAC+4
        rts

; ==============================================================================
; void fp_sin(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_sin
_fp_sin:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     SIN
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_cos(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_cos
_fp_cos:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     COS
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_tan(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_tan
_fp_tan:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     TAN
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_atn(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_atn
_fp_atn:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     ATN
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_log(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_log
_fp_log:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     LOG
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_exp(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_exp
_fp_exp:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     EXP
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_sqr(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_sqr
_fp_sqr:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     SQR
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_abs(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_abs
_fp_abs:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     ABS
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_int(const msbasic_float_t *x, msbasic_float_t *result)
; ==============================================================================
.export _fp_int
_fp_int:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1
        jsr     load_fac_from_ptr
        jsr     INT
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; void fp_pow(const msbasic_float_t *a, const msbasic_float_t *b,
;              msbasic_float_t *result)
; result = a ^ b (potencia)
; Usa FPWRT: ARG ^ FAC = EXP(LOG(ARG) * FAC)
; ==============================================================================
.export _fp_pow
_fp_pow:
        sta     result_ptr_zp
        stx     result_ptr_zp+1
        jsr     popax
        sta     b_ptr_zp
        stx     b_ptr_zp+1
        jsr     popax
        sta     a_ptr_zp
        stx     a_ptr_zp+1

        ; Limpiar extensiones
        lda     #0
        sta     FACEXTENSION
        sta     ARGEXTENSION

        ; Cargar 'a' en ARG extrayendo signo (mismo formato interno que FAC)
        ldy     #0
        lda     (a_ptr_zp),y    ; Exponente
        sta     ARG
        iny
        lda     (a_ptr_zp),y    ; Mantisa alta + signo en bit 7
        pha
        and     #$80            ; Extraer bit de signo
        beq     @a_pos
        lda     #$FF            ; Negativo
        bne     @a_set_sign
@a_pos:
        lda     #$00            ; Positivo
@a_set_sign:
        sta     ARGSIGN
        pla
        ora     #$80            ; Poner bit implícito
        sta     ARG+1
        iny
        lda     (a_ptr_zp),y
        sta     ARG+2
        iny
        lda     (a_ptr_zp),y
        sta     ARG+3
        iny
        lda     (a_ptr_zp),y
        sta     ARG+4

        ; Cargar 'b' en FAC
        lda     b_ptr_zp
        sta     a_ptr_zp
        lda     b_ptr_zp+1
        sta     a_ptr_zp+1
        jsr     load_fac_from_ptr

        ; Asegurar que Z flag refleje si FAC=0 antes de llamar FPWRT
        ; (FPWRT empieza con beq EXP para detectar FAC=0)
        lda     FAC

        ; Llamar FPWRT: ARG ^ FAC
        jsr     FPWRT

        ; Guardar resultado
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jmp     STORE_FAC_AT_YX_ROUNDED
