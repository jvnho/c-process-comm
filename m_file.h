#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>

#ifndef M_FILE
#define M_FILE

typedef struct{
    long type;
    char mtext[];
} mon_message;

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t rcond;
    pthread_cond_t wcond;
    size_t len_max; // taille maximale des messages de la file
    size_t nb_msg; // nombre de messages que la file peut stocker
    int first;
    int last;
    int connecte; // nombre de processus connecté à la file
    mon_message *msgs; //file de messages
} FILE_MSG;

typedef struct {
    int flags;
    int fd;
    FILE_MSG *file;
} MESSAGE;

int m_deconnexion(MESSAGE *file);
MESSAGE *m_connexion(const char *nom, int options,.../*, size_t nb_msg, size_t len_max, mode_t mode*/);
int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag);

#endif