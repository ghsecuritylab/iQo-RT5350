/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright, Ralink Technology, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 ***************************************************************************
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>   
#include <linux/fs.h>       
#include <linux/errno.h>    
#include <linux/types.h>    
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    
#include <asm/system.h>     
#include <linux/wireless.h>
#include <linux/delay.h>

#include "i2c_drv.h"
#include "ralink_gpio.h"

#ifdef  CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#ifdef  CONFIG_DEVFS_FS
static devfs_handle_t devfs_handle;
#endif

int i2cdrv_major =  218;
unsigned long i2cdrv_addr = 0x44;

/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	i2c_master_init                                         */
/*    INPUTS: None                                                      */
/*   RETURNS: None                                                      */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): Initialize I2C block to desired state                     */
/*                                                                      */
/*----------------------------------------------------------------------*/
void i2c_master_init(void)
{
	u32 i;
	/* reset i2c block */
	i = RT2880_REG(RT2880_RSTCTRL_REG) | RALINK_I2C_RST;
	RT2880_REG(RT2880_RSTCTRL_REG) = i;
	RT2880_REG(RT2880_RSTCTRL_REG) = i & ~(RALINK_I2C_RST);

	for(i = 0; i < 50000; i++);
	// umsleep(500);
	
	RT2880_REG(RT2880_I2C_CONFIG_REG) = I2C_CFG_DEFAULT;

	RT2880_REG(RT2880_I2C_CLKDIV_REG) = CLKDIV_VALUE;

	/*
	 * Device Address : 
	 *
	 * ATMEL 24C152 serial EEPROM
	 *       1|0|1|0|0|A1|A2|R/W
	 *      MSB              LSB
	 * 
	 * ATMEL 24C01A/02/04/08A/16A
	 *    	MSB               LSB	  
	 * 1K/2K 1|0|1|0|A2|A1|A0|R/W
	 * 4K            A2 A1 P0
	 * 8K            A2 P1 P0
	 * 16K           P2 P1 P0 
	 *
	 * so device address needs 7 bits 
	 * if device address is 0, 
	 * write 0xA0 >> 1 into DEVADDR(max 7-bits) REG  
	 */
	RT2880_REG(RT2880_I2C_DEVADDR_REG) = i2cdrv_addr;

	/*
	 * Use Address Disabled Transfer Options
	 * because it just support 8-bits, 
	 * ATMEL eeprom needs two 8-bits address
	 */
	RT2880_REG(RT2880_I2C_ADDR_REG) = 0;
}

/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	i2c_WM8751_write                                               */
/*    INPUTS: 8-bit data                                                */
/*   RETURNS: None                                                      */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): transfer 8-bit data to I2C                                */
/*                                                                      */
/*----------------------------------------------------------------------*/
#if defined (CONFIG_RALINK_I2S) || defined (CONFIG_RALINK_I2S_MODULE)
void i2c_WM8751_write(u32 address, u32 data)
{
	int i, j;
	unsigned long old_addr = i2cdrv_addr;
	
	i2cdrv_addr = WM8751_ADDR;
	
	i2c_master_init();	
	
	/* two bytes data at least so NODATA = 0 */

	RT2880_REG(RT2880_I2C_BYTECNT_REG) = 1;
	
	RT2880_REG(RT2880_I2C_DATAOUT_REG) = (address<<1)|(0x01&(data>>8));

	RT2880_REG(RT2880_I2C_STARTXFR_REG) = WRITE_CMD;

	j = 0;
	do {
		if (IS_SDOEMPTY) {	
			RT2880_REG(RT2880_I2C_DATAOUT_REG) = (data&0x0FF);	
			break;
		}
	} while (++j<max_ee_busy_loop);
	
	i = 0;
	while(IS_BUSY && i<i2c_busy_loop){
		i++;
	};
	i2cdrv_addr = old_addr;
}
#endif

///////////////////////////////////////////////////////////////////////////
/////////////////// Software defined I2C interface ////////////////////////
///////////////////////////////////////////////////////////////////////////
// Hardware-specific support functions that MUST be customized:
#define false       0
#define true        1

enum I2C_PMODE {
    INPUT = 0,
    OUTPUT,
};

enum I2C_LEVEL {
    LOW = 0,
    HIGH,
};

static u32 scl[2] = {14, 14};
static u32 sda[2] = {9, 9};
static bool started = false;

static void pinMode(u32 pin_num, enum I2C_PMODE pmode)
{
    u32 val, tmp, reg;


    if (pin_num <= 21) {
        val = 1 << pin_num;
        reg = RALINK_REG_PIODIR;
    } else if (pin_num <= 27) {
        val = 1 << (pin_num - 22);
        reg = RALINK_REG_PIO2722DIR;
    } else {
        return;
    }

    tmp = *(volatile u32 *)(reg);
    if (pmode == INPUT) {
        tmp &= ~val;
    } else if (pmode == OUTPUT) {
        tmp |= val;
    }
    *(volatile u32 *)(reg) = tmp;
}

static enum I2C_LEVEL digitalRead(int pin_num)
{
    u32 val, reg, tmp;

    if (pin_num <= 21) {
        reg = RALINK_REG_PIODATA;
        val = pin_num;
    } else if (pin_num <= 27) {
        reg = RALINK_REG_PIO2722DATA;
        val = pin_num - 22;
    } else {
        return LOW;
    }

    tmp = *(volatile u32 *)(reg);
    tmp = (tmp >> val) & 1L;
    if (tmp == 0) {
        return LOW;
    } else {
        return HIGH;
    }
}

static void digitalWrite(int pin_num, enum I2C_LEVEL lvl)
{
    u32 val, reg, tmp;

    if (pin_num <= 21) {
        reg = RALINK_REG_PIODATA;
        val = pin_num;
    } else if (pin_num <= 27) {
        reg = RALINK_REG_PIO2722DATA;
        val = pin_num - 22;
    } else {
        return;
    }

    tmp = *(volatile u32 *)(reg);
    if (lvl == LOW) {
        tmp &= ~(1L << val);
    } else if (lvl == HIGH) {
        tmp |= (1L << val);
    }
    *(volatile u32 *)(reg) = tmp;
}

static void I2C_msleep(void)
{ 
    udelay(5);
}

bool read_SCL(int i2c_idx) // Set SCL as input and return current level of line, 0 or 1
{
    pinMode(scl[i2c_idx], INPUT);
    if (digitalRead(scl[i2c_idx]) == HIGH) {
        return true;
    } else {
        return false;
    }
}

bool read_SDA(int i2c_idx) // Set SDA as input and return current level of line, 0 or 1
{
    pinMode(sda[i2c_idx], INPUT);
    udelay(2);
    if (digitalRead(sda[i2c_idx]) == HIGH) {
        return true;
    } else {
        return false;
    }  
}

void clear_SCL(int i2c_idx) // Actively drive SCL signal low
{
    pinMode(scl[i2c_idx], OUTPUT);
    digitalWrite(scl[i2c_idx], LOW);
}
void clear_SDA(int i2c_idx) // Actively drive SDA signal low
{
    pinMode(sda[i2c_idx], OUTPUT);
    digitalWrite(sda[i2c_idx], LOW);  
}

void arbitration_lost(void)
{
    printk("Error: software I2C arbitration lost, pin collision?\n");
}

void i2c_start_cond(int i2c_idx) {
    if (started) { // if started, do a restart cond
        // set SDA to 1
        read_SDA(i2c_idx);
        I2C_msleep();
        while (read_SCL(i2c_idx) == false) {  // Clock stretching
            // You should add timeout to this loop
        }
        // Repeated start setup time, minimum 4.7us
        I2C_msleep();
    }
    if (read_SDA(i2c_idx) == false) {
        arbitration_lost();
    }
    // SCL is high, set SDA from 1 to 0.
    clear_SDA(i2c_idx);
    I2C_msleep();
    clear_SCL(i2c_idx);
    started = true;
}

void i2c_stop_cond(int i2c_idx){
    // set SDA to 0
    clear_SDA(i2c_idx);
    I2C_msleep();
    // Clock stretching
    while (read_SCL(i2c_idx) == false) {
        // add timeout to this loop.
    }
    // Stop bit setup time, minimum 4us
    I2C_msleep();
    // SCL is high, set SDA from 0 to 1
    if (read_SDA(i2c_idx) == false) {
        arbitration_lost();
    }
    I2C_msleep();
    started = false;
}

// Write a bit to I2C bus
void i2c_write_bit(int i2c_idx, bool b) {
    if (b) {
        read_SDA(i2c_idx);
    } else {
        clear_SDA(i2c_idx);
    }
    I2C_msleep();
    while (read_SCL(i2c_idx) == false) { // Clock stretching
        // You should add timeout to this loop
    }
    // SCL is high, now data is valid
    // If SDA is high, check that nobody else is driving SDA
    if (b && read_SDA(i2c_idx) == false) {
        arbitration_lost();
    }
    I2C_msleep();
    clear_SCL(i2c_idx);
}

// Read a bit from I2C bus
bool i2c_read_bit(int i2c_idx) {
    bool b;
    // Let the slave drive data
    read_SDA(i2c_idx);
    I2C_msleep();
    while (read_SCL(i2c_idx) == false) { // Clock stretching
        // You should add timeout to this loop
    }
    // SCL is high, now data is valid
    b = read_SDA(i2c_idx);
    I2C_msleep();
    clear_SCL(i2c_idx);
    return b;
}

// Write a byte to I2C bus. Return 0 if ack by the slave.
bool i2c_write_byte(int i2c_idx,
        bool send_start,
        bool send_stop,
        unsigned char B) {
    unsigned b;
    bool nack;
    if (send_start) {
        i2c_start_cond(i2c_idx);
    }
    for (b = 0; b < 8; b++) {
        i2c_write_bit(i2c_idx, (B & 0x80) != 0);
        B <<= 1;
    }
    nack = i2c_read_bit(i2c_idx);
    if (send_stop) {
        i2c_stop_cond(i2c_idx);
    }
    return nack;
}

// Read a byte from I2C bus
unsigned char i2c_read_byte(int i2c_idx, bool nack, bool send_stop) {
    unsigned char B = 0;
    unsigned b;
    for (b = 0; b < 8; b++) {
        B = (B << 1) | i2c_read_bit(i2c_idx);
    }
    i2c_write_bit(i2c_idx, nack);
    if (send_stop) {
        i2c_stop_cond(i2c_idx);
    }
    return B;
}

/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	i2c_write                                               */
/*    INPUTS: 8-bit data                                                */
/*   RETURNS: None                                                      */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): transfer 8-bit data to I2C                                */
/*                                                                      */
/*----------------------------------------------------------------------*/
static void i2c_write(u32 address, u8 *data, u32 nbytes)
{
	int i, j;
	u32 n;

	/* two bytes data at least so NODATA = 0 */
	n = nbytes + ADDRESS_BYTES;
	RT2880_REG(RT2880_I2C_BYTECNT_REG) = n-1;
	if (ADDRESS_BYTES == 2)
		RT2880_REG(RT2880_I2C_DATAOUT_REG) = (address >> 8) & 0xFF;
	else
		RT2880_REG(RT2880_I2C_DATAOUT_REG) = address & 0xFF;

	RT2880_REG(RT2880_I2C_STARTXFR_REG) = WRITE_CMD;
	for (i=0; i<n-1; i++) {
		j = 0;
		do {
			if (IS_SDOEMPTY) {
				if (ADDRESS_BYTES == 2) {
					if (i==0) {
						RT2880_REG(RT2880_I2C_DATAOUT_REG) = address & 0xFF;
					} else {
						RT2880_REG(RT2880_I2C_DATAOUT_REG) = data[i-1];
					}								
				} else {
					RT2880_REG(RT2880_I2C_DATAOUT_REG) = data[i];
				}
 			break;
			}
		} while (++j<max_ee_busy_loop);
	}

	i = 0;
	while(IS_BUSY && i<i2c_busy_loop){
		i++;
	};
}

/*----------------------------------------------------------------------*/
/*   Function                                                           */
/*           	i2c_read                                                */
/*    INPUTS: None                                                      */
/*   RETURNS: 8-bit data                                                */
/*   OUTPUTS: None                                                      */
/*   NOTE(S): get 8-bit data from I2C                                   */
/*                                                                      */
/*----------------------------------------------------------------------*/
static void i2c_read(u8 *data, u32 nbytes) 
{
	int i, j;

	RT2880_REG(RT2880_I2C_BYTECNT_REG) = nbytes-1;
	RT2880_REG(RT2880_I2C_STARTXFR_REG) = READ_CMD;
	for (i=0; i<nbytes; i++) {
		j = 0;
		do {
			if (IS_DATARDY) {
				data[i] = RT2880_REG(RT2880_I2C_DATAIN_REG);
				break;
			}
		} while(++j<max_ee_busy_loop);
	}

	i = 0;
	while(IS_BUSY && i<i2c_busy_loop){
		i++;
	};
}

static inline void random_read_block(u32 address, u8 *data)
{
	/* change page */
	if (ADDRESS_BYTES == 1) {
		int page;
		
		page = ((address >> 8) & 0x7) << 1;
		/* device id always 0 */
		RT2880_REG(RT2880_I2C_DEVADDR_REG) = (i2cdrv_addr | (page>>1));
	}

   	/* dummy write */
   	i2c_write(address, data, 0);
	i2c_read(data, READ_BLOCK);	
}

static inline u8 random_read_one_byte(u32 address)
{	
	u8 data;

	/* change page */
	if (ADDRESS_BYTES == 1) {
		int page;
		
		page = ((address >> 8) & 0x7) << 1;
		/* device id always 0 */
		RT2880_REG(RT2880_I2C_DEVADDR_REG) = (i2cdrv_addr | (page>>1));
	}


   	/* dummy write */
	i2c_write(address, &data, 0);
	i2c_read(&data, 1);
	return (data);
}

void i2c_eeprom_read(u32 address, u8 *data, u32 nbytes)
{
	int i;
	int nblock = nbytes / READ_BLOCK;
	int rem = nbytes % READ_BLOCK;

	for (i=0; i<nblock; i++) {
		random_read_block(address+i*READ_BLOCK, &data[i*READ_BLOCK]);
	}

	if (rem) {
		int offset = nblock*READ_BLOCK;
		for (i=0; i<rem; i++) {
			data[offset+i] = random_read_one_byte(address+offset+i);
		}		
	}
}


void i2c_eeprom_read_one(u32 address, u8 *data, u32 nbytes)
{
	int i;

	for (i=0; i<nbytes; i++) {
		data[i] = random_read_one_byte(address+i);
	}
}

static inline void random_write_block(u32 address, u8 *data)
{
	int i;
	/* change page */
	if (ADDRESS_BYTES == 1) {
		int page;
		
		page = ((address >> 8) & 0x7) << 1;
		/* device id always 0 */
		RT2880_REG(RT2880_I2C_DEVADDR_REG) = (i2cdrv_addr | (page>>1));
	}


	i2c_write(address, data, WRITE_BLOCK);
	// mmsleep(5);
	for(i = 0; i < 500000; i++);
}

static inline void random_write_one_byte(u32 address, u8 *data)
{	
	int i;
	/* change page */
	if (ADDRESS_BYTES == 1) {
		int page;
		
		page = ((address >> 8) & 0x7) << 1;
		/* device id always 0 */
		RT2880_REG(RT2880_I2C_DEVADDR_REG) = (i2cdrv_addr | (page>>1));
	}

	i2c_write(address, data, 1);
	// mmsleep(5);
	for(i = 0; i < 500000; i++);
}

void i2c_eeprom_write(u32 address, u8 *data, u32 nbytes)
{
	int i;
	int nblock = nbytes / WRITE_BLOCK;
	int rem = nbytes % WRITE_BLOCK;

	for (i=0; i<nblock; i++) {
		random_write_block(address+i*WRITE_BLOCK, &data[i*WRITE_BLOCK]);
	}

	if (rem) {
		int offset = nblock*WRITE_BLOCK;

		for (i=0; i<rem; i++) {
			random_write_one_byte(address+offset+i, &data[offset+i]);
		}		
	}
}

void i2c_read_config(char *data, unsigned int len)
{
	i2c_master_init();
	i2c_eeprom_read(0, data, len);
}

static void i2c_isl_read(unsigned int addr, u8 *val_ptr)
{
    i2c_write(addr, NULL, 0);
    i2c_read(val_ptr, 1);
}

static void i2c_isl_write(unsigned int addr, u8 value)
{
    i2c_write(addr, &value, 1);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
int i2cdrv_ioctl(struct file *filp, unsigned int cmd, 
		unsigned long arg)
#else
int i2cdrv_ioctl(struct inode *inode, struct file *filp, \
                     unsigned int cmd, unsigned long arg)
#endif
{
	//unsigned char w_byte[4];
	unsigned int address, size;
	u8 value;
	I2C_WRITE *i2c_write;
    I2C_READ *i2c_read;

	switch (cmd) {
    /*
     * ISL29023
     */
	case RT2880_I2C_READ:
		value = 0; address = 0;
		address = (unsigned int)arg;
		//i2c_eeprom_read(address, (unsigned char*)&value, 4);
        i2c_isl_read(address, &value);
		printk("0x%02x : 0x%02x\n", address, value);
		break;
	case RT2880_I2C_WRITE:
		i2c_write = (I2C_WRITE*)arg;
		address = i2c_write->address;
		value   = (u8)(i2c_write->value);
		size    = i2c_write->size;
		//i2c_eeprom_write(address, (unsigned char*)&value, size);
        i2c_isl_write(address, value);
		break;

    /*
     * ISL29021
     */
	case RT5350_SOFT_I2C_READ:
		value = 0; address = 0;
        i2c_read = (I2C_READ *)arg;
		address = i2c_read->addr;

        i2c_write_byte(0, true, false, 0x88);
        udelay(10);
        i2c_write_byte(0, false, true, (u8)(address & 0xFF));
        udelay(10);
        i2c_write_byte(0, true, false, 0x89);
        udelay(10);
        value = i2c_read_byte(0, true, true);

        copy_to_user(i2c_read->data_p, &value, 1);
		break;
	case RT5350_SOFT_I2C_WRITE:
		i2c_write = (I2C_WRITE*)arg;
		address = i2c_write->address;
		value   = (u8)(i2c_write->value);
		size    = i2c_write->size;

        i2c_write_byte(0, true, false, 0x88);
        udelay(10);
        i2c_write_byte(0, false, false, (u8)(address & 0xFF));
        udelay(10);
        i2c_write_byte(0, false, true, value);

		break;

    /*
	case RT2880_I2C_SET_ADDR:
		i2cdrv_addr = (unsigned long)arg;
	    RT2880_REG(RT2880_I2C_DEVADDR_REG) = i2cdrv_addr;
		break;
    */

	default :
		printk("i2c_drv: unsupported command!\n");
	}

	return 0;
}

struct file_operations i2cdrv_fops = {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
	unlocked_ioctl: i2cdrv_ioctl,
#else
	ioctl:  i2cdrv_ioctl,
#endif
};

static int i2cdrv_init(void)
{
#if !defined (CONFIG_DEVFS_FS)
	int result=0;
#endif

	/* configure i2c to normal mode */
	RT2880_REG(RALINK_SYSCTL_BASE + 0x60) &= ~1;

#ifdef  CONFIG_DEVFS_FS
	if(devfs_register_chrdev(i2cdrv_major, I2C_DEV_NAME , &i2cdrv_fops)) {
		printk(KERN_WARNING " i2cdrv: can't create device node\n");
		return -EIO;
	}

	devfs_handle = devfs_register(NULL, I2C_DEV_NAME, DEVFS_FL_DEFAULT, i2cdrv_major, 0, \
			S_IFCHR | S_IRUGO | S_IWUGO, &i2cdrv_fops, NULL);
#else
	result = register_chrdev(i2cdrv_major, I2C_DEV_NAME, &i2cdrv_fops);
	if (result < 0) {
		printk(KERN_WARNING "i2c_drv: can't get major %d\n",i2cdrv_major);
		return result;
	}

	if (i2cdrv_major == 0) {
		i2cdrv_major = result; /* dynamic */
	}
#endif

    i2c_master_init();

	printk("i2cdrv_major = %d\n", i2cdrv_major);
	return 0;
}


static void i2cdrv_exit(void)
{
	printk("i2c_drv exit\n");

#ifdef  CONFIG_DEVFS_FS
	devfs_unregister_chrdev(i2cdrv_major, I2C_DEV_NAME);
	devfs_unregister(devfs_handle);
#else
	unregister_chrdev(i2cdrv_major, I2C_DEV_NAME);
#endif

}

#if defined (CONFIG_RALINK_I2S) || defined (CONFIG_RALINK_I2S_MODULE)
EXPORT_SYMBOL(i2c_WM8751_write);
#endif

module_init(i2cdrv_init);
module_exit(i2cdrv_exit);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
MODULE_PARM (i2cdrv_major, "i");
#else
module_param (i2cdrv_major, int, 0);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ralink I2C Driver");

