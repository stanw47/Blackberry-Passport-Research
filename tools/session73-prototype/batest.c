/* batest - check whether CMD6 PARTITION_ACCESS switch persists (run as profiler_daemon). */
#define SYS_exit   1
#define SYS_write  4
#define SYS_open   5
#define SYS_close  6
#define SYS_ioctl  54
#define SYS_nanosleep 162
#define MMC_IOC_CMD 0xc048b300UL
#define O_WRONLY 1
#define O_RDWR   2
static inline long sc6(long n, long a, long b, long c, long d, long e, long f){
    register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c;
    register long r3 asm("r3")=d; register long r4 asm("r4")=e; register long r5 asm("r5")=f;
    asm volatile("mov r7, %[n]\n\tsvc 0":"+r"(r0):[n]"r"(n),"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5):"r7","memory");
    return r0;
}
static long slen(const char*s){ long n=0; while(s[n]) n++; return n; }
static void lw(long fd,const char*s){ if(fd>=0) sc6(4,fd,(long)s,slen(s),0,0,0); }
static void lh(long fd,unsigned long v){ char b[12]; int i; const char*h="0123456789abcdef"; b[0]='0'; b[1]='x'; for(i=0;i<8;i++) b[2+i]=h[(v>>((7-i)*4))&0xf]; b[10]='\n'; b[11]=0; if(fd>=0) sc6(4,fd,(long)b,11,0,0,0); }
struct mmc_ioc_cmd { int write_flag, is_acmd; unsigned int opcode, arg; unsigned int response[4]; unsigned int flags, blksz, blocks; unsigned int postsleep_min_us, postsleep_max_us, data_timeout_ns, cmd_timeout_ms; unsigned int __pad; unsigned long long data_ptr; };
static struct mmc_ioc_cmd cmd; static unsigned char ex[512];
static long fd;
static long dc(int op,unsigned arg,int wf,int bs,int bl,void*p){ unsigned i; for(i=0;i<sizeof(cmd);i++)((char*)&cmd)[i]=0; cmd.opcode=op; cmd.arg=arg; cmd.write_flag=wf; cmd.blksz=bs; cmd.blocks=bl; cmd.data_ptr=(unsigned long long)(unsigned long)p; return sc6(54,fd,MMC_IOC_CMD,(long)&cmd,0,0,0); }
static void rd179(long kf,const char*tag){ dc(8,0,0,512,1,ex); lw(kf,tag); lh(kf,ex[179]); }
void _start(void){
    long kf,r; int cur;
    kf=sc6(5,(long)"/dev/kmsg",O_WRONLY,0,0,0,0);
    lw(kf,"[BT] start\n");
    fd=sc6(5,(long)"/dev/block/mmcblk0",O_RDWR,0,0,0,0);
    if(fd<0){ lw(kf,"[BT] nofd\n"); goto out; }
    dc(8,0,0,512,1,ex); cur=ex[179]; lw(kf,"[BT] initial 179="); lh(kf,(unsigned long)cur);
    dc(6, 0x03B30800UL,0,0,0,0);   /* access=1 boot0 */
    rd179(kf,"[BT] after set1 179=");
    dc(6, 0x03B31000UL,0,0,0,0);   /* access=2 boot1 */
    rd179(kf,"[BT] after set2 179=");
    dc(6, 0x03B30000UL|(((cur&~0x38)|(((cur>>3)&7)<<3))<<8),0,0,0,0); /* restore */
    rd179(kf,"[BT] after restore 179=");
    lw(kf,"[BT] done\n");
out:
    if(fd>=0) sc6(6,fd,0,0,0,0,0);
    if(kf>=0) sc6(6,kf,0,0,0,0,0);
    { struct{long s,ns;}ts; ts.s=3600; ts.ns=0; for(;;) sc6(SYS_nanosleep,(long)&ts,0,0,0,0,0); }
}
