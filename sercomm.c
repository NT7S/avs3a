/*
 *            sercomm.c
 *
 *  Thu Sep 25 09:39:22 2008
 *  Copyright  2007  Jason Milldrum
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

#include "avs3a.h"

int ser_open(char *ser_port)
{
    int
	fd, n;					// File descriptor for the port
    uint8_t
	tmp_buf[100];
    
    fd = open(ser_port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
	fprintf(stderr, "Unable to open %s\n", ser_port);
    }
    else
    {
	fcntl(fd, F_SETFL, 0);
	ser_option_set_lineout(fd);
    }
    do
    {
	n = read(fd, tmp_buf, 64);
    } while(n > 0);
    
    return(fd);
}

int ser_write(int fd, char *tx_buffer, int debug)
{
    int
	tx_len, bytes_sent;

    ioctl(fd, TCFLSH, 2);
    
    if (debug > 0) printf("%-20s", tx_buffer);

    tx_len = strlen(tx_buffer);
    bytes_sent = write(fd, tx_buffer, tx_len);
    if (bytes_sent != tx_len)
    {
	fputs("serial port write failed - ", stderr);
	fprintf(stderr, "%s", strerror(errno));
    }

    return(bytes_sent);
}

int ser_raw_write(int fd, const uint8_t *tx_buffer, int msg_len, int debug)
{
    int
	i, bytes_sent, rc, tx_cnt;
    char
	*er;

//    ioctl(fd, TCFLSH, 2);

    if (debug > 0)
    {
	for (i = 0; i < msg_len; i++)
	{
	    if ((i % 16) == 0)
	    {
		if (i == 0)
		    printf("tx: %2d :", msg_len);
		else
		    printf("       :");
	    }
	    printf("%3.2X", tx_buffer[i]);
	    if (i == 15) printf("\n");
	}
    }
    
    rc = write(fd, tx_buffer, msg_len);
	
    if (rc == -1)
    {
	er = strerror(errno);
	printf("\nerror = <%s>\n", er);
	exit(1);
    }

    return(rc);
}
	
int ser_read(int fd, char *rx_buffer, int expAckCnt, int debug)
{
    int
	i, j, k, n, done, bytes_rcvd, total_bytes_rcvd, x[100],
	l, numAcks, rc;
    uint8_t
	temp_buffer[100], *p, db;

    done = 0;
    bytes_rcvd = 0;
    total_bytes_rcvd = 0;

    rx_buffer[0] = 0;
    n = 0;
    k = 0;
    numAcks = 0;
    do
    {
	bytes_rcvd = read(fd, temp_buffer, 100);
	if (debug > 0) x[n] = bytes_rcvd; n++;
	if (bytes_rcvd > 0)
	{
	    total_bytes_rcvd += bytes_rcvd;
	    for (j = 0; j < bytes_rcvd; j++) rx_buffer[k++] = temp_buffer[j];

	    numAcks = 0;
	    p = rx_buffer;
	    for (j = 0; j <= total_bytes_rcvd-4; j++)
	    {
		rc = memcmp("ack\0", p, 4);
		if (rc == 0) numAcks++;
		p++;
	    }
	}
	else
	{
	    usleep(100);
	    //    done = 1;
	}
    } while ((done == 0) && (numAcks < expAckCnt));


    if (debug > 0)
    {
	printf("%2d [", total_bytes_rcvd);
	for (i = 0; i <  total_bytes_rcvd; i++)
	{
	    db = rx_buffer[i] & 0xFF;
	    if ((db >= ' ') && (db <= 'z'))
		printf("%c", db);
	    else
		printf("<%2.2X>", db);
	}
	printf("] acks = %d", numAcks);
	printf(", pkts = %d :", n);
	for (i = 0; i < n; i++) printf(" %d", x[i]);
	printf("\n");
    }

    if ((bytes_rcvd < 0) && (errno != EAGAIN))
    {
	fputs("serial port read failed - ", stderr);
	fprintf(stderr, "%s", strerror(errno));
	return(-1);
    }

    rc = numAcks == expAckCnt;
    return(rc);
}

void ser_close(int fd)
{
    close(fd);
}

void ser_option_set_lineout(int fd)
{
    struct termios
	ser_options;

    /*
     * Get the current options for the port
     */
    tcgetattr(fd, &ser_options);

    /*
     * Set the baud rates to 115200
     */
    cfsetispeed(&ser_options, B115200);
    cfsetospeed(&ser_options, B115200);

    /*
     * Set 8N1
     */
    ser_options.c_cflag &= ~PARENB;
    ser_options.c_cflag &= ~CSTOPB;
    ser_options.c_cflag &= ~CSIZE;
    ser_options.c_cflag |= CS8;

    /*
     * Disable hardware and software flow control
     */
    ser_options.c_cflag &= ~CRTSCTS;
    ser_options.c_iflag &= ~(IXON | IXOFF | IXANY);

    /*
     * Raw input and line output
     */
    ser_options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    ser_options.c_oflag |= OPOST;

    /*
     * Set receive timeout to 100 ms, no min chars received
     */
    ser_options.c_cc[VTIME] = 1;
    ser_options.c_cc[VMIN] = 0;

    /*
     * Enable the receiver and set local mode
     */
    ser_options.c_cflag |= (CLOCAL | CREAD);

    /*
     * Set the new options for the port
     */
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &ser_options);
}

void ser_option_set_rawout(int fd)
{
    struct termios
	ser_options;

    /*
     * Get the current options for the port
     */
    tcgetattr(fd, &ser_options);

    /*
     * Set the baud rates to 115200
     */
    cfsetispeed(&ser_options, B115200);
    cfsetospeed(&ser_options, B115200);

    /*
     * Set 8N1
     */
    ser_options.c_cflag &= ~PARENB;
    ser_options.c_cflag &= ~CSTOPB;
    ser_options.c_cflag &= ~CSIZE;
    ser_options.c_cflag |= CS8;
    
    /*
     * Disable hardware and software flow control
     */
    ser_options.c_cflag &= ~CRTSCTS;
    ser_options.c_iflag &= ~(IXON | IXOFF | IXANY);

    /*
     * Raw input and output
     */
    ser_options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    ser_options.c_oflag &= ~OPOST;

    /*
     * Set receive timeout to 100 ms, no min chars received
     */
    ser_options.c_cc[VTIME] = 1;
    ser_options.c_cc[VMIN] = 0;

    /*
     * Enable the receiver and set local mode
     */
    ser_options.c_cflag |= (CLOCAL | CREAD);

    /*
     * Set the new options for the port
     */
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &ser_options);
}
