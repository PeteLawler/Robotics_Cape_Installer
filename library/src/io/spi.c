/**
 * @file spi.c
 *
 * @author     James Strawson
 * @date       1/19/2018
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>	// for memset
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <rc/model.h>
#include <rc/gpio.h>
#include <rc/pinmux.h>
#include <rc/spi.h>

#define N_SS			2 // number of slave select lines
#define SPI10_PATH		"/dev/spidev1.0"
#define SPI11_PATH		"/dev/spidev1.1"
#define SPI_MAX_SPEED		24000000	// 24mhz
#define SPI_MIN_SPEED		1000		// 1khz
#define SPI_BITS_PER_WORD	8

// Cape SS1 gpio P9_28, normally SPI mode
#define CAPE_SS1_CHIP	3
#define CAPE_SS1_PIN	17
// Cape SS2 gpio P9_23, normally GPIO mode
#define CAPE_SS2_CHIP	1
#define CAPE_SS2_PIN	17
// Blue SS1 gpio 0_29 pin H18
#define BLUE_SS1_CHIP	0
#define BLUE_SS1_PIN	29
// Blue SS2 gpio 0_7 pin H18
#define BLUE_SS2_CHIP	0
#define BLUE_SS2_PIN	7

// Blue Only
#define BLUE_SPI_PIN_6_SS1	29	// gpio 0_29 pin H18
#define BLUE_SPI_PIN_6_SS2	7	// gpio 0_7  pin C18

static int _rc_spi_fd[N_SS];		// file descriptor for SPI1_PATH device cs0, cs1
static int rc_spi_init_flag[N_SS];	// set to 1 after successful initialization
static int rc_spi_gpio_init_flag[N_SS];// init flag for manual-mode gpio SS lines
static int rc_spi_gpio_ss_chip[N_SS];	// holds gpio pin chip for slave select lines
static int rc_spi_gpio_ss_pin[N_SS];	// holds gpio pins offset for slave select lines
static int rc_spi_speed[N_SS];		// speed in hz
static int rc_spi_pinmux_id[N_SS];


int rc_spi_init(int slave, int slave_mode, int bus_mode, int speed_hz)
{
	int bits = SPI_BITS_PER_WORD;
	rc_model_t model = rc_model();

	// sanity checks
	if(slave!=1 && slave!=2){
		fprintf(stderr,"ERROR in rc_spi_init, slave must be 1 or 2\n");
		return -1;
	}
	if(speed_hz>SPI_MAX_SPEED || speed_hz<SPI_MIN_SPEED){
		fprintf(stderr,"ERROR in rc_spi_init, speed_hz must be between %d & %d\n", SPI_MIN_SPEED, SPI_MAX_SPEED);
		return -1;
	}
	if(slave_mode!=SPI_SLAVE_MODE_AUTO && slave_mode!=SPI_SLAVE_MODE_MANUAL){
		fprintf(stderr,"ERROR in rc_spi_init, slave_mode must be SPI_SLAVE_MODE_AUTO or SPI_SLAVE_MODE_MANUAL\n");
		return -1;
	}
	if(model!=BB_BLUE && slave==2 && slave_mode==SPI_SLAVE_MODE_AUTO){
		fprintf(stderr,"ERROR in rc_spi_init, SPI_SLAVE_MODE_AUTO not available on slave 2 with Robotics Cape\n");
		return -1;
	}
	if(bus_mode!=SPI_MODE_0 && bus_mode!=SPI_MODE_1 && bus_mode!=SPI_MODE_2 && bus_mode!=SPI_MODE_3){
		fprintf(stderr,"ERROR in rc_spi_init, bus_mode must be SPI_MODE_0, 1, 2, or 3\n");
		return -1;
	}

	// get file descriptor for spi1 device
	if(slave==1){
		_rc_spi_fd[0]=open(SPI10_PATH, O_RDWR);
		if(_rc_spi_fd[0]==-1){
			perror("ERROR in rc_spi_init");
			if(errno!=EPERM) fprintf(stderr,"likely SPI is not enabled in the device tree or kernel\n");
			return -1;
		}
	}
	else{
		_rc_spi_fd[1]=open(SPI11_PATH, O_RDWR);
		if(_rc_spi_fd[1]==-1){
			perror("ERROR in rc_spi_init");
			if(errno!=EPERM) fprintf(stderr,"likely SPI is not enabled in the device tree or kernel\n");
			return -1;
		}
	}

	// set settings
	if(ioctl(_rc_spi_fd[slave-1], SPI_IOC_WR_MODE, &bus_mode)==-1){
		perror("ERROR in rc_spi_init setting spi mode");
		close(_rc_spi_fd[slave-1]);
		return -1;
	}
	if(ioctl(_rc_spi_fd[slave-1], SPI_IOC_WR_BITS_PER_WORD, &bits)==-1){
		perror("ERROR in rc_spi_init setting bits per word");
		close(_rc_spi_fd[slave-1]);
		return -1;
	}
	if(ioctl(_rc_spi_fd[slave-1], SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz)==-1){
		perror("ERROR in rc_spi_init setting max speed hz");
		close(_rc_spi_fd[slave-1]);
		return -1;
	}

	// set up slave select pins, pin definitions in <rc/pinmux.h>
	if(model==BB_BLUE){
		rc_spi_gpio_ss_chip[0] = BLUE_SS1_CHIP;
		rc_spi_gpio_ss_pin[0]  = BLUE_SS1_PIN;
		rc_spi_pinmux_id[0]    = BLUE_SPI_PIN_6_SS1;
		rc_spi_gpio_ss_chip[1] = BLUE_SS2_CHIP;
		rc_spi_gpio_ss_pin[1]  = BLUE_SS2_PIN;
		rc_spi_pinmux_id[1]    = BLUE_SPI_PIN_6_SS1;

	}
	else{
		rc_spi_gpio_ss_chip[0] = CAPE_SS1_CHIP;
		rc_spi_gpio_ss_pin[0]  = CAPE_SS1_PIN;
		rc_spi_pinmux_id[0]    = CAPE_SPI_PIN_6_SS1;
		rc_spi_gpio_ss_chip[1] = CAPE_SS2_CHIP;
		rc_spi_gpio_ss_pin[1]  = CAPE_SS2_PIN;
		rc_spi_pinmux_id[1]    = CAPE_SPI_PIN_6_SS2;
	}

	if(slave_mode == SPI_SLAVE_MODE_AUTO){
		if(rc_pinmux_set(rc_spi_pinmux_id[slave-1], PINMUX_SPI)){
			fprintf(stderr,"ERROR in rc_spi_init, failed to set slave select pinmux to SPI mode\n");
			return -1;
		}
	}
	else{
		if(rc_pinmux_set(rc_spi_pinmux_id[slave-1], PINMUX_GPIO)){
			fprintf(stderr,"ERROR in rc_spi_init, failed to set slave select pinmux to GPIO mode\n");
			return -1;
		}
		if(rc_gpio_init(rc_spi_gpio_ss_chip[slave-1],rc_spi_gpio_ss_pin[slave-1], GPIOHANDLE_REQUEST_OUTPUT)){
			fprintf(stderr,"ERROR in rc_spi_init failed to initialize slave select gpio pin\n");
			return -1;
		}
		// make sure slave begins deselected
		if(rc_gpio_set_value(rc_spi_gpio_ss_chip[slave-1],rc_spi_gpio_ss_pin[slave-1], 1)){
			fprintf(stderr,"ERROR in rc_spi_init, failed to write to gpio slave select pin\n");
			return -1;
		}
	}
	// all done, store speed and flag initialization
	rc_spi_speed[slave-1] = speed_hz;
	rc_spi_init_flag[slave-1] = 1;
	rc_spi_gpio_init_flag[slave-1]=1;
	return 0;
}


int rc_spi_fd(int slave)
{
	// sanity checks
	if(slave!=1 && slave !=2){
		fprintf(stderr,"ERROR in rc_spi_fd, slave must be 1 or 2\n");
		return -1;
	}
	if(rc_spi_init_flag[slave-1]==0){
		fprintf(stderr,"ERROR in rc_spi_fd, call rc_spi_init first\n");
		return -1;
	}
	return _rc_spi_fd[slave-1];
}


int rc_spi_close(int slave)
{
	// sanity checks
	if(slave!=1 && slave !=2){
		fprintf(stderr,"ERROR in rc_spi_close, slave must be 1 or 2\n");
		return -1;
	}
	// deselect if in manual mode
	if(rc_spi_gpio_init_flag[slave-1]){
		rc_spi_select((slave-1),0);
		rc_gpio_cleanup(rc_spi_gpio_ss_chip[slave-1],rc_spi_gpio_ss_pin[slave-1]);
	}
	close(_rc_spi_fd[slave-1]);
	rc_spi_init_flag[slave-1]=0;
	rc_spi_gpio_init_flag[slave-1]=0;
	return 0;
}


int rc_spi_select(int slave, int select)
{
	// sanity checks
	if(slave!=1 && slave !=2){
		fprintf(stderr,"ERROR in rc_spi_select, slave must be 1 or 2\n");
		return -1;
	}
	if(rc_spi_gpio_init_flag[slave-1]==0){
		fprintf(stderr,"ERROR in rc_spi_select, slave must be configured with SPI_SLAVE_MODE_MANUAL to use this function\n");
		return -1;
	}

	// invert select to it's pulled low when selecting pin
	if(rc_gpio_set_value(rc_spi_gpio_ss_chip[slave-1],rc_spi_gpio_ss_pin[slave-1], !select)==-1){
		fprintf(stderr,"ERROR in rc_spi_select writing to gpio pin\n");
		return -1;
	}
	return 0;
}


int rc_spi_transfer(int slave, char* tx_data, int tx_bytes, char* rx_data)
{
	int ret;
	struct spi_ioc_transfer xfer;

	// sanity checks
	if(slave!=1 && slave!=2){
		fprintf(stderr,"ERROR in rc_spi_transfer, slave must be 1 or 2\n");
		return -1;
	}
	if(rc_spi_init_flag[slave-1]==0){
		fprintf(stderr,"ERROR: in rc_spi_transfer call rc_spi_init first\n");
		return -1;
	}
	if(tx_bytes<1){
		fprintf(stderr,"ERROR: in rc_spi_transfer, tx_bytes must be >=1\n");
		return -1;
	}

	// fill in send struct
	xfer.cs_change = 1;
	xfer.delay_usecs = 0;
	xfer.speed_hz = rc_spi_speed[slave-1];
	xfer.bits_per_word = SPI_BITS_PER_WORD;
	xfer.tx_buf = (unsigned long) tx_data;
	xfer.rx_buf = (unsigned long) rx_data;
	xfer.len = tx_bytes;

	// do ioctl transfer
	ret=ioctl(_rc_spi_fd[slave-1], SPI_IOC_MESSAGE(1), &xfer);
	if(ret==-1){
		perror("ERROR in rc_spi_transfer");
		return -1;
	}
	return ret;
}


int rc_spi_write(int slave, char* data, int bytes)
{
	int ret;
	struct spi_ioc_transfer xfer;

	// sanity checks
	if(slave!=1 && slave!=2){
		fprintf(stderr,"ERROR in rc_spi_write, slave must be 1 or 2\n");
		return -1;
	}
	if(rc_spi_init_flag[slave-1]==0){
		fprintf(stderr,"ERROR: in rc_spi_write call rc_spi_init first\n");
		return -1;
	}
	if(bytes<1){
		fprintf(stderr,"ERROR: in rc_spi_write, bytes must be >=1\n");
		return -1;
	}

	// fill in send struct
	xfer.cs_change = 1;
	xfer.delay_usecs = 0;
	xfer.speed_hz = rc_spi_speed[slave-1];
	xfer.bits_per_word = SPI_BITS_PER_WORD;
	xfer.rx_buf = 0;
	xfer.tx_buf = (unsigned long) data;
	xfer.len = bytes;

	// send
	ret=ioctl(_rc_spi_fd[slave-1], SPI_IOC_MESSAGE(1), &xfer);
	if(ret==-1){
		perror("ERROR in rc_spi_write");
		return -1;
	}
	return ret;
}


int rc_spi_read(int slave, char* data, int bytes)
{
	int ret;
	struct spi_ioc_transfer xfer;

	// sanity checks
	if(slave!=1 && slave!=2){
		fprintf(stderr,"ERROR in rc_spi_read, slave must be 1 or 2\n");
		return -1;
	}
	if(rc_spi_init_flag[slave-1]==0){
		fprintf(stderr,"ERROR: in rc_spi_read call rc_spi_init first\n");
		return -1;
	}
	if(bytes<1){
		fprintf(stderr,"ERROR: in rc_spi_read, bytes must be >=1\n");
		return -1;
	}

	// fill in send struct
	xfer.cs_change = 1;
	xfer.delay_usecs = 0;
	xfer.speed_hz = rc_spi_speed[slave-1];
	xfer.bits_per_word = SPI_BITS_PER_WORD;
	xfer.rx_buf = (unsigned long) data;
	xfer.tx_buf = 0;
	xfer.len = bytes;

	// read
	ret=ioctl(_rc_spi_fd[slave-1], SPI_IOC_MESSAGE(1), &xfer);
	if(ret==-1){
		perror("ERROR in rc_spi_read");
		return -1;
	}
	return ret;
}


