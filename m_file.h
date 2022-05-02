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
    //size_t length; //longueur du message
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
    int destruction; // boolean qui dit si un processus demande la supression de la file
} FILE_MSG;

typedef struct {
    int flags;
    FILE_MSG *file;
} MESSAGE;

MESSAGE *m_connexion(const char *nom, int options,.../*, size_t nb_msg, size_t len_max, mode_t mode*/);
int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag);
ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags);
size_t m_message_len(MESSAGE *message);
size_t m_capacite(MESSAGE *message);
size_t m_nb(MESSAGE *message);
int m_deconnexion(MESSAGE *file);
int m_destruction(const char *nom);

#endif