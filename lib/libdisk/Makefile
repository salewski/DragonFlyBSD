# $FreeBSD: src/lib/libdisk/Makefile,v 1.27.2.7 2002/07/19 18:46:26 ru Exp $
# $DragonFly: src/lib/libdisk/Attic/Makefile,v 1.3 2005/03/13 15:10:03 swildner Exp $

LIB=	disk
SRCS=	blocks.c disklabel.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c
INCS=	libdisk.h

# Remove KERN_DISKS_BROKEN when kern.disks sysctl returns disks in sorted order
CFLAGS+= 	-Wall
CLEANFILES+=	tmp.c tst01 tst01.o
NOPROFILE=	yes
NOPIC=		yes

MAN=		libdisk.3
MLINKS+=	libdisk.3 Open_Disk.3 \
		libdisk.3 Clone_Disk.3 \
		libdisk.3 Free_Disk.3 \
		libdisk.3 Debug_Disk.3 \
		libdisk.3 Set_Bios_Geom.3 \
		libdisk.3 Delete_Chunk.3 \
		libdisk.3 Collapse_Disk.3 \
		libdisk.3 Collapse_Chunk.3 \
		libdisk.3 Create_Chunk.3 \
		libdisk.3 All_FreeBSD.3 \
		libdisk.3 CheckRules.3 \
		libdisk.3 Disk_Names.3 \
		libdisk.3 Set_Boot_Mgr.3 \
		libdisk.3 Set_Boot_Blocks.3 \
		libdisk.3 Write_Disk.3 \
		libdisk.3 Cyl_Aligned.3 \
		libdisk.3 Next_Cyl_Aligned.3 \
		libdisk.3 Prev_Cyl_Aligned.3 \
		libdisk.3 Track_Aligned.3 \
		libdisk.3 Next_Track_Aligned.3 \
		libdisk.3 Prev_Track_Aligned.3 \
		libdisk.3 Create_Chunk_DWIM.3 \
		libdisk.3 MakeDev.3 \
		libdisk.3 MakeDevDisk.3 \
		libdisk.3 ShowChunkFlags.3 \
		libdisk.3 ChunkCanBeRoot.3 \
		libdisk.3 slice_type_name.3

.include <bsd.lib.mk>

tst01: tst01.o libdisk.a
	cc ${CFLAGS} -static tst01.o -o tst01 libdisk.a
