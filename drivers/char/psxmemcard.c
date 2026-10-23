/*
 * psxmemcard - PlayStation Memory Card character device.
 *
 * Exposes the memory card interface to userspace via /dev/psxmemcard.
 * Supports standard 128KB (15 block) memory cards.
 *
 * Protocol reference: http://psx-spx.github.io/#memorycards
 *   SIO1 port: 0x1F801060
 *   Memory card selected by RTS (CS) low
 *   Standard speed: 250000 bps
 *   Block size: 8192 bytes (128 sectors * 64 bytes)
 *   Card size: 128KB = 16 blocks (block 0 = directory)
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/semaphore.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <asm/ps/sio1.h>
#include <linux/psxmemcard.h>

/* ---- Memory Card commands ---- */
#define MC_CMD_READ         0x52  /* 'R' - read sector */
#define MC_CMD_WRITE        0x57  /* 'W' - write sector */
#define MC_CMD_GET_ID       0x53  /* 'S' - get card ID (size/type) */
#define MC_CMD_FORMAT       0x46  /* 'F' - format card (erase all) */
#define MC_CMD_CHECK        0x43  /* 'C' - check card */

#define MC_RESPONSE_OK      0x00
#define MC_RESPONSE_ACK     0x01
#define MC_RESPONSE_NACK    0x02

#define MC_BLOCK_SIZE       8192   /* 128 sectors * 64 bytes */
#define MC_SECTOR_SIZE      64
#define MC_BLOCKS_PER_CARD  16
#define MC_CARD_SIZE        (MC_BLOCK_SIZE * MC_BLOCKS_PER_CARD)  /* 128KB */

/* Memory card ID structure */
struct mc_id {
    u8  type;        /* 0x00 = standard, 0x01 = multitap */
    u8  size;        /* 0x00 = 128KB, 0x01 = 256KB, etc. */
    u8  reserved[2];
    u8  manufacturer[8];
    u8  product[8];
    u8  version[8];
};

/* ---- Register access ---- */

static inline void mc_wait_ready_cmd(void)
{
    while (!(readw(SIO1_STAT_REG) & SIO1_RFW))
        ;
}

static inline void mc_wait_ready_data(void)
{
    while (!(readw(SIO1_STAT_REG) & SIO1_RFR))
        ;
}

static inline void mc_write_byte(u8 b)
{
    mc_wait_ready_cmd();
    writeb(b, SIO1_DATA_REG);
}

static inline u8 mc_read_byte(void)
{
    mc_wait_ready_data();
    return readb(SIO1_DATA_REG);
}

static inline void mc_select(int port)
{
    /* RTS low = memcard CS, DTR high = controller CS inactive */
    /* Port 0: RTS; Port 1: would need second memcard slot */
    writew((readw(SIO1_CTRL_REG) & ~SIO1_RTS) | SIO1_DTR, SIO1_CTRL_REG);
}

static inline void mc_deselect(void)
{
    writew(readw(SIO1_CTRL_REG) | SIO1_RTS | SIO1_DTR, SIO1_CTRL_REG);
}

/* ---- Memory Card communication ---- */

static int mc_send_command(int port, u8 cmd, u8 *tx, int tx_len, u8 *rx, int rx_len)
{
    int i;

    if (port < 0 || port >= PSXMEMCARD_MAX_PORTS)
        return -ENODEV;

    mc_select(port);

    /* Send command byte */
    mc_write_byte(cmd);

    /* Send TX data */
    for (i = 0; i < tx_len; i++)
        mc_write_byte(tx[i]);

    /* Read response */
    for (i = 0; i < rx_len; i++)
        rx[i] = mc_read_byte();

    mc_deselect();
    return 0;
}

/* Get card ID (type, size) */
static int mc_get_id(int port, struct mc_id *id)
{
    u8 rx[32];
    int ret;

    ret = mc_send_command(port, MC_CMD_GET_ID, NULL, 0, rx, 24);
    if (ret)
        return ret;

    if (rx[0] != MC_RESPONSE_OK && rx[0] != MC_RESPONSE_ACK)
        return -EIO;

    id->type = rx[1];
    id->size = rx[2];
    memcpy(id->manufacturer, rx + 4, 8);
    memcpy(id->product, rx + 12, 8);
    memcpy(id->version, rx + 20, 8);

    return 0;
}

/* Read a block (8192 bytes = 128 sectors) */
static int mc_read_block(int port, u16 block, u8 *buf)
{
    int sector, i;
    u8 status;

    if (block >= MC_BLOCKS_PER_CARD)
        return -EINVAL;

    for (sector = 0; sector < 128; sector++) {
        mc_select(port);
        mc_write_byte(MC_CMD_READ);
        mc_write_byte((block >> 8) & 0xFF);
        mc_write_byte(block & 0xFF);
        mc_write_byte(sector);

        /* Read sector data + status */
        status = mc_read_byte();
        if (status != MC_RESPONSE_OK && status != MC_RESPONSE_ACK) {
            mc_deselect();
            return -EIO;
        }

        for (i = 0; i < 64; i++)
            buf[sector * 64 + i] = mc_read_byte();

        mc_deselect();
    }

    return 0;
}

/* Write a block (8192 bytes) */
static int mc_write_block(int port, u16 block, const u8 *buf)
{
    int sector, i;
    u8 status, status2;

    if (block >= MC_BLOCKS_PER_CARD)
        return -EINVAL;

    for (sector = 0; sector < 128; sector++) {
        mc_select(port);
        mc_write_byte(MC_CMD_WRITE);
        mc_write_byte((block >> 8) & 0xFF);
        mc_write_byte(block & 0xFF);
        mc_write_byte(sector);

        /* Write 64 bytes of data */
        for (i = 0; i < 64; i++)
            mc_write_byte(buf[sector * 64 + i]);

        /* Read status */
        status = mc_read_byte();
        status2 = mc_read_byte();

        mc_deselect();

        if (status != MC_RESPONSE_OK && status != MC_RESPONSE_ACK)
            return -EIO;
    }

    return 0;
}

/* ---- File operations ---- */

static struct mc_state {
    struct mc_id id;
    int present;
    struct semaphore sem;
} mc_state[PSXMEMCARD_MAX_PORTS];

static int psxmemcard_open(struct inode *inode, struct file *filp)
{
    int port = MINOR(inode->i_rdev);
    struct mc_state *state;

    if (port >= PSXMEMCARD_MAX_PORTS)
        return -ENODEV;

    state = &mc_state[port];
    down(&state->sem);

    if (!state->present) {
        /* Probe for card */
        if (mc_get_id(port, &state->id) == 0) {
            state->present = 1;
        } else {
            up(&state->sem);
            return -ENODEV;
        }
    }

    filp->private_data = (void *)(long)port;
    up(&state->sem);
    return 0;
}

static int psxmemcard_release(struct inode *inode, struct file *filp)
{
    int port = (int)(long)filp->private_data;
    struct mc_state *state = &mc_state[port];
    up(&state->sem);
    return 0;
}

static ssize_t psxmemcard_read(struct file *filp, char *buf,
                               size_t count, loff_t *ppos)
{
    int port = (int)(long)filp->private_data;
    struct mc_state *state = &mc_state[port];
    u16 block = *ppos / MC_BLOCK_SIZE;
    int offset = *ppos % MC_BLOCK_SIZE;
    int to_read = count;
    u8 *tmp;
    int ret = 0;
    int bytes_read = 0;

    if (!state->present)
        return -ENODEV;

    tmp = kmalloc(MC_BLOCK_SIZE, GFP_KERNEL);
    if (!tmp)
        return -ENOMEM;

    down(&state->sem);

    while (to_read > 0 && block < MC_BLOCKS_PER_CARD) {
        int chunk = MC_BLOCK_SIZE - offset;
        if (chunk > to_read)
            chunk = to_read;

        if (mc_read_block(port, block, tmp) != 0) {
            ret = -EIO;
            break;
        }

        if (copy_to_user(buf, tmp + offset, chunk)) {
            ret = -EFAULT;
            break;
        }

        buf += chunk;
        to_read -= chunk;
        bytes_read += chunk;
        *ppos += chunk;
        block++;
        offset = 0;
    }

    up(&state->sem);
    kfree(tmp);
    return ret ? ret : bytes_read;
}

static ssize_t psxmemcard_write(struct file *filp, const char *buf,
                                size_t count, loff_t *ppos)
{
    int port = (int)(long)filp->private_data;
    struct mc_state *state = &mc_state[port];
    u16 block = *ppos / MC_BLOCK_SIZE;
    int offset = *ppos % MC_BLOCK_SIZE;
    int to_write = count;
    u8 *tmp;
    int ret = 0;
    int bytes_written = 0;

    if (!state->present)
        return -ENODEV;

    tmp = kmalloc(MC_BLOCK_SIZE, GFP_KERNEL);
    if (!tmp)
        return -ENOMEM;

    down(&state->sem);

    while (to_write > 0 && block < MC_BLOCKS_PER_CARD) {
        int chunk = MC_BLOCK_SIZE - offset;
        if (chunk > to_write)
            chunk = to_write;

        /* Read-modify-write for partial blocks */
        if (offset != 0 || chunk != MC_BLOCK_SIZE) {
            if (mc_read_block(port, block, tmp) != 0) {
                ret = -EIO;
                break;
            }
        }

        if (copy_from_user(tmp + offset, buf, chunk)) {
            ret = -EFAULT;
            break;
        }

        if (mc_write_block(port, block, tmp) != 0) {
            ret = -EIO;
            break;
        }

        buf += chunk;
        to_write -= chunk;
        bytes_written += chunk;
        *ppos += chunk;
        block++;
        offset = 0;
    }

    up(&state->sem);
    kfree(tmp);
    return ret ? ret : bytes_written;
}

static int psxmemcard_ioctl(struct inode *inode, struct file *filp,
                            unsigned int cmd, unsigned long arg)
{
    int port = (int)(long)filp->private_data;
    struct mc_state *state = &mc_state[port];
    int ret = 0;

    down(&state->sem);

    switch (cmd) {
    case PSXMEMCARD_IOC_GET_ID:
        if (copy_to_user((void *)arg, &state->id, sizeof(state->id)))
            ret = -EFAULT;
        break;

    case PSXMEMCARD_IOC_GET_SIZE:
        {
            int size = MC_CARD_SIZE;
            if (copy_to_user((void *)arg, &size, sizeof(int)))
                ret = -EFAULT;
        }
        break;

    case PSXMEMCARD_IOC_FORMAT:
        /* Not implemented - would send format command */
        ret = -ENOTTY;
        break;

    default:
        ret = -ENOTTY;
    }

    up(&state->sem);
    return ret;
}

static struct file_operations psxmemcard_fops = {
    owner:      NULL,
    open:       psxmemcard_open,
    release:    psxmemcard_release,
    read:       psxmemcard_read,
    write:      psxmemcard_write,
    ioctl:      psxmemcard_ioctl,
    llseek:     default_llseek,
};

/* ---- Module init/exit ---- */

int __init psxmemcard_init(void)
{
    int ret;
    int i;

    for (i = 0; i < PSXMEMCARD_MAX_PORTS; i++) {
        sema_init(&mc_state[i].sem, 1);
        mc_state[i].present = 0;
    }

    /* Initialize SIO1 for memory card communication */
    /* Note: SIO1 is shared with controller - already initialized by psxpad */
    /* But we ensure correct mode here */
    writew(SIO1_B250000, SIO1_RATE_REG);
    writew(SIO1_BRS16 | SIO1_CHR8 | SIO1_SB1, SIO1_MODE_REG);

    ret = register_chrdev(PSXMEMCARD_MAJOR, "psxmemcard", &psxmemcard_fops);
    if (ret) {
        printk(KERN_ERR "psxmemcard: can't get major %d\n", PSXMEMCARD_MAJOR);
        return ret;
    }

#ifdef CONFIG_DEVFS_FS
    for (i = 0; i < PSXMEMCARD_MAX_PORTS; i++) {
        devfs_register(NULL, "psxmemcard", DEVFS_FL_DEFAULT,
                       PSXMEMCARD_MAJOR, i,
                       S_IFCHR | S_IRUSR | S_IWUSR,
                       &psxmemcard_fops, NULL);
    }
#endif

    printk(KERN_INFO "psxmemcard: PSX Memory Card driver, major %d\n", PSXMEMCARD_MAJOR);
    return 0;
}

void __exit psxmemcard_exit(void)
{
    unregister_chrdev(PSXMEMCARD_MAJOR, "psxmemcard");
}

module_init(psxmemcard_init);
module_exit(psxmemcard_exit);