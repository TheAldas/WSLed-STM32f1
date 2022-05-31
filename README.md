# WSLed-STM32f1 - Library for ws2812 led strips
 
## About
 This library is for controlling single ws2812 led strip with stm32f1. It uses timer, DMA and DMA interrupts.
 
## Usage
### <ins>Initialization</ins>
For the sake of simplicity let's say we want to use PA1 pin to control ws2812b led strip, the pin is controlled by timer TIM2 channel 2 and TIM2 channel 2 is controlled by the DMA1 channel 7.
Then:
1) Configure PA1 pin as alternate function push-pull.
```c
//Example:
//Configure pin PA1 as an alternate function push-pull @50Mhz max
	GPIOA -> CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
	GPIOA -> CRL |= (uint32_t)(0b10 << GPIO_CRL_CNF1_Pos) | (uint32_t)(0b11	 << GPIO_CRL_MODE1_Pos);
```
2) Call wsled_init(...) function.
```c
//Example:
 wsled_init(DMA1_Channel7, TIM2, 2U, current_core_frequency);
/* @Note: current_core_frequency is a frequency the microcontroller is running at.
```
### <ins>Available functions</ins>
```c 
//set specific led at index rgb values.
void wsled_set_led(uint16_t index, uint8_t red, uint8_t green, uint8_t blue);
//reset specific led at index rgb values to 0, 0, 0.
void wsled_clear_led(uint16_t index);
//reset all of the leds rgb values to 0, 0, 0.
void wsled_clear_all();
//send leds values to led strip
void wsled_display();
```

## Notes
1) TiLib library that is used in example 1 can be downloaded from [here](https://github.com/TheAldas/TiLib/tree/main/Src)
