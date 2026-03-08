CC       ?= gcc
CFLAGS    = -std=c11 -Wall -Wextra -Wpedantic \
            -Wformat=2 -Wformat-security -Wshadow -Wstrict-prototypes \
            -Wdouble-promotion -Wnull-dereference \
            -O2 -fstack-protector-strong -fPIE \
            -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE \
            -Iinclude
LDFLAGS   = -pie -Wl,-z,relro,-z,now,-z,noexecstack

TARGET    = narchstab

SRCS = src/main.c    \
       src/proc.c    \
       src/cfg.c     \
       src/log.c     \
       src/notify.c  \
       src/oom.c     \
       src/dmesg.c   \
       src/report.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

DESTDIR   ?=
PREFIX    ?= /usr/local
BINDIR     = $(DESTDIR)$(PREFIX)/bin
CONFDIR    = $(DESTDIR)/etc
STATEDIR   = $(DESTDIR)/var/lib/narchstab
LOGDIR     = $(DESTDIR)/var/log

install: $(TARGET)
	install -Dm755 $(TARGET)              $(BINDIR)/$(TARGET)
	install -Dm644 etc/narchstab.conf     $(CONFDIR)/narchstab.conf
	install -d -m750                      $(STATEDIR)
	touch $(LOGDIR)/narchstab.log
	chmod 640 $(LOGDIR)/narchstab.log

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)

-include $(OBJS:.o=.d)
