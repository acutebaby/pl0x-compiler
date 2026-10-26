CC      = gcc
CFLAGS  = -O2 -Wall -std=c99
TARGET  = pl0x

all: $(TARGET)

$(TARGET): pl0x.c pl0x.h
	$(CC) $(CFLAGS) -o $(TARGET) pl0x.c -lm

clean:
	rm -f $(TARGET) *.tmp

run: $(TARGET)
	./$(TARGET) cases/t1.pl0
