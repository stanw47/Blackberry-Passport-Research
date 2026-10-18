/* pf - syscall-only ARM payload; runs as /system/bin/profiler_daemon.
 * Logs its own SELinux context + probes privileged nodes. */
#define SYS_exit   1
#define SYS_read   3
#define SYS_write  4
#define SYS_open   5
#define SYS_close  6
#define SYS_nanosleep 162

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  64
#define O_TRUNC  512

static inline long sc6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r0 asm("r0") = a;
    register long r1 asm("r1") = b;
    register long r2 asm("r2") = c;
    register long r3 asm("r3") = d;
    register long r4 asm("r4") = e;
    register long r5 asm("r5") = f;
    asm volatile("mov r7, %[n]\n\tsvc 0"
                 : "+r"(r0)
                 : [n] "r"(n), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                 : "r7", "memory");
    return r0;
}
static long slen(const char*s){ long n=0; while(s[n]) n++; return n; }
static void wl(long fd,const char*s){ if(fd>=0) sc6(SYS_write,fd,(long)s,slen(s),0,0,0); }
static void wh(long fd,unsigned long v){
    char b[12]; int i; const char*h="0123456789abcdef";
    b[0]='0'; b[1]='x';
    for(i=0;i<8;i++) b[2+i]=h[(v>>((7-i)*4))&0xf];
    b[10]='\n'; if(fd>=0) sc6(SYS_write,fd,(long)b,11,0,0,0);
}
static void cat(long lf,const char*path){
    char b[256]; long fd=sc6(SYS_open,(long)path,O_RDONLY,0,0,0,0), n;
    wl(lf,"  "); wl(lf,path); wl(lf," -> ");
    if(fd<0){ wh(lf,(unsigned long)fd); return; }
    n=sc6(SYS_read,fd,(long)b,255,0,0,0);
    if(n>0){ b[n]=0; wl(lf,b); } else wh(lf,(unsigned long)n);
    sc6(SYS_close,fd,0,0,0,0,0);
    if(n<=0||b[n-1]!='\n') wl(lf,"\n");
}
static void probe(long lf,const char*path){
    long fd=sc6(SYS_open,(long)path,O_RDWR,0,0,0,0);
    wl(lf,"  open "); wl(lf,path); wl(lf," = "); wh(lf,(unsigned long)fd);
    if(fd>=0) sc6(SYS_close,fd,0,0,0,0,0);
}

void _start(void)
{
    long lf;
    struct { long s, ns; } ts;
    lf = sc6(SYS_open,(long)"/storage/emulated/legacy/pf.txt",
             O_WRONLY|O_CREAT|O_TRUNC,0666,0,0,0);
    if(lf<0) lf = sc6(SYS_open,(long)"/data/local/tmp/pf.txt",
                     O_WRONLY|O_CREAT|O_TRUNC,0666,0,0,0);
    wl(lf,"== profiler_daemon-domain payload ==\n");
    cat(lf,"/proc/self/attr/current");
    cat(lf,"/proc/self/status");
    probe(lf,"/dev/block/mmcblk0");
    probe(lf,"/dev/block/mmcblk0rpmb");
    probe(lf,"/dev/block/bootdevice/by-name/perm");
    probe(lf,"/dev/block/bootdevice/by-name/blog");
    probe(lf,"/dev/block/bootdevice/by-name/aboot");
    probe(lf,"/dev/mem");
    probe(lf,"/system/bin/vtnvfsd");
    wl(lf,"== done, sleeping ==\n");
    if(lf>=0) sc6(SYS_close,lf,0,0,0,0,0);
    ts.s=3600; ts.ns=0;
    for(;;) sc6(SYS_nanosleep,(long)&ts,0,0,0,0,0);
}
