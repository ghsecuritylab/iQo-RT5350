/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************
 *
 * $Id: gpio.c,v 1.18.2.1 2012-03-06 13:01:51 kurtis Exp $
 */

#include <stdio.h>             
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/autoconf.h>
#include "ralink_gpio.h"

#define GPIO_DEV	"/dev/gpio"

enum {
	gpio_in,
	gpio_out,
};
enum {
#if defined (CONFIG_RALINK_RT3052)
	gpio2300,
	gpio3924,
	gpio5140,
#elif defined (CONFIG_RALINK_RT3883)
	gpio2300,
	gpio3924,
	gpio7140,
	gpio9572,
#elif defined (CONFIG_RALINK_RT3352)
	gpio2300,
	gpio3924,
	gpio4540,
#elif defined (CONFIG_RALINK_RT5350)
	gpio2100,
	gpio2722,
#elif defined (CONFIG_RALINK_RT6855A)
	gpio1500,
	gpio3116,
#else
	gpio2300,
#endif
};

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

void signal_handler(int signum)
{
	if (signum == SIGUSR1)
		printf("SIGUSR1");
	else if (signum == SIGUSR2)
		printf("SIGUSR2");
	else
		printf("%d", signum);
	printf(" received\n", signum);
}

/*
void gpio_test_intr(int gpio_num)
{
	//set gpio direction to input
#if defined (CONFIG_RALINK_RT3052)
	gpio_set_dir(gpio5140, gpio_in);
	gpio_set_dir(gpio3924, gpio_in);
	gpio_set_dir(gpio2300, gpio_in);
#elif defined (CONFIG_RALINK_RT3352)
	gpio_set_dir(gpio4540, gpio_in);
	gpio_set_dir(gpio3924, gpio_in);
	gpio_set_dir(gpio2300, gpio_in);
#elif defined (CONFIG_RALINK_RT3883)
	gpio_set_dir(gpio9572, gpio_in);
	gpio_set_dir(gpio7140, gpio_in);
	gpio_set_dir(gpio3924, gpio_in);
	gpio_set_dir(gpio2300, gpio_in);
#elif defined (CONFIG_RALINK_RT5350)
	gpio_set_dir(gpio2722, gpio_in);
	gpio_set_dir(gpio2100, gpio_in);
#elif defined (CONFIG_RALINK_RT6855A)
	gpio_set_dir(gpio3116, gpio_in);
	gpio_set_dir(gpio1500, gpio_in);
#else
	gpio_set_dir(gpio2300, gpio_in);
#endif

	//enable gpio interrupt
	gpio_enb_irq();

	//register my information
	gpio_reg_info(gpio_num);

	//issue a handler to handle SIGUSR1
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	//wait for signal
	pause();

	//disable gpio interrupt
	gpio_dis_irq();
}
*/

void usage(char *cmd)
{
	printf("Usage: %s i(dir_in) <GPIO#>\n", cmd);
	printf("       %s o(dir_out) <GPIO#>\n", cmd);
	printf("       %s s(set) <GPIO#>\n", cmd);
	printf("       %s c(clear) <GPIO#>\n", cmd);
	printf("       %s r(read) <GPIO#>\n", cmd);
	printf("       %s I(Interrupt) <GPIO#> - interrupt test for gpio number\n", cmd);
	exit(0);
}

int main(int argc, char *argv[])
{
    int val = 0;

	if (argc < 2)
		usage(argv[0]);

	switch (argv[1][0]) {
	case 'i':
	    gpio_set_dir(atoi(argv[2]), gpio_in);
		break;
	case 'o':
	    gpio_set_dir(atoi(argv[2]), gpio_out);
		break;
    case 's':
        gpio_write_int(atoi(argv[2]), 1);
        break;
    case 'c':
        gpio_write_int(atoi(argv[2]), 0);
        break;
    case 'r':
        gpio_read_int(atoi(argv[2]), &val);
        printf("Pin %d read %d\n", atoi(argv[2]), val);
        break;
    case 'I':
        signal(SIGUSR1, signal_handler);
        signal(SIGUSR2, signal_handler);
        gpio_enb_irq();
        gpio_reg_info(atoi(argv[2]));

        sleep(20);

        gpio_dis_irq();
        break;
	default:
		usage(argv[0]);
	}

	return 0;
}

