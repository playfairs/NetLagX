section .text
global packet_intercept_asm
global packet_send_delayed_asm
global packet_filter_target_asm
global packet_create_raw_socket_asm
global packet_set_promiscuous_asm
global packet_get_interface_mac_asm
global packet_calculate_checksum_asm
global packet_validate_ip_asm
global packet_extract_headers_asm

section .data
raw_socket_fd dd 0
target_ip dd 0
target_port dw 0
packet_count dq 0
bytes_processed dq 0

section .bss
packet_buffer resb 65536

packet_intercept_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rdi, r12
    
    mov eax, 41
    mov edi, 2
    mov esi, 3
    mov edx, 6
    syscall
    
    cmp eax, 0
    jl .intercept_error
    mov [raw_socket_fd], eax
    
.intercept_loop:
    mov rax, [r12 + 0]
    test rax, rax
    jz .intercept_done
    
    mov eax, 45
    mov edi, [raw_socket_fd]
    mov rsi, packet_buffer
    mov edx, 65536
    xor ecx, ecx
    push rsp
    mov r8, rsp
    push rsp
    mov r9, rsp
    syscall
    
    cmp eax, 0
    jle .intercept_loop
    
    mov rdi, packet_buffer
    mov rsi, rax
    mov rdx, r12
    call packet_process_asm
    
    jmp .intercept_loop

.intercept_done:
    mov eax, [raw_socket_fd]
    mov edi, eax
    mov eax, 3
    syscall
    
    xor eax, eax
    jmp .intercept_exit

.intercept_error:
    mov eax, -1

.intercept_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

packet_process_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    
    movzx eax, byte [r12]
    cmp al, 0x45
    jne .packet_skip
    
    mov eax, [r12 + 12]
    cmp eax, [r14 + 8]
    jne .packet_skip
    
    movzx ebx, byte [r12 + 9]
    cmp ebx, 6
    je .packet_tcp
    cmp ebx, 17
    je .packet_udp
    jmp .packet_skip

.packet_tcp:
    movzx ecx, word [r12 + 20]
    movzx edx, word [r14 + 12]
    cmp ecx, edx
    jne .packet_skip
    jmp .packet_process

.packet_udp:
    movzx ecx, word [r12 + 20]
    movzx edx, word [r14 + 12]
    cmp ecx, edx
    jne .packet_skip
    jmp .packet_process

.packet_process:
    mov eax, [r14 + 4]
    cmp eax, 1
    je .packet_lag
    cmp eax, 2
    je .packet_drop
    cmp eax, 3
    je .packet_throttle
    jmp .packet_forward

.packet_lag:
    call packet_add_to_delay_queue_asm
    jmp .packet_done

.packet_drop:
    call packet_should_drop_asm
    test eax, eax
    jnz .packet_skip
    jmp .packet_forward

.packet_throttle:
    call packet_should_throttle_asm
    test eax, eax
    jnz .packet_skip
    jmp .packet_forward

.packet_forward:
    call packet_send_immediate_asm

.packet_done:
    inc qword [packet_count]
    add rsi, qword [bytes_processed]

.packet_skip:
    xor eax, eax
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

packet_send_immediate_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
    mov eax, 41
    mov edi, 2
    mov esi, 3
    mov edx, 6
    syscall
    
    cmp eax, 0
    jl .send_error
    mov ebx, eax
    
    mov eax, 44
    mov edi, ebx
    mov rsi, r12
    mov edx, r13
    xor ecx, ecx
    push rsp
    mov r8, rsp
    push rsp
    mov r9, rsp
    syscall
    
    mov edi, ebx
    mov eax, 3
    syscall
    
    xor eax, eax
    jmp .send_exit

.send_error:
    mov eax, -1

.send_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

packet_add_to_delay_queue_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
    mov eax, 9
    xor edi, edi
    mov esi, 64
    mov edx, 3
    mov ecx, 34
    xor r8, r8
    xor r9, r9
    syscall
    
    test rax, rax
    js .delay_error
    mov rbx, rax
    
    mov eax, rdi
    mov edx, rsi
    call memcpy_asm
    
    call get_timestamp_asm
    mov rcx, [r14 + 16]
    imul rcx, 1000000
    add rax, rcx
    mov [rbx + 8], rax
    
    xor eax, eax
    jmp .delay_exit

.delay_error:
    mov eax, -1

.delay_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

packet_should_drop_asm:
    push rbp
    mov rbp, rsp
    
    call rand_asm
    xor edx, edx
    mov ecx, 10000
    div ecx
    
    mov eax, [r14 + 20]
    imul eax, 10000
    cmp edx, eax
    setb al
    movzx eax, al
    
    pop rbp
    ret

packet_should_throttle_asm:
    push rbp
    mov rbp, rsp
    
    static last_packet_time dq 0
    
    call get_timestamp_asm
    mov rbx, rax
    mov rax, [rel last_packet_time]
    sub rbx, rax
    
    mov eax, [r14 + 24]
    imul eax, 1000000
    cmp rbx, rax
    setl al
    movzx eax, al
    
    call get_timestamp_asm
    mov [rel last_packet_time], rax
    
    pop rbp
    ret

packet_create_raw_socket_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, 41
    mov edi, 2
    mov esi, 3 
    mov edx, edi
    syscall
    
    cmp eax, 0
    jl .socket_error
    
    mov edi, eax
    mov esi, 1
    mov edx, 19
    xor ecx, ecx
    mov r8d, 4
    mov eax, 54
    syscall
    
    jmp .socket_exit

.socket_error:
    mov eax, -1

.socket_exit:
    pop rbp
    ret

packet_calculate_checksum_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
    xor rax, rax
    xor rbx, rbx
    
.checksum_loop:
    cmp rsi, 2
    jl .checksum_done
    
    movzx ebx, word [r12]
    add rax, rbx
    adc rax, 0
    
    add r12, 2
    sub rsi, 2
    jmp .checksum_loop

.checksum_done:
    test rsi, rsi
    jz .checksum_fold
    movzx ebx, byte [r12]
    shl rbx, 8
    add rax, rbx
    adc rax, 0

.checksum_fold:
    mov rbx, rax
    shr rbx, 16
    add rax, rbx
    adc rax, 0
    
    mov rbx, rax
    shr rbx, 16
    add rax, rbx
    adc rax, 0
    
    and rax, 0xFFFF
    not rax
    
    pop r13
    pop r12
    pop rbx
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

get_timestamp_asm:
    push rbp
    mov rbp, rsp
    
    rdtsc
    shl rdx, 32
    or rax, rdx
    
    pop rbp
    ret

memcpy_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    
    test rdx, rdx
    jz .memcpy_done
    
    mov rcx, rdx
    rep movsb
    
.memcpy_done:
    mov rax, r12
    
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
