#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_RDWR);

    char recept[100];
    memset(recept,0,100);

    mon_message *mes = malloc( sizeof(mon_message ) + sizeof( char ) * 8 );
    mes -> type = 0;
    memmove( mes -> mtext, "A0000000", 8);
    m_envoi(m, mes, 8, 0);

    memmove( mes -> mtext, "B0000000", 8);
    m_envoi(m, mes, 8, 0);

    memmove( mes -> mtext, "C0000000", 8);
    m_envoi(m, mes, 8, 0);

    memmove( mes -> mtext, "D0000000", 8);
    m_envoi(m, mes, 8, 0);

    m_reception(m, recept, 100, 0, 0);
    printf("%s\n",recept);

    m_reception(m, recept, 100, 0, 0);
    printf("%s\n",recept);

    m_reception(m, recept, 100, 0, 0);
    printf("%s\n",recept);

    memmove( mes -> mtext, "E0000000", 8);
    m_envoi(m, mes, 8, 0);

    mes -> type = 1;
    memmove( mes -> mtext, "F0000000", 8);
    m_envoi(m, mes, 8, 0);

    mes -> type = 0;
    memmove( mes -> mtext, "G0000000", 8);
    m_envoi(m, mes, 8, 0);

    m_reception(m, recept, 100, 0, 0);
    printf("%s\n",recept);

    m_reception(m, recept, 100, 0, 0);
    printf("%s\n",recept);

    m_reception(m, recept, 100, 1, 0);
    printf("%s\n",recept);

    m_reception(m, recept, 100, 0, 0);
    printf("%s\n",recept);

    m_deconnexion(m);
    return 0;
}