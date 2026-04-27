section .text
global socket_create_raw_asm
global socket_create_tcp_server_asm
global socket_create_tcp_client_asm
global socket_create_udp_server_asm
global socket_set_nonblocking_asm
global socket_set_tcp_options_asm
global socket_send_packet_asm
global socket_receive_packet_asm
global socket_bind_interface_asm
global socket_connect_asm
global socket_accept_asm
global socket_close_asm

section .data
socket_count dd 0
bytes_sent dq 0
bytes_received dq 0
connections_active dd 0

section .bss
socket_buffer resb 65536

socket_create_raw_asm:
    push rbp
    mov rbp, rsp
    
    mov eax, 41
    mov edi, 2
    mov esi, 3
    mov edx, edi
    syscall
    
    cmp eax, 0
    jl .raw_error
    
    mov edi, eax
    mov esi, 1
    mov edx, 6
    mov ecx, 1
    mov r8d, 4
    mov eax, 54
    syscall
    
    mov eax, edi
    jmp .raw_exit

.raw_error:
    mov eax, -1

.raw_exit:
    pop rbp
    ret

socket_create_tcp_server_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
    mov eax, 41
    mov edi, 2
    mov esi, 1
    mov edx, 6
    syscall
    
    cmp eax, 0
    jl .tcp_server_error
    mov ebx, eax
    
    mov edi, ebx
    mov esi, 1
    mov edx, 2
    mov ecx, 1
    mov r8d, 4
    mov eax, 54
    syscall
    
    mov edi, ebx
    mov esi, 6
    mov edx, 1
    mov ecx, 1
    mov r8d, 4
    mov eax, 54
    syscall
    
    mov edi, ebx
    lea rsi, [rsp - 16]
    mov dword [rsi], 2
    mov word [rsi + 2], di
    xchg di, si
    call htons_asm
    mov [rsi + 2], si
    mov dword [rsi + 4], 0
    mov edx, 16
    mov eax, 49
    syscall
    
    cmp eax, 0
    jl .tcp_server_error
    
    mov edi, ebx
    mov esi, 128
    mov eax, 50
    syscall
    
    cmp eax, 0
    jl .tcp_server_error
    
    mov eax, eax
    jmp .tcp_server_exit

.tcp_server_error:
    mov eax, -1

.tcp_server_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_create_tcp_client_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13
    
    mov eax, 41
    mov edi, 2
    mov esi, 1
    mov edx, 6
    syscall
    
    cmp eax, 0
    jl .tcp_client_error
    mov ebx, eax
    
    call inet_aton_asm
    test eax, eax
    jz .tcp_client_error

    mov edi, ebx
    lea rsi, [rsp - 16]
    mov dword [rsi], 2
    mov word [rsi + 2], si
    mov dword [rsi + 4], eax
    mov edx, 16
    mov eax, 42
    syscall
    
    cmp eax, 0
    jl .tcp_client_error
    
    mov eax, ebx
    jmp .tcp_client_exit

.tcp_client_error:
    mov eax, -1

.tcp_client_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_create_udp_server_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov edi, r12

    mov eax, 41
    mov edi, 2
    mov esi, 2
    mov edx, 17
    syscall
    
    cmp eax, 0
    jl .udp_server_error
    mov ebx, eax
    
    mov edi, ebx
    lea rsi, [rsp - 16]
    mov dword [rsi], 2
    mov word [rsi + 2], di
    call htons_asm
    mov [rsi + 2], di
    mov dword [rsi + 4], 0
    mov edx, 16
    mov eax, 49
    syscall
    
    cmp eax, 0
    jl .udp_server_error
    
    mov eax, ebx
    jmp .udp_server_exit

.udp_server_error:
    mov eax, -1

.udp_server_exit:
    pop r12
    pop rbx
    pop rbp
    ret

socket_set_nonblocking_asm:
    push rbp
    mov rbp, rsp
    
    mov edi, edi
    mov eax, 36
    mov esi, 3
    syscall
    
    mov esi, eax
    or esi, 2048
    mov eax, 36
    mov edx, esi
    syscall
    
    cmp eax, 0
    jl .nonblock_error
    xor eax, eax
    jmp .nonblock_exit

.nonblock_error:
    mov eax, -1

.nonblock_exit:
    pop rbp
    ret

socket_set_tcp_options_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov edi, r12
    mov esi, r13
    mov edx, r14
    
    mov ebx, edi
    
    test esi, esi
    jz .skip_nodelay
    mov edi, ebx
    mov esi, 6
    mov edx, 1
    mov ecx, 1
    mov r8d, 4
    mov eax, 54
    syscall

.skip_nodelay:
    test edx, edx
    jz .skip_keepalive
    mov edi, ebx
    mov esi, 1
    mov edx, 9
    mov ecx, 1
    mov r8d, 4
    mov eax, 54
    syscall

.skip_keepalive:
    xor eax, eax
    
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_send_packet_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov edi, r12
    mov rsi, r13
    mov rdx, r14
    mov rcx, r15
    mov r8, 16
    
    mov eax, 44
    syscall
    
    cmp eax, 0
    jl .send_error
    
    add qword [bytes_sent], r14
    
    jmp .send_exit

.send_error:
    mov eax, -1

.send_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_receive_packet_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov edi, r12
    mov rsi, r13
    mov rdx, r14
    xor rcx, rcx
    mov r8, r15
    push rsp
    mov r9, rsp
    mov dword [r9], 16
    
    mov eax 45
    syscall
    
    cmp eax, 0
    jl .recv_error
    
    add qword [bytes_received], rax
    
    jmp .recv_exit

.recv_error:
    mov eax, -1

.recv_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_bind_interface_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov edi, r12
    mov rsi, r13
    
    call if_nametoindex_asm
    test eax, eax
    jz .bind_error
    mov ebx, eax
    
    mov edi, r12
    mov esi, 1
    mov edx, 25
    mov rcx, r13
    mov r8d, 16
    mov eax, 54
    syscall
    
    cmp eax, 0
    jl .bind_error
    xor eax, eax
    jmp .bind_exit

.bind_error:
    mov eax, -1

.bind_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_accept_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov edi, r12
    mov rsi, r13
    mov rdx, r15
    
    mov eax, 43
    syscall
    
    cmp eax, 0
    jl .accept_error
    
    inc dword [connections_active]
    
    jmp .accept_exit

.accept_error:
    mov eax, -1

.accept_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

socket_close_asm:
    push rbp
    mov rbp, rsp
    
    mov edi, edi
    mov eax, 3
    syscall
    
    dec dword [connections_active]
    
    pop rbp
    ret

htons_asm:
    push rbp
    mov rbp, rsp
    
    mov ax, di
    xchg al, ah
    mov di, ax
    
    pop rbp
    ret

inet_aton_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov rdi, r12
    
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    
.parse_loop:
    movzx edx, byte [r12]
    cmp edx, 0
    je .parse_done
    cmp edx, '.'
    je .next_octet
    
    sub edx, '0'
    cmp edx, 9
    ja .parse_error
    
    imul ebx, 10
    add ebx, edx
    inc r12
    jmp .parse_loop

.next_octet:
    shl eax, 8
    or al, bl
    xor ebx, ebx
    inc ecx
    inc r12
    cmp ecx, 3
    jg .parse_error
    jmp .parse_loop

.parse_done:
    shl eax, 8
    or al, bl
    jmp .parse_exit

.parse_error:
    xor eax, eax

.parse_exit:
    pop r12
    pop rbx
    pop rbp
    ret

if_nametoindex_asm:
    push rbp
    mov rbp, rsp
    
    mov rdi, r12
    mov byte [rsp - 16], 'l'
    mov byte [rsp - 15], 'o'
    mov byte [rsp - 14], 0
    lea rsi, [rsp - 16]
    
    call strcmp_asm
    test eax, eax
    jz .is_loopback
    
    xor eax, eax
    jmp .if_index_exit

.is_loopback:
    mov eax, 1

.if_index_exit:
    pop rbp
    ret

strcmp_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov rdi, r12
    mov rsi, r13

strcmp_loop:
    mov al, [r12]
    mov bl, [r13]
    cmp al, bl
    jne .strcmp_diff
    
    test al, al
    jz .strcmp_done
    
    inc r12
    inc r13
    jmp .strcmp_loop

strcmp_diff:
    mov al, [r12]
    mov bl, [r13]
    sub al, bl
    movsx eax, al
    jmp .strcmp_exit

strcmp_done:
    xor eax, eax

strcmp_exit:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
