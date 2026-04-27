section .text
global proxy_init_asm
global proxy_start_asm
global proxy_stop_asm
global proxy_create_forward_proxy_asm
global proxy_create_reverse_proxy_asm
global proxy_handle_client_asm
global proxy_forward_data_asm
global proxy_accept_connections_asm
global proxy_cleanup_asm

section .data
proxy_server_fd dd 0
proxy_mode dd 0
upstream_host db 0 dup 256
upstream_port dw 0
listen_port dw 0
client_count dd 0
max_clients dd 100
bytes_forwarded dq 0
connections_handled dq 0

section .bss
client_pool resb 65536
forward_buffer resb 32768

proxy_init_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov edi, r12
    mov rsi, r13
    mov edx, r14
    mov ecx, r15
    
    mov [proxy_mode], edi
    mov [upstream_port], edx
    mov [listen_port], ecx
    
    call strcpy_asm
    mov rdi, upstream_host
    mov rsi, r13
    call strcpy_asm
    
    xor eax, eax
    jmp .proxy_init_exit

.proxy_init_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

proxy_start_asm:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov edi, [listen_port]
    xor esi, esi
    call socket_create_tcp_server_asm
    
    cmp eax, 0
    jl .proxy_start_error
    mov [proxy_server_fd], eax
    mov ebx, eax
    
    xor eax, eax
    mov [client_count], eax
    
    call proxy_accept_connections_asm
    
    xor eax, eax
    jmp .proxy_start_exit

.proxy_start_error:
    mov eax, -1

.proxy_start_exit:
    pop rbx
    pop rbp
    ret

proxy_stop_asm:
    push rbp
    mov rbp, rsp
    
    mov edi, [proxy_server_fd]
    call socket_close_asm
    
    xor eax, eax
    pop rbp
    ret

proxy_create_forward_proxy_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    
    mov edi, 1
    call proxy_init_asm
    
    test eax, eax
    jnz .forward_error
    
    call proxy_start_asm
    
    test eax, eax
    jnz .forward_error
    
    xor eax, eax
    jmp .forward_exit

.forward_error:
    mov eax, -1

.forward_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret ; retard??

proxy_create_reverse_proxy_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    
    mov edi, 2
    call proxy_init_asm
    
    test eax, eax
    jnz .reverse_error
    
    call proxy_start_asm
    
    test eax, eax
    jnz .reverse_error
    
    xor eax, eax
    jmp .reverse_exit

.reverse_error:
    mov eax, -1

.reverse_exit:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

proxy_accept_connections_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
.accept_loop:
    mov eax, [proxy_server_fd]
    cmp eax, 0
    jl .accept_done
    
    mov eax, [client_count]
    cmp eax, [max_clients]
    jge .accept_loop
    
    mov edi, [proxy_server_fd]
    lea rsi, [rsp - 32]
    lea rdx, [rsp - 16]
    mov dword [rdx], 32
    call socket_accept_asm
    
    cmp eax, 0
    jl .accept_loop
    
    mov ebx, eax
    
    mov edi, ebx
    call proxy_handle_client_asm
    
    inc qword [connections_handled]
    
    jmp .accept_loop

.accept_done:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

proxy_handle_client_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov edi, r12
    mov ebx, edi
    
    call socket_set_nonblocking_asm
    
    inc dword [client_count]
    
    mov eax, [proxy_mode]
    cmp eax, 1
    je .create_upstream
    jmp .handle_client_data

.create_upstream:
    lea rdi, [upstream_host]
    mov esi, [upstream_port]
    call socket_create_tcp_client_asm
    
    cmp eax, 0
    jl .handle_client_error
    mov r13, eax
    jmp .handle_client_data

.handle_client_data:
    mov edi, ebx
    mov esi, r13
    call proxy_forward_data_asm

.handle_client_error:
    mov edi, ebx
    call socket_close_asm
    
    test r13, r13
    jz .client_done
    mov edi, r13
    call socket_close_asm

.client_done:
    dec dword [client_count]
    
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

proxy_forward_data_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov edi, r12
    mov esi, r13
    
.forward_loop:
    cmp edi, 0
    jl .forward_done
    cmp esi, 0
    jl .forward_done
    
    mov edi, r12
    lea rsi, [forward_buffer]
    mov edx, 32768
    lea rcx, [rsp - 32]
    lea rdx, [rsp - 16]
    call socket_receive_packet_asm
    
    cmp eax, 0
    jl .forward_client_error
    mov ebx, eax

    test esi, esi
    jz .skip_client_to_upstream
    mov edi, esi
    lea rsi, [forward_buffer]
    mov edx, ebx
    xor ecx, ecx
    call socket_send_packet_asm
    
    add qword [bytes_forwarded], rbx
    jmp .check_upstream

.skip_client_to_upstream:
    mov edi, esi
    lea rsi, [forward_buffer]
    mov edx, 32768
    lea rcx, [rsp - 32]
    lea rdx, [rsp - 16]
    call socket_receive_packet_asm
    
    cmp eax, 0
    jl .forward_upstream_error
    mov ebx, eax
    
    mov edi, r12
    lea rsi, [forward_buffer]
    mov edx, ebx
    xor ecx, ecx
    call socket_send_packet_asm
    
    add qword [bytes_forwarded], rbx
    jmp .forward_loop

.check_upstream:
    jmp .forward_loop

.forward_client_error:
    cmp eax, -11
    je .check_upstream
    jmp .forward_done

.forward_upstream_error:
    cmp eax, -11
    je .forward_loop
    jmp .forward_done

.forward_done:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
proxy_cleanup_asm:
    push rbp
    mov rbp, rsp
    
    call proxy_stop_asm
    
    mov dword [proxy_server_fd], 0
    mov dword [proxy_mode], 0
    mov word [upstream_port], 0
    mov word [listen_port], 0
    mov dword [client_count], 0
    mov qword [bytes_forwarded], 0
    mov qword [connections_handled], 0
    
    mov byte [upstream_host], 0
    
    xor eax, eax
    pop rbp
    ret

strcpy_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov rdi, r12
    mov rsi, r13

strcpy_loop:
    mov al, [r13]
    mov [r12], al
    test al, al
    jz .strcpy_done
    inc r12
    inc r13
    jmp .strcpy_loop

strcpy_done:
    pop r12
    pop rbx
    pop rbp
    ret

extern socket_create_tcp_server_asm
extern socket_create_tcp_client_asm
extern socket_accept_asm
extern socket_close_asm
extern socket_set_nonblocking_asm
extern socket_send_packet_asm
extern socket_receive_packet_asm
