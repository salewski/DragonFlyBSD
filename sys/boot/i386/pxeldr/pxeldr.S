/*
 * Copyright (c) 2000 John Baldwin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 *
 *
 * $FreeBSD: src/sys/boot/i386/pxeldr/pxeldr.s,v 1.9 2003/09/03 08:12:20 phk Exp $
 * $DragonFly: src/sys/boot/i386/pxeldr/Attic/pxeldr.S,v 1.4 2004/07/18 23:40:09 dillon Exp $
 */

/*
 * This simple program is a preloader for the normal boot3 loader.  It is
 * simply prepended to the beginning of a fully built and btxld'd loader.
 * It then copies the loader to the address boot2 normally loads it,
 * emulates the boot[12] environment (protected mode, a bootinfo struct,
 * etc.), and then jumps to the start of btxldr to start the boot process.
 * This method allows a stock /boot/loader to be booted over the network
 * via PXE w/o having to write a separate PXE-aware client just to load
 * the loader.
 */
#include "../bootasm.h"

		/*
		 * a.out header fields
		 */
		.set AOUT_TEXT,0x04		# text segment size
		.set AOUT_DATA,0x08		# data segment size
		.set AOUT_BSS,0x0c		# zerod BSS size
		.set AOUT_SYMBOLS,0x10		# symbol table
		.set AOUT_ENTRY,0x14		# entry point
		.set AOUT_HEADER,MEM_PAGE_SIZE	# size of the a.out header

		/*
		 * Flags for kargs->bootflags
		 */
		.set KARGS_FLAGS_PXE,0x2	# flag to indicate booting from
						#  PXE loader
		/*
		 * Boot howto bits
		 */
		.set RB_SERIAL,0x1000		# serial console

		/*
		 * Segment selectors.
		 */
		.set SEL_SDATA,0x8		# Supervisor data
		.set SEL_RDATA,0x10		# Real mode data
		.set SEL_SCODE,0x18		# PM-32 code
		.set SEL_SCODE16,0x20		# PM-16 code

		/*
		 * BTX constants
		 */
		.set INT_SYS,0x30		# BTX syscall interrupt

		/*
		 * Bit in BDA_KEYBOARD that is set if an enhanced
		 * keyboard is present.
		 */
		.set KEYBOARD_BIT,0x10

		/*
		 * We expect to be loaded by the BIOS at LOAD (0x7c00),
		 * which is the standard boot loader entry point.
		 */
		.code16
		.globl start
		.org 0x0, 0x0

		/*
		 * BTX program loader for PXE network booting
		 */
start:		cld				# string ops inc
		xorw %ax, %ax			# zero %ax
		movw %ax, %ss			# setup the
		movw $start, %sp		#  stack
		movw %es, %cx			# save PXENV+ segment
		movw %ax, %ds			# setup the
		movw %ax, %es			#  data segments
		andl $0xffff, %ecx		# clear upper words
		andl $0xffff, %ebx		#  of %ebx and %ecx
		shll $4, %ecx			# calculate the offset of
		addl %ebx, %ecx			#  the PXENV+ struct and
		pushl %ecx			#  save it on the stack
		movw $welcome_msg, %si		# %ds:(%si) -> welcome message
		callw putstr			# display the welcome message

		/*
		 * Setup the arguments that the loader is expecting
		 * from boot[12]
		 */
		movw $bootinfo_msg, %si		# %ds:(%si) -> boot args message
		callw putstr			# display the message
		movw $MEM_ARG, %bx		# %ds:(%bx) -> boot args
		movw %bx, %di			# %es:(%di) -> boot args
		xorl %eax, %eax			# zero %eax
		movw $(MEM_ARG_SIZE/4), %cx	# Size of arguments in 32-bit
						#  dwords
		rep				# Clear the arguments
		stosl				#  to zero
		orb $KARGS_FLAGS_PXE, 0x8(%bx)	# kargs->bootflags |=
						#  KARGS_FLAGS_PXE
		popl 0xc(%bx)			# kargs->pxeinfo = *PXENV+
#ifdef ALWAYS_SERIAL
		/*
		 * set the RBX_SERIAL bit in the howto byte.
		 */
		orl $RB_SERIAL, (%bx)		# enable serial console
#endif
#ifdef PROBE_KEYBOARD
		/*
		 * Look at the BIOS data area to see if we have an enhanced
		 * keyboard.  If not, set the RBX_SERIAL bit in the howto 
		 * byte.
		 */
		testb $KEYBOARD_BIT, BDA_KEYBOARD # keyboard present?
		jnz keyb			# yes, so skip
		orl $RB_SERIAL, (%bx)		# enable serial console
keyb:
#endif
		/*
		 * Turn on the A20 address line
		 */
		callw seta20			# Turn A20 on

		/*
		 * Relocate the loader and BTX using a very lazy protected
		 * mode
		 */
		movw $relocate_msg, %si		# Display the
		callw putstr			#  relocation message
		movl end+AOUT_ENTRY, %edi	# %edi is the destination
		movl $(end+AOUT_HEADER), %esi	# %esi is
						#  the start of the text
						#  segment
		movl end+AOUT_TEXT, %ecx	# %ecx = length of the text
						#  segment
		lgdt gdtdesc			# setup our own gdt
		cli				# turn off interrupts
		movl %cr0, %eax			# Turn on
		orb $0x1, %al			#  protected
		movl %eax, %cr0			#  mode
		ljmp $SEL_SCODE,$pm_start	# long jump to clear the
						#  instruction pre-fetch queue
		.code32
pm_start:	movw $SEL_SDATA, %ax		# Initialize
		movw %ax, %ds			#  %ds and
		movw %ax, %es			#  %es to a flat selector
		rep				# Relocate the
		movsb				#  text segment
		addl $(MEM_PAGE_SIZE - 1), %edi	# pad %edi out to a new page
		andl $~(MEM_PAGE_SIZE - 1), %edi #  for the data segment
		movl end+AOUT_DATA, %ecx	# size of the data segment
		rep				# Relocate the
		movsb				#  data segment
		movl end+AOUT_BSS, %ecx		# size of the bss
		xorl %eax, %eax			# zero %eax
		addb $3, %cl			# round %ecx up to
		shrl $2, %ecx			#  a multiple of 4
		rep				# zero the
		stosl				#  bss
		movl end+AOUT_ENTRY, %esi	# %esi -> relocated loader
		addl $MEM_BTX_LDR_OFF, %esi	# %esi -> BTX in the loader
		movl $MEM_BTX_ORG, %edi	# %edi -> where BTX needs to go
		movzwl 0xa(%esi), %ecx		# %ecx -> length of BTX
		rep				# Relocate
		movsb				#  BTX
		ljmp $SEL_SCODE16,$pm_16	# Jump to 16-bit PM
		.code16
pm_16:		movw $SEL_RDATA, %ax		# Initialize
		movw %ax, %ds			#  %ds and
		movw %ax, %es			#  %es to a real mode selector
		movl %cr0, %eax			# Turn off
		andb $~0x1, %al			#  protected
		movl %eax, %cr0			#  mode
		ljmp $0,$pm_end			# Long jump to clear the
						#  instruction pre-fetch queue
pm_end:		sti				# Turn interrupts back on now

		/*
		 * Copy the BTX client to MEM_BTX_USR
		 */
		xorw %ax, %ax			# zero %ax and set
		movw %ax, %ds			#  %ds and %es
		movw %ax, %es			#  to segment 0
		movw $MEM_BTX_USR, %di		# Prepare to relocate
		movw $btx_client, %si		#  the simple btx client
		movw $(btx_client_end-btx_client), %cx # length of btx client
		rep				# Relocate the
		movsb				#  simple BTX client

		/*
		 * Copy the boot[12] args to where the BTX client can
		 * see them
		 */
		movw $MEM_ARG, %si		# where the args are at now
		movw $MEM_BTX_USR_ARG, %di		# where the args are moving to
		movw $(MEM_ARG_SIZE/4), %cx	# size of the arguments in longs
		rep				# Relocate
		movsl				#  the words

		/*
		 * Save the entry point so the client can get to it later on
		 */
		movl end+AOUT_ENTRY, %eax	# load the entry point
		stosl				# add it to the end of the
						#  arguments
		/*
		 * Now we just start up BTX and let it do the rest
		 */
		movw $jump_message, %si		# Display the
		callw putstr			#  jump message
		ljmp $0,$MEM_BTX_ENTRY		# Jump to the BTX entry point

		/*
		 * Display a null-terminated string
		 */
putstr:		lodsb				# load %al from %ds:(%si)
		testb %al,%al			# stop at null
		jnz putc			# if the char != null, output it
		retw				# return when null is hit
putc:		movw $0x7,%bx			# attribute for output
		movb $0xe,%ah			# BIOS: put_char
		int $0x10			# call BIOS, print char in %al
		jmp putstr			# keep looping

		/*
		 * Enable A20
		 */
seta20: 	cli				# Disable interrupts
seta20.1:	inb $0x64,%al			# Get status
		testb $0x2,%al			# Busy?
		jnz seta20.1			# Yes
		movb $0xd1,%al			# Command: Write
		outb %al,$0x64			#  output port
seta20.2:	inb $0x64,%al			# Get status
		testb $0x2,%al			# Busy?
		jnz seta20.2			# Yes
		movb $0xdf,%al			# Enable
		outb %al,$0x60			#  A20
		sti				# Enable interrupts
		retw				# To caller

		/*
		 * BTX client to start btxldr
		 */
		.code32
btx_client:	movl $(MEM_BTX_USR_ARG-MEM_BTX_USR+MEM_ARG_SIZE-4), %esi
						# %ds:(%esi) -> end
						#  of boot[12] args
		movl $(MEM_ARG_SIZE/4), %ecx	# Number of words to push
		std				# Go backwards
push_arg:	lodsl				# Read argument
		pushl %eax			# Push it onto the stack
		loop push_arg			# Push all of the arguments
		cld				# In case anyone depends on this
		pushl MEM_BTX_USR_ARG-MEM_BTX_USR+MEM_ARG_SIZE # Entry point of
						#  the loader
		pushl %eax			# Emulate a near call
		movl $0x1, %eax			# "exec" system call
		int $INT_SYS			# BTX system call
btx_client_end:
		.code16

		.p2align 4

		/*
		 * Global descriptor table.
		 */
gdt:		.word 0x0,0x0,0x0,0x0		# Null entry
		.word 0xffff,0x0,0x9200,0xcf	# SEL_SDATA
		.word 0xffff,0x0,0x9200,0x0	# SEL_RDATA
		.word 0xffff,0x0,0x9a00,0xcf	# SEL_SCODE (32-bit)
		.word 0xffff,0x0,0x9a00,0x8f	# SEL_SCODE16 (16-bit)
gdt.1:

		/*
		 * Pseudo-descriptors.
		 */
gdtdesc:	.word gdt.1-gdt-1		# Limit
		.long gdt			# Base

welcome_msg:	.asciz	"PXE Loader 1.00\r\n\n"
bootinfo_msg:	.asciz	"Building the boot loader arguments\r\n"
relocate_msg:	.asciz	"Relocating the loader and the BTX\r\n"
jump_message:	.asciz	"Starting the BTX loader\r\n"

		.p2align 4
end:
