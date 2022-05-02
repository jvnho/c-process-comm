#include "m_file.h"
#include <wait.h>

int flag = 0;

void erreur(char *mes){
    perror(mes);
    exit(1);
}

void sig(int i){
    flag = i;
}


void fils(int i){
    MESSAGE *m = m_connexion("/ext2", O_RDWR);
    if (m == NULL) erreur("connexion");

    struct sigaction sigusr;
    sigusr.sa_handler = sig;
    sigaction(SIGUSR1, &sigusr, NULL);

    m_enregistrement(m, i, SIGUSR1, 0);
    sleep(4);
    if (flag){
        char s[1024];
        int len = sprintf(s, "Je suis le processus %d, mon pid est %d et j'ai recu le signal %d\n",i, getpid(), flag);
        write(STDOUT_FILENO, s, len);
    }
    m_deconnexion(m);
}

int main(int argc, char const *argv[]){

    MESSAGE *m = m_connexion("/ext2", O_CREAT | O_RDWR, 10, 10,0777);

    pid_t pid[5];
    for (size_t i = 0; i < 5; i++){
        switch (pid[i] = fork()){
            case -1:
                perror("fork");
                exit(1);
                break;
            case 0:
                fils(i);
                exit(0);
                break;
            default: continue;
        }
    }

    mon_message *mes = malloc( sizeof(mon_message ) + sizeof( char ) * 8 );
    memmove( mes -> mtext, "12345678", 8);
    sleep(1);
    for (size_t j = 0; j < 5; j++){
        mes -> type = j;
        m_envoi(m, mes, sizeof(mes), 0);
    }
    while (wait(0) > 0){}
    m_deconnexion(m);
    m_destruction("/ext2");
    return 0;
}