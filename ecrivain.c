#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_RDWR);
    mon_message *mes = malloc( sizeof(mon_message ) + sizeof( char ) * 8 );
    if(argc > 1)
    {
        mes -> type = atol(argv[1]);
        mes -> length = strlen(argv[2]);
        memmove( mes -> mtext, argv[2], strlen(argv[2]));
    }
    else
    {
        mes -> type = 0;
        mes -> length = 8;
        memmove( mes -> mtext, "12345678", 8);
    }
    m_envoi(m, mes, 8, 0);
    m_deconnexion(m);
    return 0;
}