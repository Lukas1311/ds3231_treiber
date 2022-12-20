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

/* CHANGES HERE*/

/*
 *File Operations für die Registrierung der Funktionen und das Modul
 */
static struct file_operations mein_fops = {
        .owner = THIS_MODULE,
        .llseek = no_llseek,
        .read = mein_read,
        .write = mein_write,
        .open = mein_open,
        .release = mein_close,
};

/*
 * Die ID wird für die Zuordnung des Treibers zum Gerät benötigt.
 */
static const struct i2c_device_id ds3231_dev_id[] = {
        {"ds3231_drc", 0},
        {}
};

MODULE_DEVICE_TABLE(i2c, ds3231_dev_id);

/*
 *  struct i2c_driver wird benötigt, damit der Treiber sich im Linux-Kernel registrieren kann.
 */
static struct i2c_driver ds3231_driver = {
        .driver = {
                .owner = THIS_MODULE,
                .name = "ds3231_drv",
        },
        .id_table = ds3231_dev_id,
        .probe = ds3231_probe,
        .remove = ds3231_remove,
};

static int date_check(ds3231_time_t* time) {
    uint8_t val = 0;

    /*
     * Überprüfung auf Gültigkeit des Datums
     */
    if (time -> years < 2000 || time -> years > 2100) {
        val = 2;
    } else if (time -> seconds < 0 || time -> seconds > 59) {
        val = 1;
    } else if (time -> minutes < 0 || time -> minutes > 59) {
        val = 1;
    } else if (time -> hours < 0 || time -> hours > 23) {
        val = 1;
    } else if (time -> days < 1 || time -> days > 31) {
        val = 1;
    } else if (time -> months < 1 || time -> months > 12) {
        val = 1;
    } else {
        int i = time -> months;
        switch (i) {
            /*
             * Wenn Februar, dann teste auf Schaltjahr, da days = 29;
             */
            case 2:
                if ((time -> years % 4 == 0) && (time -> years % 400 == 0) || (time -> years % 100 != 0)) {
                    if (time -> days > 29) {
                        val = 1;
                    }
                } else if (time -> days > 28) {
                    val = 1;
                }
                break;
            case 4,6,9,11:
                if (time -> days > 30) {
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
static int mein_open(struct inode *inode, struct file *file) {
    printk("DS3231_drv: mein_open aufgerufen\n");
    return 0;
}

/*
 * Schließt den Treiber
 */
static int mein_close(struct inode *inode, struct file *file) {
    printk("DS3231_drv: mein_close aufgerufen\n");
    return 0;
}

/*
 * Gibt dem Nutzer die ausgelesene Uhrzeit zurück
 */
static ssize_t mein_read(struct file *file, char __user* puffer, size_t bytes, loff_t *offset) {
    /* Zwischenspeicher für die Uhrzeit */
    uint8_t seconds, minutes, hours, days, months, ret, val;
    uint16_t years;
    ds3231_time_t time;

    /* char-Array für Datumsausgabe */
    char date[30];

    char *months_list[12] {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};

    /* Wenn bereits eine Ausgabe erfolgt ist, dann mein_write verlassen. */
    if (*offset != 0) {
        *offset = 0;
        return 0;
    }

    printk("DS3231_drv: mein_read aufgerufen\n");
}

/*
 * Liest Nutzereingaben und speichert die Eingaben in der jeweiligen Datenstruktur
 */
static ssize_t mein_write(struct file *file, const char __user* puffer, size_t bytes, loff_t *offset) {
   /* Zwischenspeicher für die Uhrzeit */
   ds3231_time_t time;

   /* char-Array für Datumseingabe */
   char input[30];

   /* Wenn Eingabe >= 30, dann gib Fehlermeldung aus */
   if (bytes >= 30) {
       return -EOVERFLOW;
   }

   printk("DS3231_drv: mein_write aufgerufen\n");
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
MODULE_AUTHOR("Lukas Nils Lingmann uk083418@student.uni-kassel.de, Alexander Haar uk077258@stundent.uni-kassel.de");
MODULE_DESCRIPTION("Beschreibung");
MODULE_LICENSE("GPL");
