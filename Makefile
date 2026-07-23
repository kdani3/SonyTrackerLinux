CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
LDFLAGS = -lm -lpthread
TARGET  = sony-tracker
SRC     = main.c
PREFIX  = /usr/local

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC) hidraw.h quat.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -Dm644 71-sony-head-tracker.rules $(DESTDIR)/etc/udev/rules.d/71-sony-head-tracker.rules
	@echo ""
	@echo "Installed"
	@echo ""
	@echo "Run command: sudo udevadm control --reload-rules && sudo udevadm trigger"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)/etc/udev/rules.d/71-sony-head-tracker.rules

clean:
	rm -f $(TARGET)
