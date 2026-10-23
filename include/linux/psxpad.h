/*
 * PSX Controller (psxpad) ioctl interface
 */

#ifndef _LINUX_PSXPAD_H
#define _LINUX_PSXPAD_H

#include <linux/ioctl.h>

/* Major number */
#define PSXPAD_MAJOR        210

/* Controller types */
#define PSXPAD_TYPE_NONE      0x00
#define PSXPAD_TYPE_DIGITAL   0x41
#define PSXPAD_TYPE_MOUSE     0x42
#define PSXPAD_TYPE_NEGCON    0x43
#define PSXPAD_TYPE_ANALOG    0x73  /* DualShock analog mode */
#define PSXPAD_TYPE_MULTITAP  0x80

/* Button masks (matching psxpad_state.buttons) */
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

/* Controller state structure */
struct psxpad_state {
    __u32 buttons;    /* digital buttons (inverted: 1 = pressed) */
    __u8  rjoy_x;     /* right analog stick X (0-255, 128 = center) */
    __u8  rjoy_y;     /* right analog stick Y */
    __u8  ljoy_x;     /* left analog stick X */
    __u8  ljoy_y;     /* left analog stick Y */
    __u8  type;       /* controller type */
    __u8  connected;  /* 1 if controller present */
    __u8  reserved[2];
};

/* Ioctl commands */
#define PSXPAD_IOC_MAGIC    'P'

#define PSXPAD_IOC_GET_STATE   _IOR(PSXPAD_IOC_MAGIC, 0x01, struct psxpad_state)
#define PSXPAD_IOC_GET_BUTTONS _IOR(PSXPAD_IOC_MAGIC, 0x02, __u32)
#define PSXPAD_IOC_GET_TYPE    _IOR(PSXPAD_IOC_MAGIC, 0x03, __u8)
#define PSXPAD_IOC_POLL_NOW    _IO (PSXPAD_IOC_MAGIC, 0x10)
#define PSXPAD_IOC_SET_ANALOG  _IOW(PSXPAD_IOC_MAGIC, 0x11, __u8)
#define PSXPAD_IOC_RUMBLE      _IOW(PSXPAD_IOC_MAGIC, 0x12, __u8)

#define PSXPAD_IOC_MAXNR       0x12

#endif /* _LINUX_PSXPAD_H */