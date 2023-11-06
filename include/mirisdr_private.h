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

#ifndef __MIRISDR_PRIVATE_H__
#define __MIRISDR_PRIVATE_H__

/* potřebné funkce */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef _WIN32
#include <unistd.h>
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#include <libusb.h>

#ifndef LIBUSB_CALL
#define LIBUSB_CALL
#endif

#include "mirisdr.h"

/******************************* constants.h ********************************/

#define CTRL_TIMEOUT            2000

#define DEFAULT_RATE            2000000
#define DEFAULT_FREQ            90000000
#define DEFAULT_GAIN            43

#define DEFAULT_ISO_BUFFER      1024
#define DEFAULT_ISO_BUFFERS     3
#define DEFAULT_ISO_PACKETS     8
#define DEFAULT_ISO_TIMEOUT     1000

/********************************* async.h **********************************/

#define DEFAULT_BULK_BUFFER     16384
#define DEFAULT_BULK_TIMEOUT    1000

#define DEFAULT_BUF_NUMBER      32

/******************************** structs.h *********************************/

typedef struct mirisdr_device {
    uint16_t            vid;
    uint16_t            pid;
    const char          *name;
    const char          *manufacturer;
    const char          *product;
} mirisdr_device_t;

struct mirisdr_dev {
    libusb_context      *ctx;
    struct libusb_device_handle *dh;

    /* parameters */
    uint32_t            index;
    uint32_t            freq;
    uint32_t            rate;
    int                 gain;
    int                 gain_reduction_lna;
    int                 gain_reduction_mixbuffer;
    int                 gain_reduction_mixer;
    int                 gain_reduction_baseband;
    mirisdr_hw_flavour_t hw_flavour;
    mirisdr_band_t      band;
    enum {
        MIRISDR_FORMAT_AUTO_ON = 0,
        MIRISDR_FORMAT_AUTO_OFF
    } format_auto;
    enum {
        MIRISDR_FORMAT_252_S16 = 0,
        MIRISDR_FORMAT_336_S16,
        MIRISDR_FORMAT_384_S16,
        MIRISDR_FORMAT_504_S16,
        MIRISDR_FORMAT_504_S8
    } format;
    enum {
        MIRISDR_BW_200KHZ = 0,
        MIRISDR_BW_300KHZ,
        MIRISDR_BW_600KHZ,
        MIRISDR_BW_1536KHZ,
        MIRISDR_BW_5MHZ,
        MIRISDR_BW_6MHZ,
        MIRISDR_BW_7MHZ,
        MIRISDR_BW_8MHZ,
        MIRISDR_BW_MAX
    } bandwidth;
    enum {
        MIRISDR_IF_ZERO = 0,
        MIRISDR_IF_450KHZ,
        MIRISDR_IF_1620KHZ,
        MIRISDR_IF_2048KHZ
    } if_freq;
    enum {
        MIRISDR_XTAL_19_2M = 0,
        MIRISDR_XTAL_22M,
        MIRISDR_XTAL_24M,
        MIRISDR_XTAL_24_576M,
        MIRISDR_XTAL_26M,
        MIRISDR_XTAL_38_4M
    } xtal;
    enum {
        MIRISDR_TRANSFER_BULK = 0,
        MIRISDR_TRANSFER_ISOC
    } transfer;

    /* async */
    enum {
        MIRISDR_ASYNC_INACTIVE = 0,
        MIRISDR_ASYNC_CANCELING,
        MIRISDR_ASYNC_RUNNING,
        MIRISDR_ASYNC_PAUSED,
        MIRISDR_ASYNC_FAILED
    } async_status;
    mirisdr_read_async_cb_t cb;
    void                *cb_ctx;
    size_t              xfer_buf_num;
    struct libusb_transfer **xfer;
    unsigned char       **xfer_buf;
    size_t              xfer_out_len;
    size_t              xfer_out_pos;
    unsigned char       *xfer_out;
    uint32_t            addr;
    int                 driver_active;
    int                 bias;
    int                 reg8;
    uint8_t             *samples;
    int                 samples_size;
    int                 sync_loss_cnt;
};

/********************************** soft.h ***********************************/

/*** Register 0: IC Mode / Power Control ***/

/* reg0: 4:8 (AM_MODE, VHF_MODE, B3_MODE, B45_MODE, BL_MODE) */
#define MIRISDR_MODE_AM                                 0x01
#define MIRISDR_MODE_VHF                                0x02
#define MIRISDR_MODE_B3                                 0x04
#define MIRISDR_MODE_B45                                0x08
#define MIRISDR_MODE_BL                                 0x10

/* reg0: 9 (AM_MODE2) */
#define MIRISDR_UPCONVERT_MIXER_OFF                     0
#define MIRISDR_UPCONVERT_MIXER_ON                      1

/* reg0: 10 (RF_SYNTH) */
#define MIRISDR_RF_SYNTHESIZER_OFF                      0
#define MIRISDR_RF_SYNTHESIZER_ON                       1

/* reg0: 11 (AM_PORT_SEL) */
#define MIRISDR_AM_PORT1                                0
#define MIRISDR_AM_PORT2                                1

/* reg0: 12:13 (FIL_MODE_SEL0, FIL_MODE_SEL1) */
#define MIRISDR_IF_MODE_2048KHZ                         0
#define MIRISDR_IF_MODE_1620KHZ                         1
#define MIRISDR_IF_MODE_450KHZ                          2
#define MIRISDR_IF_MODE_ZERO                            3

/* reg0: 14:16 (FIL_BW_SEL0 - FIL_BW_SEL2) */

/* reg0: 17:19 (XTAL_SEL0 - XTAL_SEL2) */

/* reg0: 20:22 (IF_LPMODE0 - IF_LPMODE2) */
#define MIRISDR_IF_LPMODE_NORMAL                        0
#define MIRISDR_IF_LPMODE_ONLY_Q                        1
#define MIRISDR_IF_LPMODE_ONLY_I                        2
#define MIRISDR_IF_LPMODE_LOW_POWER                     4

/* reg0: 23 (VCO_LPMODE) */
#define MIRISDR_VCO_LPMODE_NORMAL                       0
#define MIRISDR_VCO_LPMODE_LOW_POWER                    1

/*** Register 2: Synthesizer Programming ***/

/* reg2: 4:15 (FRAC0 - FRAC11) */

/* reg2: 16:21 (INT0 - INT5) */

/* reg2: 22 (LNACAL_EN) */
#define MIRISDR_LBAND_LNA_CALIBRATION_OFF               0
#define MIRISDR_LBAND_LNA_CALIBRATION_ON                1

/*** Register 6: RF Synthesizer Configuration ***/

/* reg5: 4:15 (THRESH0 - THRESH11) */

/* reg5: 16:21 (reserved) */
#define MIRISDR_RF_SYNTHESIZER_RESERVED_PROGRAMMING     0x28

typedef struct
{
    uint32_t low_cut;
    int mode;
    int upconvert_mixer_on;
    int am_port;
    int lo_div;
    uint32_t band_select_word;
} hw_switch_freq_plan_t;

/********************************** hard.h ***********************************/

#define MIRISDR_SAMPLE_RATE_MIN         1300000
#define MIRISDR_SAMPLE_RATE_MAX         15000000

/********************************** gain.h ***********************************/

/*** Register 1: Receiver Gain Control ***/

/* reg1: 4:9 (BBGAIN0 - BBGAIN5) */
#define MIRISDR_BASEBAND_GAIN_REDUCTION_MIN             0
#define MIRISDR_BASEBAND_GAIN_REDUCTION_MAX             0x3B

/* reg1: 10:11 (MIXBU0, MIXBU1) - AM port 1 */
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_0DB  0
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_6DB  1
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_12DB 2
#define MIRISDR_AM_PORT1_BLOCKUP_CONVERT_GAIN_REDUCTION_18DB 3

/* reg1: 10:11 (MIXBU0, MIXBU1) - AM port 2 */
#define MIRISDR_AM_PORT2_BLOCKUP_CONVERT_GAIN_REDUCTION_0DB  0
#define MIRISDR_AM_PORT2_BLOCKUP_CONVERT_GAIN_REDUCTION_24DB 3

/* reg1: 12 (MIXL) */
#define MIRISDR_LNA_GAIN_REDUCTION_OFF                  0
#define MIRISDR_LNA_GAIN_REDUCTION_ON                   1

/* reg1: 13 (LNAGR) */
#define MIRISDR_MIXER_GAIN_REDUCTION_OFF                0
#define MIRISDR_MIXER_GAIN_REDUCTION_ON                 1

/* reg1: 14:16 (DCCAL0 - DCCAL2) */
#define MIRISDR_DC_OFFSET_CALIBRATION_STATIC            0
#define MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC1         1
#define MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC2         2
#define MIRISDR_DC_OFFSET_CALIBRATION_PERIODIC3         3
#define MIRISDR_DC_OFFSET_CALIBRATION_ONE_SHOT          4
#define MIRISDR_DC_OFFSET_CALIBRATION_CONTINUOUS        5

/* reg1: 17 (DCCAL_SPEEDUP) */
#define MIRISDR_DC_OFFSET_CALIBRATION_SPEEDUP_OFF       0
#define MIRISDR_DC_OFFSET_CALIBRATION_SPEEDUP_ON        1

/*** Register 6: DC Offset Calibration setup ***/

/* reg6: 4:7 (DCTRK_TIM0 - DCTRK_TIM3) */

/* reg6: 8:21 (DCRATE_TIM0 - DCRATE_TIM11) */

/******************************** functions *********************************/

int mirisdr_adc_init (mirisdr_dev_t *p);
int mirisdr_adc_stop (mirisdr_dev_t *p);
int mirisdr_set_hard(mirisdr_dev_t *p);
int mirisdr_set_soft(mirisdr_dev_t *p);
mirisdr_device_t *mirisdr_device_get (uint16_t vid, uint16_t pid);
int mirisdr_write_reg (mirisdr_dev_t *p, uint8_t reg, uint32_t val);

int mirisdr_samples_convert_252_s16 (mirisdr_dev_t *p, unsigned char* buf, uint8_t *dst8, int cnt);
int mirisdr_samples_convert_336_s16 (mirisdr_dev_t *p, unsigned char* buf, uint8_t *dst8, int cnt);
int mirisdr_samples_convert_384_s16 (mirisdr_dev_t *p, unsigned char* buf, uint8_t *dst8, int cnt);
int mirisdr_samples_convert_504_s16 (mirisdr_dev_t *p, unsigned char* buf, uint8_t *dst8, int cnt);
int mirisdr_samples_convert_504_s8 (mirisdr_dev_t *p, unsigned char* src, uint8_t *dst, int cnt);

#endif