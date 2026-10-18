/* baredump - runs as profiler_daemon (u:r:init:s0).
 * Reads eMMC boot partitions boot0/boot1 (4 MiB each) via MMC_IOC_CMD (CMD17).
 * ALWAYS restores PARTITION_CONFIG to the value read at start. Runs once. */
#define SYS_exit   1
#define SYS_write  4
#define SYS_open   5
#define SYS_close  6
#define SYS_ioctl  54
#define SYS_nanosleep 162
#define MMC_IOC_CMD 0xc048b300UL
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
static long slen(const char *s){ long n=0; while(s[n]) n++; return n; }
static void lw(long fd, const char *s){ if(fd>=0) sc6(SYS_write, fd, (long)s, slen(s), 0,0,0); }
static void lh(long fd, unsigned long v){
    char b[12]; int i; const char*h="0123456789abcdef";
    b[0]='0'; b[1]='x';
    for(i=0;i<8;i++) b[2+i]=h[(v>>((7-i)*4))&0xf];
    b[10]='\n'; b[11]=0; if(fd>=0) sc6(SYS_write, fd, (long)b, 11, 0,0,0);
}
struct mmc_ioc_cmd {
    int write_flag, is_acmd;
    unsigned int opcode, arg;
    unsigned int response[4];
    unsigned int flags, blksz, blocks;
    unsigned int postsleep_min_us, postsleep_max_us, data_timeout_ns, cmd_timeout_ms;
    unsigned int __pad;
    unsigned long long data_ptr;
};
static struct mmc_ioc_cmd cmd;
static unsigned char extcsd[512];
static unsigned char buf[512];

static long fd;
static long do_cmd(int op, unsigned arg, int wf, int blksz, int blocks, void*ptr)
{
    unsigned int i;
    for(i=0;i<sizeof(cmd);i++) ((char*)&cmd)[i]=0;
    cmd.opcode=op; cmd.arg=arg; cmd.write_flag=wf; cmd.blksz=blksz; cmd.blocks=blocks;
    cmd.data_ptr=(unsigned long long)(unsigned long)ptr;
    return sc6(SYS_ioctl, fd, MMC_IOC_CMD, (long)&cmd, 0,0,0);
}
static void set_access(int cur, int acc)
{
    unsigned v = (cur & ~0x38) | (acc<<3);
    do_cmd(6, 0x03B30000UL | (v<<8), 0,0,0,0);
}
static long rblk(unsigned lba){ return do_cmd(17, lba, 0, 512, 1, buf); }
static void dump_part(int cur, int acc, const char*outpath, long kf)
{
    long of; unsigned lba; long r; long bad=0;
    set_access(cur, acc); do_cmd(13,0,0,0,0,0);
    lw(kf,"[BA] part "); lw(kf,outpath); lw(kf," acc="); lh(kf,(unsigned long)acc);
    of = sc6(SYS_open,(long)outpath,O_WRONLY|O_CREAT|O_TRUNC,0666,0,0,0);
    if(fd<0||of<0){ lw(kf,"[BA] open fail\n"); return; }
    for(lba=0; lba<8192; lba++){
        r = rblk(lba);
        if(r!=0){ bad++; if(bad<4){ lw(kf,"[BA] RDerr lba="); lh(kf,(unsigned long)lba); lw(kf," rc="); lh(kf,(unsigned long)r);} continue; }
        sc6(SYS_write, of, (long)buf, 512, 0,0,0);
    }
    lw(kf,"[BA] part done bad="); lh(kf,(unsigned long)bad); lw(kf,"\n");
    sc6(SYS_close, of, 0,0,0,0,0);
}
void _start(void)
{
    long kf; long r; int cur;
    kf = sc6(SYS_open,(long)"/dev/kmsg",O_WRONLY,0,0,0,0);
    lw(kf,"[BA] start\n");
    fd = sc6(SYS_open,(long)"/dev/block/mmcblk0",O_RDWR,0,0,0,0);
    lw(kf,"[BA] open="); lh(kf,(unsigned long)fd);
    if(fd<0){ lw(kf,"[BA] no fd\n"); goto out; }
    r = do_cmd(8,0,0,512,1,extcsd);
    lw(kf,"[BA] CMD8="); lh(kf,(unsigned long)r);
    if(r!=0){ lw(kf,"[BA] extcsd fail\n"); goto out; }
    cur = extcsd[179];
    lw(kf,"[BA] PC="); lh(kf,(unsigned long)cur);
    dump_part(cur, 1, "/data/local/boot0.img", kf);
    dump_part(cur, 2, "/data/local/boot1.img", kf);
    set_access(cur, (cur>>3)&7);
    do_cmd(13,0,0,0,0,0);
    lw(kf,"[BA] restored\n");
out:
    lw(kf,"[BA] done\n");
    if(fd>=0) sc6(SYS_close, fd, 0,0,0,0,0);
    if(kf>=0) sc6(SYS_close, kf, 0,0,0,0,0);
    { struct { long s, ns; } ts; ts.s=3600; ts.ns=0; for(;;) sc6(SYS_nanosleep,(long)&ts,0,0,0,0,0); }
}
