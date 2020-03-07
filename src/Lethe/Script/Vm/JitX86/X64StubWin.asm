IFDEF RAX

PUBLIC VmJitX64_Stub

.code

VmJitX64_Stub PROC
	; assume arg in rcx
	; offsets:
	;   stktop 0
	;   stkadr 1
	;   codeadr 2
	;   thisadr 3
	;   dataadr 4
	;   codebase 5
	;
	;
	; save xmm6, xmm7 because of win
	sub rsp, 16
	movups [rsp], xmm6
	sub rsp, 16
	movups [rsp], xmm7

	push rbx
	push rsi
	push rdi
	push rbp
	push r12
	push r13
	push r14

	mov rdi, [rcx]
	mov rsi, [rcx + 4*8]
	mov r12, [rcx + 1*8]
	mov r13, [rcx + 5*8]
	mov rax, [rcx + 2*8]
	mov rbp, [rcx + 3*8]

	call rax

	mov [r12], rdi

	pop r14
	pop r13
	pop r12
	pop rbp
	pop rdi
	pop rsi
	pop rbx

	movups xmm7, [rsp]
	add rsp, 16
	movups xmm6, [rsp]
	add rsp, 16
	ret
VmJitX64_Stub ENDP

ENDIF

END
