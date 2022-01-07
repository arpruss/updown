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

char *nameToConsume = "hall_sensors";

int verbose = 0;
int running;
int fd;

void sigint_handler(int sig) {
    if (verbose) 
        printf("Interrupted\n");
    running = 0;    
    close(fd);
}

int isDir(const char *path) {
   struct stat statbuf;
   
   if (stat(path, &statbuf) < 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

int main(int argc, char** argv) {
    
    int arg = 1;
    if (arg < argc) {
        if (!strcmp(argv[arg], "-v")) {
            verbose = 1;
            arg++;
        }
    }
    
    if (arg < argc) {
        nameToConsume = argv[arg];
    }
    
    DIR *d;
    struct dirent *dir;
    d = opendir("/dev/input");

    if (d==NULL) {
        fprintf(stderr, "Cannot open /dev/input\n");
        exit(1);
    }
    
    while (NULL != (dir = readdir(d))) {
        fd = -1;
        
        char filename[BUFSIZE];
        
        if (strlen(dir->d_name)+11 >= BUFSIZE)
            continue;
        
        sprintf(filename, "/dev/input/%s", dir->d_name);
        
        if (isDir(filename))
            continue;
        
        if (verbose)
            printf("Trying %s\n", filename);
        
        fd = open(filename, O_RDONLY);

        if (fd < 0) {
            if (verbose)
                printf("  Couldn't open\n");
            continue;
        }

        char name[BUFSIZE];
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
            name[0] = 0;
        }
        
        if (verbose)
            printf("  Name: %s\n", name);
        
        if (!strcmp(name, nameToConsume)) {
            if (verbose)
                printf("  Found\n");
            break;
        }
        
        close(fd);
        fd = -1;
    }
    
    if (fd < 0) {
        fprintf(stderr, "Couldn't find %s\n", nameToConsume);
        exit(9);
    }
        
    ioctl(fd, EVIOCGRAB, (void*)1);

    running = 1;

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    while(running) {
        struct input_event event;
        
        read(fd, &event, sizeof(event));
    }
        
    close(fd);

    if (verbose) {
        printf("Exiting\n");
    }
}
