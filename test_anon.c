#include "m_file.h"
#include <sys/wait.h>

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion(NULL, 0, 10, 10);
    if(m == NULL){
        perror("m_connexion() error");
        exit(1);
    }
    int r = fork();
    if(r == -1){
        perror("fork() failed");
        exit(1);
    }
    if(r == 0){ //processus enfant lit le message
        int t[2];
        int code = m_reception(m, t, sizeof(t), 0, 0);
        if(code == -1){
            perror("m_reception() error");
            exit(1);
        }
        printf("premier int reçu: %d\n", t[0]);
        printf("deuxième int reçu: %d\n", t[1]);
        m_deconnexion(m);
        exit(0);
    }
    //le père envoit le message
    int t[2] = {55,90};
    mon_message *mes = malloc(sizeof(mon_message) + sizeof(t));
    if(mes == NULL){
        perror("malloc failed");
        exit(1);
    }
    mes->type = (long)getpid();
    mes->length = sizeof(t);
    memmove(mes->mtext, t, sizeof(t));
    if(m_envoi(m, mes, sizeof(t), 0) == -1){
        perror("envoi failed");
        exit(1);
    }
    m_deconnexion(m);
    wait(NULL);
    return 0;
}