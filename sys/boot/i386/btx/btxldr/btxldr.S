/*
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Copyright (c) 1998 Robert Nordier
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
 * $FreeBSD: src/sys/boot/i386/btx/btxldr/Makefile,v 1.17 2004/04/27 19:45:16 ru Exp $
 * $DragonFly: src/sys/boot/i386/btx/btxldr/Attic/btxldr.S,v 1.3 2004/07/19 23:30:34 dillon Exp $
 */

/*
 * Prototype BTX loader program, written in a couple of hours.  The
 * real thing should probably be more flexible, and in C.
 */

#include "../../bootasm.h"
/*
 * Memory locations.
 */
		.set MEM_DATA,start+0x1000	# Data segment
/*
 * Segment selectors.
 */
		.set SEL_SCODE,0x8		# 4GB code
		.set SEL_SDATA,0x10		# 4GB data
		.set SEL_RCODE,0x18		# 64K code
		.set SEL_RDATA,0x20		# 64K data
/*
 * Paging constants.
 */
		.set PAG_SIZ,0x1000		# Page size
		.set PAG_ENT,0x4		# Page entry size
/*
 * Screen constants.
 */
		.set SCR_MAT,0x7		# Mode/attribute
		.set SCR_COL,0x50		# Columns per row
		.set SCR_ROW,0x19		# Rows per screen
/*
 * Required by aout gas inadequacy.
 */
		.set SIZ_STUB,0x1a		# Size of stub
/*
 * We expect to be loaded by boot2 at the origin defined in ./Makefile.
 * This is typically 0x200000.
 *
 * I *THINK* (not sure) that execution begins with us in 'virtual mode',
 * meaning everything is offset by MEM_BTX_USR.  We will load a gdt to
 * set the base offsets back to 0.
 */
		.globl start
/*
 * BTX program loader for ELF clients.
 */
start:		cld				# String ops inc
		movl $m_logo,%esi		# Identify
		call putstr			#  ourselves
#if !defined(MEM_BTX_USR_STK)
		movzwl BDA_MEM,%eax		# Get base memory
		shll $0xa,%eax			# Convert to bytes
#else
		movl $MEM_BTX_USR_STK,%eax
#endif
		movl %eax,%ebp			# Base of user stack
#ifdef BTXLDR_VERBOSE
		movl $m_mem,%esi		# Display
		call hexout			#  amount of
		call putstr			#  base memory
#endif

		/*
		 * Load a new GDT.  XXX what does this do to running code
		 * segments?  What if an interrupt occurs?  What if the 
		 * segment registers are reloaded?
		 */
		lgdt gdtdesc

		/*
		 * Relocate caller's arguments.
		 */
#ifdef BTXLDR_VERBOSE
		movl $m_esp,%esi		# Display
		movl %esp,%eax			#  caller
		call hexout			#  stack
		call putstr			#  pointer
		movl $m_args,%esi		# Format string
		leal 0x4(%esp,1),%ebx		# First argument
		movl $0x6,%ecx			# Count
start.1:	movl (%ebx),%eax		# Get argument and
		addl $0x4,%ebx			#  bump pointer
		call hexout			# Display it
		loop start.1			# Till done
		call putstr			# End message
#endif
		/*
		 * Arguments: (entry, boothowto, bootdev, 0, 0, 0, bootinfo)
		 *		0x00,  0x04,      0x08,             0x18
		 *
		 * sizeof(bootinfo) == 0x48 (BOOTINFO_SIZE)
		 * sizeof arguments == 0x18 (MEM_ARG_SIZE)
		 * total arguments  == 0x60 bytes (USR_ARGOFFSET)
		 */

		movl $BOOTINFO_SIZE,%ecx 	# Allocate space
		subl %ecx,%ebp			#  for bootinfo
		movl 0x18(%esp,1),%esi		# Source: bootinfo
		cmpl $0x0, %esi			# If the bootinfo pointer
		je start_null_bi		#  is null, do not copy it
		movl %ebp,%edi			# Destination
		rep				# Copy
		movsb				#  it
		movl %ebp,0x18(%esp,1)		# Update pointer
#ifdef BTXLDR_VERBOSE
		movl $m_rel_bi,%esi		# Display
		movl %ebp,%eax			#  bootinfo
		call hexout			#  relocation
		call putstr			#  message
#endif
start_null_bi:	movl $0x18,%ecx 		# Allocate space
		subl %ecx,%ebp			#  for arguments
		leal 0x4(%esp,1),%esi		# Source
		movl %ebp,%edi			# Destination
		rep				# Copy
		movsb				#  them
#ifdef BTXLDR_VERBOSE
		movl $m_rel_args,%esi		# Display
		movl %ebp,%eax			#  argument
		call hexout			#  relocation
		call putstr			#  message
#endif
/*
 * Set up BTX kernel.
 */
		movl $MEM_BTX_ESP,%esp		# Set up new stack
		movl $MEM_DATA,%ebx		# Data segment
		movl $m_vers,%esi		# Display BTX
		call putstr			#  version message
		movb 0x5(%ebx),%al		# Get major version
		addb $'0',%al			# Display
		call putchr			#  it
		movb $'.',%al			# And a
		call putchr			#  dot
		movb 0x6(%ebx),%al		# Get minor
		xorb %ah,%ah			#  version
		movb $0xa,%dl			# Divide
		divb %dl,%al			#  by 10
		addb $'0',%al			# Display
		call putchr			#  tens
		movb %ah,%al			# Get units
		addb $'0',%al			# Display
		call putchr			#  units
		call putstr			# End message

		# Relocate the BTX image from wherever it was loaded (%ebx),
		# which is typically offset 0x1000 in the load data, to
		# MEM_BTX_ORG (typically 0x9000).
		#
		# MEM_BTX_TBL + ((mappages | 0x3ff) + 1) * 4
		# mappages is typically 0x0ffn so we get 0x1000*4 = 0x4000
		# MEM_BTX_TBL is traditionally mapped at 0x5000 so the
		# whole calculation translated to MEM_BTX_ORG (0x9000).
#if 0
		/* XXX what is all of this junk? */
		movzwl 0x8(%ebx),%edi		# Compute the BTX load address
		orl $PAG_SIZ/PAG_ENT-1,%edi	# (by skipping the page table)
		incl %edi
		shll $0x2,%edi
		addl $MEM_BTX_TBL,%edi
#else
		movl $MEM_BTX_ORG,%edi
#endif
		movl %ebx,%esi			# %esi = BTX image source
		pushl %edi			# Save load address
		movzwl 0xa(%ebx),%ecx		# Image size (bytes)
#ifdef BTXLDR_VERBOSE
		pushl %ecx			# Save image size
#endif
		rep				# Relocate BTX
		movsb
		movl %esi,%ebx			# Keep place
#ifdef BTXLDR_VERBOSE
		movl $m_rel_btx,%esi		# Restore
		popl %eax			#  parameters
		call hexout			#  and
#endif
		popl %ebp			#  display
#ifdef BTXLDR_VERBOSE
		movl %ebp,%eax			#  the
		call hexout			#  relocation
		call putstr			#  message
#endif
		/*
		 * ADJUST EBP FOR USER BASE ADDRESS
		 *
		 * XXX why not just move MEM_BTX_USR into %ebp ?
		 */
		addl $MEM_BTX_USR-MEM_BTX_ORG,%ebp
#ifdef BTXLDR_VERBOSE
		movl $m_base,%esi		#  the
		movl %ebp,%eax			#  user
		call hexout			#  base
		call putstr			#  address
#endif
/*
 * Set up ELF-format client program.
 */
		cmpl $0x464c457f,(%ebx) 	# ELF magic number?
		je start.3			# Yes
		movl $e_fmt,%esi		# Display error
		call putstr			#  message
start.2:	jmp start.2			# Hang
start.3:
#ifdef BTXLDR_VERBOSE
		movl $m_elf,%esi		# Display ELF
		call putstr			#  message
		movl $m_segs,%esi		# Format string
#endif
		movl $0x2,%edi			# Segment count
		movl 0x1c(%ebx),%edx		# Get e_phoff
		addl %ebx,%edx			# To pointer
		movzwl 0x2c(%ebx),%ecx		# Get e_phnum
start.4:	cmpl $0x1,(%edx)		# Is p_type PT_LOAD?
		jne start.6			# No
#ifdef BTXLDR_VERBOSE
		movl 0x4(%edx),%eax		# Display
		call hexout			#  p_offset
		movl 0x8(%edx),%eax		# Display
		call hexout			#  p_vaddr
		movl 0x10(%edx),%eax		# Display
		call hexout			#  p_filesz
		movl 0x14(%edx),%eax		# Display
		call hexout			#  p_memsz
		call putstr			# End message
#endif
		pushl %esi			# Save
		pushl %edi			#  working
		pushl %ecx			#  registers
		movl 0x4(%edx),%esi		# Get p_offset
		addl %ebx,%esi			#  as pointer
		movl 0x8(%edx),%edi		# Get p_vaddr
		addl %ebp,%edi			#  as pointer
		movl 0x10(%edx),%ecx		# Get p_filesz
		rep				# Set up
		movsb				#  segment
		movl 0x14(%edx),%ecx		# Any bytes
		subl 0x10(%edx),%ecx		#  to zero?
		jz start.5			# No
		xorb %al,%al			# Then
		rep				#  zero
		stosb				#  them
start.5:	popl %ecx			# Restore
		popl %edi			#  working
		popl %esi			#  registers
		decl %edi			# Segments to do
		je start.7			# If none
start.6:	addl $0x20,%edx 		# To next entry
		loop start.4			# Till done
start.7:
#ifdef BTXLDR_VERBOSE
		movl $m_done,%esi		# Display done
		call putstr			#  message
#endif
		movl $start.8,%esi		# Real mode stub
		movl $BOOT0_ORIGIN,%edi		# Destination
		movl $start.9-start.8,%ecx	# Size
		rep				# Relocate
		movsb				#  it
		ljmp $SEL_RCODE,$BOOT0_ORIGIN	# To 16-bit code
		.code16
start.8:	xorw %ax,%ax			# Data
		movb $SEL_RDATA,%al		#  selector
		movw %ax,%ss			# Reload SS
		movw %ax,%ds			# Reset
		movw %ax,%es			#  other
		movw %ax,%fs			#  segment
		movw %ax,%gs			#  limits
		movl %cr0,%eax			# Switch to
		decw %ax			#  real
		movl %eax,%cr0			#  mode
		ljmp $0,$MEM_BTX_ENTRY		# Jump to BTX entry point
start.9:
		.code32
/*
 * Output message [ESI] followed by EAX in hex.
 */
hexout: 	pushl %eax			# Save
		call putstr			# Display message
		popl %eax			# Restore
		pushl %esi			# Save
		pushl %edi			# callers
		movl $buf,%edi			# Buffer
		pushl %edi			# Save
		call hex32			# To hex
		xorb %al,%al			# Terminate
		stosb				#  string
		popl %esi			# Restore
hexout.1:	lodsb				# Get a char
		cmpb $'0',%al			# Leading zero?
		je hexout.1			# Yes
		testb %al,%al			# End of string?
		jne hexout.2			# No
		decl %esi			# Undo
hexout.2:	decl %esi			# Adjust for inc
		call putstr			# Display hex
		popl %edi			# Restore
		popl %esi			# callers
		ret				# To caller
/*
 * Output zero-terminated string [ESI] to the console.
 */
putstr.0:	call putchr			# Output char
putstr: 	lodsb				# Load char
		testb %al,%al			# End of string?
		jne putstr.0			# No
		ret				# To caller
/*
 * Output character AL to the console.
 */
putchr: 	pusha				# Save
		xorl %ecx,%ecx			# Zero for loops
		movb $SCR_MAT,%ah		# Mode/attribute
		movl $BDA_POS,%ebx		# BDA pointer
		movw (%ebx),%dx 		# Cursor position
		movl $0xb8000,%edi		# Regen buffer (color)
		cmpb %ah,BDA_SCR-BDA_POS(%ebx)	# Mono mode?
		jne putchr.1			# No
		xorw %di,%di			# Regen buffer (mono)
putchr.1:	cmpb $0xa,%al			# New line?
		je putchr.2			# Yes
		xchgl %eax,%ecx 		# Save char
		movb $SCR_COL,%al		# Columns per row
		mulb %dh			#  * row position
		addb %dl,%al			#  + column
		adcb $0x0,%ah			#  position
		shll %eax			#  * 2
		xchgl %eax,%ecx 		# Swap char, offset
		movw %ax,(%edi,%ecx,1)		# Write attr:char
		incl %edx			# Bump cursor
		cmpb $SCR_COL,%dl		# Beyond row?
		jb putchr.3			# No
putchr.2:	xorb %dl,%dl			# Zero column
		incb %dh			# Bump row
putchr.3:	cmpb $SCR_ROW,%dh		# Beyond screen?
		jb putchr.4			# No
		leal 2*SCR_COL(%edi),%esi	# New top line
		movw $(SCR_ROW-1)*SCR_COL/2,%cx # Words to move
		rep				# Scroll
		movsl				#  screen
		movb $' ',%al			# Space
		movb $SCR_COL,%cl		# Columns to clear
		rep				# Clear
		stosw				#  line
		movb $SCR_ROW-1,%dh		# Bottom line
putchr.4:	movw %dx,(%ebx) 		# Update position
		popa				# Restore
		ret				# To caller
/*
 * Convert EAX, AX, or AL to hex, saving the result to [EDI].
 */
hex32:		pushl %eax			# Save
		shrl $0x10,%eax 		# Do upper
		call hex16			#  16
		popl %eax			# Restore
hex16:		call hex16.1			# Do upper 8
hex16.1:	xchgb %ah,%al			# Save/restore
hex8:		pushl %eax			# Save
		shrb $0x4,%al			# Do upper
		call hex8.1			#  4
		popl %eax			# Restore
hex8.1: 	andb $0xf,%al			# Get lower 4
		cmpb $0xa,%al			# Convert
		sbbb $0x69,%al			#  to hex
		das				#  digit
		orb $0x20,%al			# To lower case
		stosb				# Save char
		ret				# (Recursive)

		.data
		.p2align 4
/*
 * Global descriptor table.
 */
gdt:		.word 0x0,0x0,0x0,0x0		# Null entry
		.word 0xffff,0x0,0x9a00,0xcf	# SEL_SCODE
		.word 0xffff,0x0,0x9200,0xcf	# SEL_SDATA
		.word 0xffff,0x0,0x9a00,0x0	# SEL_RCODE
		.word 0xffff,0x0,0x9200,0x0	# SEL_RDATA
gdt.1:
gdtdesc:	.word gdt.1-gdt-1		# Limit
		.long gdt			# Base
/*
 * Messages.
 */
m_logo: 	.asciz " \nBTX loader 1.00  "
m_vers: 	.asciz "BTX version is \0\n"
e_fmt:		.asciz "Error: Client format not supported\n"
#ifdef BTXLDR_VERBOSE
m_mem:		.asciz "Starting in protected mode (base mem=\0)\n"
m_esp:		.asciz "Arguments passed (esp=\0):\n"
m_args: 	.asciz"<howto="
		.asciz" bootdev="
		.asciz" junk="
		.asciz" "
		.asciz" "
		.asciz" bootinfo=\0>\n"
m_rel_bi:	.asciz "Relocated bootinfo (size=48) to \0\n"
m_rel_args:	.asciz "Relocated arguments (size=18) to \0\n"
m_rel_btx:	.asciz "Relocated kernel (size=\0) to \0\n"
m_base: 	.asciz "Client base address is \0\n"
m_elf:		.asciz "Client format is ELF\n"
m_segs: 	.asciz "text segment: offset="
		.asciz " vaddr="
		.asciz " filesz="
		.asciz " memsz=\0\n"
		.asciz "data segment: offset="
		.asciz " vaddr="
		.asciz " filesz="
		.asciz " memsz=\0\n"
m_done: 	.asciz "Loading complete\n"
#endif
/*
 * Uninitialized data area.
 */
buf:						# Scratch buffer
