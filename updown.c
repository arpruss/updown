// PUBLIC DOMAIN CODE
// Remap volume up/down keys on OnePlus 9 as page-up/down

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/input.h>
#include <poll.h>
#include <linux/uinput.h>

#define MAX_DEVICES 128

#define BUFSIZE 1024
#define NUM_KEY_BITS 

#define GET_BIT(data,bit) ((data[(bit)/8] >> ((bit)%8))&1)
#define SET_BIT(data,bit) data[(bit)/8] |= 1 << ((bit)%8)
#define CLR_BIT(data,bit) data[(bit)/8] &= ~(1 << ((bit)%8))

char* skip[] = {"P: Phys=ALSA"};
int remap[][2] = {
    { 0x72, KEY_PAGEDOWN },
    { 0x73, KEY_PAGEUP },
};

struct pollfd devices[MAX_DEVICES];
unsigned long eventsToSupport = 1UL<<EV_KEY;
unsigned char keysToSupport[(KEY_CNT+7)/8] = { 0 };
int numDevices;

#define NUM_REMAP (sizeof(remap)/sizeof(*remap))

int getRemap(int in) {
    for (int i=0; i<NUM_REMAP; i++)
        if (remap[i][0] == in)
            return remap[i][1];
    return -1;
}

int main(int argc, char** argv) {
    
    int verbose = argc > 1 && !strcmp(argv[1], "-v");
    
    FILE* f = fopen("/proc/bus/input/devices", "r");
    if (f == NULL) {
        fprintf(stderr, "Cannot scan devices\n");
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
        
        for (i = 0 ; i < sizeof(skip)/sizeof(*skip); i++)
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
            int fd = open(filename, O_RDONLY|O_NONBLOCK);
            if (0 <= fd) {
                unsigned long e = 0;
                ioctl(fd, EVIOCGBIT(0, sizeof(e)), &e);
                if (0 == (e & (1UL << EV_KEY))) {
                    close(fd);
                    continue;
                }
                unsigned char k[(KEY_CNT+7)/8] = { 0 };
                ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(k)), &k);
                int need = 0;
                for (int i=0; i<NUM_REMAP; i++)
                    if (GET_BIT(k, remap[i][0])) {
                        need = 1;
                        break;
                    }
                    
                if (!need) {
                    close(fd);
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
                    printf("Including %s\n", filename);
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

    for (int i=0; i<NUM_REMAP; i++)
        SET_BIT(keysToSupport, remap[i][1]);
    
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

    for (int i=0;i<NUM_REMAP && i<=KEY_MAX; i++)
        CLR_BIT(keysToSupport, remap[i][0]);

    for (int i=0;i<NUM_REMAP && i<=KEY_MAX; i++)
        SET_BIT(keysToSupport, remap[i][1]);
    
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
                        int outCode = getRemap(event.code);
                        if (outCode >= 0) {
                            if (verbose) {
                                printf("%d -> %d\n", event.code, outCode);
                            }
                            event.code = outCode;
                        }
                        write(fakeKeyboardHandle, &event, sizeof(event));
                    }
                }
            }
        }
    }
}
