#include "m_file.h"

int main(int argc, char const *argv[]){
    //MESSAGE m;
    //m_deconnexion(&m);
    MESSAGE *message = m_connexion("/test", O_CREAT | O_RDWR, 40, 15, 0655);
    MESSAGE *message2 = m_connexion("/test", O_RDWR);
    printf("file message 1 %p\n", message->file);
    printf("2 %p\n", message2->file);
    return 0;
}
