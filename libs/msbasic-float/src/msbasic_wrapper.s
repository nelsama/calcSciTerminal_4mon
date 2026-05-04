; ==============================================================================
; msbasic_wrapper.s - Wrapper para usar rutinas de punto flotante de MSBasic
; ==============================================================================
; Proporciona una interfaz simple entre C y las rutinas de MSBasic.
;
; MSBasic usa dos registros principales:
;   - FAC (Floating Point Accumulator): 5 bytes en Zero Page
;   - ARG (Argument Register): 5 bytes en Zero Page
;
; Formato de 5 bytes: [exp][mantissa 0-3][sign]
; ==============================================================================

; Importar funciones de CC65 PRIMERO
.import popax

; Usar direcciones Zero Page FIJAS que NO use la ROM
; La ROM usa $F0-$FF, así que usamos $02-$09 que están libres
tmpptr          = $02    ; $02-$03
a_ptr_zp        = $04    ; $04-$05
b_ptr_zp        = $06    ; $06-$07
result_ptr_zp   = $08    ; $08-$09

; Definir áreas temporales en BSS
.segment "BSS"
temp_operand:   .res 5

; Exportar funciones a C
.export _fp_add, _fp_sub, _fp_mul, _fp_div
.export _fp_int_to_fac, _fp_fac_to_int
.export _fp_load_fac, _fp_save_fac
.export _fp_clear_fac

; Importar de MSBasic
.import FADD, FSUB, FMULT, FDIV
.import QINT
.import STORE_FAC_AT_YX_ROUNDED
.importzp FAC, ARG, FACSIGN, ARGSIGN
.importzp FACEXTENSION, ARGEXTENSION

; ==============================================================================
; MACRO: copy_mem_to_fac - Copia 5 bytes de memoria a FAC (con bit implícito)
; Parámetro: ptr (dirección zero page del puntero)
; ==============================================================================
.macro copy_mem_to_fac ptr
        ldy     #0
        lda     (ptr),y         ; Exponente
        sta     FAC
        iny
        lda     (ptr),y         ; Mantisa alta con signo en bit 7
        tax
        and     #$80            ; Extraer bit de signo
        beq     :+
        lda     #$FF
        bne     :++
:       lda     #$00
:       sta     FACSIGN
        txa
        ora     #$80            ; Poner bit implícito
        sta     FAC+1
        iny
        lda     (ptr),y
        sta     FAC+2
        iny
        lda     (ptr),y
        sta     FAC+3
        iny
        lda     (ptr),y
        sta     FAC+4
.endmacro

; ==============================================================================
; MACRO: copy_mem_to_temp - Copia 5 bytes de memoria a temp_operand
; Parámetro: ptr (dirección zero page del puntero)
; ==============================================================================
.macro copy_mem_to_temp ptr
        ldy     #0
        lda     (ptr),y
        sta     temp_operand
        iny
        lda     (ptr),y
        sta     temp_operand+1
        iny
        lda     (ptr),y
        sta     temp_operand+2
        iny
        lda     (ptr),y
        sta     temp_operand+3
        iny
        lda     (ptr),y
        sta     temp_operand+4
.endmacro

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

        ; Copiar 'a' a FAC
        copy_mem_to_fac a_ptr_zp

        ; Copiar 'b' a temp_operand (formato almacenamiento, sin modificar)
        copy_mem_to_temp b_ptr_zp

        ; Llamar FADD con dirección de temp_operand
        lda     #<temp_operand
        ldy     #>temp_operand
        jsr     FADD

        ; Usar STORE_FAC_AT_YX_ROUNDED para guardar correctamente
        ldx     result_ptr_zp
        ldy     result_ptr_zp+1
        jsr     STORE_FAC_AT_YX_ROUNDED

        rts

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

        ; Copiar 'b' a FAC
        copy_mem_to_fac b_ptr_zp

        ; Copiar 'a' a temp_operand (el minuendo)
        copy_mem_to_temp a_ptr_zp

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

        ; Copiar 'a' a FAC
        copy_mem_to_fac a_ptr_zp

        ; Copiar 'b' a temp_operand
        copy_mem_to_temp b_ptr_zp

        ; Llamar FMULT
        lda     #<temp_operand
        ldy     #>temp_operand
        jsr     FMULT

        ; Usar STORE_FAC_AT_YX_ROUNDED para guardar correctamente
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

        ; Copiar 'b' a FAC
        copy_mem_to_fac b_ptr_zp

        ; Copiar 'a' a temp_operand (el dividendo)
        copy_mem_to_temp a_ptr_zp

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
