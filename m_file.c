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
    return message->file->nb_msg;
}

size_t m_nb(MESSAGE *message){
    int first = message->file->first;
    int last = message->file->last;
    int capacite = m_capacite(message);
    if(first == -1) return 0;
    //on vérifie que le type de first n'est pas -1
    long type = 0;
    char *msgs = (char *)&message->file[1];
    memcpy(&type, &msgs[(sizeof(mon_message) + message->file->len_max) * first], sizeof(long));
    if(type == -1){
        printf("%ld\n", type);
        return 0;
    }
    if(first == last) return capacite;
    if(first < last) return last - first + 1; //first,first+1,...,last-1
    return capacite - first + last + 1; //first,first+1,...,n-1,0,1,...,last-1
}

int m_init_file(FILE_MSG **file, int fd, int flags, size_t taille_file, size_t nb_msg, size_t len_max){
    *file = mmap(NULL, taille_file, PROT_READ|PROT_WRITE, flags, fd, 0);
    if((void *) (*file) == MAP_FAILED)
        return 0;
    memset(*file, 0, taille_file);
    if(initialiser_mutex(&(*file)->mutex) != 0
       || initialiser_mutex(&(*file)->mutex_enregistrement) != 0
       || initialiser_cond(&(*file)->rcond) != 0
       || initialiser_cond(&(*file)->wcond) != 0
       || initialiser_cond(&(*file)->cond_er) != 0)
        return 0;
    (*file)->len_max = len_max;
    (*file)->nb_msg = nb_msg;
    (*file)->first = -1;
    (*file)->destruction = 0;
    (*file)->nb_erg = 0;
    char *msgs = (char*) &(*file)[1];
    for(size_t i = 0; i < nb_msg; i++){
        char *mes_addr = &msgs[(sizeof(mon_message)+len_max)*i];
        long val = -1;
        memcpy(mes_addr, &val, sizeof(long));
    }
    return 1;
}

MESSAGE *m_connexion(const char *nom, int options,.../*, size_t nb_msg, size_t len_max, mode_t mode*/){
    int fd; // descripteur du fichier de mémoire partagée
    FILE_MSG *file; // file de messages

    int nb_ergt = 10; // A Modifier

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
        size_t taille_file = sizeof(FILE_MSG) + ((sizeof(long) + sizeof(char) * len_max) * nb_msg) + nb_ergt * sizeof(enregistrement);
        int flags = MAP_ANONYMOUS | MAP_SHARED;
        if(!m_init_file(&file, -1, flags, taille_file, nb_msg, len_max))
            return NULL;
        options = O_RDWR;
        file->nb_ergmax = nb_ergt;
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
            if((fd = shm_open(nom, options, 0000)) == -1)
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
            if((fd = shm_open(nom, options, mode)) == -1){
                return NULL;
            }
            //création d'une nouvelle file
            size_t taille_file = sizeof(FILE_MSG) + ((sizeof(long) + sizeof(char) * len_max) * nb_msg) + nb_ergt * sizeof(enregistrement);
            if(ftruncate(fd, taille_file) == -1)
                return NULL;
            int flags = MAP_SHARED;
            if(!m_init_file(&file, fd, flags, taille_file, nb_msg, len_max))
                return NULL;
            file->nb_ergmax = nb_ergt;
        }
        else return NULL;
    }
    MESSAGE *m = malloc(sizeof(MESSAGE));
    if(m == NULL) return NULL;
    memset(m, 0, sizeof(MESSAGE));
    close(fd);
    m->flags = options;
    m->file = file;
    file->connecte++;
    return m;
}

int m_deconnexion(MESSAGE *file){
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    file->file->connecte--;
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    // munmap ?
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

int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag){
    if ( (file->flags & (O_RDWR | O_RDONLY )) == 0) return -1;
    if (len > file->file->len_max){
        errno = EMSGSIZE;
        return -1;
    }

    ////// ENVOIE UN SIGNAL SI LE TYPE DU MESSAGE EST PRESENT DANS LES ENREGISTREMENT
    int res, flag = 1;
    long type;
    memcpy(&type, msg, sizeof(long));
    while ( flag && (res = pthread_mutex_trylock(&file->file->mutex_enregistrement)) > 0){
        if (res == EBUSY){
            if (msgflag == O_NONBLOCK){
                errno = EAGAIN; // ?
                return -1;
            }
            else{
                flag = 0;
                if (pthread_mutex_lock(&file->file->mutex_enregistrement) > 0) return -1;
            }
        }
        if (res > 0) return -1;
    }
    char *f = (char *)file->file;
    char *addr = f + sizeof(FILE_MSG) + (file->file->nb_msg * (sizeof(long) + file->file->len_max));
    enregistrement *tmp = (enregistrement *) addr;
    for (int i = 0 ; i < file->file->nb_ergmax; i++){
        if (tmp->pid != 0 && tmp->type == type){
            kill(tmp->pid,tmp->signal);
            memset(tmp, 0, sizeof(enregistrement));
            i = file->file->nb_ergmax;
        }
        tmp++;
    }
    if ((res = pthread_mutex_unlock(&file->file->mutex_enregistrement) ) > 0 && res != EPERM) return -1;
    ///////////////////////////////////////////////

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
    //printf("%d %d\n",*first, *last);
    return 0;
}

size_t m_readmsg(MESSAGE *file, void *msg, int idx){
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[(sizeof(mon_message)+file->file->len_max)*idx];
    memcpy(msg, mes_addr+sizeof(long), file->file->len_max);
    memset(mes_addr+sizeof(long), 0, file->file->len_max);
    long val = -1;
    memcpy(mes_addr, &val, sizeof(long));
    return strlen(msg);
}

int shiftblock(MESSAGE *file, int idx){
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

int m_msg_index(MESSAGE *file, long type){
    int first = file->file->first;
    int last = file->file->last;
    if(m_nb(file) == 0)
        return -1;
    if(type == 0)
        return first;
    char *msgs = (char *)&file->file[1];
    size_t block_size = sizeof(mon_message) + file->file->len_max;
    for(int i = first; i != last; i = (i+1)%m_capacite(file)){
        long msg_type = 0;
        char *msg_addr = &msgs[block_size*i];
        memcpy(&msg_type, msg_addr, sizeof(long));
        if((type > 0 && msg_type == type) || (type < 0 && abs(type) <= msg_type))
            return i;
    }
    return -1;
}

ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags){
    if(len < m_message_len(file)){
        errno = EMSGSIZE;
        return -1;
    }
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    int idxsrc = 0; //idx du message de la file
    if( (idxsrc = m_msg_index(file,type)) == -1){
        if(flags == O_NONBLOCK){
            if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
            errno = EAGAIN;
            return -1;
        } else if(flags == 0){
            while ((idxsrc = m_msg_index(file,type)) == -1){
                if( pthread_cond_wait( &file->file->rcond, &file->file->mutex) > 0 ) return -1;
            }
        }
        else return -1;
    }
    int n = m_readmsg(file, msg, idxsrc); // copie du message dans msg
    int *first = &file->file->first;
    if(idxsrc != *first){
        //cas où il faut décaler la mémoire car on a pop un élément au milieu de la liste
        shiftblock(file, idxsrc);
    }
    *first = *first == file->file->nb_msg-1 ? 0 : (*first)+1;
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    pthread_cond_signal(&file->file->wcond);
    return n;
}

int m_enregistrement(MESSAGE *file, long type ,int signal, int msgflag){
    if (pthread_mutex_lock(&file->file->mutex_enregistrement) > 0) return -1;
    if (file->file->nb_erg == file->file->nb_ergmax){
        if (msgflag !=  0){
            pthread_mutex_unlock(&file->file->mutex);
            if (msgflag == O_NONBLOCK) errno = EAGAIN;
            return -1;
        }
        else{
            while (file->file->nb_erg == file->file->nb_ergmax){
                if( pthread_cond_wait( &file->file->cond_er, &file->file->mutex_enregistrement) > 0 ) return -1;
            }
        }
    }
    enregistrement er = {getpid(), type, signal};
    char *f = (char *)file->file;
    char *addr = f + sizeof(FILE_MSG) + (file->file->nb_msg * (sizeof(long) + file->file->len_max));
    enregistrement *tmp = (enregistrement *) addr;
    for (int i = 0 ; i < file->file->nb_ergmax; i++){
        if (tmp -> pid == 0){
            i = file->file->nb_ergmax;
            memcpy(tmp, &er, sizeof(enregistrement));
        }
        tmp++;
    }
    file->file->nb_erg++;
    if (pthread_mutex_unlock(&file->file->mutex_enregistrement) == -1) return -1;
    return 0;
}

int m_annulenr(MESSAGE *file){
    pid_t pid = getpid();
    char *f = (char *)file->file;
    char *addr = f + sizeof(FILE_MSG) + (file->file->nb_msg * (sizeof(long) + file->file->len_max));
    enregistrement *tmp = (enregistrement *) addr;
    if (pthread_mutex_lock(&file->file->mutex_enregistrement) > 0) return -1;
    for (size_t i = 0; i < file->file->nb_ergmax; i++){
        if (tmp -> pid == pid){
            memset(tmp, 0, file->file->nb_ergmax * sizeof(enregistrement));
            i = file->file->nb_ergmax;
        }
        tmp++;
    }
    file->file->nb_erg--;
    if (pthread_mutex_unlock(&file->file->mutex_enregistrement) > 0) return -1;
    return 0;
}