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

int initialiser_cond(pthread_cond_t *pcond){
  pthread_condattr_t condattr;
  int code;
  if( ( code = pthread_condattr_init(&condattr) ) != 0 )
    return code;
  if( ( code = pthread_condattr_setpshared(&condattr,PTHREAD_PROCESS_SHARED) ) != 0 )
    return code;
  code = pthread_cond_init(pcond, &condattr) ;
  return code;
}

size_t m_message_len(MESSAGE *message)
{
    return message->file->len_max;
}
size_t m_capacite(MESSAGE *message)
{
    return message->file->nb_msg;
}
size_t m_nb(MESSAGE *message)
{
    int first = message->file->first;
    int last = message->file->last;
    int capacite = m_capacite(message);
    if(first == -1) return 0;
    if(first == last) return capacite;
    if(first < last) return last - first + 1; //first,first+1,...,last-1
    return capacite - first - last + 1; //first,first+1,...,n-1,0,1,...,last-1
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
MESSAGE *m_connexion(const char *nom, int options,.../*, size_t nb_msg, size_t len_max, mode_t mode*/)
{
    int fd; // descripteur du fichier de mémoire partagée
    FILE_MSG *file; // file de messages
    char fd_addrname[strlen(nom)+strlen("_addr")];
    strcpy(fd_addrname, nom);
    strcat(fd_addrname,"_addr");
    if(O_CREAT & options)
    {
        //cas où l'utilisateur veut créer et se connecter à une nouvelle file
        size_t nb_msg;
        size_t len_max;
        mode_t mode;
        va_list args;
        va_start(args, options);
        for(int i = 0; i < 3; i++)
        {
            if(i == 0) nb_msg = va_arg(args, size_t);
            if(i == 1) len_max = va_arg(args, size_t);
            if(i == 2) mode = va_arg(args, mode_t);
        }
        va_end(args);
        if(O_EXCL & options)
        {
            // cas où l'utilisateur a spécifié le flag O_EXCL on doit vérifier que le fichier n'existait pas
            struct stat statbuf;
            char path[strlen("/dev/shm")+strlen(nom)]; 
            strcpy(path, "/dev/shm");
            strcat(path, nom);
            if(stat(path, &statbuf) == 0) return NULL;
        }
        fd = shm_open(nom, options, mode);
        if(fd == -1){
            return NULL;
        }
        size_t taille_file = sizeof(FILE_MSG) + ((sizeof(long) + sizeof(char) * len_max) * nb_msg);
        if(ftruncate(fd, taille_file) == -1){
            return NULL;
        }
        int fd_addr = shm_open(fd_addrname, options, mode);
        if(fd_addr == -1){
            return NULL;
        }
        if(ftruncate(fd_addr, sizeof(FILE_MSG*)) == -1){
            return NULL;
        }
        file = mmap(NULL, taille_file, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if((void *) file == MAP_FAILED){
            return NULL;
        }
        FILE_MSG **addr = mmap(NULL, sizeof(FILE_MSG*), PROT_READ|PROT_WRITE, MAP_SHARED, fd_addr, 0);
        if((void **) addr == MAP_FAILED){
            return NULL;
        }
        *addr = file;
        memset(file, 0, taille_file);
        if(initialiser_mutex(&file->mutex) != 0){
            return NULL;
        }
        if(initialiser_cond(&file->rcond) != 0){
            return NULL;
        }
        file->len_max = len_max;
        file->nb_msg = nb_msg;
        file->first = -1;
    }
    else
    {
        //cas où l'utilisateur veut se connecter à une file existante
        fd = shm_open(nom, options, 0000);
        if(fd == -1){
            return NULL;
        }
        int fd_addr = shm_open(fd_addrname, O_RDONLY, 0000);
        if(fd_addr == -1){
            return NULL;
        }
        FILE_MSG **addr = mmap(NULL, sizeof(FILE_MSG*), PROT_READ|PROT_WRITE, MAP_PRIVATE, fd_addr, 0);
        if((void **) addr == MAP_FAILED){
            return NULL;
        }
        struct stat statbuf;
        if(fstat(fd, &statbuf) == -1){
            return NULL;
        }
        file = mmap(*addr, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
        if((void *) file == MAP_FAILED){
            return NULL;
        }
    }
    MESSAGE *m = malloc(sizeof(MESSAGE));
    if(m == NULL) return NULL;
    memset(m, 0, sizeof(MESSAGE));
    m->fd = fd;
    m->flags = options;
    m->file = file;
    return m;
}

/**
 * @brief Déconnecte la file de message.
 * @param file file de message à déconnecter.
 * @return 0 si 0, -1 en cas d'erreur.
 */
int m_deconnexion(MESSAGE *file){
    if (close(file ->fd) == -1) return -1;
    file->file->connecte--;
    free(file);
    return 0;
}

int m_envoie(MESSAGE *file, const void *msg, size_t len, int msgflag){
    if (len > file->file->len_max){
        errno = EMSGSIZE;
        return -1;
    }
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    int *first = &file->file->first;
    int *last = &file->file->last;
    if (*first == *last){ // SI C'EST PLEIN
        if (msgflag == O_NONBLOCK){
            pthread_mutex_unlock(&file->file->mutex);
            errno = EAGAIN;
            return -1;
        }
        if (msgflag == 0){
            while (*first = *last){
                if( pthread_cond_wait( &file->file->rcond, &file->file->mutex) > 0 ) return -1;
            }
        }
        else return -1;
    }
    memcpy(&file->file->messages[*last], msg, len+sizeof(long));
    *last = *last == file->file->nb_msg ? 0 : (*last)++;
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    return 0;
}


ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags)
{
    if(len < m_message_len(file))
    {
        errno = EMSGSIZE;
        return -1;
    }
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    if(m_nb(file) == 0)
    {
        if(flags == O_NONBLOCK)
        {
            pthread_mutex_unlock(&file->file->mutex);
            errno = EAGAIN;
            return -1;
        }
        else if(flags == 0)
        {
            while (m_nb(file) == 0){
                if( pthread_cond_wait( &file->file->rcond, &file->file->mutex) > 0 ) return -1;
            }
        }
        else return -1;
    }
    int *first = &file->file->first;
    int *last = &file->file->last;
    int idxsrc = 0;
    if(type == 0)
    {
        idxsrc = *first;
        *first = *first == file->file->nb_msg ? 0 : (*first)++;
        if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    }
    else
    {  
        int i;
        int found = 0;
        for(i = *first; i != *last & found == 0; i = (i+1)%m_capacite(file))
        {
            if((type > 0 && file->file->messages[i].type == type) 
                || (type < 0 && file->file->messages[i].type <= abs(type)))
            {
                found = 1;
                idxsrc = i;
            }
            if(found == 1)
            {
                if(idxsrc != *first)
                {
                    //cas où il faut décaler la mémoire car on a pop un élément au milieu de la liste
                }
                *first = *first == file->file->nb_msg ? 0 : (*first)++;
                if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
            }
        }
    }
    memcpy(msg, file->file->messages[idxsrc].mtext, file->file->len_max);
    return strlen(msg);
}