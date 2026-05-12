CC := gcc
CFLAGS := -Wall

main: main.o phonebook.o
	$(CC) $(CFLAGS) main.o phonebook.o -o main

main.o: main.c phonebook.h
	$(CC) $(CFLAGS) -c main.c -o main.o

phonebook.o: phonebook.c phonebook.h
	$(CC) $(CFLAGS) -c phonebook.c -o phonebook.o

clean:
	rm -f phonebook.o main.o main test.pb
