#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_WRONLY);
    if(m == NULL){
        perror("connection failed");
        exit(1);
    }
    char buffer[100];
    memset(buffer, 0, 100);
    int code = m_reception(m, buffer, 100, 0, 0);
    if(code == -1 && errno == EAGAIN)
    {
        printf("pas encore de message\n");
    }
    else if(code == -1)
    {
        perror("failed");
        exit(1);
    }
    else
    {
        printf("reçu %s\n", buffer);
    }
    m_deconnexion(m);
}