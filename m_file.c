#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "m_file.h"

int initialiser_mutex(pthread_mutex_t *pmutex){
    pthread_mutexattr_t mutexattr;
    int code;
    code = pthread_mutexattr_init(&mutexattr);
    if(code != 0)
        return code;
    code = pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    if(code != 0)
        return code;
    code = pthread_mutex_init(pmutex, &mutexattr);
    return code;
}

/**
 * @brief Initialise un tableaux de pointeurs de pointeurs
 * @param nb_msg taille du tableau
 * @param len_msg taille maximale des chaînes de caractères du tableau
 * @return char**
 */
char **initialize_array(size_t nb_msg, size_t len_msg)
{
    char **ret = malloc(sizeof(char*) * nb_msg);
    if(ret == NULL) return NULL;
    for(size_t i = 0; i < nb_msg; i++)
    {
        ret[i] = malloc(sizeof(char) * len_msg);
        if(ret[i] == NULL) return NULL;
        memset(ret[i], 0, sizeof(char) * len_msg);
    }
    return ret;
}

/**
 * @brief Renvoie un pointeur vers un object MESSAGE vers une file de message existante
 * @param nom nom de la file de messages
 * @param options flags
 * @return MESSAGE*
*/
MESSAGE *m_connexion(const char *nom, int options)
{
    if(O_CREAT & options == 1)
    {
        //l'utilisateur a donné 2 arguments mais avec le flag O_CREAT
        return NULL;
    }
    int fd = shm_open(nom, options, 0000);
    if(fd == -1) return NULL;
    struct stat statbuf;
    if(fstat(fd, &statbuf) == -1) return NULL;

    FILE_MSG *file = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if((void *) file == MAP_FAILED) return NULL;

    MESSAGE *m = malloc(sizeof(MESSAGE));
    if(m == NULL) return NULL;
    memset(m, 0, sizeof(MESSAGE));
    m->fd = fd;
    m->flags = options;
    m->file = file;
    return m;
}

/**
 * @brief Crée une nouvelle file de message et renvoie un pointeur
 * vers un object MESSAGE vers cette file
 * @param nom nom de la file de messages
 * @param options flags
 * @param nb_msg est la capacité de la file
 * @param len_max longueur maximale d'un message
 * @param mode permissions pour la nouvelle file de messages
 * @return MESSAGE*
*/
MESSAGE *m_connexion(const char *nom, int options, size_t nb_msg, size_t len_max, mode_t mode)
{
    if(O_CREAT & options == 0)
    {
        //l'utilisateur a donné 5 arguments mais sangs le flag O_CREAT
        return NULL;
    }
    int fd = shm_open(nom, options, mode);
    if(fd == -1) return NULL;
    size_t taille_file = sizeof(FILE_MSG) + (sizeof(char) * nb_msg * len_max);
    if(ftruncate(nom, taille_file == -1)) return NULL;

    FILE_MSG *file = mmap(NULL, taille_file, PROT_READ|PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if((void *) file == MAP_FAILED) return NULL;
    memset(file, 0, taille_file);
    if(initialiser_mutex(&file->mutex) != 0) return NULL;
    file->len_max = len_max;
    file->nb_msg = nb_msg;
    file->first = -1;
    file->msg = initialize_array(nb_msg, len_max);

    MESSAGE *message = malloc(sizeof(MESSAGE));
    if(message == NULL) return NULL;
    memset(message, 0, sizeof(MESSAGE));
    message->flags = options;
    message->fd = fd;
    message->file = file;
    return message;
}

int m_deconnexion(MESSAGE *file){
    if (close(*(file -> file)) == -1) return -1 ;
    free(file);
    return 0;
}