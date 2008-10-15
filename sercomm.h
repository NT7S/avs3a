/***************************************************************************
 *            sercomm.h
 *
 *  Thu Sep 25 09:39:22 2008
 *  Copyright  2007  Jason Milldrum
 *  <milldrum@gmail.com>
 ****************************************************************************/

/*
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

extern int	ser_open(char *ser_port);
extern int	ser_write(int fd, char *tx_buffer);
extern int	ser_raw_write(int fd, const uint8_t *tx_buffer, int msg_len);
extern int	ser_read(int fd, char *rx_buffer);
extern void	ser_close(int fd);
extern void	ser_option_set_lineout(int fd);
extern void	ser_option_set_rawout(int fd);
