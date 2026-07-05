; ==============================================================================
; msbasic_trig.s - Funciones trigonométricas de MSBasic
; ==============================================================================
; Proporciona SIN, COS, TAN, ATN usando las rutinas de punto flotante de
; MSBasic. Depende de float.s (incluido en msbasic_float_only.s) para las
; operaciones aritméticas básicas y el evaluador de polinomios.
;
; USO: Este archivo se ensambla por separado de msbasic_float_only.s.
;       Debe incluir defines.s y zeropage.s para las definiciones de ZP.
;
; Para ensamblar:
;   ca65 --cpu 6502 --feature force_range -D CONFIG_2 -I$(MSBASIC_DIR) -o msbasic_trig.o msbasic_trig.s
; ==============================================================================

.feature force_range
.setcpu "6502"

; Constantes
BYTES_FP = 5
MANTISSA_BYTES = 4

; ==============================================================================
; IMPORTAR desde msbasic_float_only.s (ZP)
; ==============================================================================
.importzp FAC, FACSIGN, ARG, ARGSIGN
.importzp FACEXTENSION, ARGEXTENSION
.importzp CPRMASK
.importzp STRNG1, STRNG2
.importzp TEMP1, TEMP3

; ==============================================================================
; IMPORTAR desde float.s (CODE)
; ==============================================================================
.import FADD, FSUB, FSUBT, FMULT, FDIV, DIV
.import FADDH
.import INT, NEGOP
.import POLYNOMIAL_ODD
.import COPY_FAC_TO_ARG_ROUNDED
.import STORE_FAC_IN_TEMP1_ROUNDED
.import STORE_FAC_AT_YX_ROUNDED
.import LOAD_FAC_FROM_YA

; ==============================================================================
; GOMOVMF - Necesario por TAN, es un alias de STORE_FAC_AT_YX_ROUNDED
; ==============================================================================
GOMOVMF:
        jmp     STORE_FAC_AT_YX_ROUNDED

; ==============================================================================
; EXPORTAR funciones trigonométricas
; ==============================================================================
.export COS, SIN, TAN, ATN
.export GOMOVMF

; ==============================================================================
; SIN, COS, TAN, ATN
; ==============================================================================
; NOTA: SIN/COS/ATN operan sobre el valor actual en FAC.
;       El resultado queda en FAC.
; ==============================================================================

; ----------------------------------------------------------------------------
; "COS" FUNCTION
; COS(X) = SIN(X + PI/2)
; ----------------------------------------------------------------------------
COS:
        lda     #<CON_PI_HALF
        ldy     #>CON_PI_HALF
        jsr     FADD

; ----------------------------------------------------------------------------
; "SIN" FUNCTION
; SIN(X) usa reducción de rango: 0 <= X < 2*PI
; Luego mapea a un cuarto de círculo y usa POLYNOMIAL_ODD
; ----------------------------------------------------------------------------
SIN:
        jsr     COPY_FAC_TO_ARG_ROUNDED
        lda     #<CON_PI_DOUB
        ldy     #>CON_PI_DOUB
        ldx     ARGSIGN
        jsr     DIV
        jsr     COPY_FAC_TO_ARG_ROUNDED
        jsr     INT
        lda     #$00
        sta     STRNG1
        jsr     FSUBT
; ----------------------------------------------------------------------------
; (FAC) = ANGLE AS A FRACTION OF A FULL CIRCLE
;
; NOW FOLD THE RANGE INTO A QUARTER CIRCLE
; ----------------------------------------------------------------------------
        lda     #<QUARTER
        ldy     #>QUARTER
        jsr     FSUB
        lda     FACSIGN
        pha
        bpl     SIN1
        jsr     FADDH
        lda     FACSIGN
        bmi     L3F5B
        lda     CPRMASK
        eor     #$FF
        sta     CPRMASK
; ----------------------------------------------------------------------------
; IF FALL THRU, RANGE IS 0...1/2
; IF BRANCH HERE, RANGE IS 0...1/4
; ----------------------------------------------------------------------------
SIN1:
        jsr     NEGOP
; ----------------------------------------------------------------------------
; IF FALL THRU, RANGE IS -1/2...0
; IF BRANCH HERE, RANGE IS -1/4...0
; ----------------------------------------------------------------------------
L3F5B:
        lda     #<QUARTER
        ldy     #>QUARTER
        jsr     FADD
        pla
        bpl     L3F68
        jsr     NEGOP
L3F68:
        lda     #<POLY_SIN
        ldy     #>POLY_SIN
        jmp     POLYNOMIAL_ODD

; ----------------------------------------------------------------------------
; "TAN" FUNCTION
; TAN(X) = SIN(X) / COS(X)
; ----------------------------------------------------------------------------
TAN:
        jsr     STORE_FAC_IN_TEMP1_ROUNDED
        lda     #$00
        sta     CPRMASK
        jsr     SIN
        ldx     #TEMP3
        ldy     #$00
        jsr     GOMOVMF
        lda     #TEMP1+(5-BYTES_FP)
        ldy     #$00
        jsr     LOAD_FAC_FROM_YA
        lda     #$00
        sta     FACSIGN
        lda     CPRMASK
        jsr     TAN1
        lda     #TEMP3
        ldy     #$00
        jmp     FDIV
TAN1:
        pha
        jmp     SIN1

; ==============================================================================
; CONSTANTES Y POLINOMIOS (formato 5 bytes, no CONFIG_SMALL)
; ==============================================================================
CON_PI_HALF:
        .byte   $81,$49,$0F,$DA,$A2
CON_PI_DOUB:
        .byte   $83,$49,$0F,$DA,$A2
QUARTER:
        .byte   $7F,$00,$00,$00,$00
POLY_SIN:
        .byte   $05,$84,$E6,$1A,$2D,$1B,$86,$28
        .byte   $07,$FB,$F8,$87,$99,$68,$89,$01
        .byte   $87,$23,$35,$DF,$E1,$86,$A5,$5D
        .byte   $E7,$28,$83,$49,$0F,$DA,$A2

; Easter egg data (requerido por ensamblado, no se usa)
MICROSOFT:
        .byte   $A1,$54,$46,$8F,$13,$8F,$52,$43
        .byte   $89,$CD

; ----------------------------------------------------------------------------
; "ATN" FUNCTION (ArcTangente)
; ATN(X) en radianes
; ----------------------------------------------------------------------------
ATN:
        lda     FACSIGN
        pha
        bpl     L3FDB
        jsr     NEGOP
L3FDB:
        lda     FAC
        pha
        cmp     #$81
        bcc     L3FE9
        lda     #<CON_ONE
        ldy     #>CON_ONE
        jsr     FDIV
; ----------------------------------------------------------------------------
; 0 <= X <= 1
; 0 <= ATN(X) <= PI/8
; ----------------------------------------------------------------------------
L3FE9:
        lda     #<POLY_ATN
        ldy     #>POLY_ATN
        jsr     POLYNOMIAL_ODD
        pla
        cmp     #$81
        bcc     L3FFC
        lda     #<CON_PI_HALF
        ldy     #>CON_PI_HALF
        jsr     FSUB
L3FFC:
        pla
        bpl     L4002
        jmp     NEGOP
L4002:
        rts

; Constante ONE para ATN
CON_ONE:
        .byte   $81,$00,$00,$00,$00

; Polinomio para ATN
POLY_ATN:
        .byte   $0B
		.byte	$76,$B3,$83,$BD,$D3
		.byte	$79,$1E,$F4,$A6,$F5
		.byte	$7B,$83,$FC,$B0,$10
        .byte   $7C,$0C,$1F,$67,$CA
		.byte	$7C,$DE,$53,$CB,$C1
		.byte	$7D,$14,$64,$70,$4C
		.byte	$7D,$B7,$EA,$51,$7A
		.byte	$7D,$63,$30,$88,$7E
		.byte	$7E,$92,$44,$99,$3A
		.byte	$7E,$4C,$CC,$91,$C7
		.byte	$7F,$AA,$AA,$AA,$13
        .byte   $81,$00,$00,$00,$00
