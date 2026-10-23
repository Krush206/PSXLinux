/*
 * psxpad - PlayStation Controller character device.
 *
 * Exposes the controller interface to userspace via /dev/psxpad.
 * Supports standard digital, DualShock analog, and multitap.
 *
 * Protocol reference: http://psx-spx.github.io/#controllers
 *   SIO1 port: 0x1F801060
 *   Controller selected by DTR (CS) low
 *   Memory card selected by RTS (CS) low
 *   Standard speed: 250000 bps
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
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <asm/ps/sio1.h>
#include <linux/psxpad.h>

/* ---- Register access ---- */

static inline void sio1_wait_ready_cmd(void)
{
    while (!(readw(SIO1_STAT_REG) & SIO1_RFW))
        ;
}

static inline void sio1_wait_ready_data(void)
{
    while (!(readw(SIO1_STAT_REG) & SIO1_RFR))
        ;
}

static inline void sio1_write_byte(u8 b)
{
    sio1_wait_ready_cmd();
    writeb(b, SIO1_DATA_REG);
}

static inline u8 sio1_read_byte(void)
{
    sio1_wait_ready_data();
    return readb(SIO1_DATA_REG);
}

static inline void sio1_select_controller(void)
{
    /* DTR low = controller CS, RTS high = memcard CS inactive */
    writew((readw(SIO1_CTRL_REG) & ~SIO1_DTR) | SIO1_RTS, SIO1_CTRL_REG);
}

static inline void sio1_select_memcard(void)
{
    /* RTS low = memcard CS, DTR high = controller CS inactive */
    writew((readw(SIO1_CTRL_REG) & ~SIO1_RTS) | SIO1_DTR, SIO1_CTRL_REG);
}

static inline void sio1_deselect_all(void)
{
    /* Both CS high */
    writew(readw(SIO1_CTRL_REG) | SIO1_DTR | SIO1_RTS, SIO1_CTRL_REG);
}

/* ---- Controller communication ---- */

#define PSXPAD_MAX_PORTS     2

/* Controller types */
#define PSXPAD_TYPE_NONE      0x00
#define PSXPAD_TYPE_DIGITAL   0x41
#define PSXPAD_TYPE_MOUSE     0x42
#define PSXPAD_TYPE_NEGCON    0x43
#define PSXPAD_TYPE_ANALOG    0x73  /* DualShock in analog mode */
#define PSXPAD_TYPE_MULTITAP  0x80

/* Button bit masks (returned in standard 32-bit format) */
#define PSXPAD_BTN_SELECT     (1u << 0)
#define PSXPAD_BTN_L3         (1u << 1)
#define PSXPAD_BTN_R3         (1u << 2)
#define PSXPAD_BTN_START      (1u << 3)
#define PSXPAD_BTN_UP         (1u << 4)
#define PSXPAD_BTN_RIGHT      (1u << 5)
#define PSXPAD_BTN_DOWN       (1u << 6)
#define PSXPAD_BTN_LEFT       (1u << 7)
#define PSXPAD_BTN_L2         (1u << 8)
#define PSXPAD_BTN_R2         (1u << 9)
#define PSXPAD_BTN_L1         (1u << 10)
#define PSXPAD_BTN_R1         (1u << 11)
#define PSXPAD_BTN_TRIANGLE   (1u << 12)
#define PSXPAD_BTN_CIRCLE     (1u << 13)
#define PSXPAD_BTN_CROSS      (1u << 14)
#define PSXPAD_BTN_SQUARE     (1u << 15)

/* Internal controller state */
static struct psxpad_state pad_state[PSXPAD_MAX_PORTS];
static wait_queue_head_t pad_wait[PSXPAD_MAX_PORTS];
static int pad_data_ready[PSXPAD_MAX_PORTS];

/* Read a single controller (port 0 or 1) */
static int psxpad_read_controller(int port, struct psxpad_state *out)
{
    u8 response[32];
    u8 cmd[5] = { 0x01, 0x42, 0x00, 0x00, 0x00 };
    int i;

    if (port < 0 || port >= PSXPAD_MAX_PORTS)
        return -ENODEV;

    sio1_select_controller();

    /* Send command sequence */
    for (i = 0; i < 5; i++)
        sio1_write_byte(cmd[i]);

    /* Read response - first byte is controller type, second is button count */
    response[0] = sio1_read_byte();  /* should be 0xFF (idle) */
    response[1] = sio1_read_byte();  /* controller type */

    if (response[1] == PSXPAD_TYPE_NONE) {
        sio1_deselect_all();
        out->connected = 0;
        out->type = 0;
        return 0;
    }

    out->type = response[1];
    out->connected = 1;

    /* Read button data (2 bytes for digital, more for analog) */
    response[2] = sio1_read_byte();  /* buttons low */
    response[3] = sio1_read_byte();  /* buttons high */

    out->buttons = ((u32)response[3] << 8) | response[2];
    out->buttons = ~out->buttons;  /* invert: 1 = pressed */

    /* If DualShock analog, read analog sticks */
    if (response[1] == PSXPAD_TYPE_ANALOG) {
        /* Send additional bytes to get analog data */
        sio1_write_byte(0x00);
        response[4] = sio1_read_byte();  /* right stick X */
        sio1_write_byte(0x00);
        response[5] = sio1_read_byte();  /* right stick Y */
        sio1_write_byte(0x00);
        response[6] = sio1_read_byte();  /* left stick X */
        sio1_write_byte(0x00);
        response[7] = sio1_read_byte();  /* left stick Y */

        out->rjoy_x = response[4];
        out->rjoy_y = response[5];
        out->ljoy_x = response[6];
        out->ljoy_y = response[7];
    } else {
        out->rjoy_x = out->rjoy_y = out->ljoy_x = out->ljoy_y = 0x80;
    }

    sio1_deselect_all();
    return 0;
}

/* Read all connected controllers */
static void psxpad_poll_all(void)
{
    int i;
    for (i = 0; i < PSXPAD_MAX_PORTS; i++) {
        if (psxpad_read_controller(i, &pad_state[i]) == 0) {
            pad_data_ready[i] = 1;
            wake_up_interruptible(&pad_wait[i]);
        }
    }
}

/* ---- File operations ---- */

static int psxpad_open(struct inode *inode, struct file *filp)
{
    int port = MINOR(inode->i_rdev);

    if (port >= PSXPAD_MAX_PORTS)
        return -ENODEV;

    filp->private_data = (void *)(long)port;
    return 0;
}

static int psxpad_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t psxpad_read(struct file *filp, char *buf,
                           size_t count, loff_t *ppos)
{
    int port = (int)(long)filp->private_data;
    struct psxpad_state *state = &pad_state[port];
    size_t to_copy;

    if (count < sizeof(struct psxpad_state))
        return -EINVAL;

    /* Wait for new data if non-blocking not set */
    if (!(filp->f_flags & O_NONBLOCK)) {
        wait_event_interruptible(pad_wait[port], pad_data_ready[port]);
        if (signal_pending(current))
            return -ERESTARTSYS;
    }

    pad_data_ready[port] = 0;
    to_copy = sizeof(struct psxpad_state);
    if (copy_to_user(buf, state, to_copy))
        return -EFAULT;

    return to_copy;
}

static unsigned int psxpad_poll(struct file *filp, poll_table *wait)
{
    int port = (int)(long)filp->private_data;
    unsigned int mask = 0;

    poll_wait(filp, &pad_wait[port], wait);
    if (pad_data_ready[port])
        mask |= POLLIN | POLLRDNORM;

    return mask;
}

static int psxpad_ioctl(struct inode *inode, struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
    int port = (int)(long)filp->private_data;
    struct psxpad_state *state = &pad_state[port];

    switch (cmd) {
    case PSXPAD_IOC_GET_STATE:
        if (copy_to_user((void *)arg, state, sizeof(*state)))
            return -EFAULT;
        break;

    case PSXPAD_IOC_GET_BUTTONS:
        if (copy_to_user((void *)arg, &state->buttons, sizeof(state->buttons)))
            return -EFAULT;
        break;

    case PSXPAD_IOC_GET_TYPE:
        if (copy_to_user((void *)arg, &state->type, sizeof(state->type)))
            return -EFAULT;
        break;

    case PSXPAD_IOC_POLL_NOW:
        /* Force immediate read */
        psxpad_read_controller(port, state);
        pad_data_ready[port] = 1;
        wake_up_interruptible(&pad_wait[port]);
        break;

    case PSXPAD_IOC_SET_ANALOG:
        /* Enable/disable analog mode on DualShock */
        /* Not implemented - requires mode switch command */
        return -ENOTTY;

    case PSXPAD_IOC_RUMBLE:
        /* Rumble control for DualShock */
        /* Not implemented - requires motor control commands */
        return -ENOTTY;

    default:
        return -ENOTTY;
    }
    return 0;
}

static struct file_operations psxpad_fops = {
    owner:      NULL,
    open:       psxpad_open,
    release:    psxpad_release,
    read:       psxpad_read,
    poll:       psxpad_poll,
    ioctl:      psxpad_ioctl,
};

/* ---- Timer-based polling ---- */

static struct timer_list pad_timer;

static void psxpad_timer_fn(unsigned long data)
{
    psxpad_poll_all();
    mod_timer(&pad_timer, jiffies + HZ / 60);  /* 60 Hz polling */
}

int __init psxpad_init(void)
{
    int ret;
    int i;

    /* Initialize wait queues */
    for (i = 0; i < PSXPAD_MAX_PORTS; i++) {
        init_waitqueue_head(&pad_wait[i]);
        pad_data_ready[i] = 0;
    }

    /* Initialize SIO1 for controller communication */
    writew(0x50, SIO1_CTRL_REG);           /* reset */
    writew(SIO1_B250000, SIO1_RATE_REG);   /* 250000 bps */
    writew(SIO1_BRS16 | SIO1_CHR8 | SIO1_SB1, SIO1_MODE_REG);  /* 8N1 */
    writew(SIO1_TX | SIO1_RX | SIO1_DTR | SIO1_RTS, SIO1_CTRL_REG);

    ret = register_chrdev(PSXPAD_MAJOR, "psxpad", &psxpad_fops);
    if (ret) {
        printk(KERN_ERR "psxpad: can't get major %d\n", PSXPAD_MAJOR);
        return ret;
    }

#ifdef CONFIG_DEVFS_FS
    for (i = 0; i < PSXPAD_MAX_PORTS; i++) {
        devfs_register(NULL, "psxpad", DEVFS_FL_DEFAULT,
                       PSXPAD_MAJOR, i,
                       S_IFCHR | S_IRUSR | S_IWUSR,
                       &psxpad_fops, NULL);
    }
#endif

    /* Start polling timer */
    init_timer(&pad_timer);
    pad_timer.function = psxpad_timer_fn;
    pad_timer.expires = jiffies + HZ / 60;
    add_timer(&pad_timer);

    printk(KERN_INFO "psxpad: PSX controller driver, major %d\n", PSXPAD_MAJOR);
    return 0;
}

void __exit psxpad_exit(void)
{
    del_timer(&pad_timer);
    unregister_chrdev(PSXPAD_MAJOR, "psxpad");
}

module_init(psxpad_init);
module_exit(psxpad_exit);