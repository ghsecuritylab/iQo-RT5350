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
unsigned char color_r = 0x80;
unsigned char color_g = 0x80;
unsigned char color_b = 0x80;

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

void toggle_led(void)
{
    static int led_status = 0;
    unsigned char led_settings[3] = {0};

    if (led_status == 0) { 
        led_settings[0] = color_r;
        led_settings[1] = color_g;
        led_settings[2] = color_b;
        led_status = 1;
    } else {
        led_status = 0;
    }

    if (intermcu_tx(0x01, 3, &(led_settings[0])) != 0) {
        printf("intermcu_tx error!\n");
    }
}


static unsigned short iir_filter(unsigned short newTemp)
{
    static unsigned char firstSample = 1;
    static float x_n = 0.0;
    static float x_n_1 = 0.0;
    static float x_n_2 = 0.0;
    static float x_n_3 = 0.0;
    static float y_n = 0.0;
    static float y_n_1 = 0.0;
    static float y_n_2 = 0.0;
    static float y_n_3 = 0.0;
    float ret_y = 0.0;

    const float a_2 = -2.3377, a_3 = 1.8787, a_4 = -0.5091;
    const float b_1 = 0.0558, b_2 = -0.0399, b_3 = -0.0399, b_4 = 0.0558;

    x_n = (float)newTemp;
    // INIT
    if (firstSample == 1) {
        firstSample = 0;
        x_n_1 = x_n;
        x_n_2 = x_n;
        x_n_3 = x_n;
        y_n_1 = x_n;
        y_n_2 = x_n;
        y_n_3 = x_n;
    }
    y_n = b_1 * x_n + b_2 * x_n_1 + b_3 * x_n_2 + b_4 * x_n_3 -
        (a_2 * y_n_1 + a_3 * y_n_2 + a_4 * y_n_3);
    ret_y = y_n;

    x_n_3 = x_n_2;
    x_n_2 = x_n_1;
    x_n_1 = x_n;
    y_n_3 = y_n_2;
    y_n_2 = y_n_1;
    y_n_1 = y_n;

    return (unsigned short)ret_y;
}

void interrupt_handler(int s)
{
    interrupted = 1;
    printf("Interrupted, quit...\n");
}

#define SENSOR_MEMORY_DEPTH     50
#define SENSOR_ALERT_THRESHOLD  80

// Check sensor output and ask NRF to tune the light
int main(int argc, char *argv[])
{
    unsigned char sensor_data[2] = {0};
    unsigned short sensor_sample;
    int i2c_fd, filter_init_cnt = 0;
    unsigned char cfg0 = 0x60, cfg1 = 0x40;
    unsigned char value;
    struct sigaction sigIntHandler;
    int skip_init = 1;
    unsigned short sensor_memory[SENSOR_MEMORY_DEPTH] = {0};
    unsigned short sensor_min, sensor_max;
    int sensor_idx = 0, sensor_alert = 0, sensor_memory_need_init = 1, sensor_target, sensor_skip_cnt = 0;
    int idx;

    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    sigIntHandler.sa_handler = interrupt_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

	gpio_set_dir(7, gpio_in);
    gpio_enb_irq();
    gpio_reg_info(7);

    i2c_fd = open_i2c_fd();

    if ((argc == 2) && (strcmp(argv[1], "init") == 0)) {
        gpio_set_dir(11, gpio_out);
        gpio_write_int(11, 0);
        sleep(1);
        gpio_write_int(11, 1);
        sleep(3);
        gpio_write_int(11, 0);
        printf("Signal NRF init done!\n");
        sleep(2);

        i2c_write(i2c_fd, 0x01, cfg1);

    } else if (argc == 2) {
        // Initialize proximity sensor cfg1
        cfg1 = strtoul(argv[1], NULL, 16);
        i2c_write(i2c_fd, 0x01, cfg1);
        close(i2c_fd);
        gpio_dis_irq();
        return 0;

    } else if (argc == 4) {
        color_r = strtoul(argv[1], NULL, 16);
        color_g = strtoul(argv[2], NULL, 16);
        color_b = strtoul(argv[3], NULL, 16);
    }

    while (1) {
        if (interrupted == 1) {
            break;
        }
        i2c_write(i2c_fd, 0x00, cfg0);
        sensor_data[0] = i2c_read(i2c_fd, 0x02);
        sensor_data[1] = i2c_read(i2c_fd, 0x03);

        if (skip_init == 1) {
            skip_init = 0;
            usleep(100000);
            continue;
        }

        sensor_sample = iir_filter((unsigned short)sensor_data[1] << 8 | (unsigned short)sensor_data[0]);
        printf("%d\n", sensor_sample);

        if (filter_init_cnt < 50) {
            filter_init_cnt++;
            usleep(100000);
            if (filter_init_cnt >= 50) {
                printf("Started\n");
            }
            continue;
        }

        if (sensor_skip_cnt > 0) {
            sensor_skip_cnt--;
            usleep(100000);
            continue;
        }

        // Initialize sensor memory for the first time
        if (sensor_memory_need_init == 1) {
            sensor_memory_need_init = 0;
            for (sensor_idx = 0; sensor_idx < SENSOR_MEMORY_DEPTH; sensor_idx++) {
                sensor_memory[sensor_idx] = sensor_sample;
            }
            sensor_idx = 0;
        }

        sensor_memory[sensor_idx] = sensor_sample;
        sensor_idx++;
        if (sensor_idx >= SENSOR_MEMORY_DEPTH) {
            sensor_idx = 0;
        }

        sensor_min = sensor_memory[0];
        sensor_max = sensor_memory[0];
        for (idx = 0; idx < SENSOR_MEMORY_DEPTH; idx++) {
            if (sensor_min > sensor_memory[idx]) {
                sensor_min = sensor_memory[idx];
            }
            if (sensor_max < sensor_memory[idx]) {
                sensor_max = sensor_memory[idx];
            }
        }

        if (sensor_alert == 0) {
            if (((sensor_max - sensor_min) >= SENSOR_ALERT_THRESHOLD) && (sensor_sample == sensor_min)) {
                sensor_alert = 1;
                sensor_target = sensor_min + (sensor_max - sensor_min) / 2;
                printf("Sensor alert, target: %d\n", sensor_target);
            }
        } else {
            if (sensor_sample >= sensor_target) {
                printf("Hit target, trigger LED now\n");
                sensor_skip_cnt = 50;
                sensor_memory_need_init = 1;
                sensor_alert = 0;
                toggle_led();
            }
        }
        
        usleep(100000);
    }

    close(i2c_fd);
    gpio_dis_irq();

    return 0;
}

