#include <pthread.h>

#ifndef M_FILE
#define M_FILE

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t rcond;
    size_t len_max; // taille maximale des messages de la file
    size_t nb_msg; // nombre de messages que la file peut stocker
    int first;
    int last;
    int connecte; // nombre de processus connecté à la file
    char **msg; // file de messages
} FILE_MSG;

typedef struct {
    int flags;
    int fd;
    FILE_MSG *file;
} MESSAGE;

typedef struct{
    long type;
    char mtext[];
} mon_message;

int m_deconnexion(MESSAGE *file);
MESSAGE *m_connexion(const char *nom, int options, const char *format,.../*, size_t nb_msg, size_t len_max, mode_t mode*/);
int m_envoie(MESSAGE *file, const void *msg, size_t len, int msgflag);

#endif