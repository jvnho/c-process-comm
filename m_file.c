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
    return capacite - first + last + 1; //first,first+1,...,n-1,0,1,...,last-1
}

int m_init_file(FILE_MSG **file, int fd, int flags, size_t taille_file, size_t nb_msg, size_t len_max)
{
    *file = mmap(NULL, taille_file, PROT_READ|PROT_WRITE, flags, fd, 0);
    if((void *) (*file) == MAP_FAILED){
        return 0;
    }
    memset(*file, 0, taille_file);
    if(initialiser_mutex(&(*file)->mutex) != 0){
        return 0;
    }
    if(initialiser_cond(&(*file)->rcond) != 0){
        return 0;
    }
    if(initialiser_cond(&(*file)->wcond) != 0){
        return 0;
    }
    (*file)->len_max = len_max;
    (*file)->nb_msg = nb_msg;
    (*file)->first = -1;
    return 1;
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
    if(nom == NULL) {
        //file anonyme
        size_t nb_msg;
        size_t len_max;
        va_list args;
        va_start(args, options);
        for(int i = 0; i < 2; i++){
            if(i == 0) nb_msg = va_arg(args, size_t);
            if(i == 1) len_max = va_arg(args, size_t);
        }
        va_end(args);
        size_t taille_file = sizeof(FILE_MSG) + ((sizeof(long) + sizeof(char) * len_max) * nb_msg);
        int flags = MAP_ANONYMOUS | MAP_SHARED;
        if(!m_init_file(&file, -1, flags, taille_file, nb_msg, len_max))
            return NULL;
        options = O_RDWR;
    } else {
        //on vérifie si le fichier temporaire existe
        //on adaptera selon les flags fournis
        char path[strlen("/dev/shm")+strlen(nom)];
        strcpy(path, "/dev/shm");
        strcat(path, nom);
        struct stat statbuf;
        int fileexist = (stat(path, &statbuf) == 0);
        if(fileexist == 1 && !(O_EXCL & options)){
            //file existe et pas de flags O_EXCL
            //connexion à une file existante
            fd = shm_open(nom, options, 0000);
            if(fd == -1)
                return NULL;
            struct stat statbuf;
            if(fstat(fd, &statbuf) == -1)
                return NULL;
            file = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
            if((void *) file == MAP_FAILED)
                return NULL;
        } else if(O_CREAT & options){
            //création et connexion à une nouvelle file
            size_t nb_msg;
            size_t len_max;
            mode_t mode;
            va_list args;
            va_start(args, options);
            for(int i = 0; i < 3; i++){
                if(i == 0) nb_msg = va_arg(args, size_t);
                if(i == 1) len_max = va_arg(args, size_t);
                if(i == 2) mode = va_arg(args, mode_t);
            }
            va_end(args);
            fd = shm_open(nom, options, mode);
            if(fd == -1){
                return NULL;
            }
            //création d'une nouvelle file
            size_t taille_file = sizeof(FILE_MSG) + ((sizeof(long) + sizeof(char) * len_max) * nb_msg);
            if(ftruncate(fd, taille_file) == -1)
                return NULL;
            int flags = MAP_SHARED;
            if(!m_init_file(&file, fd, flags, taille_file, nb_msg, len_max))
                return NULL;
        }
        else return NULL;
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

int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag){
    if (len > file->file->len_max){
        errno = EMSGSIZE;
        return -1;
    }
    int *first = &file->file->first;
    int *last = &file->file->last;
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    int l = *last;
    *last = *last == file->file->nb_msg-1 ? 0 : (*last) + 1;
    if (*first == -1) (*first)++;
    if (*first == *last){ // SI C'EST PLEIN
        if (msgflag !=  0){
            pthread_mutex_unlock(&file->file->mutex);
            if (msgflag == O_NONBLOCK) errno = EAGAIN;
            return -1;
        }
        else{
            while (*first == *last){
                if( pthread_cond_wait( &file->file->wcond, &file->file->mutex) > 0 ) return -1;
            }
        }
    }
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[(sizeof(mon_message)+file->file->len_max )*l];
    memset(mes_addr, 0, sizeof(long) + file->file->len_max);
    memcpy(mes_addr, msg, len+sizeof(long));
    pthread_cond_signal(&file->file->rcond);
    printf("%d %d\n",*first, *last);
    return 0;
}

size_t m_readmsg(MESSAGE *file, void *msg, int idx)
{
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[(sizeof(mon_message)+file->file->len_max)*idx];
    memcpy(msg, mes_addr+sizeof(long), file->file->len_max);
    memset(mes_addr, 0, file->file->len_max+sizeof(long)); // supprime le message de la file
    return strlen(msg);
}

int shiftblock(MESSAGE *file, int idx)
{
    int first = file->file->first;
    char *msgs = (char *)&file->file[1];
    size_t block_size = sizeof(mon_message) + file->file->len_max;
    int capacite = m_capacite(file);
    for(int i = idx; i != first; i = (i-1)%capacite)
    {
        char *addr_dst = &msgs[block_size*i];
        char *addr_src = &msgs[block_size*((i-1)%capacite)];
        memmove(addr_dst, addr_src, block_size);
    }
    return 1;
}

ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags)
{
    if(len < m_message_len(file)){
        errno = EMSGSIZE;
        return -1;
    }
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    if(m_nb(file) == 0){
        if(flags == O_NONBLOCK){
            if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
            errno = EAGAIN;
            return -1;
        } else if(flags == 0){
            while (m_nb(file) == 0){
                if( pthread_cond_wait( &file->file->rcond, &file->file->mutex) > 0 ) return -1;
            }
        }
        else return -1;
    }
    int *first = &file->file->first;
    int *last = &file->file->last;
    int idxsrc = -1;
    if(type == 0){
        idxsrc = *first;
        *first = *first == file->file->nb_msg-1 ? 0 : (*first)+1;
        if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
        pthread_cond_signal(&file->file->wcond);
        return m_readmsg(file, msg, idxsrc);
    } else {
        int found = 0;
        for(int i = *first; i != *last && found == 0; i = (i+1)%m_capacite(file)){
            if((type > 0 && file->file->messages[i].type == type)
                || (type < 0 && file->file->messages[i].type <= abs(type)))
            {
                found = 1;
                idxsrc = i;
            }
        }
        if(found == 1){
            int n = m_readmsg(file, msg, idxsrc); // copie du message dans msg
            //cas où il faut décaler la mémoire car on a pop un élément au milieu de la liste
            shiftblock(file, idxsrc);
            *first = *first == file->file->nb_msg-1 ? 0 : (*first)+1;
            if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
            pthread_cond_signal(&file->file->wcond);
            return n;
        } else {
            if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
            return 0;
        }
    }
}