typedef unsigned long size_t;
extern int write(int, const void *, size_t);
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern void _exit(int);
static void ph(unsigned long v){ static const char h[]="0123456789abcdef"; char b[11]; int i;
  b[0]='0';b[1]='x'; for(i=0;i<8;i++)b[2+i]=h[(v>>((7-i)*4))&0xf]; b[10]=' '; write(1,b,11); }
int main(void){
    void *u = dlopen("libutils.so", 0); write(1, u?"U":"u", 1);
    void *h = u ? dlopen("libbinder.so", 0) : 0; write(1, h?"B":"b", 1);
    void *p = h ? dlsym(h, "_ZN7android12ProcessState4selfEv") : 0; write(1, p?"S":"s", 1);
    /* sp<> returns use the hidden sret pointer (non-trivial copy ctor): the
     * callee takes r0 = &result. */
    void *r = 0;
    if (p) ((void (*)(void *))p)(&r);
    write(1, " self=", 6); ph((unsigned long)r); write(1, "\n", 1);
    void *d = h ? dlsym(h, "_ZN7android21defaultServiceManagerEv") : 0;
    write(1, d?"D":"d", 1);
    if (d) {
        void *m = 0;
        ((void (*)(void *))d)(&m);
        write(1, " dsm=", 5); ph((unsigned long)m); write(1, "\n", 1);
    }
    _exit(0);
    return 0;
}
