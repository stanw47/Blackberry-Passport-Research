extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern int devctl(int fd, int dcmd, void *dev_data_ptr, unsigned long nbytes, int *dev_info_ptr);
extern int printf(const char *fmt, ...);
extern int setgid(int gid);

#define O_RDWR 2
#define DCMD_MMCSD_CARD_REGISTER 0xC0181A14
#define DCMD_MMCSD_WRITE_PROTECT 0xC0201A11

struct card_reg { unsigned int action, type, address, length, rsvd[2]; };
struct wp { unsigned int action, mode; unsigned long long lba, nlba, rsvd2; };

static unsigned char buf[24 + 512];

static int read_extcsd(int fd)
{
    struct card_reg *cr = (struct card_reg *)buf;
    int rc;
    cr->action = 0; cr->type = 2; cr->address = 0; cr->length = 512;
    cr->rsvd[0] = 0; cr->rsvd[1] = 0;
    rc = devctl(fd, DCMD_MMCSD_CARD_REGISTER, buf, 24 + 512, 0);
    return rc;
}

int main(void)
{
    int fd, rc;
    unsigned char *e;
    struct wp w;

    setgid(132);
    fd = open("/dev/emmc/boot0", O_RDWR);
    printf("open boot0 O_RDWR = %d\n", fd);
    if (fd < 0) return 1;

    rc = read_extcsd(fd);
    e = buf + 24;
    printf("before: rc=%d [170]=%02x [173]=%02x [174]=%02x [179]=%02x\n",
           rc, e[170], e[173], e[174], e[179]);

    w.action = 0;      /* CLR */
    w.mode = 0;
    w.lba = 0;
    w.nlba = 8192;     /* 4 MB / 512 */
    w.rsvd2 = 0;
    rc = devctl(fd, DCMD_MMCSD_WRITE_PROTECT, &w, sizeof(w), 0);
    printf("WRITE_PROTECT CLR mode=0 nlba=8192 rc=%d\n", rc);
    rc = read_extcsd(fd);
    e = buf + 24;
    printf("after : rc=%d [173]=%02x [174]=%02x\n", rc, e[173], e[174]);

    w.mode = 1;
    rc = devctl(fd, DCMD_MMCSD_WRITE_PROTECT, &w, sizeof(w), 0);
    printf("WRITE_PROTECT CLR mode=1 nlba=8192 rc=%d\n", rc);
    rc = read_extcsd(fd);
    e = buf + 24;
    printf("after2: rc=%d [173]=%02x [174]=%02x\n", rc, e[173], e[174]);

    close(fd);
    return 0;
}
