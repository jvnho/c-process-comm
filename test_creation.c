#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/test", O_CREAT | O_RDWR, 10, 10,0777);
    if(m == NULL){
        perror("connexion failed");
        exit(1);
    }
    m_deconnexion(m);
    return 0;
}