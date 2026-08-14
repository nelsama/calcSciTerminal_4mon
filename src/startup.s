; ============================================
; startup.s - Código de inicio para programas C
; ============================================
; Inicializa el runtime CC65 para programas cargados en RAM
; Se ejecuta desde $0800
;
; El monitor llama al programa con JSR (subrutina).
; IMPORTANTE: NO se resetea el stack hardware (SP). Se usa el
; stack desde donde el monitor lo dejó hacia abajo, de modo que
; el contexto del monitor (return addresses) queda intacto.
; Al salir, un simple RTS vuelve al prompt sin reiniciar.
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
save_sp:    .res 2   ; Software stack pointer del monitor (sp ZP)

.segment "STARTUP"

_init:
    ; Deshabilitar interrupciones durante init
    sei
    cld
    
    ; NO tocar el stack hardware: el monitor hizo JSR a $0800 y
    ; la dirección de retorno está en el stack. Si reseteamos SP
    ; perderíamos el contexto del monitor y al volver se corrompería.
    ; Nuestro programa usa el stack desde SP hacia abajo y lo balancea.
    
    ; Guardar el software stack pointer del monitor (sp ZP de CC65)
    lda sp
    sta save_sp
    lda sp+1
    sta save_sp+1
    
    ; Inicializar NUESTRO stack pointer de CC65 (software stack)
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
    ; El SP hardware nunca se tocó de forma destructiva: todos los
    ; JSR/RTS del programa están balanceados y SP apunta al return
    ; address del monitor.
    ; ============================================================
    ; Restaurar el software stack pointer del monitor
    lda save_sp
    sta sp
    lda save_sp+1
    sta sp+1
    
    ; Re-habilitar interrupciones (el monitor las tenía activas)
    cli
    
    ; Volver al monitor
    rts

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
