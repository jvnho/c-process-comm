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

size_t m_message_len(MESSAGE *message){
    return message->file->len_max;
}
size_t m_capacite(MESSAGE *message){
    return message->file->max_byte;
}

//renvoie la quantité d'octets utilisée par la file de messages
size_t m_nb(MESSAGE *message){
    int first = message->file->first;
    int last = message->file->last;
    if(first == -1) return 0;
    if(first == last) return last;
    if(first < last) return last - first;
    return m_capacite(message) - first + last;
}

int m_creat_file(FILE_MSG **file, int fd, int flags, size_t taille_file, size_t nb_msg, size_t len_max){
    *file = mmap(NULL, taille_file, PROT_READ|PROT_WRITE, flags, fd, 0);
    if((void *) (*file) == MAP_FAILED)
        return 0;
    memset(*file, 0, taille_file);
    if(initialiser_mutex(&(*file)->mutex) != 0
       || initialiser_cond(&(*file)->rcond) != 0
       || initialiser_cond(&(*file)->wcond) != 0)
        return 0;
    (*file)->len_max = len_max;
    (*file)->max_byte = (sizeof(mon_message) + sizeof(char) * len_max) * nb_msg;
    (*file)->first = -1;
    (*file)->destruction = 0;
    return 1;
}

MESSAGE *m_connexion(const char *nom, int options,.../*, size_t nb_msg, size_t len_max, mode_t mode*/){
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
        size_t taille_file = sizeof(FILE_MSG) + ((sizeof(mon_message) + sizeof(char) * len_max) * nb_msg);
        if(!m_creat_file(&file, -1, MAP_ANONYMOUS | MAP_SHARED, taille_file, nb_msg, len_max))
            return NULL;
        options = O_RDWR;
    } else {
        int flags = O_RDWR;
        if(O_CREAT & options){
            flags |= O_CREAT;
            if(O_EXCL & options){
                flags |= O_EXCL;
            }
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
            if((fd = shm_open(nom, flags, mode)) == -1){
                return NULL;
            }
            size_t taille_file = sizeof(FILE_MSG) + ((sizeof(mon_message) + sizeof(char) * len_max) * nb_msg);
            if(ftruncate(fd, taille_file) == -1)
                return NULL;
            if(!m_creat_file(&file, fd, MAP_SHARED, taille_file, nb_msg, len_max))
                return NULL;
        } else {
            //connexion à une file existante
            if((fd = shm_open(nom, flags, 0000)) == -1)
                return NULL;
            struct stat statbuf;
            if(fstat(fd, &statbuf) == -1)
                return NULL;
            file = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
            if((void *) file == MAP_FAILED)
                return NULL;
        }
    }
    MESSAGE *m = malloc(sizeof(MESSAGE));
    if(m == NULL) return NULL;
    memset(m, 0, sizeof(MESSAGE));
    close(fd);
    m->flags = options;
    m->file = file;
    if (pthread_mutex_lock(&file->mutex) > 0) return NULL;
    if(file->destruction){
        //un processus a fait une demande de destruction de la file on annule la connexion
        free(file);
    } else {
        file->connecte++;
    }
    if (pthread_mutex_unlock(&file->mutex) > 0) return NULL;
    return m;
}

int m_deconnexion(MESSAGE *file){
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    file->file->connecte--;
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    free(file);
    return 0;
}

int m_destruction(const char *nom){
    int fd = shm_open(nom, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) return -1;
    struct stat statbuf;
    if(fstat(fd, &statbuf) == -1) return -1;
    FILE_MSG *file = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(file == MAP_FAILED) return -1;
    if (close(fd) == -1) return -1;
    if (pthread_mutex_lock(&file->mutex) > 0) return -1;
    file->destruction = 1;
    while (file->connecte){
        if( pthread_cond_wait( &file->rcond, &file->mutex) > 0 ) return -1;
    }
    if (pthread_mutex_unlock(&file->mutex) > 0) return -1;
    if (shm_unlink(nom) == -1) return -1;
    return 0;
}

//fonction qui renvoie le nombre d'octets libre dans la file
int getfreespace(MESSAGE *file)
{
    if(file->file->first == -1) return m_capacite(file);
    return m_capacite(file) - m_nb(file);
}

int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag){
    if (!(file->flags & (O_RDONLY | O_RDWR))){
        errno = EACCES;
        return -1;
    }
    if (len > file->file->len_max){
        errno = EMSGSIZE;
        return -1;
    }
    int *first = &file->file->first;
    int *last = &file->file->last;
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    if (*first == -1) *first = 0;
    if (getfreespace(file) < sizeof(mon_message) + len){ // on regarde si la file a assez de place pour stocker le message
        if (msgflag !=  0){
            pthread_mutex_unlock(&file->file->mutex);
            if (msgflag == O_NONBLOCK) errno = EAGAIN;
            return -1;
        }
        else{
            while (getfreespace(file) < sizeof(mon_message) + len){
                if( pthread_cond_wait( &file->file->wcond, &file->file->mutex) > 0 ) return -1;
            }
        }
    }
    int l = *last;
    *last = (*last + sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[l];
    if(*first < *last) {
        memcpy(mes_addr, msg, sizeof(mon_message) + sizeof(char) * len);
    } else { //le message doit être coupé en deux
        int split = m_capacite(file)-l;
        memcpy(mes_addr, msg, split); // écriture à droite de la file
        memcpy(&msgs, msg+split, sizeof(mon_message) + sizeof(char) * len - split); // écriture à gauche de la file
    }
    pthread_cond_broadcast(&file->file->rcond);
    return 0;
}

size_t m_lecture(MESSAGE *file, void *msg, int addr){
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[addr];
    mon_message *cast = (mon_message*) mes_addr;
    size_t len = cast->length;
    int overflow = (addr + sizeof(mon_message) + sizeof(char) * len) % m_capacite(file); 
    if(overflow > addr){
        memcpy(msg, cast->mtext, len);
    } else {
        int split = m_capacite(file) - addr;
        memcpy(msg, mes_addr, split);
        memcpy(msg+split,&msgs[0], len-addr);
    }
    return len;
}

int shiftblock(MESSAGE *file, int addr){
    char *msgs = (char *)&file->file[1];
    int *first = &file->file->first;
    int *last = &file->file->last;
    char tmp[m_capacite(file)];
    memcpy(tmp, msgs, m_capacite(file)); // fais une copie de la file de messages courante
    int i = addr;
    int j = addr;
    int k = addr;
    size_t total = 0;
    while(k != *last){
        mon_message dst;
        memcpy(&dst, tmp+k,sizeof(mon_message));
        size_t len = dst.length;
        if(k == addr){
            //premiere iteration, on récupère la taille du bloc qu'on veut suppriimer
            total = sizeof(mon_message) + len;
        }
        mon_message src;
        j = (k + sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
        memcpy(&src, tmp+j,sizeof(mon_message));
        size_t src_len = src.length;
        memcpy(&msgs[i], tmp+j, sizeof(mon_message) * src_len);
        i = (i + sizeof(mon_message) + sizeof(char) * src_len) % m_capacite(file);
        k = (k + sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
    }
    *last = *last - total;
    return 0;
}

int m_addr(MESSAGE *file, long type){
    int *first = &file->file->first;
    int *last = &file->file->last;
    if(*first == *last) return -1;
    if(m_nb(file) == 0) return -1;
    char *msgs = (char *)&file->file[1];
    if(type == 0) return *first;
    int i = *first;
    while(i != *last){
        mon_message *msg_addr = (mon_message*) &msgs[i];
        long msg_type = msg_addr->type;
        size_t len = msg_addr->length;
        if((type > 0 && msg_type == type) || (type < 0 && abs(type) >= msg_type))
            return i;
        i = (i + sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
    }
    return -1;
}

ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags){
    if(!(file->flags & (O_RDONLY | O_RDWR))){
        errno = EACCES;
        return -1;
    }
    if(len < m_message_len(file)){
        errno = EMSGSIZE;
        return -1;
    }
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    int addr = 0; // adresse/offset du message de la file
    if( (addr = m_addr(file,type)) == -1){
        if(flags == O_NONBLOCK){
            if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
            errno = EAGAIN;
            return -1;
        } else if(flags == 0){
            while ((addr = m_addr(file,type)) == -1){
                if( pthread_cond_wait( &file->file->rcond, &file->file->mutex) > 0 ) return -1;
            }
        }
        else return -1;
    }
    int *first = &file->file->first;
    int n = m_lecture(file, msg, addr); // copie du message dans msg
    if(addr == *first){
        *first = (*first + sizeof(mon_message) + sizeof(char) * n) %m_capacite(file);
    } else {
        shiftblock(file,addr);
    }
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    pthread_cond_broadcast(&file->file->wcond);
    return n;
}