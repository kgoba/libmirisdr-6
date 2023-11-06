/*
 * MiriSDR
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Dimitri Stolnikov <horiz0n@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include "getopt/getopt.h"
#endif

#include "mirisdr.h"
#include "convenience/convenience.h"

#define DEFAULT_SAMPLE_RATE       2048000
#define DEFAULT_BUF_LENGTH        (16 * 16384)
#define MINIMAL_BUF_LENGTH        512
#define MAXIMAL_BUF_LENGTH        (256 * 16384)

static int do_exit = 0;
static uint32_t bytes_to_read = 0;
static mirisdr_dev_t *dev = NULL;

void usage(void)
{
    fprintf(stderr,
        "Usage:\t -f frequency_to_tune_to [Hz]\n"
        "\t[-m sample format (default: auto]\n"
        "\t    504:    S8 (fastest)\n"
        "\t    384:    S10 +2bits \n"
        "\t    336:    S12\n"
        "\t    252:    S14\n"
#ifndef _WIN32
        "\t[-e USB transfer mode (default: 1)]\n"
#else
        "\t[-e USB transfer mode (default: 2)]\n"
#endif
        "\t    1:      Isochronous (maximum 196.608 Mbit/s) \n"
        "\t    2:      Bulk (maximum 333Mbit/s, depends on controller type)\n"
        "\t[-i IF mode (default: ZERO]\n"
        "\t    0:       ZERO\n"
        "\t    450000:  450 kHz\n"
        "\t    1620000: 1620kHz\n"
        "\t    2048000: 2048kHz\n"
        "\t[-w BW mode (default: 8MHz]\n"
        "\t    200000:  200kHz\n"
        "\t    300000:  300kHz\n"
        "\t    600000:  600kHz\n"
        "\t    1536000: 1536kHz\n"
        "\t    5000000: 5MHz\n"
        "\t    6000000: 6MHz\n"
        "\t    7000000: 7MHz\n"
        "\t    8000000: 8MHz\n"
        "\t[-s samplerate (default: 2048000 Hz)]\n"
        "\t[-d device_index (default: 0)]\n"
        "\t[-D device_type device variant (default: 0)]\n"
        "\t    0:       Default\n"
        "\t    1:       SDRPlay\n"
        "\t[-g gain (default: 0 for auto)]\n"
        "\t[-p ppm_error (default: 0)]\n"
        "\t[-b output_block_size (default: 16 * 16384)]\n"
        "\t[-S force sync output (default: async)]\n"
        "\tfilename (a '-' dumps samples to stdout)\n\n");
    exit(1);
}

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
    if (CTRL_C_EVENT == signum) {
        fprintf(stderr, "Signal caught, exiting!\n");
        do_exit = 1;
        mirisdr_cancel_async(dev);
        return TRUE;
    }
    return FALSE;
}
#else
static void sighandler(int signum)
{
    (void) signum;
    signal(SIGPIPE, SIG_IGN);
    fprintf(stderr, "Signal caught, exiting!\n");
    do_exit = 1;
    mirisdr_cancel_async(dev);
}
#endif

static void mirisdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
    if (ctx) {
        if (do_exit)
            return;

        if ((bytes_to_read > 0) && (bytes_to_read < len)) {
            len = bytes_to_read;
            do_exit = 1;
            mirisdr_cancel_async(dev);
        }

        if (fwrite(buf, 1, len, (FILE*)ctx) != len) {
            fprintf(stderr, "Short write, samples lost, exiting!\n");
            mirisdr_cancel_async(dev);
        }

        if (bytes_to_read > 0)
            bytes_to_read -= len;
    }
}

int main(int argc, char **argv)
{
#ifndef _WIN32
    struct sigaction sigact;
#endif
    char *filename = NULL;
    int n_read;
    int r, opt;
    int i;
    int gain = 0;
    int custom_ppm = 0;
    int ppm_error = 0;
    int sync_mode = 0;
    FILE *file;
    uint8_t *buffer;
    uint32_t format = 0;
#if !defined (_WIN32) || defined(__MINGW32__)
    uint32_t transfer = 1;
#else
    uint32_t transfer = 2;
#endif
    uint32_t if_mode = 0;
    uint32_t bw = 8000000;
    int dev_index = 0;
    int dev_given = 0;
    uint32_t frequency = 100000000;
    uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
    uint32_t out_block_size = DEFAULT_BUF_LENGTH;
    int count;
    int gains[100];
    uint32_t rates[100];
    mirisdr_hw_flavour_t hw_flavour = MIRISDR_HW_DEFAULT;
    int intval;

    while ((opt = getopt(argc, argv, "b:d:D:e:f:g:p:i:m:s:w:S::")) != -1) {
        switch (opt) {
        case 'b':
            out_block_size = (uint32_t)atof(optarg);
            break;
        case 'd':
            dev_index = verbose_device_search(optarg);
            dev_given = 1;
            break;
        case 'D':
            intval = atoi(optarg);
            if ((intval >=0) && (intval <= 1))
            {
                hw_flavour = (mirisdr_hw_flavour_t) intval;
            }
            break;
        case 'e':
            if ((strcmp("ISOC", optarg) == 0) ||
                (strcmp("1", optarg) == 0)) {
                transfer = 1;}
            if ((strcmp("BULK", optarg) == 0) ||
                (strcmp("2", optarg) == 0)) {
                transfer = 2;}
            break;
        case 'f':
            frequency = (uint32_t)atofs(optarg);
            break;
        case 'g':
            gain = (int)(atof(optarg) * 10); /* tenths of a dB */
            break;
        case 's':
            samp_rate = (uint32_t)atofs(optarg);
            break;
		case 'p':
			ppm_error = atoi(optarg);
            custom_ppm = 1;
			break;
        case 'i':
            if_mode = atoi(optarg);
            break;
        case 'm':
            if (strcmp("504", optarg) == 0) {
                format = 1;}
            if (strcmp("384", optarg) == 0) {
                format = 2;}
            if (strcmp("336", optarg) == 0) {
                format = 3;}
            if (strcmp("252", optarg) == 0) {
                format = 4;}
            break;
        case 'w':
            bw = atoi(optarg);
            break;
        case 'S':
            sync_mode = 1;
            break;
        default:
            usage();
            break;
        }
    }

    if (argc <= optind) {
        usage();
    } else {
        filename = argv[optind];
    }

    if(out_block_size < MINIMAL_BUF_LENGTH ||
       out_block_size > MAXIMAL_BUF_LENGTH ){
        fprintf(stderr,
            "Output block size wrong value, falling back to default\n");
        fprintf(stderr,
            "Minimal length: %u\n", MINIMAL_BUF_LENGTH);
        fprintf(stderr,
            "Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
        out_block_size = DEFAULT_BUF_LENGTH;
    }

    buffer = malloc(out_block_size * sizeof(uint8_t));

	if (!dev_given) {
		dev_index = verbose_device_search("0");
	}

	if (dev_index < 0) {
		exit(1);
	}

    r = mirisdr_open(&dev, (uint32_t)dev_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open mirisdr device #%d.\n", dev_index);
        exit(1);
    }

    mirisdr_set_hw_flavour(dev, hw_flavour);

#ifndef _WIN32
    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
#else
    SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, TRUE );
#endif
    /* Set the sample rate */
	verbose_set_sample_rate(dev, samp_rate);
    samp_rate = mirisdr_get_sample_rate(dev);

    /* Set the frequency */
	verbose_set_frequency(dev, frequency);

    count = mirisdr_get_tuner_gains(dev, NULL);
    fprintf(stderr, "Supported gain values (%d): ", count);

    count = mirisdr_get_tuner_gains(dev, gains);
    for (i = 0; i < count; i++)
        fprintf(stderr, "%.1f ", gains[i] / 10.0);
    fprintf(stderr, "\n");

    if (0 == gain) {
        /* Enable automatic gain */
        verbose_auto_gain(dev);
    } else {
        /* Enable manual gain */
        gain = nearest_gain(dev, gain);
        verbose_gain_set(dev, gain);
    }

    /* Set sample format */
    switch (format) {
    case 1:
        mirisdr_set_sample_format(dev, "504_S8");
        break;
    case 2:
        mirisdr_set_sample_format(dev, "384_S16");
        break;
    case 3:
        mirisdr_set_sample_format(dev, "336_S16");
        break;
    case 4:
        mirisdr_set_sample_format(dev, "252_S16");
        break;
    default:
        mirisdr_set_sample_format(dev, "AUTO");
        break;
    }

    /* Set USB transfer type */
    switch (transfer) {
    case 1:
        mirisdr_set_transfer(dev, "ISOC");
        break;
    case 2:
        mirisdr_set_transfer(dev, "BULK");
        break;
    }

    /* Set IF mode */
    mirisdr_set_if_freq(dev, if_mode);

    /* Set bandwidth */
    mirisdr_set_bandwidth(dev, bw);

    if (!custom_ppm) {
		verbose_ppm_eeprom(dev, &(ppm_error));
	}
    verbose_ppm_set(dev, ppm_error);

    if(strcmp(filename, "-") == 0) { /* Write samples to stdout */
        file = stdout;
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    } else {
        file = fopen(filename, "wb");
        if (!file) {
            fprintf(stderr, "Failed to open %s\n", filename);
            goto out;
        }
    }

    /* Reset endpoint before we start reading from it (mandatory) */
    verbose_reset_buffer(dev);

    if (sync_mode) {
        fprintf(stderr, "Reading samples in sync mode...\n");
        while (!do_exit) {
            r = mirisdr_read_sync(dev, buffer, out_block_size, &n_read);
            if (r < 0) {
                fprintf(stderr, "WARNING: sync read failed.\n");
                break;
            }

            if ((bytes_to_read > 0) && (bytes_to_read < (uint32_t)n_read)) {
                n_read = bytes_to_read;
                do_exit = 1;
            }

            if (fwrite(buffer, 1, n_read, file) != (size_t)n_read) {
                fprintf(stderr, "Short write, samples lost, exiting!\n");
                break;
            }

            if ((uint32_t)n_read < out_block_size) {
                fprintf(stderr, "Short read, samples lost, exiting!\n");
                break;
            }

            if (bytes_to_read > 0)
                bytes_to_read -= n_read;
        }
    } else {
        fprintf(stderr, "Reading samples in async mode...\n");
        r = mirisdr_read_async(dev, mirisdr_callback, (void *)file,
                      0, out_block_size);
    }

    if (do_exit)
        fprintf(stderr, "\nUser cancel, exiting...\n");
    else
        fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

    if (file != stdout)
        fclose(file);

    mirisdr_close(dev);
    free (buffer);
out:
    return r >= 0 ? r : -r;
}
