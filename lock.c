/********************************************************************
 *
 * lock.c -- Lock file handling routines (UUCP style)
 *
 * Copyright (C) 1998 - 2021 SCS GmbH & Co. KG, Hanau, Germany
 * written by Peter Mack (peter.mack@scs-ptc.com)
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


/********************************************************************
 * Include files
 ********************************************************************/
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "lock.h"


/********************************************************************
 * lock_device
 *
 * lock the given device
 *
 * return
 *  -1 on error
 *   0 on success
 ********************************************************************/
int lock_device (char *device)
{
	char lckf[128];
	int lfh;
	pid_t lckpid;
	char *devicename;
	char lckpidstr[20];
	int nb;
	struct stat buf;

	devicename = strrchr (device, '/');
	snprintf (lckf, 128, "%s/%s%s", LF_PATH, LF_PREFIX, (devicename ? (devicename + 1) : device));

	// Check if there's already a LCK..* file
	if (stat (lckf, &buf) == 0)
	{
		// we must now expend effort to learn if it's stale or not.
		if ((lfh = open (lckf, O_RDONLY)) != -1)
		{
			nb = read (lfh, &lckpidstr, min(20, buf.st_size));
			if (nb > 0)
			{
				lckpidstr[nb] = 0;
				sscanf (lckpidstr, "%d", &lckpid);
				if (kill (lckpid, 0) == 0)
				{
					fprintf (stderr, "Device %s is locked by process %d\n", device, lckpid);
					return -1;
				}

				// The lock file is stale. Remove it.
				if (unlink (lckf))
				{
					fprintf (stderr, "Unable to unlink stale lock file \"%s\"\n", lckf);
					return -1;
				}
			}
			else
			{
				fprintf (stderr, "Cannot read from lock file \"%s\"\n", lckf);
				return -1;
			}
		}
		else
		{
			fprintf (stderr, "Cannot open existing lock file\"%s\"\n", lckf);
			return -1;
		}
	}

	if ((lfh = open (lckf, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) < 0)
	{
		fprintf (stderr, "Cannot create lockfile.\n");
		return -1;
	}
	snprintf (lckpidstr, 20, "%10d\n", getpid ());
	write (lfh, lckpidstr, strlen (lckpidstr));
	close (lfh);

	return 0;
}


/********************************************************************
 * unlock_device
 *
 * unlock the given device
 *
 * return
 *  -1 on error
 *   0 on success
 ********************************************************************/
int unlock_device (char *device)
{
	char lckf[128];
	char *devicename;

	devicename = strrchr (device, '/');
	snprintf (lckf, 128, "%s/%s%s", LF_PATH, LF_PREFIX, (devicename ? (devicename + 1) : device));

	if (unlink (lckf))
	{
		fprintf (stderr, "Unable to unlink lock file \"%s\"\n", lckf);
		return -1;
	}

	return 0;
}
