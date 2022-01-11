#include <stdint.h>
#include <linux/input.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

uint32_t hex(char* x) {
    return strtoul(x, NULL, 16);
}

int main(int argc, char** argv) {
    struct input_event e;
    memset(&e, 0, sizeof(e));
    
    for (int i=1 ; i+2<argc; i+=3) {
        e.type = hex(argv[i]);
        e.code = hex(argv[i+1]);
        e.value = hex(argv[i+2]);
        write(1, &e, sizeof(e));
    }
    return 0;
}
