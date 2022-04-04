#ifndef M_FILE
#define M_FILE

typedef struct {
    int flags;
    int *file;
} MESSAGE;

int m_deconnexion(MESSAGE *file);

#endif