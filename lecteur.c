#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_RDWR);

    if(m == NULL)
    {
        perror("failed");
        exit(1);
    }

    char recept[100];
    memset(recept,0,100);
    long type = 0;
    if(argc > 1)
    {
        type = atol(argv[1]);
    }
    int code = m_reception(m, recept, 100, type, 0);
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
        printf("reçu %s\n", recept);
    }
    m_deconnexion(m);
    return 0;
}