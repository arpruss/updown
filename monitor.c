#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 16384

const char DEFAULT[] = "-";
const char MATCH[] = "ActivityManager: Displayed ";

int main(int argc, char** argv) {
    char* currentApp = NULL;
    
    while(1) {
        FILE* log = popen("logcat ActivityManager:I *:S", "r");
        
        if (log == NULL) {
            fprintf(stderr,"Cannot open log\n");
            return 2;
        }
        setlinebuf(log);
        
        char buf[BUFFER_SIZE];
        int current = -1;
        
        while (NULL != fgets(buf, BUFFER_SIZE-1, log)) {
            buf[BUFFER_SIZE-1] = 0;
            char* p = strstr(buf, MATCH); 
            if (p != NULL) {
                p += sizeof(MATCH)-1;
                char* slash = strchr(p, '/');
                if (slash != NULL) {
                    *slash = 0;
                    int i;
                    for (i = 1 ; i + 2 < argc ; i++) 
                        if (!strcmp(argv[i], p)) 
                            break;
                    if (i + 2 >= argc) {
                        for (i = 1 ; i + 2 < argc ; i++) 
                            if (!strcmp(argv[i], DEFAULT))
                                break;
                        if (i + 2 >= argc)
                            i = -1;
                    }
                    if (current != i) {
                        if (current >= 0 && strcmp(argv[current+2],DEFAULT))
                            system(argv[current+2]);
                        if (i >= 0 && strcmp(argv[i+1],DEFAULT))
                            system(argv[i+1]);
                        current = i;
                    }
                }
            }
        }
        
        fclose(log);
    }
}
