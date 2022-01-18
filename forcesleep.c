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
#include <time.h>

int sleepTime = 30;

void goToSleep(void) {
    system("(dumpsys power | grep mWakefulness=Awake > /dev/null) && input keyevent 26");
}

int main(int argc, char** argv) {
    if (argc > 1)
        sleepTime = atoi(argv[1]);
    if (sleepTime < 5) {
        fprintf(stderr, "Safety measure: less than 5 seconds not allowed.");
        sleepTime = 5;
    }
    FILE* getevents = popen("getevent", "r");
    if (getevents == NULL) {
        fprintf(stderr, "Cannot read events.");
        return 1;
    }
    int fd = fileno(getevents);
    struct pollfd devices;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    devices.fd = fd;
    devices.events = POLLIN;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    unsigned long long lastEvent = t.tv_sec;
    
    while(1) {
        clock_gettime(CLOCK_MONOTONIC, &t);
        int dt;
        if (t.tv_sec - lastEvent > sleepTime) {
            goToSleep();
            lastEvent = t.tv_sec;
            dt = sleepTime;
        }
        else {
            dt = sleepTime - (t.tv_sec - lastEvent);
        }
        if (0 < poll(&devices, 1, dt)) {
            char c;
            read(fd, &c, 1);
            clock_gettime(CLOCK_MONOTONIC, &t);
            lastEvent = t.tv_sec;
        }
    }
}