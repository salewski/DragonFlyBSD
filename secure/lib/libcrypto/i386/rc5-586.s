	# $FreeBSD: src/secure/lib/libcrypto/i386/rc5-586.s,v 1.1.4.1 2003/02/14 22:38:15 nectar Exp $
	# $DragonFly: src/secure/lib/libcrypto/i386/Attic/rc5-586.s,v 1.2 2003/06/17 04:27:48 dillon Exp $
	# Dont even think of reading this code 
	# It was automatically generated by rc5-586.pl 
	# Which is a perl program used to generate the x86 assember for 
	# any of elf, a.out, BSDI, Win32, gaswin (for GNU as on Win32) or Solaris 
	# eric <eay@cryptsoft.com> 

	.file	"rc5-586.s"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16
.globl RC5_32_encrypt
	.type	RC5_32_encrypt,@function
RC5_32_encrypt:

	pushl	%ebp
	pushl	%esi
	pushl	%edi
	movl	16(%esp),	%edx
	movl	20(%esp),	%ebp
	# Load the 2 words 
	movl	(%edx),		%edi
	movl	4(%edx),	%esi
	pushl	%ebx
	movl	(%ebp),		%ebx
	addl	4(%ebp),	%edi
	addl	8(%ebp),	%esi
	xorl	%esi,		%edi
	movl	12(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	16(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	20(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	24(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	28(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	32(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	36(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	40(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	44(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	48(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	52(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	56(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	60(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	64(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	68(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	72(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	cmpl	$8,		%ebx
	je	.L000rc5_exit
	xorl	%esi,		%edi
	movl	76(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	80(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	84(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	88(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	92(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	96(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	100(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	104(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	cmpl	$12,		%ebx
	je	.L000rc5_exit
	xorl	%esi,		%edi
	movl	108(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	112(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	116(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	120(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	124(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	128(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
	xorl	%esi,		%edi
	movl	132(%ebp),	%eax
	movl	%esi,		%ecx
	roll	%cl,		%edi
	addl	%eax,		%edi
	xorl	%edi,		%esi
	movl	136(%ebp),	%eax
	movl	%edi,		%ecx
	roll	%cl,		%esi
	addl	%eax,		%esi
.L000rc5_exit:
	movl	%edi,		(%edx)
	movl	%esi,		4(%edx)
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret
.L_RC5_32_encrypt_end:
	.size	RC5_32_encrypt,.L_RC5_32_encrypt_end-RC5_32_encrypt
.ident	"desasm.pl"
.text
	.align 16
.globl RC5_32_decrypt
	.type	RC5_32_decrypt,@function
RC5_32_decrypt:

	pushl	%ebp
	pushl	%esi
	pushl	%edi
	movl	16(%esp),	%edx
	movl	20(%esp),	%ebp
	# Load the 2 words 
	movl	(%edx),		%edi
	movl	4(%edx),	%esi
	pushl	%ebx
	movl	(%ebp),		%ebx
	cmpl	$12,		%ebx
	je	.L001rc5_dec_12
	cmpl	$8,		%ebx
	je	.L002rc5_dec_8
	movl	136(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	132(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	128(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	124(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	120(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	116(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	112(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	108(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
.L001rc5_dec_12:
	movl	104(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	100(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	96(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	92(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	88(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	84(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	80(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	76(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
.L002rc5_dec_8:
	movl	72(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	68(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	64(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	60(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	56(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	52(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	48(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	44(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	40(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	36(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	32(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	28(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	24(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	20(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	movl	16(%ebp),	%eax
	subl	%eax,		%esi
	movl	%edi,		%ecx
	rorl	%cl,		%esi
	xorl	%edi,		%esi
	movl	12(%ebp),	%eax
	subl	%eax,		%edi
	movl	%esi,		%ecx
	rorl	%cl,		%edi
	xorl	%esi,		%edi
	subl	8(%ebp),	%esi
	subl	4(%ebp),	%edi
.L003rc5_exit:
	movl	%edi,		(%edx)
	movl	%esi,		4(%edx)
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret
.L_RC5_32_decrypt_end:
	.size	RC5_32_decrypt,.L_RC5_32_decrypt_end-RC5_32_decrypt
.ident	"desasm.pl"
.text
	.align 16
.globl RC5_32_cbc_encrypt
	.type	RC5_32_cbc_encrypt,@function
RC5_32_cbc_encrypt:

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	28(%esp),	%ebp
	# getting iv ptr from parameter 4 
	movl	36(%esp),	%ebx
	movl	(%ebx),		%esi
	movl	4(%ebx),	%edi
	pushl	%edi
	pushl	%esi
	pushl	%edi
	pushl	%esi
	movl	%esp,		%ebx
	movl	36(%esp),	%esi
	movl	40(%esp),	%edi
	# getting encrypt flag from parameter 5 
	movl	56(%esp),	%ecx
	# get and push parameter 3 
	movl	48(%esp),	%eax
	pushl	%eax
	pushl	%ebx
	cmpl	$0,		%ecx
	jz	.L004decrypt
	andl	$4294967288,	%ebp
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
	jz	.L005encrypt_finish
.L006encrypt_loop:
	movl	(%esi),		%ecx
	movl	4(%esi),	%edx
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	RC5_32_encrypt
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L006encrypt_loop
.L005encrypt_finish:
	movl	52(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L007finish
	xorl	%ecx,		%ecx
	xorl	%edx,		%edx
	movl	.L008cbc_enc_jmp_table(,%ebp,4),%ebp
	jmp	*%ebp
.L009ej7:
	movb	6(%esi),	%dh
	sall	$8,		%edx
.L010ej6:
	movb	5(%esi),	%dh
.L011ej5:
	movb	4(%esi),	%dl
.L012ej4:
	movl	(%esi),		%ecx
	jmp	.L013ejend
.L014ej3:
	movb	2(%esi),	%ch
	sall	$8,		%ecx
.L015ej2:
	movb	1(%esi),	%ch
.L016ej1:
	movb	(%esi),		%cl
.L013ejend:
	xorl	%ecx,		%eax
	xorl	%edx,		%ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	RC5_32_encrypt
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
	movl	%eax,		(%edi)
	movl	%ebx,		4(%edi)
	jmp	.L007finish
.align 16
.L004decrypt:
	andl	$4294967288,	%ebp
	movl	16(%esp),	%eax
	movl	20(%esp),	%ebx
	jz	.L017decrypt_finish
.L018decrypt_loop:
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	RC5_32_decrypt
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
	movl	16(%esp),	%ecx
	movl	20(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%ecx,		(%edi)
	movl	%edx,		4(%edi)
	movl	%eax,		16(%esp)
	movl	%ebx,		20(%esp)
	addl	$8,		%esi
	addl	$8,		%edi
	subl	$8,		%ebp
	jnz	.L018decrypt_loop
.L017decrypt_finish:
	movl	52(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L007finish
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
	movl	%eax,		8(%esp)
	movl	%ebx,		12(%esp)
	call	RC5_32_decrypt
	movl	8(%esp),	%eax
	movl	12(%esp),	%ebx
	movl	16(%esp),	%ecx
	movl	20(%esp),	%edx
	xorl	%eax,		%ecx
	xorl	%ebx,		%edx
	movl	(%esi),		%eax
	movl	4(%esi),	%ebx
.L019dj7:
	rorl	$16,		%edx
	movb	%dl,		6(%edi)
	shrl	$16,		%edx
.L020dj6:
	movb	%dh,		5(%edi)
.L021dj5:
	movb	%dl,		4(%edi)
.L022dj4:
	movl	%ecx,		(%edi)
	jmp	.L023djend
.L024dj3:
	rorl	$16,		%ecx
	movb	%cl,		2(%edi)
	sall	$16,		%ecx
.L025dj2:
	movb	%ch,		1(%esi)
.L026dj1:
	movb	%cl,		(%esi)
.L023djend:
	jmp	.L007finish
.align 16
.L007finish:
	movl	60(%esp),	%ecx
	addl	$24,		%esp
	movl	%eax,		(%ecx)
	movl	%ebx,		4(%ecx)
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.align 16
.L008cbc_enc_jmp_table:
	.long 0
	.long .L016ej1
	.long .L015ej2
	.long .L014ej3
	.long .L012ej4
	.long .L011ej5
	.long .L010ej6
	.long .L009ej7
.align 16
.L027cbc_dec_jmp_table:
	.long 0
	.long .L026dj1
	.long .L025dj2
	.long .L024dj3
	.long .L022dj4
	.long .L021dj5
	.long .L020dj6
	.long .L019dj7
.L_RC5_32_cbc_encrypt_end:
	.size	RC5_32_cbc_encrypt,.L_RC5_32_cbc_encrypt_end-RC5_32_cbc_encrypt
.ident	"desasm.pl"
