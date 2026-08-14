; ============================================
; startup.s - Código de inicio para programas C
; ============================================
; Inicializa el runtime CC65 para programas cargados en RAM
; Se ejecuta desde $0800
;
; El monitor llama al programa con JSR (subrutina).
; Al salir, se restaura la dirección de retorno y el SP
; del monitor para volver al prompt SIN reiniciar.
; ============================================

.export _init
.export __STARTUP__ : absolute = 1

.import _main
.import __BSS_RUN__, __BSS_SIZE__
.importzp sp

; Variables temporales en zero page
.segment "ZEROPAGE"
ptr1:       .res 2
ptr2:       .res 2
count:      .res 2
ret_lo:     .res 1   ; Byte bajo de la dirección de retorno al monitor
ret_hi:     .res 1   ; Byte alto de la dirección de retorno al monitor
mon_sp:     .res 1   ; SP hardware del monitor al momento del JSR
save_sp:    .res 2   ; Software stack pointer del monitor (sp ZP)

.segment "STARTUP"

_init:
    ; Deshabilitar interrupciones durante init
    sei
    cld
    
    ; Guardar la dirección de retorno del monitor (JSR $0800)
    ; El monitor hizo JSR: el stack tiene [ret_hi][ret_lo], SP→ret_lo
    pla
    sta ret_lo
    pla
    sta ret_hi
    
    ; Guardar SP del monitor (nivel del contexto, antes del JSR)
    tsx
    stx mon_sp
    
    ; Guardar el software stack pointer del monitor (sp ZP de CC65)
    lda sp
    sta save_sp
    lda sp+1
    sta save_sp+1
    
    ; Inicializar stack pointer del 6502
    ldx #$FF
    txs
    
    ; Inicializar stack pointer de CC65 (software stack)
    ; Usar $3DFF como tope del stack
    lda #<$3DFF
    sta sp
    lda #>$3DFF
    sta sp+1
    
    ; Inicializar BSS a ceros
    jsr zerobss
    
    ; Llamar a main
    jsr _main
    
    ; ============================================================
    ; SALIDA: volver al monitor SIN reiniciar
    ; El monitor nos llamó con JSR, así que con RTS volvemos al
    ; prompt (muestra "Retorno de $0800") en lugar de reiniciar.
    ; ============================================================
    ; Restaurar el software stack pointer del monitor
    lda save_sp
    sta sp
    lda save_sp+1
    sta sp+1
    
    ; Restaurar SP hardware del monitor (nivel del contexto)
    ldx mon_sp
    txs
    
    ; Re-habilitar interrupciones (el monitor las tenía activas)
    cli
    
    ; Re-construir la dirección de retorno en el stack
    lda ret_hi
    pha          ; [mon_sp-1] = ret_hi
    lda ret_lo
    pha          ; [mon_sp-2] = ret_lo, SP = mon_sp-2
    rts          ; → vuelve a mon_execute → prompt del monitor

; ============================================
; zerobss - Inicializa BSS a ceros
; ============================================
zerobss:
    ; Si BSS_SIZE es 0, no hay nada que hacer
    lda #<__BSS_SIZE__
    ora #>__BSS_SIZE__
    beq @done
    
    ; Inicializar puntero al inicio de BSS
    lda #<__BSS_RUN__
    sta ptr1
    lda #>__BSS_RUN__
    sta ptr1+1
    
    ; Contador de bytes
    lda #<__BSS_SIZE__
    sta count
    lda #>__BSS_SIZE__
    sta count+1
    
    ; Llenar con ceros
    ldy #0
    lda #0
@loop:
    sta (ptr1),y
    
    ; Incrementar puntero
    inc ptr1
    bne @skip
    inc ptr1+1
@skip:
    
    ; Decrementar contador
    lda count
    bne @dec_low
    dec count+1
@dec_low:
    dec count
    
    ; Verificar si terminamos
    lda count
    ora count+1
    bne @loop
    
@done:
    rts
