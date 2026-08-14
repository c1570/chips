# Makefile for the m6522 viavarious tests (ported from
# vice-testprogs/drive/viavarious/)

CC = cc
CFLAGS = -std=c11 -Wall -Wextra
INCLUDES = -I../..

TARGET = m6522_viavarious_test
SOURCES = m6522_viavarious_test.c
HEADERS = viavarious.h m6522_via1ref.h m6522_via2ref.h m6522_via3ref.h \
          m6522_via3aref.h m6522_via4ref.h m6522_via5ref.h m6522_via9ref.h \
          m6522_via10ref.h m6522_via11ref.h m6522_via12ref.h \
          m6522_via13ref.h m6522_via14ref.h m6522_via20ref.h \
          m6522_via21ref.h

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES)

# runs the tests; make fails if any sub test fails (nonzero exit code)
test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
