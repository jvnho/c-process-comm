CC=gcc
CFLAGS=-Wall -g -pedantic
LDFLAGS=-pthread -lrt

all:clean test_creation test_envoi test_reception test_anon ext2

test_creation: m_file.o
	$(CC) $(CFLAGS) test_creation.c -o test_creation m_file.o $(LDFLAGS)

test_envoi: m_file.o
	$(CC) $(CFLAGS) test_envoi.c -o test_envoi m_file.o $(LDFLAGS)

test_reception: m_file.o
	$(CC) $(CFLAGS) test_reception.c -o test_reception m_file.o $(LDFLAGS)

test_anon: m_file.o
	$(CC) $(CFLAGS) test_anon.c -o test_anon m_file.o $(LDFLAGS)

ext2: m_file.o
	$(CC) $(CFLAGS) ext2.c -o ext2 m_file.o $(LDFLAGS)

m_file.o:
	gcc -c m_file.c

clean:
	rm -rf *.o