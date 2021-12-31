// PUBLIC DOMAIN CODE
// Remap volume up/down keys on OnePlus 9 as page-up/down

#include <sys/stat.h>
#include <signal.h>
#include <dirent.h> 
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/input.h>
#include <poll.h>
#include <linux/uinput.h>

#define MAX_DEVICES 128
#define MAX_REMAP 256
#define MAX_FILTERS 256

#define BUFSIZE 1024
#define NUM_KEY_BITS

#define GET_BIT(data,bit) ((data[(bit)/8] >> ((bit)%8))&1)
#define SET_BIT(data,bit) data[(bit)/8] |= 1 << ((bit)%8)
#define CLR_BIT(data,bit) data[(bit)/8] &= ~(1 << ((bit)%8))

char* DEFAULT_OPTIONS[] = { "--reject-location", "ALSA", "114", "109", "115", "104" };

const char CMD_PREFIX[] = ">/dev/null 2>/dev/null ";

int verbose = 0;
int fakeKeyboardHandle = -1;
int running;

enum filterBy {
    LOCATION=0,
    BUS,
    NAME
};
#define MAX_FILTERBY NAME

enum filterType {
    ONLY=0,
    REJECT,
    ADD
};

int numFilters = 0;

struct filter {
    enum filterBy by;
    enum filterType type;
    char* data;
} filters[MAX_FILTERS];

struct remap {
    int in;
    int out;
    char* cmd;
} remap[MAX_REMAP];
int numRemap;

struct pollfd devices[MAX_DEVICES];
unsigned long eventsToSupport = 1UL<<EV_KEY;
unsigned char keysToSupport[(KEY_CNT+7)/8] = { 0 };
int numDevices = 0;

struct remap* getRemap(int in) {
    for (int i=0; i<numRemap; i++)
        if (remap[i].in == in)
            return &remap[i];
    return NULL;
}
 
void closeAll(void) {
    for (int i=0; i<numDevices; i++) {
        close(devices[i].fd);
    }
    numDevices = 0;
    
    if (0 <= fakeKeyboardHandle)
        close(fakeKeyboardHandle);
    
    fakeKeyboardHandle = -1;
}

void sigint_handler(int sig) {
    if (verbose) 
        printf("Interrupted\n");
    running = 0;    
}

int isDir(const char *path) {
   struct stat statbuf;
   
   if (stat(path, &statbuf) < 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

int main(int argc, char** argv) {
    int arg = 1;
    
    if (arg == argc) {
        arg = 0;
        argv = DEFAULT_OPTIONS;
        argc = sizeof(DEFAULT_OPTIONS)/sizeof(*DEFAULT_OPTIONS);
    }
    
    while (arg < argc && argv[arg][0] == '-') {
        if (argv[arg][1] == 'h') {
            printf("updown [-h] [--(only|reject|add)-(location|bus|name) xxx ...] [-v] REMAP LIST...\n"
                   " where REMAP LIST... is a list of entries of the form:\n"
                   "  x y                : remap key code x (decimal) to y; if y is -1, disable x\n"
                   "  x cmd SHELLCOMMAND : run SHELLCOMMAND when key code x is pressed\n"
                   "If no arguments given, rejects location ALSA and remaps VOLUME UP/DOWN to PAGE UP/DOWN.\n");
            exit(0);
        }
        if (argv[arg][1] == 'v')
            verbose = 1;
        if (!strncmp(argv[arg]+1,"-reject-", 8) || !strncmp(argv[arg]+1,"-only-",6) ||
            !strncmp(argv[arg]+1,"-add-", 4)) {
                
            if (arg+1 >= argc) {
                fprintf(stderr, "Missing argument for %s.\n", argv[arg]);
                exit(1);
            }
        
            if (numFilters < MAX_FILTERS) {
                char* type = strchr(argv[arg]+2,'-');
                
                if (type[0] == 'l')
                    filters[numFilters].by = LOCATION;
                else if (type[0] == 'n')
                    filters[numFilters].by = NAME;
                else if (type[0] == 'b')
                    filters[numFilters].by = BUS;

                filters[numFilters].data = argv[arg+1];

                if (argv[arg][2] == 'o') 
                    filters[numFilters].type = ONLY;
                else if (argv[arg][2] == 'r')
                    filters[numFilters].type = REJECT;
                else if (argv[arg][2] == 'a') {
                    filters[numFilters].type = ADD;
                }
                
                numFilters++;
            }
            else {
                fprintf(stderr, "Too many filters\n");
            }
            
            arg++;
        }
        
        arg++;
    }
    
    numRemap = 0;
    
    while (arg + 1 < argc) {
        remap[numRemap].in = atoi(argv[arg]);
        if (!strcmp(argv[arg+1],"cmd")) {
            remap[numRemap].out = -1;
            if (arg + 2 >= argc) {
                fprintf(stderr, "No command given for %d\n", remap[numRemap].in);
                exit(12);
            }
            remap[numRemap].cmd = argv[arg+2];
            arg += 3;
        }
        else {
            remap[numRemap].out = atoi(argv[arg+1]);
            remap[numRemap].cmd = NULL;
            arg += 2;
        }
        numRemap++;
    }
    
    if (verbose) {
        for (int i=0; i<numRemap; i++) 
            printf("Configured remap: %d -> %d %s\n", remap[i].in, remap[i].out, remap[i].cmd != NULL ? remap[i].cmd : "(no command)");
    }
        
    numDevices = 0;
    
    DIR *d;
    struct dirent *dir;
    d = opendir("/dev/input");

    if (d==NULL) {
        fprintf(stderr, "Cannot open /dev/input\n");
        exit(1);
    }

    while (numDevices < MAX_DEVICES) {
        dir = readdir(d);
        
        if (NULL == dir)
            break;
        
        char filename[BUFSIZE];
        
        if (strlen(dir->d_name)+11 >= BUFSIZE)
            continue;
        
        sprintf(filename, "/dev/input/%s", dir->d_name);
        
        if (isDir(filename))
            continue;
        
        if (verbose)
            printf("Trying %s\n", filename);
        
        int fd = open(filename, O_RDONLY|O_NONBLOCK);
        if (fd < 0) {
            if (verbose)
                printf("  Couldn't open\n");
            continue;
        }

        char location[BUFSIZE];
        if (ioctl(fd, EVIOCGPHYS(sizeof(location) - 1), &location) < 1) {
            location[0] = 0;
        }
        
        if (verbose)
            printf("  Location: %s\n", location);
        
        char name[BUFSIZE];
        if (ioctl(fd, EVIOCGNAME(sizeof(location) - 1), &name) < 1) {
            name[0] = 0;
        }
        
        if (verbose)
            printf("  Name: %s\n", name);
        
        struct input_id id;
        if(ioctl(fd, EVIOCGID, &id)) {
            memset(&id, 0, sizeof(id));
        }

        if (verbose)
            printf("  Bus: %d\n", id.bustype);
        
        int reject = 0;
        int accept = 0;

        for (int t=0; t<=MAX_FILTERBY && !accept ; t++) {
            int foundAddFilter = 0;
            
            for (int i=0; i<numFilters && !accept ; i++) {
                if (filters[i].by != t)
                    continue;
                
                int matches = 0;
                
                switch(t) {
                    case BUS:
                        matches = atoi(filters[i].data) == id.bustype;
                        break;
                    case LOCATION:
                        matches = !strcmp(filters[i].data, location);
                        break;
                    case NAME:
                        matches = !strcmp(filters[i].data, name);
                        break;
                }
                
                switch(filters[i].type) {
                    case ADD:
                        foundAddFilter = 1;
                        if (matches) {
                            accept = 1;
                        }
                        break;
                    case REJECT:
                        if (matches)
                            reject = 1;
                        break;
                    case ONLY:
                        if (!matches)
                            reject = 1;
                        break;
                }
            }
            if (foundAddFilter && !accept) 
                reject = 1;
        }
        
        if (!accept && reject) {
            if (verbose) fprintf(stderr, " Rejected\n");
            close(fd);
            continue;
        }
        
        unsigned long e = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(e)), &e);
        if (0 == (e & (1UL << EV_KEY))) {
            close(fd);
            if (verbose)
                printf(" No keys\n");
            continue;
        }
        unsigned char k[(KEY_CNT+7)/8] = { 0 };
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(k)), &k);
        int need = 0;
        for (int i=0; i<numRemap; i++)
            if (GET_BIT(k, remap[i].in)) {
                need = 1;
                break;
            }
            
        if (!need) {
            close(fd);
            if (verbose) printf(" No needed keys\n");
            continue;
        }

        eventsToSupport |= e;
        for (int i=0; i<=KEY_CNT; i++) {
            if (GET_BIT(k, i))
                SET_BIT(keysToSupport, i);
        }
        
        ioctl(fd, EVIOCGRAB, (void*)1);
        devices[numDevices].fd = fd;
        devices[numDevices].events = POLLIN;
        if (verbose) {
            printf(" Including!\n");
        }
        numDevices++;
    }
    
    if (numDevices==0) {
        fprintf(stderr, "No input devices that generate the specified keys found\n");
        exit(7);
    }

    struct uinput_setup fakeKeyboard;
    fakeKeyboardHandle = open("/dev/uinput", O_WRONLY);
    if(fakeKeyboardHandle < 0) {
         fprintf(stderr, "Unable to open /dev/uinput\n");
         exit(4);
    }   
    
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    memset(&fakeKeyboard, 0, sizeof(fakeKeyboard));
    strcpy(fakeKeyboard.name, "MyFakeKeyboard");
    fakeKeyboard.id.bustype = BUS_VIRTUAL;

    for (int i=0;i<=EV_MAX;i++)
        if (i == EV_KEY || eventsToSupport & (1UL<<i)) 
            ioctl(fakeKeyboardHandle, UI_SET_EVBIT, i);
        
    unsigned bit = 0;
    unsigned long e = eventsToSupport;

    while (e & 1) {
        ioctl(fakeKeyboardHandle, UI_SET_EVBIT, bit);
        bit++;
        e >>= 1;
    }

    for (int i=0;i<numRemap && i<=KEY_MAX; i++)
        CLR_BIT(keysToSupport, remap[i].in);

    for (int i=0;i<numRemap && i<=KEY_MAX; i++)
        if (0 <= remap[i].out)
            SET_BIT(keysToSupport, remap[i].out);
    
    for (int i=0; i<=KEY_MAX; i++)
        if (GET_BIT(keysToSupport, i))
            ioctl(fakeKeyboardHandle, UI_SET_KEYBIT, i);
    
    ioctl(fakeKeyboardHandle, UI_DEV_SETUP, &fakeKeyboard);
    ioctl(fakeKeyboardHandle, UI_DEV_CREATE);
    
    running = 1;
    
    while(running) {
        if (verbose)
            printf("polling\n");
        
        if(0 < poll(devices, numDevices, 2000)) {
            for (int i=0; i<numDevices; i++) {
                if (devices[i].revents) {
                    struct input_event event;
                    
                    if (sizeof(event) == read(devices[i].fd, &event, sizeof(event))) {
                        struct remap* rp = getRemap(event.code);
                        if (rp != NULL && rp->out >= 0) {
                            if (verbose) {
                                printf("%d -> %d\n", event.code, rp->out);
                            }
                            event.code = rp->out;
                        }
                        if (rp == NULL || rp->out >= 0) {
                            write(fakeKeyboardHandle, &event, sizeof(event));
                        }
                        if (event.value == 1 && rp != NULL && rp->cmd != NULL) {
                            if (verbose) 
                                system(rp->cmd);
                            else {
                                char* buf = malloc(strlen(rp->cmd)+sizeof(CMD_PREFIX)+1);
                                if (buf != NULL) {
                                    strcpy(buf, CMD_PREFIX);
                                    strcat(buf, rp->cmd);
                                    system(buf);
                                    free(buf);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    closeAll();

    if (verbose) {
        printf("Exiting\n");
    }
}
