CC=gcc
CFLAGS=-Wall -g -pedantic
LDFLAGS=-pthread -lrt

all:clean main 

main: m_file.o
	$(CC) $(CFLAGS) main.c -o main m_file.o $(LDFLAGS)

m_file.o:
	gcc -c m_file.c

clean:
	rm -rf *.o