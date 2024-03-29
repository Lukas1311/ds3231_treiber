/*******************************************************************************
* Institut für Rechnerarchitektur und Systemprogrammierung *
* Universität Kassel *
*******************************************************************************
* Benutzeraccount : sysprog-04 *
*******************************************************************************
* Author-1 : Lukas Nils Lingmann
* Matrikelnummer-1 : 35826330

*******************************************************************************
* Author-2 : Alexander Haar
* Matrikelnummer-2 : 35679594
*******************************************************************************
* Beschreibung: Linux Treiber für RTC-DS3231
* Version: 1.0
******************************************************************************/

#include "ds3231_drv.h"

/*
 *File Operations für die Registrierung der Funktionen und das Modul
 */
static struct file_operations ds3231_fops = {
        .owner   = THIS_MODULE,
        .llseek  = no_llseek,
        .read    = ds3231_read,
        .write   = ds3231_write,
        .open    = ds3231_open,
        .release = ds3231_close,
};

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

static int date_check(ds3231_time_t *time) {
    uint8_t val = 0;

    /*
     * Überprüfung auf Gültigkeit des Datums
     */
    if (time->years < 2000 || time->years > 2100) {
        val = 2;
    } else if (time->seconds < 0 || time->seconds > 59) {
        val = 1;
    } else if (time->minutes < 0 || time->minutes > 59) {
        val = 1;
    } else if (time->hours < 0 || time->hours > 23) {
        val = 1;
    } else if (time->days < 1 || time->days > 31) {
        val = 1;
    } else if (time->months < 1 || time->months > 12) {
        val = 1;
    } else {
        int i = time->months;
        switch (i) {
            /*
             * Wenn Februar, dann teste auf Schaltjahr, da days == 29;
             */
            case 2:
                if ((time->years % 4 == 0) && ((time->years % 400 == 0) || (time->years % 100 != 0))) {
                    if (time->days > 29) {
                        val = 1;
                    }
                } else if (time->days > 28) {
                    val = 1;
                }
                break;
            case 4:
            case 6:
            case 9:
            case 11:
                if (time->days > 30) {
                    val = 1;
                }
                break;
        }
    }
    return val;
}

/*
 * Öffnet den Treiber
 */

static int ds3231_open(struct inode *inode, struct file *file) {
    printk("DS3231_drv: ds3231_open aufgerufen\n");
    return 0;
}

/*
 * Schließt den Treiber
 */

static int ds3231_close(struct inode *inode, struct file *file) {
    printk("DS3231_drv: ds3231_close aufgerufen\n");
    return 0;
}

/*
 * Gibt dem Nutzer die ausgelesene Uhrzeit zurück
 */

static ssize_t ds3231_read(struct file *file, char __user * puffer, size_t bytes, loff_t *offset) {
    /* Zwischenspeicher für die Uhrzeit */
    uint8_t seconds, minutes, hours, days, months, ret, val;
    uint16_t years;
    ds3231_time_t time;

    /* Zwischenspeicherung des Status des RTC-Chips */
    ds3231_status_t status;

    /* zu lesenden Bytes */
    ssize_t count;

    /* char-Array für Datumsausgabe */
    char date[30];

    char *months_list[12] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};

    /* Wenn bereits eine Ausgabe erfolgt ist, dann mein_write verlassen. */
    if (*offset != 0) {
        *offset = 0;
        return 0;
    }

    printk("DS3231_drv: ds3231_read aufgerufen\n");

    /* Reservierung des Datenbusses */
    ret = spin_trylock(&lock);

    /* Überprüfung, ob Datenbus besetzt */
    if (!ret) {
        return -EBUSY;
    }

    /* Auslesung des Status */
    status.full = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_STATUS);
    status.osf  = status.full & DS3231_OSFBIT;
    status.bsy  = status.full & DS3231_BSYBIT;
    status.temp = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_TEMP);

    /* Überprüfung, ob Oszillator deaktiviert.
     * Wenn Oszillator deaktiviert, dann aktiviere ihn und gib Fehlermeldung aus.
     */
    if ((status.osf >> 7) == 1) {
        status.full = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_CONTROL);
        i2c_smbus_write_byte_data(ds3231_client, DS3231_REG_CONTROL, (status.full | DS3231_BIT_nEOSC));
        i2c_smbus_write_byte_data(ds3231_client, DS3231_REG_STATUS, (status.full & ~DS3231_OSFBIT));
        printk("DS3231: Oszillator ist deaktiviert.\n\n Aktivierung des Oszillators gestartet.\n");
        return -EAGAIN;

    /* Temperaturprüfung */
    } else if (status.temp < -40 || status.temp > 85) {
        printk("DS3231: ACHTUNG! Temperatur liegt außerhalb des Arbeitsbereichs!\n");
    }

    /* Auslesen und Abspeicherung der Uhrzeit-Werte */
    seconds = i2c_smbus_read_byte_data(ds3231_client, DS3231_SECONDS);
    minutes = i2c_smbus_read_byte_data(ds3231_client, DS3231_MINUTES);
    hours   = i2c_smbus_read_byte_data(ds3231_client, DS3231_HOURS);
    days    = i2c_smbus_read_byte_data(ds3231_client, DS3231_DAYS);
    months  = i2c_smbus_read_byte_data(ds3231_client, DS3231_MONTHS);
    years   = i2c_smbus_read_byte_data(ds3231_client, DS3231_YEARS);

    /* Freigabe des Datenbusses */
    spin_unlock(&lock);

    /* Darstellung von BCD → BIN */
    time.seconds = bcd2bin(seconds & DS3231_SECSBITS);
    time.minutes = bcd2bin(minutes & DS3231_MINSBITS);
    time.days    = bcd2bin(days & DS3231_DAYSBITS);
    time.months  = bcd2bin(months & DS3231_MONTHSBITS);
    time.hours   = bcd2bin(hours & DS3231_HRSBITS);
    /* +2000, damit Jahr richtig dargestellt wird */
    time.years   = bcd2bin(years & DS3231_YEARSBITS) + 2000;

    /* Gültigkeitsprüfung des Datums */
    val = date_check(&time);

    /* Fehlermeldung für ungültige Daten */
    if (val == 1) {
        return -ENOEXEC;
    } else if (val == 2) {
        return -EOVERFLOW;
    }

    /* Speicherung der Uhrzeit-Werte */
    scnprintf(date, sizeof(date), "%02d. %s %02d:%02d:%02d %04d\n",
              time.days, months_list[time.months - 1], time.hours,
              time.minutes, time.seconds, time.years);

    /* Anzahl bytes, die ausgegeben werden */
    count = strlen(date);

    /* Ausgabe und Rückgabe der nicht gelesenen Bytes */
    bytes = copy_to_user(puffer, date, count);

    /* Wurde bereits gelesen, muss nicht mehr gemacht werden */
    *offset = ~bytes;

    return count;
}

/*
 * Liest Nutzereingaben und speichert die Eingaben in der jeweiligen Datenstruktur
 */

static ssize_t ds3231_write(struct file *file, const char __user* puffer, size_t bytes, loff_t *offset) {
    /* Zwischenspeicher für die Uhrzeit */
    ds3231_time_t time;

    /* Zwischenspeicherung des Status des RTC-Chips */
    ds3231_status_t status;

    uint8_t ret, val;

    /* char-Array zur Speicherung der Eingabe */
    char input[30];

   /* Wenn Eingabe >= 30, dann gib Fehlermeldung aus */
   if (bytes >= 30) {
       return -EOVERFLOW;
   }

    printk("DS3231_drv: ds3231_write aufgerufen\n");

    /* Einlesen des Datums */
    if (copy_from_user(input, puffer, bytes)) {
        return -EINVAL;
    }

    /* Prüfe auf manuelle Änderung der Temperatur */
    if (input[0] == '$') {
    sscanf(input + 1, "%hhd %u-%hu-%hu %hu:%hu:%hu", &status.temp, &time.years, &time.months, &time.days, &time.hours, &time.minutes, &time.seconds);
    printk(KERN_ALERT "Temperaturwert: %hhd", status.temp);

    /* Gültigkeitsprüfung des Datums */
    val = date_check(&time);

    /* Fehlermeldung für ungültige Daten */
    if (val == 1) {
        return -ENOEXEC;
    } else if (val == 2) {
        return -EOVERFLOW;
    }

    /* Darstellung von BIN → BCD */
    time.seconds = bin2bcd(time.seconds);
    time.minutes = bin2bcd(time.minutes);
    time.days    = bin2bcd(time.days);
    time.hours   = bin2bcd(time.hours);
    time.months  = bin2bcd(time.months);
    time.years   = time.years - 2000;
    time.years   = bin2bcd(time.years);

    /* Reservierung des Datenbusses */
    ret = spin_trylock(&lock);

    if (!ret) {
        return -EBUSY;
    }

    /* Auslesung des Status */
    status.full = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_STATUS);
    status.osf  = status.full & DS3231_OSFBIT;
    status.bsy  = status.full & DS3231_BSYBIT;

    /* Überprüfung, ob Oszillator deaktiviert.
     * Wenn Oszillator deaktiviert, dann aktiviere ihn und gib Fehlermeldung aus.
     */
    if ((status.osf >> 7) == 1) {
        status.full = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_CONTROL);
        i2c_smbus_write_byte_data(ds3231_client, DS3231_REG_CONTROL, (status.full | DS3231_BIT_nEOSC));
        i2c_smbus_write_byte_data(ds3231_client, DS3231_REG_STATUS, (status.full & ~DS3231_OSFBIT));
        printk("DS3231: Oszillator ist nicht aktiviert.\n\n Aktivierung des Oszillators gestartet.\n");
        return -EAGAIN;

    /* Temperaturprüfung */
    } else if (status.temp < -40 || status.temp > 85) {
        printk("DS3231: ACHTUNG! Temperatur liegt außerhalb des Arbeitsbereichs!\n");
    }

    /* Speicherung der Eingabe */
    i2c_smbus_write_byte_data(ds3231_client, DS3231_SECONDS, time.seconds);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_MINUTES, time.minutes);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_HOURS, time.hours);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_DATE, time.days);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_MONTHS, time.months);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_YEARS, time.years);

    /* Freigabe des Datenbusses */
    spin_unlock(&lock);

    return bytes;
    }

    /* Speicherung der Eingabe in input[] und Zuweisung der Uhrzeit und des Datums */
    sscanf(input, "%u-%hu-%hu %hu:%hu:%hu", &time.years, &time.months, &time.days, &time.hours, &time.minutes, &time.seconds);

    /* Gültigkeitsprüfung des Datums */
    val = date_check(&time);

    /* Fehlermeldung für ungültige Daten */
    if (val == 1) {
        return -ENOEXEC;
    } else if (val == 2) {
        return -EOVERFLOW;
    }

    /* Darstellung von BIN → BCD */
    time.seconds = bin2bcd(time.seconds);
    time.minutes = bin2bcd(time.minutes);
    time.days    = bin2bcd(time.days);
    time.hours   = bin2bcd(time.hours);
    time.months  = bin2bcd(time.months);
    time.years   = time.years - 2000;
    time.years   = bin2bcd(time.years);

    /* Reservierung des Datenbusses */
    ret = spin_trylock(&lock);

    if (!ret) {
        return -EBUSY;
    }

    /* Auslesung des Status */
    status.full = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_STATUS);
    status.osf  = status.full & DS3231_OSFBIT;
    status.bsy  = status.full & DS3231_BSYBIT;

    /* Überprüfung, ob Oszillator deaktiviert.
     * Wenn Oszillator deaktiviert, dann aktiviere ihn und gib Fehlermeldung aus.
     */
    if ((status.osf >> 7) == 1) {
        status.full = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_CONTROL);
        i2c_smbus_write_byte_data(ds3231_client, DS3231_REG_CONTROL, (status.full | DS3231_BIT_nEOSC));
        i2c_smbus_write_byte_data(ds3231_client, DS3231_REG_STATUS, (status.full & ~DS3231_OSFBIT));
        printk("DS3231: Oszillator ist nicht aktiviert.\n\n Aktivierung des Oszillators gestartet.\n");
        return -EAGAIN;

    /* Temperaturprüfung */
    } else if (status.temp < -40 || status.temp > 85) {
        printk("DS3231: ACHTUNG! Temperatur liegt außerhalb des Arbeitsbereichs!\n");
    }

    /* Speicherung der Eingabe */
    i2c_smbus_write_byte_data(ds3231_client, DS3231_SECONDS, time.seconds);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_MINUTES, time.minutes);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_HOURS, time.hours);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_DATE, time.days);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_MONTHS, time.months);
    i2c_smbus_write_byte_data(ds3231_client, DS3231_YEARS, time.years);

    /* Freigabe des Datenbusses */
    spin_unlock(&lock);

    return bytes;
}

/*
 * Initialisierung des Treibers und Devices.
 * Diese Funktion wird von Linux-Kernel aufgerufen, aber erst, nachdem ein zum
 * Treiber passende Device-Information gefunden wurde. Innerhalb der Funktion
 * wird der Treiber und das Device initialisiert.
 */

static int ds3231_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    s32 reg0, reg1;
    u8 reg_cnt, reg_sts;
    int ret;

    printk("DS3231_drv: ds3231_probe aufgerufen\n");

    /* Control und Status Register auslesen. */
    reg0 = i2c_smbus_read_byte_data(client, DS3231_REG_CONTROL);
    reg1 = i2c_smbus_read_byte_data(client, DS3231_REG_STATUS);
    if (reg0 < 0 || reg1 < 0) {
        printk("DS3231_drv: Fehler beim Lesen von Control oder Status Register.\n");
        return -ENODEV;
    }
    reg_cnt = (u8) reg0;
    reg_sts = (u8) reg1;
    printk("DS3231_drv: Control: 0x%02X, Status: 0x%02X\n", reg_cnt, reg_sts);

    /* Prüfen, ob der Oszillator abgeschaltet ist, falls ja, muss dieser
     * eingeschaltet werden (damit die Zeit läuft).
     */

    if (reg_cnt & DS3231_BIT_nEOSC) {
        printk("DS3231_drv: Oszillator einschalten\n");
        reg_cnt &= ~DS3231_BIT_nEOSC;
    }

    printk("DS3231_drv: Interrupt und Alarms abschalten\n");
    reg_cnt &= ~(DS3231_BIT_INTCN | DS3231_BIT_A2IE | DS3231_BIT_A1IE);

    /* Control-Register setzen */

    i2c_smbus_write_byte_data(client, DS3231_REG_CONTROL, reg_cnt);

    /* Prüfe Oszillatorzustand. Falls Fehler vorhanden, wird das Fehlerfalg
     * zurückgesetzt.
     */

    if (reg_sts & DS3231_BIT_OSF) {
        reg_sts &= ~DS3231_BIT_OSF;
        i2c_smbus_write_byte_data(client, DS3231_REG_STATUS, reg_sts);
        printk("DS3231_drv: Oszillator Stop Flag (OSF) zurückgesetzt.\n");
    }

    /* Gerätenummer für das RTC-Gerät reservieren */
    ret = alloc_chrdev_region(&ds3231_device, 0, 1, "ds3231_drv");
    if (ret < 0) {
        printk(KERN_ALERT
        "DS3231: Fehler bei alloc_chrdev_region()\n");
        return ret;
    }

    /* Schnittstellen und Datenstrukturen initialisieren */
    cdev_init(&cds3231_device, &ds3231_fops);
    ret = cdev_add(&cds3231_device, ds3231_device, 1);
    if (ret < 0) {
        printk(KERN_ALERT
        "DS3231: Fehler bei registrierung von CDEV-Struktur\n");
        goto unreg_chrdev;
    }

    /* Das Gerät im sysfs registrieren. Erstellen von der Gerät-Datei
     * von dem udev-Dienst
     */
    ds3231_device_class = class_create(THIS_MODULE, "chardev");
    if (ds3231_device_class == NULL) {
        printk(KERN_ALERT
        "DS3231: Class konnte nicht erstellt werden.\n" );
        goto cleanup_cdev;
    }

    if (device_create(ds3231_device_class, NULL, ds3231_device, NULL, "ds3231_drv") == NULL) {
        printk(KERN_ALERT
        "DS3231: Device konnte nicht erstellt werden.\n");
        goto cleanup_chrdev_class;
    }

    /* DS3231 erfolgreich initialisiert */

    return 0;

    /* Fehler melden und Ressourcen freigeben */
    cleanup_chrdev_class:
    	class_destroy(ds3231_device_class);
    cleanup_cdev:
	cdev_del(&cds3231_device);
    unreg_chrdev:
	unregister_chrdev_region(ds3231_device, 1);

    return -EIO;
}

/*
 * Freigebe der Ressourcen.
 * Diese Funktion wird beim Entfernen des Treibers oder Gerätes
 * von Linux-Kernel aufgerufen. Hier sollten die Ressourcen, welche
 * in der "ds3231_probe()" Funktion angefordert wurden, freigegeben.
 */

static int ds3231_remove(struct i2c_client *client) {
    printk("DS3231_drv: ds3231_remove aufgerufen\n");
    device_destroy(ds3231_device_class, ds3231_device);
    class_destroy(ds3231_device_class);
    cdev_del(&cds3231_device);
    unregister_chrdev_region(ds3231_device, 1);
    return 0;
}

/*
 * Initialisierungsroutine des Kernel-Modules.
 * Wird beim Laden des Moduls aufgerufen. Innerhalb der Funktion
 * wird das neue Device (DS3231) registriert und der I2C Treiber
 * hinzugefügt.
 */

static int __init ds3231_module_init(void) {
	int ret;
	struct i2c_adapter *adapter;

	/*
	 * Normalerweise werden die Informationen bezüglich der verbauten
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
    spin_lock_init(&lock);
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
 * Wird beim Entladen des Moduls aufgerufen. Innerhalb der Funktion
 * werden alle Ressourcen wieder freigegeben.
 */

static void __exit ds3231_module_exit(void) {
	printk("DS3231_drv: ds3231_module_exit aufgerufen\n");
	if(ds3231_client != NULL) {
		i2c_del_driver(&ds3231_driver);
		i2c_unregister_device(ds3231_client);
        ds3231_client = NULL;
	}
}

module_exit(ds3231_module_exit);

/* Module-Informationen. */
MODULE_AUTHOR("Lukas Nils Lingmann uk083418@student.uni-kassel.de, Alexander Haar uk077258@stundent.uni-kassel.de");
MODULE_DESCRIPTION("RTC-Treiber für Labor Embedded Systems im Fachgebiet Rechnerarchitektur und Systemprogrammierung WS22/23");
MODULE_LICENSE("GPL");
