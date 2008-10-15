/*
 *            avs3a.c
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

#include "avs3a.h"

#define	MAX_TX			32
#define	DEBUG			1

enum	{ACK=0, BOOL, STR};
enum	{UART=0, SPI, JTAG, NONE};

typedef struct CMD_S
{
    char	*text;
    int		numParams;
    int		expAckCnt;
    int		offset;
    int		type;
} cmd_t;

/*
 * Keep the order of the table in sync with the enum list
 */
cmd_t cmd_tbl[] =
{
    {"load_config",	1, 1, 0, ACK},
    {"drive_prog",	1, 1, 0, ACK},
    {"drive_mode",	1, 1, 0, ACK},
    {"spi_mode",	1, 1, 0, ACK},
    {"read_init",	0, 2, 4, BOOL},
    {"read_done",	0, 2, 4, BOOL},
    {"sf_transfer",	0, 1, 0, ACK},
    {"ss_program",	1, 1, 0, ACK},
    {"jtag_mux",	0, 1, 0, ACK},
    {"get_config",	0, 1, 0, BOOL},
    {"usb_bridge",	0, 1, 0, ACK},
    {"fpga_rst",	1, 1, 0, ACK},
    {"get_ver",		0, 1, 0, STR}
};

enum	{LOAD_CONFIG=0, DRIVE_PROG, DRIVE_MODE, SPI_MODE,
	 READ_INIT, READ_DONE, SF_TRANSFER, SS_PROGRAM,
	 JTAG_MUX, GET_CONFIG, USB_BRIDGE, FPGA_RST, GET_VER};

/*
 * drive_prog, spi_mode defines
 */
enum	{LOW=0, HIGH};

/*
 * read_init defines
 */
#define INIT_LOW		0
#define INIT_HIGH		1

/*
 * drive_mode defines
 */
enum	{SLAVE_SERIAL=7, TRI_STATE};

/*
 * Command defines
 */
enum {SS_CONFIG = 1};

/*
 * First 13 bytes of a typical Xilinx bit-stream file
 */
uint8_t hdr[] = {0x00, 0x09, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x00, 0x00, 0x01};

struct options_t
{
    char	*filename;
    char	*serial_port;
    int		cmd;
} options;

static const struct option long_options[] =
{
    {"bitstream",	required_argument,	NULL, 'b'},
    {"port",		required_argument,	NULL, 'p'},
    {"slaveser",	no_argument,		NULL, 's'},
    {"verbose",		no_argument,		NULL, 'v'},
    {"help",		no_argument,		NULL, 'h'},
    { NULL,		no_argument,		NULL, 0 }
};

static int ser_fd = 0;					// Serial port file handle
static const char* option_string = "sb:p:h?:v";		// getopts_long option string

char	sourceFile[256];
char	devType[32];
char	buildDate[32];
char	buildTime[32];
int	verbose;

int main(int argc, char *argv[])
{
    FILE
	*fp;
    uint32_t
	fileLen;
    uint8_t
	*bitData, *p, et;
    int
	i, n, rc, opt, long_index, configSize;

    ser_fd = 0;
    opt = 0;
    long_index = 0;
    opterr = 0;
    options.cmd = 0;
    verbose = 0;
    
    while ((opt = getopt_long(argc, argv, option_string, long_options, &long_index)) != EOF)
    {
	switch(opt)
	{
	case 'b':
	    options.filename = optarg;
	    break;
	case 'p':
	    options.serial_port = optarg;
	    break;
	case 's':
	    options.cmd = SS_CONFIG;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	case 'h':
	case 0:
	    if (optopt == 'b')
		fprintf(stderr, "You must specify a filename\n");
	    else if (optopt == 'p')
		fprintf(stderr, "You must specify a serial port\n");
	    else
		display_usage();
	    return EXIT_FAILURE;
	default:
	    abort();
	}
    }

    /*
     * Check for required arguments
     */
    if (argc == 1)
    {
	display_usage();
	return(EXIT_FAILURE);
    }
    if (options.filename == NULL)
    {
	fprintf(stderr, "No bitstream specified\n");
	return(EXIT_FAILURE);
    }
    if (options.serial_port == NULL)
    {
	fprintf(stderr, "No serial port specified\n");
	return(EXIT_FAILURE);
    }

    /*
     * Don't think we should get here, but just in case...
     */
    if (options.cmd == 0)
    {
	fprintf(stderr, "Invalid command\n");
	return(EXIT_FAILURE);
    }
    
    /*
     * If we couldn't open the serial port, fail and exit
     */
    ser_fd = ser_open(options.serial_port);
    if (ser_fd == -1) return(EXIT_FAILURE);
    
    /*
     * Open the bitstream
     */
    fp = fopen(options.filename, "rb");
    if (fp == NULL)
    {
	fprintf(stderr, "Failed to open file\n");
	return(EXIT_FAILURE);
    }

    /*
     * Get the file length
     */
    fseek(fp, 0, SEEK_END);
    fileLen = ftell(fp);
    rewind(fp);

    /*
     * Dynamically allocate memory for file
     */
    bitData = calloc(fileLen + 1, sizeof(uint8_t));

    /*
     * Get the binary data in one big chunk, then close the file
     */
    fread(bitData, sizeof(uint8_t), fileLen, fp);
    fclose(fp);

    /*
     * First 13 bytes must mean somebody at Xilinx. We just read and compare
     * against expected. For now, fail with warning message on miscompare.
     */
    p = bitData;
    for (i = 0; i < 13; i++)
    {
	if (hdr[i] != *p++)
	{
	    printf("This file does not appear to be a valid Xilinx bitstream\n");
	    exit(1);
	}
    }

    /*
     * Extract the data file name
     */
    et = *p++;
    n = (p[0] << 8) | p[1];
    p += 2;
    for (i = 0; i < n; i++) sourceFile[i] = *p++;

    /*
     * Extract the device type
     */
    et = *p++;
    n = (p[0] << 8) | p[1];
    p += 2;
    for (i = 0; i < n; i++) devType[i] = *p++;
    
    /*
     * Extract the build date
     */
    et = *p++;
    n = (p[0] << 8) | p[1];
    p += 2;
    for (i = 0; i < n; i++) buildDate[i] = *p++;
    
    /*
     * Extract the build time
     */
    et = *p++;
    n = (p[0] << 8) | p[1];
    p += 2;
    for (i = 0; i < n; i++) buildTime[i] = *p++;
    
    /*
     * Extract the image size
     */
    et = *p++;
    configSize = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    p += 4;

    if (verbose)
    {
	printf("\n");
	printf("Source File: %s\n", sourceFile);
	printf("Build Date : %s %s\n", buildDate, buildTime);
	printf("Device Type: %s\n", devType);
	printf("\n");
    }
    
    if (strcmp(devType, "3s400aft256") != 0)
    {
	printf("\nDevice code %s is not supported by this utility\n\n", devType);
	exit(EXIT_FAILURE);
    }

    switch(options.cmd)
    {
    case 1:
	rc = slave_serial(configSize, p);
	if (rc != EXIT_SUCCESS) rc = EXIT_FAILURE;
	break;
    default:
	fprintf(stderr, "Invalid command\n");
	rc = EXIT_FAILURE;
	break;
    }

    free(bitData);
    ser_close(ser_fd);
    
    return(rc);
}

/*
 * Perform the configure FPGA via the PSOS
 */
int slave_serial(uint32_t bitStreamLen, uint8_t *bitStream)
{
    uint8_t
	init_status;

    get_ver();
    init_status = 0;
    load_config(SPI);
    drive_prog(LOW);
    drive_mode(SLAVE_SERIAL);
    spi_mode(HIGH);
    drive_prog(HIGH);
    do
    {
	init_status = read_init();
    }
    while(init_status != INIT_HIGH);

    drive_mode(TRI_STATE);
    fpga_rst(LOW);
    do
    {
	init_status = get_config();
    }
    while(init_status != 0x31);
    
    ss_program(bitStreamLen, bitStream);

    spi_mode(LOW);

    if (read_init() != INIT_HIGH) return(EXIT_FAILURE);
    if (read_done() != INIT_HIGH) return(EXIT_FAILURE);
    load_config(UART);
    fpga_rst(HIGH);
    
    return(EXIT_SUCCESS);
}


/*
 * Generic AVS2A interface routine
 */
void *avs3aXfer(int cmd, int param, int trace)
{
    void
	*p;
    int
	i, j, rc, maxPass;
    static char
	command[100], response[200];

    j = 0;

    switch (cmd_tbl[cmd].numParams)
    {
    case 0:
	sprintf(command, "%s", cmd_tbl[cmd].text);
	break;
    case 1:
	sprintf(command, "%s %d", cmd_tbl[cmd].text, param);
	break;
    }
    
    maxPass = 5;
    do
    {
	ser_write(ser_fd, command, trace);
	rc = ser_read(ser_fd, response, cmd_tbl[cmd].expAckCnt, trace);
	if (rc == 0)
	{
	    usleep(250);
	    maxPass--;
	}
    } while ((rc == 0) && (maxPass > 0));

    if (maxPass == 0)
    {
	printf("\nError : unable to receive expected response\n");
	exit(1);
    }
    
    p = NULL;
    switch (cmd_tbl[cmd].type)
    {
    case ACK:
	p = NULL;
	break;
    case BOOL:
	i = response[cmd_tbl[cmd].offset];
	p = (void *)i;
	break;
    case STR:
	p = &response[cmd_tbl[cmd].offset];
	break;
    }

    return(p);
};


/*
 * Set the PSOC config level
 */
int load_config(int config)
{
    avs3aXfer(LOAD_CONFIG, config, verbose);

    return(0);
}


/*
 * Assert the PROG level
 *
 */
int drive_prog(int level)
{
    avs3aXfer(DRIVE_PROG, level, verbose);

    return(0);
}


/*
 * Assert the DRIVE mode
 */
int drive_mode(int mode)
{
    avs3aXfer(DRIVE_MODE, mode, verbose);

    return(0);
}


/*
 * Assert the SPI mode
 */
int spi_mode(int mode)
{
    avs3aXfer(SPI_MODE, mode, verbose);

    return(0);
}


/*
 * Get the INIT signal level
 */
int read_init(void)
{
    int
	rc;

    rc = (int)avs3aXfer(READ_INIT, 0, verbose);

    return(rc);
}


/*
 * Read the DONE bit
 */
int read_done(void)
{
    int
	rc;
    
    rc = (int)avs3aXfer(READ_DONE, 0, verbose);

    return(rc);
}


/*
 * Download the bit stream FPGA configuration file
 */
int ss_program(uint32_t bitStreamLen, uint8_t *bitStream)
{
    char
	command[100], response[100];
    int
	pktSize, i, txByteCnt, pkts, rc, expNumAcks, trace;
    uint8_t
	*bsp;
    

    avs3aXfer(SS_PROGRAM, bitStreamLen, verbose);

    ser_option_set_rawout(ser_fd);
    printf("\n");

    trace = 0;
    /*
     * Send bitfile bytes. We cannot use the generic command because
     * there is no (to date) way to recover from a bit stream error.
     */
    pkts = bitStreamLen / MAX_TX;
    bsp = bitStream;
    txByteCnt = 0;
    do
    {
	if ((bitStreamLen - txByteCnt) >= MAX_TX)
	    pktSize = MAX_TX;
	else
	    pktSize = bitStreamLen - txByteCnt;

	if (pktSize > 16)
	    expNumAcks = 2;
	else
	    expNumAcks = 1;

	expNumAcks = 1;
	txByteCnt += ser_raw_write(ser_fd, bsp, pktSize, trace);
       	bsp += pktSize;
	rc = ser_read(ser_fd, response, expNumAcks, trace);

	if (rc != 1)
	{
	    printf("\nError: invalid ack sequence\n");
	    exit(1);
	}
	if (trace == 0) printf("Bytes written: %d\r", txByteCnt);
	fflush(stdout);
    }
    while(txByteCnt+1 < bitStreamLen);
    printf("\n\n");
    
    ser_option_set_lineout(ser_fd);

    /*
     * Get one last ACK
     */
    rc = ser_read(ser_fd, response, 1, 0);
    if (rc != 1) printf("Error\n");
    
    return(0);
}


/*
 * Select the JTAX mux
 */
int jtag_mux(int level)
{
    avs3aXfer(JTAG_MUX, level, verbose);
    
    return(0);
}


/*
 * Get the current config level
 */
int get_config(void)
{
    int
	rc;
    
    rc = (int)avs3aXfer(GET_CONFIG, 0, verbose);

    return(rc);
}


/*
 * Assert the FPGA reset pin
 */
int fpga_rst(int level)
{
    avs3aXfer(FPGA_RST, level, verbose);

    return(0);
}


/*
 * Get Version
 */
int get_ver(void)
{
    char
	*version;

    version = avs3aXfer(GET_VER, 0, verbose);
    
    if (verbose == 0) printf("\nPSoC Version %s\n", version);
    return(0);
}


/*
 *
 *
 */
void display_usage(void)
{
    fprintf(stderr, "Usage: avs3a [options]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -b, --bitstream=BITSTREAM  Bitstream used to configure FPGA\n");
    fprintf(stderr, " -p, --port=SERIAL-PORT     Serial port connected to eval board\n");
    fprintf(stderr, " -s, --slaveser             Configure FPGA in Slave Serial mode\n");
    fprintf(stderr, " -v, --verbose              Set verbose mode\n");
}


    
