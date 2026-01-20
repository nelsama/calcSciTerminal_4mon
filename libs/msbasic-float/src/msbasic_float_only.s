;===============================================================================
; msbasic_float_only.s - Solo operaciones de punto flotante de MSBasic
;===============================================================================
; Incluye float.s con los stubs mínimos necesarios.
;===============================================================================

.feature force_range
.setcpu "6502"
.macpack longbranch

; Configurar Zero Page para el Monitor 6502
ZP_START1 = $20
ZP_START2 = $2A
ZP_START3 = $35
ZP_START4 = $80

; Incluir definiciones y macros
.include "defines.s"
.include "macros.s"

; Incluir Zero Page (variables compartidas)
.include "zeropage.s"

; ===============================================================================
; STUBS para símbolos que float.s necesita pero no usaremos
; ===============================================================================

; Constantes de tokens (no las usaremos pero deben estar definidas)
TOKEN_PLUS = $AA
TOKEN_MINUS = $AB
TOKEN_EQUAL = $CD

; Códigos de error (no los usaremos)
ERR_OVERFLOW = $0F
ERR_ZERODIV = $14

; Funciones stub que nunca se llamarán en nuestro uso
; Algunas se usan con branches cortos, así que van antes de float.s
.segment "CODE"

ERROR:
STROUT:
QT_IN:
IQERR:
RTS3:
RTS4:
        rts

.export ERROR, STROUT, QT_IN, IQERR, RTS3, RTS4

; Símbolos genéricos de CHRGET (rutina de parseo - no la usaremos)
GENERIC_TXTPTR = $7A
GENERIC_CHRGET = $7A
GENERIC_CHRGOT = $7B
GENERIC_CHRGOT2 = $7C
GENERIC_RNDSEED = $7D

; ===============================================================================
; GIVAYF - Convierte entero de 16 bits (Y,A) a float en FAC  
; ===============================================================================
.export GIVAYF
GIVAYF:
        ldx     #$00
        stx     FACSIGN         ; Signo positivo
        sta     FAC+1           ; Byte bajo del entero
        sty     FAC+2           ; Byte alto del entero
        lda     #$00
        sta     FAC+3
        sta     FAC+4
        ldx     #$90            ; Exponente para 16 bits
        stx     FAC
        ; Normalizar
@norm:
        lda     FAC+1
        bne     @done
        lda     FAC+2
        sta     FAC+1
        lda     #$00
        sta     FAC+2
        dec     FAC
        bne     @norm
@done:
        rts

; Incluir float.s (operaciones principales)
.include "float.s"

; ===============================================================================
; Exportar símbolos públicos de float.s y zeropage.s
; ===============================================================================
.export FADD, FSUB, FMULT, FDIV
.export QINT
.export STORE_FAC_AT_YX_ROUNDED  ; Para guardar FAC en formato correcto
.export FAC, ARG, FACSIGN, ARGSIGN
.export FACEXTENSION, ARGEXTENSION
