#include "debounce.h"
#include "timer.h"
#include <stdlib.h>

#ifdef PROTOCOL_CHIBIOS
#    if CH_CFG_USE_MEMCORE == FALSE
#        error ChibiOS is configured without a memory allocator. Your keyboard may have set `#define CH_CFG_USE_MEMCORE FALSE`, which is incompatible with this debounce algorithm.
#    endif
#endif

#ifndef DEBOUNCE_INITIAL_DELAY
#    define DEBOUNCE_INITIAL_DELAY 5
#endif

#ifndef DEBOUNCE_LOCKOUT_PERIOD
#    define DEBOUNCE_LOCKOUT_PERIOD 12
#endif

#define TOTAL_DELAY (DEBOUNCE_INITIAL_DELAY + DEBOUNCE_LOCKOUT_PERIOD)

#define ROW_SHIFTER ((matrix_row_t)1)

typedef struct {
    bool    pressed : 1;
    uint8_t time : 7;
} debounce_counter_t;

#if DEBOUNCE_INITIAL_DELAY > 0 || DEBOUNCE_LOCKOUT_PERIOD > 0
static debounce_counter_t *debounce_counters;
static fast_timer_t        last_time;
static bool                counters_need_update;
static bool                cooked_changed;

#    define DEBOUNCE_ELAPSED 0

static void update_debounce_counters_and_transfer_if_expired(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, uint8_t elapsed_time);

// we use num_rows rather than MATRIX_ROWS to support split keyboards
void debounce_init(uint8_t num_rows) {
    debounce_counters = malloc(num_rows * MATRIX_COLS * sizeof(debounce_counter_t));
    int i             = 0;
    for (uint8_t r = 0; r < num_rows; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            debounce_counters[i].time = 0;
            debounce_counters[i].pressed = true;
            i++;
        }
    }
}

void debounce_free(void) {
    free(debounce_counters);
    debounce_counters = NULL;
}

bool debounce(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, bool changed) {
    cooked_changed    = false;

    if (counters_need_update || changed) {
        fast_timer_t now          = timer_read_fast();
        fast_timer_t elapsed_time = TIMER_DIFF_FAST(now, last_time);

        last_time    = now;
        if (elapsed_time > UINT8_MAX) {
            elapsed_time = UINT8_MAX;
        }

        if (elapsed_time > 0) {
            update_debounce_counters_and_transfer_if_expired(raw, cooked, num_rows, elapsed_time);
        }
    }
    return cooked_changed;
}

static void update_debounce_counters_and_transfer_if_expired(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, uint8_t elapsed_time) {
    debounce_counter_t *debounce_pointer = debounce_counters;

    counters_need_update = false;

    for (uint8_t row = 0; row < num_rows; row++) {
        matrix_row_t delta = raw[row] ^ cooked[row];
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            matrix_row_t col_mask = (ROW_SHIFTER << col);

            // Handle conditions where a timer is still going
            if (debounce_pointer->time != DEBOUNCE_ELAPSED) {
                // check if a key output needs to be transferred yet
                if (debounce_pointer->time <= DEBOUNCE_LOCKOUT_PERIOD && debounce_pointer->pressed == false) {
                    matrix_row_t cooked_next = (cooked[row] & ~col_mask) | (raw[row] & col_mask);
                    cooked_changed |= cooked_next ^ cooked[row];
                    cooked[row] = cooked_next;
                    
                    if (cooked_changed){
                        debounce_pointer->pressed = true;
                    }
                }
                if (debounce_pointer->time <= elapsed_time) {
                // debounce timer has expired
                    debounce_pointer->time = DEBOUNCE_ELAPSED;
                } else {
                    debounce_pointer->time -= elapsed_time;
                    counters_need_update = true;
                }
                
            } else {
                if (delta & col_mask) {
                    // key has changed and requires attention
                    debounce_pointer->pressed = false;
                    debounce_pointer->time    = TOTAL_DELAY;
                    counters_need_update      = true;
                }
            }
            debounce_pointer++;
        }
    }
}

#else
#    include "none.c"
#endif
