PROG=	acpiexec
SRCS=	aeexec.c aemain.c \
	../../common/getopt.c \
	../../debugger/dbcmds.c \
	../../debugger/dbdisply.c \
	../../debugger/dbexec.c \
	../../debugger/dbfileio.c \
	../../debugger/dbhistry.c \
	../../debugger/dbinput.c \
	../../debugger/dbstats.c \
	../../debugger/dbutils.c \
	../../debugger/dbxface.c \
	../../disassembler/dmbuffer.c \
	../../disassembler/dmnames.c \
	../../disassembler/dmobject.c \
	../../disassembler/dmopcode.c \
	../../disassembler/dmresrc.c \
	../../disassembler/dmresrcl.c \
	../../disassembler/dmresrcs.c \
	../../disassembler/dmutils.c \
	../../disassembler/dmwalk.c \
	../../events/evevent.c \
	../../events/evgpeblk.c \
	../../events/evgpe.c \
	../../events/evmisc.c \
	../../events/evregion.c \
	../../events/evrgnini.c \
	../../events/evsci.c \
	../../events/evxface.c \
	../../events/evxfevnt.c \
	../../events/evxfregn.c \
	../../hardware/hwacpi.c \
	../../hardware/hwgpe.c \
	../../hardware/hwregs.c \
	../../hardware/hwsleep.c \
	../../interpreter/dispatcher/dsfield.c \
	../../interpreter/dispatcher/dsinit.c \
	../../interpreter/dispatcher/dsmethod.c \
	../../interpreter/dispatcher/dsmthdat.c \
	../../interpreter/dispatcher/dsobject.c \
	../../interpreter/dispatcher/dsopcode.c \
	../../interpreter/dispatcher/dsutils.c \
	../../interpreter/dispatcher/dswexec.c \
	../../interpreter/dispatcher/dswload.c \
	../../interpreter/dispatcher/dswscope.c \
	../../interpreter/dispatcher/dswstate.c \
	../../interpreter/executer/exconfig.c \
	../../interpreter/executer/exconvrt.c \
	../../interpreter/executer/excreate.c \
	../../interpreter/executer/exdump.c \
	../../interpreter/executer/exfield.c \
	../../interpreter/executer/exfldio.c \
	../../interpreter/executer/exmisc.c \
	../../interpreter/executer/exmutex.c \
	../../interpreter/executer/exnames.c \
	../../interpreter/executer/exoparg1.c \
	../../interpreter/executer/exoparg2.c \
	../../interpreter/executer/exoparg3.c \
	../../interpreter/executer/exoparg6.c \
	../../interpreter/executer/exprep.c \
	../../interpreter/executer/exregion.c \
	../../interpreter/executer/exresnte.c \
	../../interpreter/executer/exresolv.c \
	../../interpreter/executer/exresop.c \
	../../interpreter/executer/exstore.c \
	../../interpreter/executer/exstoren.c \
	../../interpreter/executer/exstorob.c \
	../../interpreter/executer/exsystem.c \
	../../interpreter/executer/exutils.c \
	../../interpreter/parser/psargs.c \
	../../interpreter/parser/psopcode.c \
	../../interpreter/parser/psparse.c \
	../../interpreter/parser/psscope.c \
	../../interpreter/parser/pstree.c \
	../../interpreter/parser/psutils.c \
	../../interpreter/parser/pswalk.c \
	../../interpreter/parser/psxface.c \
	../../namespace/nsaccess.c \
	../../namespace/nsalloc.c \
	../../namespace/nsdump.c \
	../../namespace/nsdumpdv.c \
	../../namespace/nseval.c \
	../../namespace/nsinit.c \
	../../namespace/nsload.c \
	../../namespace/nsnames.c \
	../../namespace/nsobject.c \
	../../namespace/nsparse.c \
	../../namespace/nssearch.c \
	../../namespace/nsutils.c \
	../../namespace/nswalk.c \
	../../namespace/nsxfeval.c \
	../../namespace/nsxfname.c \
	../../namespace/nsxfobj.c \
	../../resources/rsaddr.c \
	../../resources/rscalc.c \
	../../resources/rscreate.c \
	../../resources/rsdump.c \
	../../resources/rsio.c \
	../../resources/rsirq.c \
	../../resources/rslist.c \
	../../resources/rsmemory.c \
	../../resources/rsmisc.c \
	../../resources/rsutils.c \
	../../resources/rsxface.c \
	../../tables/tbconvrt.c \
	../../tables/tbgetall.c \
	../../tables/tbget.c \
	../../tables/tbinstal.c \
	../../tables/tbrsdt.c \
	../../tables/tbutils.c \
	../../tables/tbxface.c \
	../../tables/tbxfroot.c \
	../../utilities/utalloc.c \
	../../utilities/utcopy.c \
	../../utilities/utdebug.c \
	../../utilities/utdelete.c \
	../../utilities/uteval.c \
	../../utilities/utglobal.c \
	../../utilities/utinit.c \
	../../utilities/utmath.c \
	../../utilities/utmisc.c \
	../../utilities/utobject.c \
	../../utilities/utxface.c \
	../../osunixxf.c


CFLAGS+= -Wall -g -D_LINUX -DNDEBUG -D_CONSOLE -D_ACPI_EXEC_APP -D_MULTI_THREADED -Wstrict-prototypes -I../../include 


acpiexec : $(patsubst %.c,%.o, $(SRCS))
	$(CC) $(LDFLAGS) $(patsubst %.c,%.o, $(SRCS)) -o $(PROG)

CLEANFILES= $(PROG)

clean : 
	rm -f $(CLEANFILES) $(patsubst %.c,%.o, $(SRCS))

