#include <linux/slab.h>
#include <linux/bcd.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <asm/errno.h>
#include <asm/delay.h>


/* Register Definitionen */
#define DS3231_REG_CONTROL 0x0e
#define DS3231_BIT_nEOSC   0x80
#define DS3231_BIT_INTCN   0x04
#define DS3231_BIT_A2IE	   0x02
#define DS3231_BIT_A1IE	   0x01
#define DS3231_REG_STATUS  0x0f
#define DS3231_BIT_OSF     0x80


/*
 * Funktionsdeklaration
 */

static ssize_t mein_read(struct file *file, char __user* puffer, size_t bytes, loff_t *offset);

static ssize_t mein_write(struct file *file, const char __user* puffer, size_t bytes, loff_t *offset);

static int mein_open(struct inode *inode, struct file *file);

static int mein_close(struct inode *inode, struct file *file);
