// PUBLIC DOMAIN CODE
// Remap volume up/down keys on OnePlus 9 as page-up/down

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
#define MAX_SKIPS 256

#define BUFSIZE 1024
#define NUM_KEY_BITS

#define GET_BIT(data,bit) ((data[(bit)/8] >> ((bit)%8))&1)
#define SET_BIT(data,bit) data[(bit)/8] |= 1 << ((bit)%8)
#define CLR_BIT(data,bit) data[(bit)/8] &= ~(1 << ((bit)%8))

const char CMD_PREFIX[] = ">/dev/null 2>/dev/null ";

char* skip[MAX_SKIPS] = {};
int numSkips;
struct remap {
    int in;
    int out;
    char* cmd;
} remap[MAX_REMAP];
int numRemap;

struct pollfd devices[MAX_DEVICES];
unsigned long eventsToSupport = 1UL<<EV_KEY;
unsigned char keysToSupport[(KEY_CNT+7)/8] = { 0 };
int numDevices;

struct remap* getRemap(int in) {
    for (int i=0; i<numRemap; i++)
        if (remap[i].in == in)
            return &remap[i];
    return NULL;
}

int main(int argc, char** argv) {
    
    int arg = 1;
    int verbose = 0;
    int skipOptions = 0;
    int numSkips = 0;
    
    while (arg < argc && argv[arg][0] == '-') {
        if (argv[arg][1] == 'v')
            verbose = 1;
        if (!strcmp(argv[arg]+1,"-skip")) {
            if (arg+1 >= argc) {
                fprintf(stderr, "--skip needs an argument\n");
                exit(8);
            }
            skipOptions = 1;
            if (numSkips < MAX_SKIPS) {
                skip[numSkips] = argv[arg+1];
                numSkips++;
            }
            else {
                fprintf(stderr, "Too many skips\n");
            }
            arg++;
        }
        else if (!strcmp(argv[arg]+1,"-no-skip")) {
            skipOptions = 1;
            numSkips=0;
        }
        arg++;
    }
    
    if (! skipOptions) {
        skip[0] = "P: Phys=ALSA";
        numSkips = 1;
    }
    
    numRemap = 0;
    
    while (arg + 1 < argc) {
        remap[numRemap].in = atoi(argv[arg]);
        if (!strcmp(argv[arg+1],"su")) {
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
    
    if (numRemap == 0) {
        remap[0].in = KEY_VOLUMEDOWN;
        remap[0].out = KEY_PAGEDOWN;
        remap[0].cmd = NULL;
        remap[1].in = KEY_VOLUMEUP;
        remap[1].out = KEY_PAGEUP;
        remap[1].cmd = NULL;
        numRemap = 2;
    }
    
    if (verbose) {
        for (int i=0; i<numRemap; i++) 
            printf("Configured remap: %d -> %d %s\n", remap[i].in, remap[i].out, remap[i].cmd != NULL ? remap[i].cmd : "(no command)");
    }

    FILE* f = fopen("/proc/bus/input/devices", "r");
    if (f == NULL) {
        fprintf(stderr, "Cannot read list of devices\n");
        exit(1);
    }
        
    char line[BUFSIZE+1];
    
    int skipCurrent = 0;
    numDevices = 0;
    
    while (NULL != fgets(line,BUFSIZE,f)) {
        if (numDevices >= MAX_DEVICES)
            break;
        
        int i;
        
        line[BUFSIZE] = 0;
        int l = strlen(line);
        if (l == 0)
            continue;
        if (line[l-1] == '\n')
            line[l-1] = 0;
        
        if (!strncmp(line, "I: ", 3)) {
            skipCurrent = 0;
            continue;
        }
        
        if (skipCurrent)
            continue;
        
        for (i = 0 ; i < numSkips; i++)
            if (!strcmp(line, skip[i])) {
                skipCurrent = 1;
                break;
            }

        if (skipCurrent)
            continue;

        if (!strncmp(line, "H: Handlers=", 57-45)) {
            skipCurrent = 1;

            char filename[BUFSIZE];
            char* s = strstr(line, "event");
            if (s != NULL) {
                sprintf(filename, "/dev/input/event%d", atoi(s+5));
            }
            if (verbose)
                printf("Trying %s\n", filename);
            int fd = open(filename, O_RDONLY|O_NONBLOCK);
            if (0 <= fd) {
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
        }
    }
    fclose(f);
    
    if (numDevices==0) {
        fprintf(stderr, "No good input devices found\n");
        exit(7);
    }

    static int fakeKeyboardHandle;
    struct uinput_setup fakeKeyboard;
    fakeKeyboardHandle = open("/dev/uinput", O_WRONLY);
    if(fakeKeyboardHandle < 0) {
         fprintf(stderr, "Unable to open /dev/uinput\n");
         exit(4);
    }   
    
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
    
    while(1) {
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
}
