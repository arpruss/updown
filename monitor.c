#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 16384

const char DEFAULT[] = "-";
const char MATCH_DISPLAYED[] = "Displayed ";
const char MATCH_START[] = "START ";
const char MATCH_CMP[] = " cmp=";
const char MATCH_ANIMATION[] = "createRemoteAnimationTarget Task";
const char MATCH_ACTIVITYRECORD[] = " ActivityRecord{";

int main(int argc, char** argv) {
    char* currentApp = NULL;
    
    while(1) {
        FILE* log = popen("logcat -v raw ActivityTaskManager:I ActivityManager:I *:S", "r");
        
        if (log == NULL) {
            fprintf(stderr,"Cannot open log\n");
            return 2;
        }
        setlinebuf(log);
        
        char buf[BUFFER_SIZE];
        int current = -1;
        
        while (NULL != fgets(buf, BUFFER_SIZE-1, log)) {
            buf[BUFFER_SIZE-1] = 0;
            char *pkg = NULL;
            if (!strncmp(buf, MATCH_DISPLAYED, sizeof(MATCH_DISPLAYED)-1)) {
                pkg = buf + sizeof(MATCH_DISPLAYED)-1;
                char* slash = strchr(pkg, '/');
                if (slash != NULL) {
                    *slash = 0;
                }
                else {
                    pkg = NULL;
                }
            }
            else if (!strncmp(buf, MATCH_START, sizeof(MATCH_START)-1)) {
                pkg = strstr(buf + sizeof(MATCH_START)-1, MATCH_CMP);
                if (pkg != NULL) {
                    pkg += sizeof(MATCH_CMP)-1;
                    char* slash = strchr(pkg, '/');
                    if (slash != NULL) {
                        *slash = 0;
                    }
                    else {
                        pkg = NULL;
                    }
                }
            }
            else if (!strncmp(buf, MATCH_ANIMATION, sizeof(MATCH_ANIMATION)-1)) {
                pkg = strstr(buf + sizeof(MATCH_ANIMATION)-1, MATCH_ACTIVITYRECORD);
                if (pkg != NULL) {
                    pkg += sizeof(MATCH_ACTIVITYRECORD)-1;
                    char* slash = strchr(pkg, '/');
                    if (slash != NULL) {
                        *slash = 0;
                        pkg = strrchr(pkg, ' ');
                        if (pkg != NULL) 
                            pkg++;
                    }
                    else {
                        pkg = NULL;
                    }
                }
            }
            
            
            if (pkg != NULL) {
                int i;
                for (i = 1 ; i + 2 < argc ; i++) 
                    if (!strcmp(argv[i], pkg)) 
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
        
        fclose(log);
    }
}
