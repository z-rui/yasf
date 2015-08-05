CC=gcc
CFLAGS=-Og -g -Wall

LEDC=bin/ledc

CFLAGS+=-I.

OBJS=main.o led.o stub.o cb.o db.o dialogs.o util.o

ifneq "$(findstring w4, $(TEC_UNAME))" ""
 ifneq "$(findstring 64, $(TEC_UNAME))" ""
  RCFLAGS=-DTEC_64
  MANIFEST=src/iup64.manifest
 else
  MANIFEST=src/iup.manifest
endif

OBJS+=rc.o
endif

all: yasf

led.c: src/yasf.led
	$(LEDC) $<

yasf: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) \
		-liup -liupcontrols -liupcd \
		-liup_scintilla \
		-lsqlite3

main.o: src/main.c src/yasf.h
	$(CC) $(CFLAGS) -c -o $@ $<

stub.o: src/stub.c regcb.c
	$(CC) $(CFLAGS) -c -o $@ $<

led.o: led.c
	$(CC) $(CFLAGS) -c -o $@ $<

cb.o: src/cb.c src/yasf.h
	$(CC) $(CFLAGS) -c -o $@ $<

db.o: src/db.c pragmas.c src/yasf.h
	$(CC) $(CFLAGS) -c -o $@ $<

dialogs.o: src/dialogs.c src/yasf.h
	$(CC) $(CFLAGS) -c -o $@ $<

util.o: src/util.c
	$(CC) $(CFLAGS) -c -o $@ $<

pragmas.c: src/pragmas.lua
	lua $<>$@

regcb.c regcb.h: src/regcb.lua src/db.c src/cb.c src/dialogs.c pragmas.c
	lua $< src/db.c src/cb.c src/dialogs.c pragmas.c

rc.o: src/iup.rc $(MANIFEST)
	windres $(RCFLAGS) $< -o $@

clean:
	rm -f *.o led.c pragmas.c regcb.c regcb.h
