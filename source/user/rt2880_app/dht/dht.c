#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <linux/autoconf.h>
#include "ralink_gpio.h"

#define GPIO_DEV	"/dev/gpio"

enum {
	gpio_in,
	gpio_out,
};

enum {
	gpio2100,
	gpio2722,
};

int interrupted = 0;

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

void interrupt_handler(int s)
{
    interrupted = 1;
    printf("Interrupted, quit...\n");
}

void signal_handler(int signum)
{
	if (signum == SIGUSR1) {
	} else if (signum == SIGUSR2) {
	} else {
		printf("Unknown signal: %d", signum);
    }
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

// Get DHT output 
int main(int argc, char *argv[])
{
    int fd, idx;
    unsigned char data[5] = {0};
    struct sigaction sigIntHandler;
    ralink_gpio_dht_signal dht;

    // Install signal handler
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    sigIntHandler.sa_handler = interrupt_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    // Configure pin IRQ
    gpio_enb_irq();
    gpio_reg_info(0);

	gpio_set_dir(0, gpio_out);
    usleep(18 * 1000);
    gpio_write_int(0, 1);
    //usleep(40);
	gpio_set_dir(0, gpio_in);

    sleep(1);

    gpio_dis_irq();

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}

	if (ioctl(fd, RALINK_GPIO_GET_DHT_EDGE_INFO, &dht) < 0) {
		perror("ioctl RALINK_GPIO_GET_DHT_EDGE_INFO");
		close(fd);
		return -1;
	}

    /*
    printf("count: %d\n", dht.info_idx);
    for (idx = 0; idx < dht.info_idx; idx++) {
        printf("%d, %lld\n", dht.einfo[idx].edge_type, dht.einfo[idx].timestamp);
    }
    */
    close(fd);

    for (idx = 0; idx < 40; idx++) {
        if ((dht.einfo[4 + idx * 2].timestamp - dht.einfo[3 + idx * 2].timestamp) < 40 * 1000) {
            // 0
        } else {
            // 1
            data[idx / 8] |= 1 << (7 - (idx - (idx / 8) * 8));
        }
    }

    if ((unsigned char)(data[0] + data[1] + data[2] + data[3]) == data[4]) {
        printf("0x%02x 0x%02x 0x%02x 0x%02x\n", data[0], data[1], data[2], data[3]);
        printf("result good\n");
    } else {
        printf("checksum error\n");
    }

    return 0;
}

/*
    // read in timings
    for ( i=0; i< TOTALBYTES; i++) {
        for (j = 0; j < 8; j++) {
            counter = 0;
            while (readSignal() == laststate) {
                counter++;
                delayMicroseconds(1);
                if (counter == 30000) {
                    break;
                }
            }
            laststate = readSignal();

            if (counter == 30000) break;

            counter = 0;
            while (readSignal() == laststate) {
                counter++;
                delayMicroseconds(1);
                if (counter == 30000) {
                    break;
                }
            }
            laststate = readSignal();

            if (counter == 30000) break;

            if (counter > 20) {
                data[i] |= (1 << (7 - j));
                //toggleSignal();
            }
            //toggleSignal();
        }
    }

    interrupts();

    // check we read 40 bits and that the checksum matches
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        return true;
    }
    //toggleSignal();
    //toggleSignal();
    return false;


//boolean S == Scale.  True == Farenheit; False == Celcius
float DHT::readTemperature(bool S) {
    float f;

    f = data[2] & 0x7F;
    f *= 256;
    f += data[3];
    f /= 10;
    if (data[2] & 0x80)
        f *= -1;
    if(S)
        f = convertCtoF(f);

    return f;
}

float DHT::convertCtoF(float c) {
    return c * 9 / 5 + 32;
}

float DHT::readHumidity(void) {
    float f;

    f = data[0];
    f *= 256;
    f += data[1];
    f /= 10;
    return f;
}

int DHT::readSignal(void) {
    //toggleSignal();
    return digitalRead(_pin);
}

boolean DHT::read(void) {
    uint8_t laststate = LOW;
    uint16_t counter = 0;
    uint8_t j = 0, i;
    unsigned long currenttime;

}

*/
