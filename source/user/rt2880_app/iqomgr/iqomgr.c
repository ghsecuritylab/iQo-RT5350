#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/autoconf.h>
#include "spi_drv.h"
#include "ralink_gpio.h"
#include <time.h>
#include "jansson.h"

#define GPIO_DEV	"/dev/gpio"

#define FAIL        0
#define SUCCESS     1

#define NRF_CMD_POLL            0x00

#define NRF_IDLE_NOTIFY         0x20
#define NRF_ACC_NOTIFY          0x21
#define NRF_TEMP_NOTIFY         0x22

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

int acc_count = 0;
int period_count = 0;
int max_period_count = 12;      // By default dump data every hour
int nrf_link_up = 0;
time_t previous_time = 0;
json_t *json_array_ptr = NULL;

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

void interrupt_handler(int s)
{
    interrupted = 1;
    printf("Interrupted, quit...\n");
}

// Issue interface reset towards nRF51822
void reset_nrf_link(void)
{
    gpio_write_int(11, 0);
    sleep(1);
    gpio_write_int(11, 1);
    sleep(1);
    gpio_write_int(11, 0);
    sleep(1);
    gpio_write_int(11, 1);
    sleep(1);
    gpio_write_int(11, 0);
    sleep(1);
    printf("nrf link reset done!\n");
}

int nrf_link_poll(void)
{
    unsigned char header_buf[2] = {0};
    unsigned char body_buf[4] = {0};
	int flag = 0, fd;
    unsigned char cnt;
    SPI_INTERMCU intermcu;

    signal_up = signal_down = 0;

    // First cmd: POLL
    gpio_write_int(11, 1);
    
    cnt = 0;
    while (signal_up == 0 && cnt < 10) {
        usleep(10000);
        cnt++;
    }
    if (signal_up == 0) {
        printf("nrf link poll failed stage 1!\n");
        gpio_write_int(11, 0);
        return FAIL;
    }
    signal_up = 0;
    gpio_write_int(11, 0);

    header_buf[0] = NRF_CMD_POLL;
    header_buf[1] = 4;

    usleep(10000);
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

    cnt = 0;
    while (signal_down == 0 && cnt < 10) {
        usleep(10000);
        cnt++;
    }
    if (signal_down == 0) {
        printf("nrf link poll failed stage 2!\n");
        return FAIL;
    }
    signal_down = 0;

    // Second cmd: the cmd body
    gpio_write_int(11, 1);
    
    cnt = 0;
    while (signal_up == 0 && cnt < 10) {
        usleep(10000);
        cnt++;
    }
    if (signal_up == 0) {
        printf("nrf link poll failed stage 3!\n");
        gpio_write_int(11, 0);
        return FAIL;
    }
    signal_up = 0;
    gpio_write_int(11, 0);

    usleep(10000);
    signal_down = 0;
    fd = open("/dev/spiS0", O_RDONLY); 
    if (fd <= 0) {
        printf("Please insmod module spi_drv.o!\n");
        return -1;
    }
    intermcu.size = 4;
    intermcu.buf = body_buf;
    ioctl(fd, RT2880_SPI_INTERMCU_READ, &intermcu);
	close(fd);

    if (body_buf[0] == NRF_ACC_NOTIFY) {
        acc_count++;
        printf("got acc notify\n");
    } else if (body_buf[0] != NRF_IDLE_NOTIFY) {
        printf("got unknown notify: 0x%x\n", body_buf[0]);
    }

    cnt = 0;
    while (signal_down == 0 && cnt < 10) {
        usleep(10000);
        cnt++;
    }
    if (signal_down == 0) {
        printf("nrf link poll failed stage 4!\n");
        return FAIL;
    }
    signal_down = 0;
    return SUCCESS;
}

// Check sensor output and ask NRF to tune the light
int main(int argc, char *argv[])
{
    time_t time_now;
    struct sigaction sigIntHandler;
    char fn[128] = {0};
    char cmd_str[128] = {0};

    if (argc == 2) {
        max_period_count = atoi(argv[1]);
    }
    printf("default dump period: %d minutes\n", (max_period_count * 5));

    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    sigIntHandler.sa_handler = interrupt_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    // Configure IO port
	gpio_set_dir(7, gpio_in);
    gpio_enb_irq();
    gpio_reg_info(7);
    gpio_set_dir(11, gpio_out);

    while (interrupted == 0) {
        if (nrf_link_poll() == FAIL) {
            reset_nrf_link();
        } else {
            // Initialize link status
            if (nrf_link_up == 0) {
                nrf_link_up = 1;
                previous_time = time(NULL);
            }
        }
        // Update time counter
        if (nrf_link_up == 1) {
            time_now = time(NULL);
            if ((time_now - previous_time) > 30) {
                period_count++;
                previous_time = time_now;
                if (json_array_ptr == NULL) {
                    json_array_ptr = json_array();
                } 

                if (json_array_ptr == NULL) {
                    printf("JSON Error 1\n");
                } else {
                    json_t *json_element = json_array();
                    if (json_element == NULL) {
                        printf("JSON Error 2\n");
                    } else {
                        json_array_append_new(json_element, json_integer(time_now));
                        json_array_append_new(json_element, json_integer(acc_count));
                        acc_count = 0;
                        json_array_append(json_array_ptr, json_element);
                    }
                }
            }

            if (period_count == max_period_count) {
                period_count = 0;
                struct tm *calendar = localtime(&time_now);

                if (calendar == NULL) {
                    printf("localtime failed!\n");
                } else {
                    sprintf(fn, "/tmp/%d_%d_%d_%d_%d_%d.json", calendar->tm_year, calendar->tm_mon,
                                calendar->tm_mday, calendar->tm_hour, calendar->tm_min, calendar->tm_sec);
                    if (json_dump_file(json_array_ptr, fn, 0) != 0) {
                        printf("json_dump_file failed!\n");
                    } else {
                        printf("json_dump_file %s done!\n", fn);
                        strcpy(cmd_str, "/bin/aws_upload ");
                        strcat(cmd_str, fn);
                        system(cmd_str);
                        printf("aws_upload done!\n");
                    }
                    size_t index;
                    json_t *elem;

                    json_array_foreach(json_array_ptr, index, elem) {
                        json_decref(elem);
                    }
                    json_decref(json_array_ptr);
                    json_array_ptr = NULL;
                }
            }
        }
        usleep(100000);
    }

    gpio_dis_irq();

    return 0;
}

