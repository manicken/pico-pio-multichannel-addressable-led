#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include <stdatomic.h>
#include "pico/multicore.h"


#include "tusb.h"
#include "blink.pio.h"
#include "ws28xx-16ch.pio.h"



#define BLINK_LED_PIO pio1
#define BLINK_LED_PIO_SM 0
uint blink_led_pio_offset = 0;

#define WS28XX_16CH_PIO_FREQ_DIV 15.6f
#define WS28XX_16CH_PIO_START_PIN 0
#define WS28XX_16CH_PIO pio0
#define WS28XX_16CH_PIO_SM 0
uint ws28xx_16ch_pio_offset = 0;
#define WS28XX_16CH_PIXEL_TRIGGER_PIO_SM 1
uint ws28xx_16ch_pixel_trigger_pio_offset = 0;
#define WS28XX_16CH_PIO_DMA_CH 0

#define PIXEL_DONE_PIN 21
#define FRAME_DONE_PIN 22
bool frame_done_pin_state = false;


// Simple LED toggle state, can be used to get state
bool led_on = false; 

#define LED_COUNT_PER_CH 100
#define BYTES_PER_PIXEL 3
#define CHANNELS 16
#define NUMBER_OF_BUFFERS 2
#define NUMBER_OF_PLANES_PER_DATA 2
#define BITPLANE_BUFFER_SIZE (BYTES_PER_PIXEL*CHANNELS*LED_COUNT_PER_CH/NUMBER_OF_PLANES_PER_DATA)

uint32_t bitplane_frame_a_buffer[BITPLANE_BUFFER_SIZE];
uint32_t bitplane_frame_b_buffer[BITPLANE_BUFFER_SIZE];
/** is either 0 or 1 */
uint32_t currentBitplaneBufferIndex = 0;
volatile uint32_t bitplaneBufferChanged = 0;

volatile uint32_t rgbDataBufferChanged = 0;

//#define ONLY_SEND_WHEN_CHANGED

int64_t latch_alarm_cb(alarm_id_t id, void *user_data) {
    if (atomic_exchange(&bitplaneBufferChanged, 0)) {
        currentBitplaneBufferIndex ^= 1;
#ifndef ONLY_SEND_WHEN_CHANGED
    }
#endif

    dma_channel_set_read_addr(
        WS28XX_16CH_PIO_DMA_CH,
        currentBitplaneBufferIndex ?
            bitplane_frame_b_buffer :
            bitplane_frame_a_buffer,
        true
    );

    // Ask core1 for next frame (non-blocking!)
   // if (atomic_exchange(&rgbDataBufferChanged, 0) && multicore_fifo_wready()) {
   //     multicore_fifo_push_blocking(1);
   // }
#ifdef ONLY_SEND_WHEN_CHANGED
}
#endif
    // --- FRAME DONE DEBUG/DEVELOPMENT PULSE ---
    gpio_put(FRAME_DONE_PIN, frame_done_pin_state);   // toggle
    frame_done_pin_state = !frame_done_pin_state;

    return 0;
}

void __isr ws28xx_16ch_pio_dma_irq_handler() {
    dma_hw->ints0 = 1u << WS28XX_16CH_PIO_DMA_CH; // clear IRQ
    add_alarm_in_us(50, latch_alarm_cb, NULL, false);
}


void blink_pin(bool on) {
    if (on) {
        pio_sm_put_blocking(BLINK_LED_PIO, BLINK_LED_PIO_SM, 0xFFFF); // simple hack: send long delay
    } else {
        pio_sm_put_blocking(BLINK_LED_PIO, BLINK_LED_PIO_SM, 0x0000); // stop
    }
}

void usb_cmd_process() {
    while (tud_cdc_available()) {
        char ch;
        tud_cdc_read(&ch, 1);

        switch (ch) {
            case '1':
                led_on = true;
                blink_pin(true);
                tud_cdc_write_str("LED ON\r\n");
                break;
            case '0':
                led_on = false;
                blink_pin(false);
                tud_cdc_write_str("LED OFF\r\n");
                break;
            case 'p':
                tud_cdc_write_str("PONG\r\n");
                break;
            default:
                tud_cdc_write_str("Unknown command\r\n");
        }
        tud_cdc_write_flush(); // still flush to send immediately
    }
}

int main() {
    
    stdio_init_all();       // USB stdio is optional here, we use CDC API directly
    tusb_init();            // TinyUSB initialization
    gpio_init(FRAME_DONE_PIN);
    gpio_set_dir(FRAME_DONE_PIN, true);
    gpio_put(FRAME_DONE_PIN, 0);  // start low

    gpio_init(PIXEL_DONE_PIN);
    gpio_set_dir(PIXEL_DONE_PIN, true);
    //gpio_put(PIXEL_DONE_PIN, 0);  // start low

    // Load PIO program for LED blink
    //pio_clear_instruction_memory(BLINK_LED_PIO);
    blink_led_pio_offset = pio_add_program(BLINK_LED_PIO, &blink_program);

    blink_program_init(BLINK_LED_PIO, BLINK_LED_PIO_SM, blink_led_pio_offset, PICO_DEFAULT_LED_PIN);
    pio_sm_set_enabled(BLINK_LED_PIO, BLINK_LED_PIO_SM, true);

    
    pio_clear_instruction_memory(WS28XX_16CH_PIO); // just to make sure we start on clean plate
    // Load PIO program for WS28xx driver
    ws28xx_16ch_pio_offset = pio_add_program(WS28XX_16CH_PIO, &ws28xx_16ch_program);
    ws28xx_16ch_program_init(WS28XX_16CH_PIO, WS28XX_16CH_PIO_SM, ws28xx_16ch_pio_offset, WS28XX_16CH_PIO_START_PIN, WS28XX_16CH_PIO_FREQ_DIV); // start at GPIO0
    pio_sm_set_enabled(WS28XX_16CH_PIO, WS28XX_16CH_PIO_SM, true);
    
    // Load PIO program for WS28xx pixel trigger debug output driver
    ws28xx_16ch_pixel_trigger_pio_offset = pio_add_program(WS28XX_16CH_PIO, &ws28xx_16ch_pixel_trigger_debug_program);
    ws28xx_16ch_pixel_trigger_debug_program_init(WS28XX_16CH_PIO, WS28XX_16CH_PIXEL_TRIGGER_PIO_SM, ws28xx_16ch_pixel_trigger_pio_offset, PIXEL_DONE_PIN); // use gpio22
    pio_sm_set_enabled(WS28XX_16CH_PIO, WS28XX_16CH_PIXEL_TRIGGER_PIO_SM, true);


    dma_channel_config c = dma_channel_get_default_config(WS28XX_16CH_PIO_DMA_CH);

    // memory → peripheral
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

    // increment source, not destination
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    // pace DMA by PIO TX FIFO
    channel_config_set_dreq(&c, pio_get_dreq(WS28XX_16CH_PIO, WS28XX_16CH_PIO_SM, true));

    for (int i=0;i<BITPLANE_BUFFER_SIZE;i++) {
        bitplane_frame_a_buffer[i] = 0; // clear all data
        bitplane_frame_b_buffer[i] = 0; // clear all data
    }
    for (int i=0;i<BITPLANE_BUFFER_SIZE;i++) {
        bitplane_frame_a_buffer[i] = i;//0x0000FFFF; // test set
        
    }
    currentBitplaneBufferIndex = 0;
    dma_channel_configure(
        WS28XX_16CH_PIO_DMA_CH,
        &c,
        &pio0->txf[WS28XX_16CH_PIO_SM],  // write addr (PIO FIFO)
        bitplane_frame_a_buffer,                // read addr (RAM)
        BITPLANE_BUFFER_SIZE,                // number of 32-bit transfers
        false                            // don't start yet
    );

    dma_channel_set_irq0_enabled(WS28XX_16CH_PIO_DMA_CH, true);
    irq_set_exclusive_handler(DMA_IRQ_0, ws28xx_16ch_pio_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_start_channel_mask(1u << WS28XX_16CH_PIO_DMA_CH);

    

    //
    //
    //dma_channel_set_read_addr(WS28XX_16CH_PIO_DMA_CH, bitplane_frame_a_buffer, true);
   // bool led_state = false;
    //uint32_t last_ms = to_ms_since_boot(get_absolute_time()); // last toggle timestamp
    //gpio_set_dir(FRAME_DONE_PIN, true);
    //gpio_put(FRAME_DONE_PIN, 0);  // start low

    //gpio_set_dir(PIXEL_DONE_PIN, true);
    //gpio_put(PIXEL_DONE_PIN, 0);  // start low

    while (true) {
        tud_task();          // TinyUSB device task, MUST run often
        usb_cmd_process();   // read/write USB CDC commands

        
        /*uint32_t now = to_ms_since_boot(get_absolute_time());

        // Toggle every 500ms (1Hz blink)
        if (now - last_ms >= 5) {
            last_ms = now;
            led_state = !led_state;
            gpio_put(FRAME_DONE_PIN, led_state);
            gpio_put(PIXEL_DONE_PIN, led_state);
        }*/
    }
}
/*
int main() {
    gpio_init(FRAME_DONE_PIN);
    gpio_set_dir(FRAME_DONE_PIN, true);
    gpio_put(FRAME_DONE_PIN, 0);

    gpio_init(PIXEL_DONE_PIN);
    gpio_set_dir(PIXEL_DONE_PIN, true);
    gpio_put(PIXEL_DONE_PIN, 0);

    while (true) {
        gpio_put(FRAME_DONE_PIN, 1);
        gpio_put(PIXEL_DONE_PIN, 1);
        sleep_ms(1);  // HIGH for 1 second

        gpio_put(FRAME_DONE_PIN, 0);
        gpio_put(PIXEL_DONE_PIN, 0);
        sleep_ms(1);  // LOW for 1 second
    }
}*/
