CC = gcc
CFLAGS = -std=gnu99 -Wall -fsanitize=address,undefined
LDFLAGS = -fsanitize=address,undefined
LDLIBS = -lpthread -lm

TARGET = sop-bitronka
SRC = sop-bitronka.c

.PHONY: all clean

# Domyślny cel - kompilacja programu
all: $(TARGET)

# Reguła budowania pliku wykonywalnego
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

# Cel czyszczący - usuwa plik wykonywalny i pliki obiektowe (jeśli powstaną)
clean:
	rm -f $(TARGET) *.o