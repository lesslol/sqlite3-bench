# Makefile

CC = cl.exe -nologo
AS = ml.exe
LD = link.exe -nologo
AR = link.exe -lib -nologo
RC = rc.exe

CFLAGS  = -TP -DNDEBUG -D_TIME32_T_DEFINED -MD -W3 -O2 -GS- -GF -GL -arch:SSE2 -I.
WFLAGS  =
ASFLAGS = -coff
LDFLAGS = -nodefaultlib -incremental:no -manifest:no -opt:ref,icf -ltcg:status -machine:x86\
	-subsystem:console,6.0 sqlite3.lib msvcrt.lib oldnames.lib kernel32.lib
ARFLAGS = -nologo -ltcg -machine:x86
RCFLAGS = /dWIN32 /r

OBJS = random.obj util.obj histogram.obj benchmark.obj main.obj

# targets
all: bench.exe

bench.exe: $(OBJS)
	$(LD) $(LDFLAGS) -out:$@ $(OBJS)
	if exist $@.manifest mt -nologo -manifest $@.manifest -outputresource:$@;2

{.}.c.obj:
	$(CC) -c $(CFLAGS) $<

# cleanup
clean:
	-del *.obj *.pdb
