/* mmcdump - runs as profiler_daemon payload in u:r:init:s0.
 * Opens whole /dev/block/mmcblk0 and issues MMC_IOC_CMD (safe: CMD13 + CMD8 SEND_EXT_CSD).
 * Dumps EXT_CSD to /data/local/extcsd.bin and logs key bytes to /dev/kmsg. */
#define SYS_exit   1
#define SYS_write  4
#define SYS_open   5
#define SYS_close  6
#define SYS_ioctl  54

#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  64
#define O_TRUNC  512
#define MMC_IOC_CMD 0xc048b300UL

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
    b[10]=' '; b[11]=0; if(fd>=0) sc6(SYS_write, fd, (long)b, 11, 0,0,0);
}
static void lline(long fd, const char*tag, unsigned long v){ if(fd<0)return; lw(fd,tag); lh(fd,v); lw(fd,"\n"); }

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
static unsigned char buf[512];
static unsigned char buf2[512];

void _start(void)
{
    long fd, lf, kf, r;
    unsigned int i;

    lf = sc6(SYS_open, (long)"/data/local/extcsd.bin", O_WRONLY|O_CREAT|O_TRUNC, 0666,0,0,0);
    kf = sc6(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0,0,0,0);
    lw(kf, "[M] ctx=");
    { long c=sc6(SYS_open,(long)"/proc/self/attr/current",0,0,0,0,0); char cb[64]; long n=0;
      if(c>=0){ n=sc6(3,c,(long)cb,60,0,0,0); if(n>0){ cb[n]=0; lw(kf,cb);} sc6(SYS_close,c,0,0,0,0,0);} }
    lw(kf, "[M] open mmcblk0");
    fd = sc6(SYS_open, (long)"/dev/block/mmcblk0", O_RDWR, 0,0,0,0);
    lw(kf, " rc="); lh(kf,(unsigned long)fd); lw(kf,"\n");
    if(fd < 0){ lw(kf,"[M] FAIL open\n"); goto out; }

    for(i=0;i<sizeof(cmd);i++) ((char*)&cmd)[i]=0;
    cmd.opcode = 13; /* CMD13 SEND_STATUS */
    r = sc6(SYS_ioctl, fd, MMC_IOC_CMD, (long)&cmd, 0,0,0);
    lw(kf,"[M] CMD13 rc="); lh(kf,(unsigned long)r);
    lw(kf,"r0="); lh(kf,cmd.response[0]); lw(kf,"r3="); lh(kf,cmd.response[3]); lw(kf,"\n");

    for(i=0;i<sizeof(cmd);i++) ((char*)&cmd)[i]=0;
    cmd.opcode = 8;  /* CMD8 SEND_EXT_CSD */
    cmd.write_flag = 0;
    cmd.blksz = 512; cmd.blocks = 1;
    cmd.data_ptr = (unsigned long long)(unsigned long)buf;
    r = sc6(SYS_ioctl, fd, MMC_IOC_CMD, (long)&cmd, 0,0,0);
    lw(kf,"[M] CMD8(EXT_CSD) rc="); lh(kf,(unsigned long)r); lw(kf," nl="); lh(kf,(unsigned long)cmd.blocks); lw(kf,"\n");
    if(r==0){
        if(lf>=0) sc6(SYS_write, lf, (long)buf, 512, 0,0,0);
        lline(kf,"[M] PARTITION_CONFIG(179)=", buf[179]);
        lline(kf,"[M] BOOT_BUS_WIDTH(177)=", buf[177]);
        lline(kf,"[M] BOOT_SIZE_MULT(226)=", buf[226]);
        lline(kf,"[M] REV(192)=", buf[192]);
        lline(kf,"[M] CSD_STRUCT(194)=", buf[194]);
        lline(kf,"[M] SEC_COUNT0=", buf[212]);
        lline(kf,"[M] SEC_COUNT3=", buf[215]);
        lline(kf,"[M] DEV_VER(157)=", buf[157]);
        lline(kf,"[M] FW_VER(254)=", buf[254]);
    }
    sc6(SYS_close, fd, 0,0,0,0,0);
out:
    lw(kf,"[M] done\n");
    if(lf>=0) sc6(SYS_close, lf, 0,0,0,0,0);
    if(kf>=0) sc6(SYS_close, kf, 0,0,0,0,0);
    sc6(SYS_exit, 0, 0,0,0,0,0);
    for(;;){}
}
