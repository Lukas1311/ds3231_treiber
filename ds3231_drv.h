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
#include <linux/mutex.h>
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
#define DS3231_REG_TEMP	   0x11

/* Datum und Uhrzeit Definition */
#define DS3231_SECONDS		0x00
#define DS3231_MINUTES		0x01
#define DS3231_HOURS		0x02
#define DS3231_DAYS		    0x03
#define DS3231_DATE		    0x04
#define DS3231_MONTHS		0x05
#define DS3231_YEARS		0x06

/* Definitionen für das Auslesen der Uhrzeit */
#define DS3231_SECSBITS     0b01111111
#define DS3231_MINSBITS     0b01111111
#define DS3231_HRSBITS      0b00111111
#define DS3231_12n24        0b01000000
#define DS3231_DAYSBITS     0b00111111
#define DS3231_MONTHSBITS   0b00011111
#define DS3231_CENTURYBITS  0b10000000
#define DS3231_YEARSBITS    0b11111111

/* Definitionen für das Auslesen des Status des RTC-Chips */
#define DS3231_OSFBIT       0b10000000
#define DS3231_BSYBIT       0b00000100

/* Struktur zur Speicherung und Übergeben der Uhrzeit */
typedef struct time {
    uint16_t seconds, minutes, hours, months, days;
    uint32_t years;
} ds3231_time_t;

/* Struktur zur Speicherung und Übergeben des Status des RTC-Chips */
typedef struct status {
    uint8_t osf, bsy, full;
    int8_t temp;
} ds3231_status_t;

/* Funktionsdefinition */

static ssize_t mein_read(struct file *file, char __user* puffer, size_t bytes, loff_t *offset);

static ssize_t mein_write(struct file *file, const char __user* puffer, size_t bytes, loff_t *offset);

static int mein_open(struct inode *inode, struct file *file);

static int mein_close(struct inode *inode, struct file *file);


static int ds3231_probe(struct i2c_client *client, const struct i2c_device_id *id);

static int mein_remove(struct i2c_client *client);

static int date_check(ds3231_time_t* time);

/* Geräte Dateien */
static dev_t ds3231_device;
static struct cdev cds3231_device;
static struct class *ds3231_device_class;


static struct i2c_client *ds3231_client;


static spinlock_t the_lock;
