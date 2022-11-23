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
#define DS3231_REG_CONTROL	0x0e
# define DS3231_BIT_nEOSC	0x80
# define DS3231_BIT_INTCN	0x04
# define DS3231_BIT_A2IE	0x02
# define DS3231_BIT_A1IE	0x01
#define DS3231_REG_STATUS	0x0f
# define DS3231_BIT_OSF		0x80


/* CHANGES HERE*/
 
static dev_t mein_first_dev;
static struct cdev mein_cdev;
static struct class *mein_device_class;

static struct file_operations mein_fops = 
{
	.open= mein_open,
	.release= mein_release,
	.read= mein_read,
	.write= mein_write
};

static int __init mein_treiber_init(void)
{
	int ret;

	/*
	 * Gerätenummer für 1 Gerät allozieren
	 */

	ret = alloc_chrdev_region(&mein_first_dev, 0, 1, "mein_treiber");
	if(ret < 0) {
		printk(KERN_ALERT "Fehler bei alloc_chrdev_region()\n");
		return ret;
	}

	/*
	 * cdev-Struktur initialisieren und dem Kernel bekannt machen
	 */
	cdev_init(&mein_cdev, &mein_fops);
	ret = cdev_add(&mein_cdev, mein_first_dev, 1);
	if(ret < 0) {
		printk(KERN_ALERT "Fehler bei Registrierung von CDEV-Struktur");
		goto unreg_chrdev;
	}

	/*
	 *Eintrag im sysfs registrieren. Dadurch wird die Device-Datei automatisch von udev Dienst erstellt.
	 */
	mein_device_class = class_create(THIS_MODULE, "chardev");
	if(mein_device_class == NULL) {
		printk(KERN_ALERT "Class konnte nicht erstellt werden.\n");
		goto clenup_cdev;
	}

	if(device_create(mein_device_class,
			 NULL,
			 mein_first_dev,
			 NULL,
			 "mein_treiber") == NULL) {
		printk(KERN_ALERT "Device konnte nicht erstellt werden.\n");
		goto cleanup_chrdev_class;
	}
	
	/*
	 * mein_treiber wurde erfolgreich initialisert
	 */
	return 0;

	/*
	 * Resourcen freigeben und Fehler melden
	 */
	cleanup_chrdev_class:
	  class_destroy(mein_device_class);
	clenup_cdev:
	  cdev_del(&mein_cdev);
	unreg_chrdev:
	  unregister_chrdev_region(mein_first_dev, 1);
	 
	return 0;
}

static void __exit mein_treiber_exit(void)
{
	printk("Treiber entladen\n");
	device_destroy(mein_device_class, mein_first_dev);
	class_destroy(mein_device_class);
	cdev_del(&mein_cdev);
	unregister_chrdev_region(mein_first_dev, 1);
}


static int mein_open(struct inode *inode, struct file *file)
{
	return 0;  //Zugriffschutz beachten
}

static int mein_close(struct inode *inode, struct file *file) 
{
	return 0; //Zugriffschutz beachten
}

static ssize_t mein_read(struct file *file,
			 char __user* puffer,
			 size_t bytes,
			 loff_t *offset)
{
	count = copy_to_user(puffer, string, N);
	return N -count;
}

static ssize_t mein_write(struct file *file,
			  const char __user* puffer,
			  size_t bytes,
			  loff_t *offset)
{
	copy_from_user(string, puffer, bytes);
	return bytes;
}

 /*CHANGES HERE*/



/*
 * Der Zeiger wird bei Initialisierung gesetzt und wird für die
 * i2c Kommunikation mit dem  Device (DS3231) benötigt.
 */
static struct i2c_client *ds3231_client;


/*
 * Initialisierung des Treibers und Devices.
 *
 * Diese Funktion wird von Linux-Kernel aufgerufen, aber erst nachdem ein zum
 * Treiber passende Device-Information gefunden wurde. Innerhalb der Funktion
 * wird der Treiber und das Device initialisiert.
 */
static int ds3231_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	s32 reg0, reg1;
	u8 reg_cnt, reg_sts;

	printk("DS3231_drv: ds3231_probe called\n");

	/*
	 * Control und Status Register auslesen.
	 */
	reg0 = i2c_smbus_read_byte_data(client, DS3231_REG_CONTROL);
	reg1 = i2c_smbus_read_byte_data(client, DS3231_REG_STATUS);
	if(reg0 < 0 || reg1 < 0) {
		printk("DS3231_drv: Fehler beim Lesen von Control oder Status Register.\n");
		return -ENODEV;
	}
	reg_cnt = (u8)reg0;
	reg_sts = (u8)reg1;
	printk("DS3231_drv: Control: 0x%02X, Status: 0x%02X\n", reg_cnt, reg_sts);

	/* 
	 * Prüfen ob der Oscilattor abgeschaltet ist, falls ja, muss dieser
	 * eingeschltet werden (damit die Zeit läuft).
	 */
	if (reg_cnt & DS3231_BIT_nEOSC) {
		printk("DS3231_drv: Oscilator einschalten\n");
		reg_cnt &= ~DS3231_BIT_nEOSC;
	}

	printk("DS3231_drv: Interrupt und Alarms abschalten\n");
	reg_cnt &= ~(DS3231_BIT_INTCN | DS3231_BIT_A2IE | DS3231_BIT_A1IE);

	/* Control-Register setzen */
	i2c_smbus_write_byte_data(client, DS3231_REG_CONTROL, reg_cnt);

	/*
	 * Prüfe Oscilator zustand. Falls Fehler vorhanden, wird das Fehlerfalg
	 * zurückgesetzt.
	 */
	if (reg_sts & DS3231_BIT_OSF) {
		reg_sts &= ~DS3231_BIT_OSF;
		i2c_smbus_write_byte_data(client, DS3231_REG_STATUS, reg_sts);
		printk("DS3231_drv: Oscilator Stop Flag (OSF) zurückgesetzt.\n");
	}

	/* DS3231 erfolgreich initialisiert */
	return 0;
}


/*
 * Freigebe der Resourcen.
 *
 * Diese Funktion wird beim Entfernen des Treibers oder Gerätes
 * von Linux-Kernel aufgerufen. Hier sollten die Resourcen, welche
 * in der "ds3231_probe()" Funktion angefordert wurden, freigegeben.
 */
static int ds3231_remove(struct i2c_client *client)
{
	printk("DS3231_drv: ds3231_remove called\n");
	return 0;
}


/*
 * Device-Id. Wird für die Zuordnung des Treibers zum Gerät benötigt.
 * Das für den Treiber passendes Gerät mit der hier definierten Id wird
 * bei der Initialisierung des Moduls hinzugefügt.
 */
static const struct i2c_device_id ds3231_dev_id[] = {
	{ "ds3231_drv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds3231_dev_id);


/*
 * I2C Treiber-Struktur. Wird für die Registrierung des Treibers im
 * Linux-Kernel benötigt.
 */
static struct i2c_driver ds3231_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "ds3231_drv",
	},
	.id_table = ds3231_dev_id,
	.probe	  = ds3231_probe,
	.remove	  = ds3231_remove,
};


/*
 * Initialisierungsroutine des Kernel-Modules.
 * 
 * Wird beim Laden des Moduls aufgerufen. Innerhalb der Funktion
 * wird das neue Device (DS3231) registriert und der I2C Treiber
 * hinzugefügt.
 */
static int __init ds3231_module_init(void)
{
	int ret;
	struct i2c_adapter *adapter;

	/*
	 * Normaleweise werden die Informationen bezüglich der verbauten
	 * Geräten (Devices) während der Kernel-Initialisierung mittels
	 * eines Device-Trees Eintrages definiert. Anhand dieser Informationen
	 * sucht der Kernel einen passenden Treiber für das Gerät (i2c_device_id).
	 * In unserem Fall müssen die Informationen nachträglich definiert
	 * und hinzugefügt werden.
	 */
	const struct i2c_board_info info = {
		I2C_BOARD_INFO("ds3231_drv", 0x68)
	};

	printk("DS3231_drv: ds3231_module_init aufgerufen\n");

	ds3231_client = NULL;
	adapter = i2c_get_adapter(1);
	if(adapter == NULL) {
		printk("DS3231_drv: I2C Adapter nicht gefunden\n");
		return -ENODEV;
	}

	/* Neues I2C Device registrieren */
	ds3231_client = i2c_new_client_device(adapter, &info);
	if(ds3231_client == NULL) {
		printk("DS3231_drv: I2C Client: Registrierung fehlgeschlagen\n");
		return -ENODEV;
	}

	/* Treiber registrieren */
	ret = i2c_add_driver(&ds3231_driver);
	if(ret < 0) {
		printk("DS3231_drv: Treiber konnte nicht hinzugefügt werden (errorn = %d)\n", ret);
		i2c_unregister_device(ds3231_client);
		ds3231_client = NULL;
	}
	return ret;
}
module_init(ds3231_module_init);


/*
 * Aufräumroutine des Kernel-Modules.
 * 
 * Wird beim Enladen des Moduls aufgerufen. Innerhalb der Funktion
 * werden alle Resourcen wieder freigegeben.
 */
static void __exit ds3231_module_exit(void)
{
	printk("DS3231_drv: ds3231_module_exit aufgerufen\n");
	if(ds3231_client != NULL) {
		i2c_del_driver(&ds3231_driver);
		i2c_unregister_device(ds3231_client);
	}
}
module_exit(ds3231_module_exit);


/* Module-Informationen. */
MODULE_AUTHOR("Klaus Musterman <musterr@uni-kassel.de>");
MODULE_DESCRIPTION("Beschreibung");
MODULE_LICENSE("GPL");
