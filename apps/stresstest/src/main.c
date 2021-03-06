/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "sysinit/sysinit.h"
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include <sound/sound_pwm.h>
#include <microbit_matrix/microbit_matrix.h>
#include <ws2812b/ws2812b.h>
#include <hal/hal_i2c.h>

/* ADC */
#include <adc/adc.h>
#include "adc_nrf51_driver/adc_nrf51_driver.h"

#define SIZE_OF_VALUES  32
uint16_t values[SIZE_OF_VALUES];
uint8_t  value_ix = 0;

int samples_total = 0;

struct adc_dev *adc;
static int adc_pro_sec = 32;
static int adc_sec = 1;

#define NUMBER_OF_CALIBRATION_SAMPLES  25

static bool calibrating = false;
static int calibration_sample_counter = 0;
static int calibration_sum = 0;
static uint16_t calibration_average = 0;

static struct os_callout timer_callout;

static void adc_timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    adc_sample(adc);
    if(adc_pro_sec > 0) {
        os_callout_reset(&timer_callout, (adc_sec * OS_TICKS_PER_SEC) / adc_pro_sec);
    }
}

static void init_adc_timer(void) {
    os_callout_init(&timer_callout, os_eventq_dflt_get(),
                    adc_timer_ev_cb, NULL);
}

static void start_adc_timer(void) {
        os_callout_reset(&timer_callout, (adc_sec * OS_TICKS_PER_SEC) / adc_pro_sec);
}

/*
static uint16_t
calc_new_average(uint16_t value){
    values[value_ix++] = value;
    uint16_t sum_of_values = 0;
    if(value_ix >= SIZE_OF_VALUES){
        value_ix = 0;
    }
    for(int ix=0; ix < SIZE_OF_VALUES; ix++){
        sum_of_values += values[ix];
    }
    return sum_of_values/SIZE_OF_VALUES;
}
*/

static void
start_calibrate() {
    calibrating = true;
}

static void
stop_calibrate() {
    calibrating = false;
    calibration_average = calibration_sum/calibration_sample_counter;
    console_printf("avg%d\n", (int)calibration_average);
}

void
print_bytes(const uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        console_printf("%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
    console_printf("\n");
}

int
adc_read_event(struct adc_dev *dev, void *arg, uint8_t etype,
               void *buffer, int buffer_len) {
    int value;
    int rc = -1;
    value = adc_nrf51_driver_read(buffer, buffer_len);
    if (value >= 0) {
        value = value / 3;
//        console_printf("v%d\n", (int)value);
        if(calibrating) {
            calibration_sum += value;
            calibration_sample_counter++;
            showInt_0_25((uint8_t) calibration_sample_counter % 25);
            if(calibration_sample_counter >= NUMBER_OF_CALIBRATION_SAMPLES){
                stop_calibrate();
            }
            return 0;
        }
        uint8_t diff = (calibration_average - value) + 12;
        console_printf("%d\n", (int)diff);
        showInt_0_25((uint8_t )diff);
/*        uint16_t frequenz = 2000 - value;
        if(frequenz > 20) {
            sound_on(frequenz);
        } else {
                sound_off();
        }
*/
    } else {
        console_printf("Error while reading: %d\n", value);
    }
    return (rc);
}

static int
adc_init() {
    init_adc_timer();
    adc = adc_nrf51_driver_get();
    int rc = adc_event_handler_set(adc, adc_read_event, (void *) NULL);
    return rc;
}

/**
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void) {
    /* Initialize OS */
    sysinit();

    ws2812_init();
    adc_init();
    init_adc_timer();
    start_calibrate();
    start_adc_timer();
    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}
