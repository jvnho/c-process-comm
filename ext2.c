#include "m_file.h"

int main(int argc, char const *argv[]){
    MESSAGE *m = m_connexion("/ext2", O_CREAT | O_RDWR, 10, 10,0777);
    printf("ret : %d\n",m_enregistrement(m, 2, 2, 0));
    m_deconnexion(m);
    //m_destruction("/ext2");
    return 0;
}