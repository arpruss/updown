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

char *deviceNames[] = { "N: Name=\"qpnp_pon\"", "N: Name=\"gpio-keys\"" };

#define NUM_DEVICES (sizeof(deviceNames)/sizeof(*deviceNames))

#define BUFSIZE 1024
#define NUM_KEY_BITS 

#define GET_BIT(data,bit) ((data[(bit)/8] >> ((bit)%8))&1)
#define SET_BIT(data,bit) data[(bit)/8] |= 1 << ((bit)%8)

char deviceFilenames[NUM_DEVICES][BUFSIZE] = { "", "" };
struct pollfd devices[NUM_DEVICES];
unsigned long eventsToSupport;
unsigned char keysToSupport[(KEY_CNT+7)/8] = { 0 };

int remap[][2] = {
    { 0x72, KEY_PAGEDOWN },
    { 0x73, KEY_PAGEUP },
};

void emit(int fd, int type, int code, int val)
   {
      struct input_event ie;

      ie.type = type;
      ie.code = code;
      ie.value = val;
      /* timestamp values below are ignored */
      ie.time.tv_sec = 0;
      ie.time.tv_usec = 0;

      write(fd, &ie, sizeof(ie));
   }


#define NUM_REMAP (sizeof(remap)/sizeof(*remap))

int main() {
    FILE* f = fopen("/proc/bus/input/devices", "r");
    if (f == NULL) {
        fprintf(stderr, "Cannot scan devices\n");
        exit(1);
    }
        
    char line[BUFSIZE+1];
    int current = -1;
    int number = -1;
    while (NULL != fgets(line,BUFSIZE,f)) {
        int i;
        
        line[BUFSIZE] = 0;
        if (!strncmp(line, "I: ", 3)) {
            number++;
            continue;
        }
        for (i = 0 ; i < NUM_DEVICES ; i++) 
            if (!strncmp(line, deviceNames[i], strlen(deviceNames[i]))) {
                sprintf(deviceFilenames[i], "/dev/input/event%d", number);
                break;
            }
    }
    fclose(f);
    for (int i=0; i<NUM_DEVICES ; i++) {
        if (deviceFilenames[i][0] == 0) {
            fprintf(stderr, "Cannot find device %s\n", deviceNames[i]);
            exit(2);
        }
    }
    
    for (int i=0; i<NUM_DEVICES ; i++) {
        devices[i].fd = open(deviceFilenames[i], O_RDONLY|O_NONBLOCK);
        if (devices[i].fd < 0) {
            fprintf(stderr, "Error opening %s\n", deviceFilenames[i]);
            exit(3);
        }
        unsigned long e = 0;
        ioctl(devices[i].fd, EVIOCGBIT(0, sizeof(e)), &e);
        eventsToSupport |= e;
        unsigned char k[(KEY_CNT+7)/8] = { 0 };
        ioctl(devices[i].fd, EVIOCGBIT(EV_KEY, sizeof(k)), &k);
        for (int i=0; i<sizeof(k); i++)
            keysToSupport[i] |= k[i];
        devices[i].events = POLLIN;
        ioctl(devices[i].fd, EVIOCGRAB, (void*)1);
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
    eventsToSupport |= 1UL<<EV_KEY;
    unsigned bit = 0;
    unsigned long e = eventsToSupport;
    while (e & 1) {
        ioctl(fakeKeyboardHandle, UI_SET_EVBIT, bit);
        bit++;
        e >>= 1;
    }

    for (int i=0;i<NUM_REMAP && i<=KEY_MAX; i++)
        SET_BIT(keysToSupport, remap[i][1]);
    
    for (int i=0; i<=KEY_MAX; i++)
        if (GET_BIT(keysToSupport, i))
            ioctl(fakeKeyboardHandle, UI_SET_KEYBIT, i);
    
    ioctl(fakeKeyboardHandle, UI_DEV_SETUP, &fakeKeyboard);
    ioctl(fakeKeyboardHandle, UI_DEV_CREATE);
      
    
    while(1) {
        if(0 < poll(devices, NUM_DEVICES, 2000)) {
            for (int i=0; i<NUM_DEVICES; i++) {
                if (devices[i].revents) {
                    struct input_event event;
                    
                    if (sizeof(event) == read(devices[i].fd, &event, sizeof(event))) {
                        for (int j=0; j<NUM_REMAP; j++) 
                            if (event.code == remap[j][0]) {
                                event.code = remap[j][1];
                                break;
                            }
                        write(fakeKeyboardHandle, &event, sizeof(event));
                    }
                }
            }
        }
    }
}
