.global checksum_asm
.global memcpy_asm
.global memset_asm
.global rdtsc_asm
.global cpuid_asm
.global prefetch_asm
.global memory_barrier_asm
.global packet_filter_fast_asm
.global precise_delay_ns_asm
.global get_timestamp_counter_asm
.global popcount_asm
.global find_first_set_asm
.global find_last_set_asm
.global strlen_asm
.global strcmp_asm
.global strchr_asm
.global detect_cpu_features_asm
.global is_x86_64_asm
.global reset_perf_counters_asm
.global read_perf_counters_asm

.section .data
perf_counters:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0m

cpu_features:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .space 16
    .space 64

.section .text

checksum_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    push    %r13
    
    mov     %rdi, %r12
    mov     %rsi, %r13
    
    xor     %rax, %rax
    
    test    %r13, %r13
    jz      .checksum_done
    
.checksum_loop:
    cmp     $8, %r13
    jl      .checksum_remaining
    
    mov     (%r12), %rbx
    
    add     %rbx, %rax
    
    adc     $0, %rax
    
    add     $8, %r12
    sub     $8, %r13
    jmp     .checksum_loop
    
.checksum_remaining:
    test    %r13, %r13
    jz      .checksum_final
    
    xor     %rbx, %rbx

.checksum_byte_loop:
    shl     $8, %rbx
    movzb   (%r12), %rcx
    or      %rcx, %rbx
    inc     %r12
    dec     %r13
    test    %r13, %r13
    jnz     .checksum_byte_loop
    
    add     %rbx, %rax
    adc     $0, %rax
    
.checksum_final:
    mov     %rax, %rbx
    shr     $32, %rbx
    add     %rbx, %rax
    adc     $0, %rax
    
    mov     %rax, %rbx
    shr     $16, %rbx
    add     %rbx, %rax
    adc     $0, %rax
    
    and     $0xFFFF, %rax
    
    pop     %r13
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

memcpy_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    push    %r13
    push    %r14
    
    mov     %rdi, %r12
    mov     %rsi, %r13
    mov     %rdx, %r14
    
    test    %r14, %r14
    jz      .memcpy_done
    
    mov     %r13, %rax
    and     $15, %rax
    mov     %r12, %rbx
    and     $15, %rbx
    or      %rax, %rbx
    jnz     .memcpy_unaligned
    
    test    $16, %r14
    jz      .memcpy_small_aligned
    
    movdqa  (%r13), %xmm0
    movdqa  %xmm0, (%r12)
    
    add     $16, %r13
    add     $16, %r12
    sub     $16, %r14
    
    test    $16, %r14
    jnz     .memcpy_aligned_loop
    
    jmp     .memcpy_remaining
    
.memcpy_aligned_loop:
    movdqa  (%r13), %xmm0
    movdqa  %xmm0, (%r12)
    movdqa  16(%r13), %xmm1
    movdqa  %xmm1, 16(%r12)
    
    add     $32, %r13
    add     $32, %r12
    sub     $32, %r14
    
    test    $32, %r14
    jnz     .memcpy_aligned_loop
    
.memcpy_remaining:
    test    %r14, %r14
    jz      .memcpy_done
    
.memcopy_byte_loop:
    movb    (%r13), %al
    movb    %al, (%r12)
    inc     %r13
    inc     %r12
    dec     %r14
    jnz     .memcopy_byte_loop
    
    jmp     .memcpy_done
    
.memcpy_unaligned:
    mov     %r14, %rcx
    rep     movsb
    
.memcpy_small_aligned:
    test    %r14, %r14
    jz      .memcpy_done
    
.memcopy_small_loop:
    movb    (%r13), %al
    movb    %al, (%r12)
    inc     %r13
    inc     %r12
    dec     %r14
    jnz     .memcopy_small_loop
    
.memcpy_done:
    mov     %rdi, %rax
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    push    %r13
    
    mov     %rdi, %r12
    mov     %esi, %ebx
    mov     %rdx, %r13
    
    test    %r13, %r13
    jz      .memset_done
    
    movd    %ebx, %xmm0
    punpcklbw %xmm0, %xmm0
    punpcklwd %xmm0, %xmm0
    pshufd  $0, %xmm0, %xmm0
    
    test    $16, %r12
    jnz     .memset_unaligned
    
    test    $16, %r13
    jz      .memset_small
    
    movdqa  %xmm0, (%r12)
    add     $16, %r12
    sub     $16, %r13
    
    test    $16, %r13
    jnz     .memset_aligned_loop
    
    jmp     .memset_remaining
    
.memset_aligned_loop:
    movdqa  %xmm0, (%r12)
    movdqa  %xmm0, 16(%r12)
    add     $32, %r12
    sub     $32, %r13
    test    $32, %r13
    jnz     .memset_aligned_loop
    
.memset_remaining:
    test    %r13, %r13
    jz      .memset_done
    
.memset_byte_loop:
    movb    %bl, (%r12)
    inc     %r12
    dec     %r13
    jnz     .memset_byte_loop
    
    jmp     .memset_done
    
.memset_unaligned:
    mov     %r13, %rcx
    mov     %rbx, %rax
    rep     stosb
    
.memset_small:
    test    %r13, %r13
    jz      .memset_done
    
.memset_small_loop:
    movb    %bl, (%r12)
    inc     %r12
    dec     %r13
    jnz     .memset_small_loop
    
.memset_done:
    mov     %rdi, %rax
    pop     %r13
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

rdtsc_asm:
    rdtsc
    shl     $32, %rdx
    or      %rdx, %rax
    ret

cpuid_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    
    mov     %edi, %eax
    xor     %ecx, %ecx
    cpuid
    
    mov     %eax, (%rsi)
    mov     %ebx, (%rdx)
    mov     %ecx, 8(%rsi)
    mov     %edx, 12(%rsi)

    pop     %rbx
    pop     %rbp
    ret

prefetch_asm:
    prefetcht0 (%rdi)
    ret

memory_barrier_asm:
    mfence
    ret

get_timestamp_counter_asm:
    push    %rcx
    push    %rdx
    rdtscp
    shl     $32, %rdx
    or      %rdx, %rax
    pop     %rdx
    pop     %rcx
    ret

popcount_asm:
    mov     %edi, %eax
    popcnt  %eax, %eax
    ret

find_first_set_asm:
    mov     %edi, %eax
    test    %eax, %eax
    jz      .ffs_zero
    bsf     %eax, %eax
    inc     %eax
    ret
.ffs_zero:
    xor     %eax, %eax
    ret

find_last_set_asm:
    mov     %edi, %eax
    test    %eax, %eax
    jz      .fls_zero
    bsr     %eax, %eax
    inc     %eax
    ret
.fls_zero:
    xor     %eax, %eax
    ret

strlen_asm:
    push    %rbp
    mov     %rsp, %rbp
    xor     %rax, %rax
    mov     %rdi, %rcx
    
.strlen_loop:
    movb    (%rcx), %dl
    test    %dl, %dl
    jz      .strlen_done
    inc     %rcx
    inc     %rax
    jmp     .strlen_loop
    
.strlen_done:
    pop     %rbp
    ret

strcmp_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    push    %r13
    
    mov     %rdi, %r12
    mov     %rsi, %r13
    xor     %eax, %eax
    
.strcmp_loop:
    movb    (%r12), %bl
    cmpb    (%r13), %bl
    jne     .strcmp_diff
    
    test    %bl, %bl
    jz      .strcmp_done
    
    inc     %r12
    inc     %r13
    jmp     .strcmp_loop
    
.strcmp_diff:
    movb    (%r13), %cl
    sub     %cl, %bl
    movsx   %bl, %eax
    jmp     .strcmp_done
    
.strcmp_done:
    pop     %r13
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

strchr_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    
    mov     %rdi, %r12
    mov     %esi, %ebx
    
.strchr_loop:
    movb    (%r12), %al
    cmp     %bl, %al
    je      .strchr_found
    
    test    %al, %al
    jz      .strchr_not_found
    
    inc     %r12
    jmp     .strchr_loop
    
.strchr_found:
    mov     %r12, %rax
    jmp     .strchr_done
    
.strchr_not_found:
    xor     %rax, %rax
    
.strchr_done:
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

is_x86_64_asm:
    mov     $1, %eax
    ret

detect_cpu_features_asm:
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12
    push    %r13
    push    %r14
    
    mov     %rdi, %r12
    
    mov     $0, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx
    cpuid
    
    mov     %ebx, 32(%r12)
    mov     %edx, 36(%r12)
    mov     %ecx, 40(%r12)
    movb    $0, 47(%r12)
    
    mov     $1, %eax
    xor     %ecx, %ecx
    cpuid
    test    $0x04000000, %edx
    setnz   0(%r12)
    
    test    $0x00080000, %ecx
    setnz   8(%r12)
    
    test    $0x10000000, %ecx
    setnz   16(%r12)
    
    mov     $7, %eax
    mov     $1, %ecx
    xor     %edx, %edx
    cpuid
    test    $0x00000020, %ebx
    setnz   24(%r12)
    
    mov     $0x80000001, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx
    cpuid
    test    $0x08000000, %edx
    setnz   32(%r12)
    
    mov     $0x80000007, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx
    cpuid
    test    $0x00000100, %edx
    setnz   40(%r12)
    
    lea     80(%r12), %r13
    
    mov     $0x80000002, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx
    cpuid
    mov     %eax, (%r13)
    mov     %ebx, 4(%r13)
    mov     %ecx, 8(%r13)
    mov     %edx, 12(%r13)
    
    mov     $0x80000003, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx
    cpuid
    mov     %eax, 16(%r13)
    mov     %ebx, 20(%r13)
    mov     %ecx, 24(%r13)
    mov     %edx, 28(%r13)
    
    mov     $0x80000004, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx
    cpuid
    mov     %eax, 32(%r13)
    mov     %ebx, 36(%r13)
    mov     %ecx, 40(%r13)
    mov     %edx, 44(%r13)
    
    movb    $0, 63(%r13)
    
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

reset_perf_counters_asm:
    push    %rbp
    mov     %rsp, %rbp
    
    mov     %rdi, %rax
    xor     %rcx, %rcx
    mov     $6, %rdx
    
.clear_loop:
    mov     %rcx, (%rax)
    add     $8, %rax
    dec     %rdx
    jnz     .clear_loop
    
    pop     %rbp
    ret

read_perf_counters_asm:
    push    %rbp
    mov     %rsp, %rbp
    
    mov     %rdi, %rax
    mov     $perf_counters, %rsi
    mov     $6, %rcx
    
.copy_loop:
    mov     (%rsi), %rdx
    mov     %rdx, (%rax)
    add     $8, %rax
    add     $8, %rsi
    dec     %rcx
    jnz     .copy_loop
    
    pop     %rbp
    ret
