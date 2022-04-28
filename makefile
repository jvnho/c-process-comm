CC=gcc
CFLAGS=-Wall -g -pedantic
LDFLAGS=-pthread -lrt

all:clean main ecrivain lecteur ext2

main: m_file.o
	$(CC) $(CFLAGS) main.c -o main m_file.o $(LDFLAGS)

ecrivain: m_file.o
	$(CC) $(CFLAGS) ecrivain.c -o ecrivain m_file.o $(LDFLAGS)

lecteur: m_file.o
	$(CC) $(CFLAGS) lecteur.c -o lecteur m_file.o $(LDFLAGS)

ext2: m_file.o
	$(CC) $(CFLAGS) ext2.c -o ext2 m_file.o $(LDFLAGS)

m_file.o:
	gcc -c m_file.c

clean:
	rm -rf *.o