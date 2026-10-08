#include <unistd.h>
extern int SchedYield(void);
extern int ChannelCreate(unsigned);
extern void _exit(int);
int main(void){
    write(1,"A\n",2);
    SchedYield();
    write(1,"B\n",2);
    int c = ChannelCreate(0);
    _exit((c & 0xff) ? (c & 0xff) : 42);
}
