#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_CREAT|O_RDWR,10,10,0777);

    mon_message *mes = malloc( sizeof(mon_message ) + sizeof( char ) * 8 );
    mes -> type = 0;
    memmove( mes -> mtext, "12345678", 8);

    for (size_t i = 0; i < 10; i++){
        m_envoi(m, mes, 8, 0);
    }

    char recept[100];
    memset(recept,0,100);
    if(m_reception(m, recept, 100, 0, 0) == -1)
    {
        printf("error\n");
    }
    else
    {
        printf("reçu %s\n", recept);
    }
    printf("%ld\n", m_nb(m));
    m_deconnexion(m);
    return 0;
}