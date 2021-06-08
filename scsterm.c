/********************************************************************
 *
 * SCS simple terminal
 *
 * Copyright (C) 2005-2021 SCS GmbH & Co. KG, Hanau, Germany
 * written by Peter Mack, DL3FCJ (peter.mack@scs-ptc.com)
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 ********************************************************************/

/* compile with: gcc -O3 -Wall -pedantic -o scsterm scsterm.c lock.c -lusb-1.0 */

#define _GNU_SOURCE

/********************************************************************
 * Includes
 ********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <libusb-1.0/libusb.h>
#include <dirent.h>

#include "lock.h"


/*
 USB Product IDs of the SCS devices:
    0xD010 SCS PTC-IIusb
    0xD011 SCS Tracker / DSP TNC
    0xD012 SCS P4dragon DR-7800
    0xD013 SCS P4dragon DR-7400
    0xD014 -
    0xD015 SCS PTC-IIIusb
    0xD016 -
    0xD017 -
*/


/********************************************************************
 * Defines
 ********************************************************************/
#define SYSFS_PATH "/sys/bus/usb/devices/%u-"
#define MAX_SCS_DEVICES 8		// max. number of SCS devices we search for
#ifndef VERSION
#define VERSION "x.x"
#endif


/********************************************************************
 * Structs
 ********************************************************************/
struct Modem {
	const char *type;
	const int baud;
};

struct SCS_Devices {
	char tty[20];	// the tty device, e.g. /dev/ttyUSB1
	uint8_t type;	// index to the modems array
};


/********************************************************************
 * Global Variables
 ********************************************************************/
const struct Modem modems[] = {
	{"PTC-IIusb",			B115200},	// 0
	{"Tracker / DSP TNC",	B38400},	// 1
	{"P4dragon DR-7800",	B38400},	// 2
	{"P4dragon DR-7400",	B38400},	// 3
	{"", 0},							// 4
	{"PTC-IIIusb",			B115200},	// 5
	{"", 0},							// 6
	{"", 0}								// 7
};

int run;


/********************************************************************
 * Helper function for scandir
 * to find the USB serial device name
 ********************************************************************/
static int srchtty (const struct dirent *ep)
{
	if (strstr (ep->d_name, "ttyUSB"))
	{
		return 1;
	}

	return 0;
}


/********************************************************************
 * Helper function to free dirent structure
 ********************************************************************/
void free_dirent (struct dirent ***ent, int n)
{
	struct dirent **ep;

	ep = *ent;
	for (int i = 0; i < n; i++)
	{
		free (ep[i]);
	}
	free (ep);
}


/********************************************************************
 * Search for SCS USB devices
 ********************************************************************/
static int find_devices (struct SCS_Devices devs[])
{
	int err = 0;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor desc;
	int status;
	ssize_t num_devs, i;
	char path[PATH_MAX];
	int n;
	struct dirent **ent;

#define PNUM_MAX 8
	uint8_t pnums[PNUM_MAX];
	int numports;

#define MAX_BUF 10
	char buf[MAX_BUF];

	status = 0;	// 0 device not found, > 0 device found

	err = libusb_init (&ctx);
	if (err)
	{
		fprintf (stderr, "Error: unable to initialize libusb: %i\n", err);
		goto error;
	}

	num_devs = libusb_get_device_list (ctx, &list);
	if (num_devs < 0)
	{
		fprintf (stderr, "Error: getting device list: %li\n", num_devs);
		goto error1;
	}

	for (i = 0; i < num_devs && status < MAX_SCS_DEVICES; ++i)
	{
		libusb_device *dev = list[i];

		libusb_get_device_descriptor (dev, &desc);
		if ((0x0403 != desc.idVendor) || (0xD010 != (desc.idProduct & 0xFFF8)))
			continue;

		status++;

		uint8_t bnum = libusb_get_bus_number (dev);
		numports = libusb_get_port_numbers (dev, pnums, PNUM_MAX);

#ifdef DEBUG
		printf ("ID %04x:%04x - ", desc.idVendor, desc.idProduct);
#endif /* DEBUG */

		snprintf (path, sizeof(path), SYSFS_PATH, bnum);

		for (n = 0; n < numports - 1; n++)
		{
			snprintf (buf, MAX_BUF, "%u.", pnums[n]);
			strcat (path, buf);
		}
		snprintf (buf, MAX_BUF, "%u:1.0/", pnums[n]);
		strcat (path, buf);

#ifdef DEBUG
		printf ("%s -> ", path);
#endif /* DEBUG */

		n = scandir (path, &ent, srchtty, alphasort);
		if (1 != n)
		{
			fprintf (stderr, "USB search: tty search error (%d)", n);
		}

		// tty name is in ent[0]->d_name
#ifdef DEBUG
		printf ("/dev/%s\n", ent[0]->d_name);
#endif /* DEBUG */

		snprintf (devs[status - 1].tty, 20, "/dev/%s", ent[0]->d_name);
		devs[status - 1].type = desc.idProduct & 0x7;

		free_dirent (&ent, n);
	}

	libusb_free_device_list (list, 0);

error1:
	libusb_exit (ctx);

error:
	return status;
}


/********************************************************************
 * Signal handler
 ********************************************************************/
void sigHandler (int sig)
{
	if (sig == SIGINT || sig == SIGTERM || sig == SIGQUIT || sig == SIGKILL || sig == SIGHUP)
	{
		run = 0;
	}
}


/********************************************************************
 * Main function
 ********************************************************************/
int main (int argc, char *argv[])
{
	char serdev[256];
	int ser;
	fd_set readfds, testfds;
	ssize_t rd;
	char buf[256];
	struct termios options;
	struct serial_struct sstruct;
//	bool p4baud = false;
	int baudrate;
	int i, n;
	int num = 0;
	struct SCS_Devices devs[MAX_SCS_DEVICES];

	printf ("SCS Term\n"
		"Version " VERSION "\n"
		"Copyright (C) 2005-2021 SCS GmbH & Co. KG, Hanau, Germany\n"
		"press CTRL-C to end program\n\n");

	if (argc > 1)
	{
		if (!strncmp (argv[1], "/dev/tty", 8))
		{
			//printf ("Device: %s\n", argv[1]);
			strcpy (serdev, argv[1]);
			baudrate = B115200;
			goto no_auto;
		}
//		else if (!strncmp (argv[1], "4", 1))
//		{
//			p4baud = true;
//		}
	}

	n = find_devices (devs);	// find all SCS USB devices

#ifdef DEBUG
	printf ("Found %d SCS devices\n", n);

	for (i = 0; i < n; i++)
	{
		printf ("%s on %s\n", modems[devs[i].type].type, devs[i].tty);
	}
#endif /* DEBUG */

	if (n > 1)
	{
		printf ("More than one SCS modem found! Plaese choose:\n");
		for (i = 0; i < n; i++)
		{
			printf ("%d: %-16s %s\n", i + 1, devs[i].tty, modems[devs[i].type].type);
		}
		printf ("Enter a number: ");
		scanf ("%d", &num);

#ifdef DEBUG
		printf ("Num = %d\n", num);
#endif /* DEBUG */

		if (num < 1 || num > n)
		{
			fprintf (stderr, "Error: invalid input\n");
			goto ERR_EXIT;
		}
		num--;
	}

	printf ("Using %s on %s\n", modems[devs[num].type].type, devs[num].tty);

	strcpy (serdev, devs[num].tty);
	baudrate = modems[devs[num].type].baud;

no_auto:
	if (lock_device (serdev) < 0)
	{
		fprintf (stderr, "Error: could not lock %s\n", serdev);
		return EXIT_FAILURE;
	}

	if ((ser = open (serdev, O_RDWR | O_NOCTTY)) == -1)
	{
		fprintf (stderr, "Error: could not open %s\n", serdev);
		unlock_device (serdev);
		return EXIT_FAILURE;
	}

	tcgetattr (ser, &options);

	options.c_cc[VTIME] = 0;
	options.c_cc[VMIN] = 1;
	options.c_iflag = 0;
	options.c_iflag |= IGNBRK;
	options.c_oflag = 0;
	options.c_lflag = 0;
	options.c_cflag |= (CS8 | CRTSCTS | CREAD | CLOCAL);
	options.c_cflag &= ~(CSTOPB | PARENB | PARODD | HUPCL);

	cfsetispeed (&options, baudrate);
	cfsetospeed (&options, baudrate);

	tcsetattr (ser, TCSANOW, &options);

	if (devs[num].type == 2 || devs[num].type == 3)	// is it a P4dragon ?
	{
		// special P4dragon handling
		printf ("Set the P4dragon special baudrate!\n");

		// get serial_struct
		if (ioctl (ser, TIOCGSERIAL, &sstruct) < 0)
		{
			fprintf (stderr, "Error: could not get comm ioctl\n");
			close (ser);
			unlock_device (serdev);
			return EXIT_FAILURE;
		}

		if (!(sstruct.flags & ASYNC_SPD_CUST))	// test if the custom divisor is already selected
		{
			// set custom divisor to get 829440 baud
			sstruct.custom_divisor = 29;
			sstruct.flags |= ASYNC_SPD_CUST;

			// set serial_struct
			if (ioctl (ser, TIOCSSERIAL, &sstruct) < 0)
			{
				fprintf (stderr, "Error: could not set custom comm baud divisor\n");
				close (ser);
				unlock_device (serdev);
				return EXIT_FAILURE;
			}
		}
	}

	signal (SIGALRM, sigHandler);
	signal (SIGINT,  sigHandler);
	signal (SIGTERM, sigHandler);
	signal (SIGQUIT, sigHandler);
	signal (SIGKILL, sigHandler);
	signal (SIGHUP,  sigHandler);

	FD_ZERO (&readfds);
	FD_SET (ser, &readfds);
	FD_SET (0, &readfds);

	write (ser, "\r", 1);

	run = 1;
	while (run)
	{
		testfds = readfds;
		if (select (FD_SETSIZE, &testfds, NULL, NULL, NULL) < 0)
		{
#ifdef DEBUG
			fprintf (stderr, "\nSelect error!\n");
#endif /* DEBUG */
			goto EXIT;
		}

		if (FD_ISSET (ser, &testfds))
		{
			rd = read (ser, buf, 256);
			write (1, buf, rd);
		}

		if (FD_ISSET (0, &testfds))
		{
			rd = read (0, buf, 256);
			buf[rd - 1] = '\r';
			write (ser, buf, rd);
		}
	}

EXIT:
	close (ser);
	unlock_device (serdev);

ERR_EXIT:
	printf ("\n");

	return EXIT_SUCCESS;
}
