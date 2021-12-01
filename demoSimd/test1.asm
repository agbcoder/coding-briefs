; ----------------------------------------------------------------------------------------
; Testing interface with C
; ----------------------------------------------------------------------------------------

          global    dosometh

          section   .text

; extern int dosometh(int a, int b);
;
; Adds 2 integers.
;
; rdi, rsi, rdx, rcx, r8, r9
;
;    rdi:  1st operand
;    rsi:  2nd operand
;    returns:  rax result   

dosometh: mov       rax, rdi                 ; copies 1st operand
          add       rax, rsi                 ; adds 2nd operand
          ret                                ; returns sum in rax

;          section   .data
;message:  db        "Hello, World", 10      ; note the newline at the end