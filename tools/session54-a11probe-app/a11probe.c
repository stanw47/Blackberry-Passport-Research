typedef unsigned int u32;

extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern int write(int fd, const void *buf, unsigned n);
extern int devctl(int fd, int dcmd, void *p, unsigned n, int *info);
extern unsigned getuid(void);
extern unsigned geteuid(void);
extern unsigned getgid(void);
extern unsigned getegid(void);
extern int *__get_errno_ptr(void);
extern int resmgr_attach(void *ctp, void *attr, const char *path, int ftype,
                         unsigned flags, void *iofuncs, void *handle);

#define errno (*__get_errno_ptr())

#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0x100
#define O_TRUNC  0x200

static int fd = -1;

static unsigned slen(const char *s) { unsigned n = 0; if (!s) return 0; while (s[n]) n++; return n; }
static void outs(const char *s) { unsigned n = slen(s); if (n) { if (fd >= 0) write(fd, s, n); write(1, s, n); } }
static void outh(u32 v)
{
    const char *hx = "0123456789abcdef";
    char b[11]; int i;
    b[0] = '0'; b[1] = 'x';
    for (i = 0; i < 8; i++) b[2 + i] = hx[(v >> ((7 - i) * 4)) & 0xf];
    b[10] = ' ';
    if (fd >= 0) write(fd, b, 11); write(1, b, 11);
}
static void outln(void) { if (fd >= 0) write(fd, "\n", 1); write(1, "\n", 1); }

int main(int argc, char **argv)
{
    const char *paths[] = {
        "/accounts/1000/shared/misc/a11probe.log",
        "/tmp/a11probe.log",
        0
    };
    struct { const char *p; int f; } tst[] = {
        { "/dev/emmc", O_RDWR },
        { "/dev/emmc/user0", O_RDWR },
        { "/dev/emmc/boot0", O_RDWR },
        { "/dev/binder", O_RDWR },
        { "/accounts/1000/shared/misc", O_RDWR },
        { 0, 0 }
    };
    char attr[512], iof[512];
    int i, rc;

    for (i = 0; paths[i]; i++) { fd = open(paths[i], O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) break; }

    outs("\n=== A11PROBE ===\n");
    outs("argc="); outh((u32)argc); outs("argv0="); outs(argc > 0 ? argv[0] : "?"); outln();
    outs("uid=");   outh(getuid());
    outs("euid=");  outh(geteuid());
    outs("gid=");   outh(getgid());
    outs("egid=");  outh(getegid()); outln();

    for (i = 0; tst[i].p; i++) {
        int x;
        errno = 0;
        x = open(tst[i].p, tst[i].f);
        outs("open "); outs(tst[i].p); outs(" = "); outh((u32)x);
        outs("errno="); outh((u32)errno); outln();
        if (x >= 0) close(x);
    }

    for (i = 0; i < 512; i++) { attr[i] = 0; iof[i] = 0; }
    errno = 0;
    rc = resmgr_attach(0, attr, "/a11probe", 0, 0, iof, 0);
    outs("resmgr_attach rc="); outh((u32)rc); outs("errno="); outh((u32)errno); outln();

    outs("=== END ===\n");
    return 0;
}
