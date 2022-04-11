#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_CREAT|O_RDWR,10,10,0777);

    mon_message *mes = malloc( sizeof(mon_message ) + sizeof( char ) * 8 );
    mes -> type = 0;
    memmove( mes -> mtext, "12345678", 8);

    for (size_t i = 0; i < 14; i++){
        m_envoi(m, mes, 8, 0);
    }

    m_deconnexion(m);
    return 0;
}