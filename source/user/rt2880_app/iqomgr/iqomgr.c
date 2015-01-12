#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/autoconf.h>
#include "spi_drv.h"
#include "ralink_gpio.h"
#include "i2c_drv.h"

#define GPIO_DEV	"/dev/gpio"

enum {
	gpio_in,
	gpio_out,
};

enum {
	gpio2100,
	gpio2722,
};

int signal_up = 0;
int signal_down = 0;
int interrupted = 0;

void signal_handler(int signum)
{
	if (signum == SIGUSR1) {
        signal_up = 1;
        signal_down = 0;
	} else if (signum == SIGUSR2) {
        signal_down = 1;
        signal_up = 0;
	} else {
		printf("Unknown signal: %d", signum);
    }
}

int gpio_enb_irq(void)
{
	int fd;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (ioctl(fd, RALINK_GPIO_ENABLE_INTP) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_dis_irq(void)
{
	int fd;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (ioctl(fd, RALINK_GPIO_DISABLE_INTP) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_reg_info(int gpio_num)
{
	int fd;
	ralink_gpio_reg_info info;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	info.pid = getpid();
	info.irq = gpio_num;
	if (ioctl(fd, RALINK_GPIO_REG_IRQ, &info) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_set_dir(int pin_num, int dir)
{
	int fd, req, r;
    unsigned int val;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
    if (pin_num <= 21) {
        r = gpio2100;
        val = 1 << pin_num;
    } else if (pin_num <= 27) {
        r = gpio2722;
        val = 1 << (pin_num - 22);
    } else {
		perror("pin number invalid");
		close(fd);
		return -1;
    }
	if (dir == gpio_in) {
        // Input
		if (r == gpio2722) {
			req = RALINK_GPIO2722_SET_DIR_IN;
		} else {
			req = RALINK_GPIO_SET_DIR_IN;
        }
	} else {
        // Output
		if (r == gpio2722) {
			req = RALINK_GPIO2722_SET_DIR_OUT;
		} else {
			req = RALINK_GPIO_SET_DIR_OUT;
        }
	}
	if (ioctl(fd, req, val) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_read_int(int pin_num, int *value)
{
	int fd, req, r;

	*value = 0;
	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
    if (pin_num <= 21) {
        r = gpio2100;
    } else if (pin_num <= 27) {
        r = gpio2722;
    } else {
		perror("pin number invalid");
		close(fd);
		return -1;
    }
	if (r == gpio2722) {
		req = (((unsigned int)(pin_num - 22) & 0xFF) << 24) | RALINK_GPIO2722_READ_BIT;
	} else {
		req = (((unsigned int)pin_num & 0xFF) << 24) | RALINK_GPIO_READ_BIT;
    }
	if (ioctl(fd, req, value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_write_int(int pin_num, int value)
{
	int fd, req, r;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
    if (pin_num <= 21) {
        r = gpio2100;
    } else if (pin_num <= 27) {
        r = gpio2722;
    } else {
		perror("pin number invalid");
		close(fd);
		return -1;
    }
	if (r == gpio2722) {
		req = (((unsigned int)(pin_num - 22) & 0xFF) << 24) | RALINK_GPIO2722_WRITE_BIT;
	} else {
		req = (((unsigned int)pin_num & 0xFF) << 24) | RALINK_GPIO_WRITE_BIT;
    }
	if (ioctl(fd, req, value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int intermcu_tx(unsigned char type, unsigned char len, unsigned char *buf)
{
    unsigned char header_buf[2] = {0};
	int flag = 0, fd;
    unsigned char idx;
    SPI_INTERMCU intermcu;

    signal_up = signal_down = 0;
    gpio_write_int(11, 1);
    
    while (signal_up == 0) {
        usleep(10000);
    }
    signal_up = 0;

    header_buf[0] = type;
    header_buf[1] = len;

    signal_down = 0;
    fd = open("/dev/spiS0", O_RDONLY); 
    if (fd <= 0) {
        printf("Please insmod module spi_drv.o!\n");
        return -1;
    }
    intermcu.size = 2;
    intermcu.buf = header_buf;
    ioctl(fd, RT2880_SPI_INTERMCU_WRITE, &intermcu);
	close(fd);

    while (signal_down == 0) {
        usleep(10000);
    }
    signal_down = 0;

    while (signal_up == 0) {
        usleep(10000);
    }
    signal_up = 0;

    signal_down = 0;
    fd = open("/dev/spiS0", O_RDONLY); 
    if (fd <= 0) {
        printf("Please insmod module spi_drv.o!\n");
        return -1;
    }
    intermcu.size = len;
    intermcu.buf = buf;
    ioctl(fd, RT2880_SPI_INTERMCU_WRITE, &intermcu);
	close(fd);

    while (signal_down == 0) {
        usleep(10000);
    }
    signal_down = 0;

    gpio_write_int(11, 0);

    return 0;
}

int open_i2c_fd(void)
{
	char nm[16];
	int fd;

	snprintf(nm, 16, "/dev/%s", I2C_DEV_NAME);
	if ((fd = open(nm, O_RDONLY)) < 0) {
		perror(nm);
		exit(fd);
	}
	return fd;
}

void i2c_write(int fd, unsigned char addr, unsigned char val)
{
	struct i2c_write_data wdata;

    wdata.address = addr;
    wdata.size = 1;
    wdata.value = val;
	ioctl(fd, RT5350_SOFT_I2C_WRITE, &wdata);
}

unsigned char i2c_read(int fd, unsigned char addr)
{
    I2C_READ i2c_r;
    unsigned char data = 0;

    i2c_r.addr = (unsigned char)(addr & 0xFF);
    i2c_r.data_p = &data;
    ioctl(fd, RT5350_SOFT_I2C_READ, &i2c_r);

    return data;
}

void interrupt_handler(int s)
{
    interrupted = 1;
    printf("Interrupted, quit...\n");
}

// Check sensor output and ask NRF to tune the light
int main(int argc, char *argv[])
{
    unsigned char sensor_data[2] = {0};
    unsigned short sensor_min = 0xFFFF, sensor_max = 0, sensor_tmp;
    int i2c_fd;
    unsigned char cfg0 = 0x60, cfg1 = 0xc0;
    unsigned char value;
    struct sigaction sigIntHandler;

    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    sigIntHandler.sa_handler = interrupt_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

	gpio_set_dir(7, gpio_in);
    gpio_enb_irq();
    gpio_reg_info(7);

    if ((argc == 2) && (strcmp(argv[1], "init") == 0)) {
        gpio_set_dir(11, gpio_out);
        gpio_write_int(11, 0);
        sleep(1);
        gpio_write_int(11, 1);
        sleep(3);
        gpio_write_int(11, 0);
        printf("Signal NRF init done!\n");
        sleep(5);
    } else if ((argc == 3) && (strcmp(argv[1], "led") == 0)) {
        value = atoi(argv[2]);
        if (intermcu_tx(0x01, 1, &value) != 0) {
            printf("intermcu_tx error!\n");
        }
        gpio_dis_irq();
        return 0;
    } else if (argc == 2) {
        cfg1 = strtoul(argv[1], NULL, 16);
    }

    // Initialize proximity sensor
    i2c_fd = open_i2c_fd();
    i2c_write(i2c_fd, 0x01, cfg1);

    while (1) {
        if (interrupted == 1) {
            break;
        }
        i2c_write(i2c_fd, 0x00, cfg0);
        sensor_data[0] = i2c_read(i2c_fd, 0x02);
        sensor_data[1] = i2c_read(i2c_fd, 0x03);
        printf("%d\n", (unsigned short)sensor_data[1] << 8 | (unsigned short)sensor_data[0]);
        //printf("0x%02x%02x\n", sensor_data[1], sensor_data[0]);

/*
        sensor_tmp = (unsigned short)sensor_data[1] << 8 | (unsigned short)sensor_data[0];
        if (sensor_tmp != 0) {
            if (sensor_tmp < sensor_min) {
                sensor_min = sensor_tmp;
            }
            if (sensor_tmp > sensor_max) {
                sensor_max = sensor_tmp;
            }
            if (sensor_max > sensor_min) {
                value = (unsigned char)((sensor_tmp - sensor_min) * 255 / (sensor_max - sensor_min));
                if (intermcu_tx(0x01, 1, &value) != 0) {
                    printf("intermcu_tx error!\n");
                }
            }
        }
        */

        usleep(100000);
    }

    close(i2c_fd);
    gpio_dis_irq();

    return 0;
}

