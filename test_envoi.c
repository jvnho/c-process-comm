#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_RDWR);
    if(m == NULL){
        perror("connection failed");
        exit(1);
    }
    mon_message *mes = malloc( sizeof(mon_message ) + sizeof( char ) * 10 );
    if(mes == NULL){
        perror("malloc failed");
        exit(1);
    }
    size_t len = 0;
    if(argc > 1)
    {
        mes -> type = atol(argv[1]);
        mes -> length = strlen(argv[2]);
        len = strlen(argv[2]);
        memmove( mes -> mtext, argv[2], strlen(argv[2]));
    }
    else
    {
        mes -> type = 0;
        mes -> length = 8;
        len = 8;
        memmove( mes -> mtext, "12345678", 8);
    }
    if(m_envoi(m, mes, len, 0) == -1){
        perror("m_envoi failed");
        exit(1);
    }
    m_deconnexion(m);
    return 0;
}