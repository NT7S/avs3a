/*
 *            avs3a.h
 *
 *  Thu Sep 25 09:39:22 2008
 *  Copyright  2008  Jason Milldrum
 *  <milldrum@gmail.com>
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>				// UNIX standard function definitions
#include <fcntl.h>				// File control definitions
#include <errno.h>				// Error number definitions
#include <termios.h>				// POSIX terminal control definitions
#include <sys/ioctl.h>

#define	VER		1
#define	REV		3

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;

extern	int	slave_serial(uint32_t bitStreamLen, uint8_t *bitstream);
extern	int	load_config(int config);
extern	int	drive_prog(int level);
extern	int	drive_mode(int mode);
extern	int	spi_mode(int mode);
extern	int	get_config(void);
extern	int	read_init(void);
extern	int	read_done(void);
extern	int	ss_program(uint32_t bitStreamLen);
extern	int	jtag_mux(int level);
extern	int	fpga_rst(int level);
extern	int	get_ver(void);
extern	void	display_usage(void);
extern	int	ss_xfer(uint32_t bitStreamLen, uint8_t *bitstream);

extern	int	ser_open(char *ser_port);
extern	int	ser_write(int fd, char *tx_buffer, int debug);
extern	int	ser_raw_write(int fd, const uint8_t *txPkt, int pktLen, int debug);
extern	int	ser_read(int fd, char *rx_buffer, int cnt, int debug);
extern	void	ser_close(int fd);
extern	void	ser_option_set_lineout(int fd);
extern	void	ser_option_set_rawout(int fd);
