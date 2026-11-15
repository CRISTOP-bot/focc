define(`FOCC_EXIT_CODE', `9')dnl
.text
.globl main
main:
    movl $FOCC_EXIT_CODE, %eax
    ret
