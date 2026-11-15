#define ASSEMBLY_EXIT_CODE 8
.text
.globl main
main:
    movl $ASSEMBLY_EXIT_CODE, %eax
    ret
