#pragma once

typedef enum {
    LED_WIFI_CONNECTING,  /* fast blink 100ms */
    LED_CAPTURE_OK,       /* 2 quick pulses */
    LED_ERROR,            /* 5 rapid flashes */
    LED_SLEEPING,         /* single long pulse then off */
} led_pattern_t;

void led_init(void);
void led_blink(led_pattern_t pattern);
