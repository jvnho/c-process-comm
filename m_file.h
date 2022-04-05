#include <pthread.h>

#ifndef M_FILE
#define M_FILE

typedef struct {
    int flags;
    int fd;
    FILE_MSG *file;
} MESSAGE;

typedef struct
{
    pthread_mutex_t mutex;
    size_t len_max; // taille maximale des messages de la file
    size_t nb_msg; // nombre de messages que la file peut stocker
    int first;
    int last;
    int connecte; // nombre de processus connecté à la file
    char **msg; // file de messages
} FILE_MSG;

int m_deconnexion(MESSAGE *file);
MESSAGE *m_connexion(const char *nom, int options, const char *format,.../*, size_t nb_msg, size_t len_max, mode_t mode*/);
#endif