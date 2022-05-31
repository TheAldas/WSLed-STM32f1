/*
 * wsled.h
 *
 */

#ifndef __WSLED_H_
#define __WSLED_H_

#include <stdint.h>
#include <stm32f1xx.h>

#define WS_NUMBER_OF_LEDS 60UL
#define WS_DMA_BUFFERED_LEDS 4UL

#define USE_GAMMA_CORRECTION

/* frequency for timer */
#define LED_PWM_FREQUENCY 800000UL
/* pin high voltage time is 1/LED_PWM_FREQUENCY * LED_HIGH_BIT */
#define LED_HIGH_BIT 90/3*2
/* pin low voltage time is 1/LED_PWM_FREQUENCY * LED_LOW_BIT */
#define LED_LOW_BIT 90/3
/* total time it takes to send 1 bit of data to ws2812 led strip is 1/LED_PWM_FREQUENCY * (LED_HIGH_BIT + LED_LOW_BIT) */

#ifdef __cplusplus
extern "C" {
#endif

int wsled_init(DMA_Channel_TypeDef *dma_channel_ptr,
		TIM_TypeDef *timer_ptr,
		uint8_t timer_channel,
		uint32_t core_clock_freq);
void wsled_set_led(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
void wsled_display(void);
void wsled_clear_led(uint16_t index);
void wsled_clear_all(void);

#ifdef __cplusplus
}
#endif


#endif /* INC_WSLED_H_ */
