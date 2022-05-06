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
    if(first == last) return m_capacite(message);
    if(first < last) return last - first;
    return m_capacite(message) - first + last;
}

int m_init_file(FILE_MSG **file, int fd, int flags, size_t taille_file, size_t nb_msg, size_t len_max){
    *file = mmap(NULL, taille_file, PROT_READ|PROT_WRITE, flags, fd, 0);
    if((void *) (*file) == MAP_FAILED)
        return 0;
    memset(*file, 0, taille_file);
    if(initialiser_mutex(&(*file)->mutex) != 0
       || initialiser_cond(&(*file)->rcond) != 0
       || initialiser_cond(&(*file)->wcond) != 0)
        return 0;
    (*file)->len_max = len_max;
    (*file)->max_byte = taille_file - sizeof(FILE_MSG);
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
            size_t taille_file = sizeof(FILE_MSG) + ((sizeof(mon_message) + sizeof(char) * len_max) * nb_msg);
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
    free(file); // munmap ?
    return 0;
}

//fonction qui renvoie le nombre d'octets libre dans la file
int getfreespace(MESSAGE *file)
{
    if(file->file->first == -1) return m_capacite(file);
    return m_capacite(file) - m_nb(file);
}

int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag){
    if ((file->flags & (O_RDONLY | O_RDWR)) == 0) return -1;
    if (len > file->file->len_max){
        errno = EMSGSIZE;
        return -1;
    }
    int *first = &file->file->first;
    int *last = &file->file->last;
    if (pthread_mutex_lock(&file->file->mutex) > 0) return -1;
    int l = *last;
    if (*first == -1) *first = 0;
    if (getfreespace(file) < len){ // on regarde si la file a assez de place pour stocker le message
        if (msgflag !=  0){
            pthread_mutex_unlock(&file->file->mutex);
            if (msgflag == O_NONBLOCK) errno = EAGAIN;
            return -1;
        }
        else{
            while (getfreespace(file) < len){
                //TODO: = à repenser
                if( pthread_cond_wait( &file->file->wcond, &file->file->mutex) > 0 ) return -1;
            }
        }
    }
    *last = (*last + sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[l];
    if(*first < *last) { //assez de place sur la droite de la file, TODO: à changer *last peut changer entre temps
        memcpy(mes_addr, msg, sizeof(mon_message) + sizeof(char) * len);
    } else { //le message doit être coupé en deux
        int split = m_capacite(file)-l;
        memcpy(mes_addr, msg, split); // écriture à droite de la file
        memcpy(&msgs, msg+split, len-split); // écriture à gauche de la file
    }
    pthread_cond_signal(&file->file->rcond);
    printf("%d %d\n",*first, *last);
    return 0;
}

size_t m_readmsg(MESSAGE *file, void *msg, int addr){
    char *msgs = (char *)&file->file[1];
    char *mes_addr = &msgs[addr];
    int len = *(mes_addr+sizeof(long));
    memcpy(msg, mes_addr+sizeof(long), len);
    memset(mes_addr, 0, sizeof(mon_message) + sizeof(char)*len); //supprime le message de la file en remplissant la zone d'octets nuls
    return len;
}

int shiftblock(MESSAGE *file, int idx){
    int first = file->file->first;
    char tmp[sizeof(file->file)];
    memcpy(tmp, file->file, sizeof(file->file)); // fais une copie de la file de messages courante
    char *msgs = (char *)&file->file[1];
    int i = first, j = first;
    while(j != idx){
        char *addr_src = &msgs[i];
        size_t len = *(addr_src+sizeof(long));
        j = j + (sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
        char *addr_dst = &msgs[j];
        memcpy(addr_dst, tmp+i, len);
        i = i + (sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
    }
    return 1;
}

int m_msg_index(MESSAGE *file, long type){
    int first = file->file->first;
    int last = file->file->last;
    if(!m_nb(file))
        return -1;
    if(type == 0)
        return first;
    char *msgs = (char *)&file->file[1];
    int i = first;
    while(i != last){
        char *msg_addr = &msgs[i];
        long msg_type = *msg_addr;
        if((type > 0 && msg_type == type) || (type < 0 && abs(type) >= msg_type))
            return i;
        size_t len = *(msg_addr+sizeof(long));
        i = i + (sizeof(mon_message) + sizeof(char) * len) % m_capacite(file);
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
    //*first = *first == file->file->nb_msg-1 ? 0 : (*first)+1;
    *first = *first + sizeof(mon_message) + sizeof(char) * n;
    if (pthread_mutex_unlock(&file->file->mutex) > 0) return -1;
    pthread_cond_signal(&file->file->wcond);
    return n;
}