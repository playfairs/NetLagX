section .text
global shaping_init_asm
global shaping_apply_delay_asm
global shaping_apply_jitter_asm
global shaping_apply_loss_asm
global shaping_calculate_optimal_delay_asm
global shaping_get_queue_size_asm
global shaping_reset_stats_asm
global shaping_get_statistics_asm

section .data
delay_queue_head dq 0
delay_queue_tail dq 0
delay_queue_size dd 0
max_queue_size dd 1000
current_delay_ms dd 0
jitter_mean_ms dd 50
jitter_variance_ms dd 25
loss_rate_percent dd 0
packets_delayed dq 0
packets_dropped dq 0
packets_processed dq 0
total_delay_ns dq 0

section .bss
delay_queue resb 65536
jitter_buffer resb 1024
stats_buffer resb 256

shaping_init_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, edi
    mov [max_queue_size], eax
    
    mov qword [delay_queue_head], delay_queue
    mov qword [delay_queue_tail], delay_queue
    mov dword [delay_queue_size], 0
    
    call shaping_reset_stats_asm
    
    xor eax, eax
    pop rbp
    ret

shaping_apply_delay_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rdi, r12    
    mov rsi, r12
    mov edx, r14
    
    mov eax, [delay_queue_size]
    cmp eax, [max_queue_size]
    jge .delay_drop
    
    call get_timestamp_ns_asm
    mov rbx, rax
    
    mov eax, r14d
    imul rax, 1000000
    add rbx, rax
    
    mov rdi, [delay_queue_tail]
    mov rsi, r12
    mov rdx, r13
    call memcpy_asm
    
    mov [rdi + r13], rbx
    
    add r13, 16
    add qword [delay_queue_tail], r13
    inc dword [delay_queue_size]
    
    inc qword [packets_delayed]
    add qword [total_delay_ns], rax
    
    mov eax, 1
    jmp .delay_exit

.delay_drop:
    xor eax, eax

.delay_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

shaping_apply_jitter_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
    call generate_jitter_asm
    mov ebx, eax
    
    mov edx, ebx
    call shaping_apply_delay_asm
    
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

shaping_apply_loss_asm:
    push rbp
    mov rbp, rsp
    
    call rand_asm
    xor edx, edx
    mov ecx, 10000
    div ecx
    
    mov eax, [loss_rate_percent]
    imul eax, 100
    cmp edx, eax
    setb al
    movzx eax, al
    
    test eax, eax
    jz .loss_exit
    inc qword [packets_dropped]

.loss_exit:
    pop rbp
    ret

shaping_calculate_optimal_delay_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov edi, r12
    mov esi, r13
    
    xor eax, eax
    mov ebx, [max_queue_size]
    test ebx, ebx
    jz .calc_done
    
    mov eax, edi
    xor edx, edx
    div ebx
    
    imul eax, 100
    
    cmp eax, 1000
    jle .calc_done
    mov eax, 1000

.calc_done:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

shaping_get_queue_size_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, [delay_queue_size]
    pop rbp
    ret

shaping_reset_stats_asm:
    push rbp
    mov rbp, rsp
    
    mov qword [packets_delayed], 0
    mov qword [packets_dropped], 0
    mov qword [packets_processed], 0
    mov qword [total_delay_ns], 0
    
    pop rbp
    ret

shaping_get_statistics_asm:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rdi, rbx

    mov rax, [packets_delayed]
    mov [rbx], rax
    
    mov rax, [packets_dropped]
    mov [rbx + 8], rax
    
    mov rax, [packets_processed]
    mov [rbx + 16], rax
    
    mov rax, [total_delay_ns]
    mov [rbx + 24], rax
    
    mov eax, [delay_queue_size]
    mov [rbx + 32], eax
    
    mov eax, [max_queue_size]
    mov [rbx + 36], eax
    
    pop rbx
    pop rbp
    ret

generate_jitter_asm:
    push rbp
    mov rbp, rsp
    
    call rand_asm
    xor edx, edx
    mov ecx, 100
    div ecx
    
    mov eax, edx
    imul eax, [jitter_mean_ms]
    imul eax, 2
    mov ecx, 100
    div ecx
    
    call rand_asm
    and eax, 1
    test eax, eax
    jz .jitter_subtract
    
    mov eax, [jitter_mean_ms]
    add eax, edx
    jmp .jitter_done

.jitter_subtract:
    mov eax, [jitter_mean_ms]
    sub eax, edx
    jge .jitter_done
    xor eax, eax

.jitter_done:
    pop rbp
    ret

get_timestamp_ns_asm:
    push rbp
    mov rbp, rsp
    
    rdtsc
    shl rdx, 32
    or rax, rdx
    
    mov rbx, 2500000000
    xor rdx, rdx
    div rbx
    
    pop rbp
    ret

rand_asm:
    push rbp
    mov rbp, rsp
    
    static seed dq 123456789
    mov rax, [rel seed]
    imul rax, 1103515245
    add rax, 12345
    mov [rel seed], rax
    shr rax, 16
    and rax, 0x7FFF
    
    pop rbp
    ret

memcpy_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    
    test rdx, rdx
    jz .memcpy_done
    
    mov rcx, rdx
    rep movsb

.memcpy_done:
    mov rax, r12
    
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

global shaping_process_ready_packets_asm
shaping_process_ready_packets_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
.process_loop:
    mov eax, [delay_queue_size]
    test eax, eax
    jz .process_done
    
    call get_timestamp_ns_asm
    mov rbx, rax
    
    mov r8, [delay_queue_head]
    mov r9, [r8]
    mov r10, [r8 + r9]
    
    cmp rbx, r10
    jl .process_done
    
    mov rdi, r8
    mov rsi, r9
    mov rdx, r13
    call r12
    
    add r9, 16
    add qword [delay_queue_head], r9
    dec dword [delay_queue_size]
    
    inc qword [packets_processed]
    
    jmp .process_loop

.process_done:
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

global shaping_set_delay_params_asm
shaping_set_delay_params_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, edi
    mov [current_delay_ms], eax
    
    mov eax, esi
    mov [jitter_mean_ms], eax
    
    mov eax, edx
    mov [jitter_variance_ms], eax
    
    pop rbp
    ret

global shaping_set_loss_params_asm
shaping_set_loss_params_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, edi
    mov [loss_rate_percent], eax
    
    pop rbp
    ret
