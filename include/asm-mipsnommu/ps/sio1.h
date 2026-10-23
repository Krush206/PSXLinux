/* 
 *    PlayStation SIO1 (Controller/Memory Card) Registers
 */

#ifndef __ASM_PS_SIO1_H 
#define __ASM_PS_SIO1_H 

/*
 * SIO1 registers (Controller/Memory Card port)
 * Base: 0x1F801060
 */
#define SIO1_DATA_REG   0x1F801060  /* data register */
#define SIO1_STAT_REG   0x1F801064  /* status register */
#define SIO1_MODE_REG   0x1F801068  /* mode register */
#define SIO1_CTRL_REG   0x1F80106A  /* control register */
#define SIO1_RATE_REG   0x1F80106E  /* baudrate register */

/* SIO1 status register bits (same as SIO0) */
#define SIO1_RFW        0x0001      /* ready for write */
#define SIO1_RFR        0x0002      /* ready for read */
#define SIO1_RFT        0x0004      /* transmitter empty */
#define SIO1_PERR       0x0008      /* parity error */
#define SIO1_OERR       0x0010      /* overrun error */
#define SIO1_FERR       0x0020      /* frame error */
#define SIO1_DSR        0x0080      /* DSR */
#define SIO1_CTS        0x0100      /* CTS */
#define SIO1_IRQ        0x0200      /* interrupt */

/* SIO1 control register bits (same as SIO0) */
#define SIO1_TX         0x0001      /* TX enable */
#define SIO1_DTR        0x0002      /* DTR - used as CS for controller */
#define SIO1_RX         0x0004      /* RX enable */
#define SIO1_ACKIRQ     0x0010      /* acknowledge irq */
#define SIO1_RTS        0x0020      /* RTS - used as CS for memory card */
#define SIO1_RESET      0x0040      /* SIO reset */
#define SIO1_BUF2       0x0100      /* buffer size = 2 bytes */
#define SIO1_BUF4       0x0200      /* buffer size = 4 bytes */
#define SIO1_BUF8       0x0300      /* buffer size = 8 bytes */
#define SIO1_TXIRQ      0x0400      /* TX interrupt enable */
#define SIO1_RXIRQ      0x0800      /* RX interrupt enable */
#define SIO1_DSRIRQ     0x1000      /* DSR interrupt enable */
#define SIO1_ALLIRQ     (SIO1_DSRIRQ|SIO1_RXIRQ|SIO1_TXIRQ)

/* SIO1 mode register bits (same as SIO0) */
#define SIO1_BRS1       0x0001      /* baudrate prescaler = 1 */
#define SIO1_BRS16      0x0002      /* baudrate prescaler = 16 */
#define SIO1_BRS64      0x0003      /* baudrate prescaler = 64 */
#define SIO1_CHR6       0x0004      /* char length = 6 bit */
#define SIO1_CHR7       0x0008      /* char length = 7 bit */
#define SIO1_CHR8       0x000c      /* char length = 8 bit */
#define SIO1_PARITY     0x0010      /* parity bit: 0 - off, 1 - on */
#define SIO1_PAROOD     0x0020      /* parity mode: 0 - even, 1 - odd */
#define SIO1_SB1        0x0040      /* stop bit length = 1 */
#define SIO1_SB1_5      0x0080      /* stop bit length = 1.5 */
#define SIO1_SB2        0x00c0      /* stop bit length = 2 */

/* Baud rates */
#define SIO1_B250000    0x01        /* 250000 bps (controller speed) */
#define SIO1_B500000    0x00        /* 500000 bps */

#ifndef __ASSEMBLY__

/* SIO1 function prototypes */
int ps_sio1_get_status(void);
int ps_sio1_get_control(void);
int ps_sio1_get_mode(void);
int ps_sio1_get_baudrate(void);
int ps_sio1_get_byte(void);

void ps_sio1_set_control(int value);
void ps_sio1_set_mode(int value);
void ps_sio1_set_baudrate(int value);
void ps_sio1_set_byte(int value);

#endif /* __ASSEMBLY__ */

#endif /* __ASM_PS_SIO1_H */