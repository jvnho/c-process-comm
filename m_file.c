#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "m_file.h"

int m_deconnexion(MESSAGE *file){
    if (close(*(file -> file)) == -1) return -1 ;
    free(file);
    return 0;
}