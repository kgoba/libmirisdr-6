/*
 * Copyright (C) 2013 by Miroslav Slugen <thunder.m@email.cz
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

#include "mirisdr_private.h"
#include <stdio.h>

/* uložení dat */
static int mirisdr_feed_async (mirisdr_dev_t *p, unsigned char *samples, uint32_t bytes) {
    uint32_t i;

    if (!p) goto failed;
    if (!p->cb) goto failed;

#if MIRISDR_DEBUG >= 2
    fprintf( stderr, "%lu %lu %u\n", p->xfer_out_len, p->xfer_out_pos, bytes);
#endif

    /* auto size */
    if (!p->xfer_out_len) {
        /* direct call */
        p->cb(samples, bytes, p->cb_ctx);
    /* fixed buffer size without previous data */
    } else {
        while (p->xfer_out_pos + bytes >= p->xfer_out_len) {
            i = p->xfer_out_len - p->xfer_out_pos;

            if (p->xfer_out_pos > 0) {
                memcpy(p->xfer_out + p->xfer_out_pos, samples, (size_t)i);
                p->cb(p->xfer_out, p->xfer_out_len, p->cb_ctx);
            }
            else {
                p->cb(samples, i, p->cb_ctx);
            }

            bytes -= i;
            samples += i;
            p->xfer_out_pos = 0;
        }
        if (bytes > 0) {
            memcpy(p->xfer_out + p->xfer_out_pos, samples, (size_t)bytes);
            p->xfer_out_pos += bytes;
        }
    }
    return 0;

failed:
    return -1;
}

static uint8_t *samples_realloc(mirisdr_dev_t *p, int size)
{
    if(p->samples_size < size)
    {
        if(p->samples)
            free(p->samples);
        p->samples=malloc(size);
        p->samples_size=size;
    }
    return p->samples;
}

static int _process_isochronous_transfer(mirisdr_dev_t *p, struct libusb_transfer *xfer) {
    size_t i;
    int len, bytes = 0;
    static unsigned char *iso_packet_buf;
    uint8_t *samples = p->samples;

    switch (p->format) {
    case MIRISDR_FORMAT_252_S16:
        samples = samples_realloc(p, 504 * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS * 2);
        for (i = 0; i < DEFAULT_ISO_PACKETS; i++) {
            struct libusb_iso_packet_descriptor *packet = &xfer->iso_packet_desc[i];

            /* buffer_simple je pouze pro stejně velké pakety */
            if ((packet->actual_length > 0) &&
                (iso_packet_buf = libusb_get_iso_packet_buffer_simple(xfer, i))) {
                /* size smaller than 3072 is fine, it's a multiple of 1024, anything else is an error */
                len = mirisdr_samples_convert_252_s16(p, iso_packet_buf, samples + bytes, packet->actual_length);
                bytes+= len;
            }
        }
        break;
    case MIRISDR_FORMAT_336_S16:
        samples = samples_realloc(p, 672 * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS * 2);
        for (i = 0; i < DEFAULT_ISO_PACKETS; i++) {
            struct libusb_iso_packet_descriptor *packet = &xfer->iso_packet_desc[i];
            if ((packet->actual_length > 0) &&
                (iso_packet_buf = libusb_get_iso_packet_buffer_simple(xfer, i))) {
                len = mirisdr_samples_convert_336_s16(p, iso_packet_buf, samples + bytes, packet->actual_length);
                bytes+= len;
            }
        }
        break;
    case MIRISDR_FORMAT_384_S16:
        samples = samples_realloc(p, 768 * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS * 2);
        for (i = 0; i < DEFAULT_ISO_PACKETS; i++) {
            struct libusb_iso_packet_descriptor *packet = &xfer->iso_packet_desc[i];
            if ((packet->actual_length > 0) &&
                (iso_packet_buf = libusb_get_iso_packet_buffer_simple(xfer, i))) {
                len = mirisdr_samples_convert_384_s16(p, iso_packet_buf, samples + bytes, packet->actual_length);
                bytes+= len;
            }
        }
        break;
    case MIRISDR_FORMAT_504_S16:
        samples = samples_realloc(p, 1008 * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS * 2);
        for (i = 0; i < DEFAULT_ISO_PACKETS; i++) {
            struct libusb_iso_packet_descriptor *packet = &xfer->iso_packet_desc[i];
            if ((packet->actual_length > 0) &&
                (iso_packet_buf = libusb_get_iso_packet_buffer_simple(xfer, i))) {
                len = mirisdr_samples_convert_504_s16(p, iso_packet_buf, samples + bytes, packet->actual_length);
                bytes+= len;
            }
        }
        break;
    case MIRISDR_FORMAT_504_S8:
        samples = samples_realloc(p, 1008 * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS);
        for (i = 0; i < DEFAULT_ISO_PACKETS; i++) {
            struct libusb_iso_packet_descriptor *packet = &xfer->iso_packet_desc[i];
            if ((packet->actual_length > 0) &&
                (iso_packet_buf = libusb_get_iso_packet_buffer_simple(xfer, i))) {
                len = mirisdr_samples_convert_504_s8(p, iso_packet_buf, samples + bytes, packet->actual_length);
                bytes+= len;
            }
        }
        break;
    }

    return bytes;
}

static int _process_bulk_transfer(mirisdr_dev_t *p, struct libusb_transfer *xfer) {
    int bytes = 0;
    uint8_t *samples = p->samples;

    switch (p->format) {
    case MIRISDR_FORMAT_252_S16:
        samples = samples_realloc(p, (DEFAULT_BULK_BUFFER / 1024) * 1008);
        bytes = mirisdr_samples_convert_252_s16(p, xfer->buffer, samples, xfer->actual_length);
        break;
    case MIRISDR_FORMAT_336_S16:
        samples = samples_realloc(p, (DEFAULT_BULK_BUFFER / 1024) * 1344);
        bytes = mirisdr_samples_convert_336_s16(p, xfer->buffer, samples, xfer->actual_length);
        break;
    case MIRISDR_FORMAT_384_S16:
        samples = samples_realloc(p, (DEFAULT_BULK_BUFFER / 1024) * 1536);
        bytes = mirisdr_samples_convert_384_s16(p, xfer->buffer, samples, xfer->actual_length);
        break;
    case MIRISDR_FORMAT_504_S16:
        samples = samples_realloc(p, (DEFAULT_BULK_BUFFER / 1024) * 2016);
        bytes = mirisdr_samples_convert_504_s16(p, xfer->buffer, samples, xfer->actual_length);
        break;
    case MIRISDR_FORMAT_504_S8:
        samples = samples_realloc(p, (DEFAULT_BULK_BUFFER / 1024) * 1008);
        bytes = mirisdr_samples_convert_504_s8(p, xfer->buffer, samples, xfer->actual_length);
        break;
    }

    return bytes;
}

/* called when data is received */
static void LIBUSB_CALL _libusb_callback (struct libusb_transfer *xfer) {
    int bytes = 0;
    mirisdr_dev_t *p = (mirisdr_dev_t*) xfer->user_data;

    if (!p) goto failed;

    /* we will only process a completed transfer */
    if (xfer->status == LIBUSB_TRANSFER_COMPLETED) {
        /*
          * To determine the correct buffer size, this part must be done
          * in one step, otherwise the format may change in the middle of the process,
          * the second option is to use lock.
         */
        switch (xfer->type) {
        case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
            bytes = _process_isochronous_transfer(p, xfer);
            break;
        case LIBUSB_TRANSFER_TYPE_BULK:
            bytes = _process_bulk_transfer(p, xfer);
            break;
        default:
            fprintf( stderr, "not isoc or bulk transfer type on usb device: %u\n", p->index);
            goto failed;
        }

        if (bytes > 0) mirisdr_feed_async(p, p->samples, bytes);

        if (xfer->type == LIBUSB_TRANSFER_TYPE_BULK)
        {
            if(p->sync_loss_cnt > (int)p->xfer_buf_num)
            {
                p->sync_loss_cnt = -p->xfer_buf_num +1;
                xfer->length = DEFAULT_BULK_BUFFER - 512;
                fprintf(stderr,"libmirisdr: Sync lost. Trying to synchronize.\n");
            }else
                xfer->length = DEFAULT_BULK_BUFFER;
        }
        /* resubmit the transfer */
        if (libusb_submit_transfer(xfer) < 0) {
            fprintf( stderr, "error re-submitting URB on device %u\n", p->index);
            goto failed;
        }
    } else if (xfer->status != LIBUSB_TRANSFER_CANCELLED) {
        fprintf( stderr, "error async transfer status %d on device %u\n", xfer->status, p->index);
        goto failed;
    }

    return;

failed:
    mirisdr_cancel_async(p);
    /* the failed status has absolute priority */
    p->async_status = MIRISDR_ASYNC_FAILED;
}

/* ukončení async části */
int mirisdr_cancel_async (mirisdr_dev_t *p) {
    if (!p) goto failed;

    switch (p->async_status) {
    case MIRISDR_ASYNC_INACTIVE:
    case MIRISDR_ASYNC_CANCELING:
        goto canceled;
    case MIRISDR_ASYNC_RUNNING:
    case MIRISDR_ASYNC_PAUSED:
        p->async_status = MIRISDR_ASYNC_CANCELING;
        break;
    case MIRISDR_ASYNC_FAILED:
        goto failed;
    }

    return 0;

failed:
    return -1;

canceled:
    return -2;
}

/* ukončení async části včetně čekání */
int mirisdr_cancel_async_now (mirisdr_dev_t *p) {
    if (!p) goto failed;

    switch (p->async_status) {
    case MIRISDR_ASYNC_INACTIVE:
        goto done;
    case MIRISDR_ASYNC_CANCELING:
        break;
    case MIRISDR_ASYNC_RUNNING:
    case MIRISDR_ASYNC_PAUSED:
        p->async_status = MIRISDR_ASYNC_CANCELING;
        break;
    case MIRISDR_ASYNC_FAILED:
        goto failed;
    }

    /* cyklujeme dokud není vše ukončeno */
    while ((p->async_status != MIRISDR_ASYNC_INACTIVE) &&
           (p->async_status != MIRISDR_ASYNC_FAILED))
#if defined (_WIN32) && !defined(__MINGW32__)
    Sleep(20);
#else
    usleep(20000);
#endif

done:
    return 0;

failed:
    return -1;
}

/* allocation of asynchronous buffers */
static int mirisdr_async_alloc (mirisdr_dev_t *p) {
    size_t i;

    if (!p->xfer) {
        p->xfer = malloc(p->xfer_buf_num * sizeof(*p->xfer));

        for (i = 0; i < p->xfer_buf_num; i++) {
            switch (p->transfer) {
            case MIRISDR_TRANSFER_BULK:
                p->xfer[i] = libusb_alloc_transfer(0);
                p->xfer[i]->type = LIBUSB_TRANSFER_TYPE_BULK;
                break;
            case MIRISDR_TRANSFER_ISOC:
                p->xfer[i] = libusb_alloc_transfer(DEFAULT_ISO_PACKETS);
                p->xfer[i]->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
                break;
            }
        }
    }

    if (!p->xfer_buf) {
        p->xfer_buf = malloc(p->xfer_buf_num * sizeof(*p->xfer_buf));

        for (i = 0; i < p->xfer_buf_num; i++) {
            switch (p->transfer) {
            case MIRISDR_TRANSFER_BULK:
                p->xfer_buf[i] = malloc(DEFAULT_BULK_BUFFER);
                break;
            case MIRISDR_TRANSFER_ISOC:
                p->xfer_buf[i] = malloc(DEFAULT_ISO_BUFFER * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS);
                break;
            }
        }
    }

    if ((!p->xfer_out) &&
        (p->xfer_out_len)) {
        p->xfer_out = malloc(p->xfer_out_len * sizeof(*p->xfer_out));
    }

    return 0;
}

/* releasing asynchronous buffers */
static int mirisdr_async_free (mirisdr_dev_t *p) {
    size_t i;

    if (p->xfer) {
        for (i = 0; i < p->xfer_buf_num; i++) {
            if (p->xfer[i]) libusb_free_transfer(p->xfer[i]);
        }

        free(p->xfer);
        p->xfer = NULL;
    }

    if (p->xfer_buf) {
        for (i = 0; i < p->xfer_buf_num; i++) {
            if (p->xfer_buf[i]) free(p->xfer_buf[i]);
        }

        free(p->xfer_buf);
        p->xfer_buf = NULL;
    }

    if (p->xfer_out) {
        free(p->xfer_out);
        p->xfer_out = NULL;
    }

    return 0;
}

/* spuštění async části */
int mirisdr_read_async (mirisdr_dev_t *p, mirisdr_read_async_cb_t cb, void *ctx, uint32_t num, uint32_t len) {
    size_t i;
    int r, semafor;
    struct timeval tv = {1, 0};

    if (!p) goto failed;
    if (!p->dh) goto failed;

    /* nedovolíme spustit jiný stav než neaktivní */
    if (p->async_status != MIRISDR_ASYNC_INACTIVE) goto failed;

    p->cb = cb;
    p->cb_ctx = ctx;

    p->xfer_buf_num = (num == 0) ? DEFAULT_BUF_NUMBER : num;
    /* jde o fixní velikost výstupního bufferu */
    p->xfer_out_len = (len == 0) ? 0 : len;
    p->xfer_out_pos = 0;
#if MIRISDR_DEBUG >= 1
    fprintf( stderr, "async read on device %u, buffers: %lu, output size: ",
                                p->index, (long)p->xfer_buf_num);
    if (p->xfer_out_len) {
        fprintf( stderr, "%lu", (long)p->xfer_out_len);
    } else {
        fprintf( stderr, "auto");
    }
#endif
    p->sync_loss_cnt = 0;
    /* použití správného rozhraní které zasílá data - není kritické */
    switch (p->transfer) {
    case MIRISDR_TRANSFER_BULK:
#if MIRISDR_DEBUG >= 1
        fprintf( stderr, ", transfer: bulk\n");
#endif
        if ((r = libusb_set_interface_alt_setting(p->dh, 0, 3)) < 0) {
            fprintf( stderr, "failed to use alternate setting for Bulk mode on miri usb device %u with code %d\n", p->index, r);
        }
        break;
    case MIRISDR_TRANSFER_ISOC:
#if MIRISDR_DEBUG >= 1
        fprintf( stderr, ", transfer: isochronous\n");
#endif
        if ((r = libusb_set_interface_alt_setting(p->dh, 0, 1)) < 0) {
            fprintf( stderr, "failed to use alternate setting for Isochronous mode on miri usb device %u with code %d\n", p->index, r);
        }
        break;
    default:
        fprintf( stderr, "\nunsupported transfer type on miri usb device %u\n", p->index);
        goto failed;
    }

    mirisdr_async_alloc(p);

    /* submit the transfers */
    for (i = 0; i < p->xfer_buf_num; i++) {
        switch (p->transfer) {
        case MIRISDR_TRANSFER_BULK:
            libusb_fill_bulk_transfer(p->xfer[i],
                                      p->dh,
                                      0x81,
                                      p->xfer_buf[i],
                                      DEFAULT_BULK_BUFFER,
                                      _libusb_callback,
                                      (void*) p,
                                      DEFAULT_BULK_TIMEOUT);
            break;
        case MIRISDR_TRANSFER_ISOC:
            libusb_fill_iso_transfer(p->xfer[i],
                                     p->dh,
                                     0x81,
                                     p->xfer_buf[i],
                                     DEFAULT_ISO_BUFFER * DEFAULT_ISO_BUFFERS * DEFAULT_ISO_PACKETS,
                                     DEFAULT_ISO_PACKETS,
                                     _libusb_callback,
                                     (void*) p,
                                     DEFAULT_ISO_TIMEOUT);
            libusb_set_iso_packet_lengths(p->xfer[i], DEFAULT_ISO_BUFFER * DEFAULT_ISO_BUFFERS);
            break;
        default:
            fprintf( stderr, "unsupported transfer type\n");
            goto failed_free;
        }

        r = libusb_submit_transfer(p->xfer[i]);

		if (r < 0) {
			fprintf(stderr, "Failed to submit transfer %lu reason: %d\n", i, r);
			goto failed_free;
		}
    }

    /* spustíme streamování dat */
    mirisdr_streaming_start(p);

    p->async_status = MIRISDR_ASYNC_RUNNING;

    while (p->async_status != MIRISDR_ASYNC_INACTIVE) {
        /* počkáme na další událost */
        if ((r = libusb_handle_events_timeout(p->ctx, &tv)) < 0) {
            fprintf( stderr, "libusb_handle_events returned: %d\n", r);
            if (r == LIBUSB_ERROR_INTERRUPTED) continue; /* stray */
            goto failed_free;
        }

        /* dochází k ukončení */
        if (p->async_status == MIRISDR_ASYNC_CANCELING) {
            if (!p->xfer) {
                p->async_status = MIRISDR_ASYNC_INACTIVE;
                break;
            }

            /* ukončíme všechny přenosy */
            semafor = 1;
            for (i = 0; i < p->xfer_buf_num; i++) {
                if (!p->xfer[i]) continue;

                /* pro isoc režim je completed i v případě chyb */
                if (p->xfer[i]->status != LIBUSB_TRANSFER_CANCELLED) {
                    libusb_cancel_transfer(p->xfer[i]);
                    semafor = 0;
                }
            }

            /* nedošlo k žádnému vynuceném ukončení přenosu, skončíme */
            if (semafor) {
                p->async_status = MIRISDR_ASYNC_INACTIVE;
                /* počkáme na dokončení všech procesů */
                libusb_handle_events_timeout(p->ctx, &tv);
                break;
            }
        } else if (p->async_status == MIRISDR_ASYNC_FAILED) {
            goto failed_free;
        }
    }

    /* dealokujeme buffer */
    mirisdr_async_free(p);

    /* ukončíme streamování dat */
#if defined (_WIN32) && !defined(__MINGW32__)
    Sleep(20);
#else
    usleep(20000);
#endif
    mirisdr_streaming_stop(p);
    /* je vhodné ukončit i adc, jenže pak by při dalším otevření bylo nutné provést inicializaci */

    return 0;

failed_free:
    mirisdr_async_free(p);

failed:
    return -1;
}

/* spuštění streamování */
int mirisdr_start_async (mirisdr_dev_t *p) {
    size_t i;

    /* nedovolíme jiný stav než pozastavený */
    if (p->async_status != MIRISDR_ASYNC_PAUSED) goto failed;

    /* reset interního bufferu */
    p->xfer_out_pos = 0;

    for (i = 0; i < p->xfer_buf_num; i++) {
        if (!p->xfer[i]) continue;

        if (libusb_submit_transfer(p->xfer[i])< 0) {
            goto failed;
        }
    }

    if (p->async_status != MIRISDR_ASYNC_PAUSED) goto failed;

    mirisdr_streaming_start(p);

    p->async_status = MIRISDR_ASYNC_RUNNING;

    return 0;

failed:
    return -1;
}

/* zastavení streamování */
int mirisdr_stop_async (mirisdr_dev_t *p) {
    size_t i;
    int r, semafor;
    struct timeval tv = {1, 0};

    /* nedovolíme jiný stav než spuštěný */
    if (p->async_status != MIRISDR_ASYNC_RUNNING) goto failed;

    while (p->async_status == MIRISDR_ASYNC_RUNNING) {
        semafor = 1;
        for (i = 0; i < p->xfer_buf_num; i++) {
            if (!p->xfer[i]) continue;

            /* pro isoc režim je completed i v případě chyb */
            if (p->xfer[i]->status != LIBUSB_TRANSFER_CANCELLED) {
                libusb_cancel_transfer(p->xfer[i]);
                semafor = 0;
            }
        }

        if (semafor) break;

        /* počkáme na další událost */
        if ((r = libusb_handle_events_timeout(p->ctx, &tv)) < 0) {
            fprintf( stderr, "libusb_handle_events returned: %d\n", r);
            if (r == LIBUSB_ERROR_INTERRUPTED) continue; /* stray */
            goto failed;
        }
    }

    if (p->async_status != MIRISDR_ASYNC_RUNNING) goto failed;

#if defined (_WIN32) && !defined(__MINGW32__)
    Sleep(20);
#else
    usleep(20000);
#endif
    mirisdr_streaming_stop(p);

    p->async_status = MIRISDR_ASYNC_PAUSED;

    return 0;

failed:
    return -1;
}
