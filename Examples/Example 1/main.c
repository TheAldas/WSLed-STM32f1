/**
  ******************************************************************************
  * @file: main.c
  * @brief: wsled library code example 1
  ******************************************************************************
  */

#include "main.h"

#define BASE_CORE_FREQUENCY 8000000UL
#define FREQ_DIV_SYSTICK 1000UL //for 1khz system tick
#define LED_COUNT 60U

static volatile uint32_t current_core_frequency = BASE_CORE_FREQUENCY;

/* Initialization functions*/
static void _init(void);
static void _sysclock_config(void);
static void _GPIO_init(void);
/*			*/


int main(void)
{
  /* Initialize the flash interface and set the systick. */
  _init();

  /* Configure the system clock */
  _sysclock_config();

  /* Initialize all configured peripherals */
  _GPIO_init();

  /*
   * wsled_init doesn't configure pin, TIMx channel x pin has to be configured as an Alternate function output Push-pull
   * In this case we pass TIM2 and its channel 2 to wsled_init function, TIM2 channel 2 pin is PA1, so in _GPIO_init() we configure PA1.
   *
   * current_core_frequency is set to 72Mhz in _sysclock_config() function
  */
  wsled_init(DMA1_Channel7, TIM2, 2U, current_core_frequency);

  /* Infinite loop */
  while (1)
  {
	  for(int16_t i = 0; i < LED_COUNT; i++)
	  {
		  wsled_clear_all();
		  wsled_set_led(i, 80, 80, 80);
		  wsled_display();
		  tilib_delay(50);
	  }
	  for(int16_t i = (LED_COUNT); i > 0; i--)
	  {
		  wsled_clear_all();
		  wsled_set_led(i, 80, 80, 80);
		  wsled_display();
		  tilib_delay(50);
	  }
  }
}

static void _init(void)
{
	  /* Configure Flash prefetch buffer(cache)  */
	  FLASH -> ACR |= FLASH_ACR_PRFTBE;

	  /* Set Interrupt Group Priority */
	  NVIC_SetPriorityGrouping(0x3U);

	  /* Use systick as time base source and configure 1ms tick (default clock after Reset is HSI) */
	  tilib_set_systick(current_core_frequency/FREQ_DIV_SYSTICK);
}


static void _sysclock_config(void)
{
	//HSI settings

	//make flash latency 2 cycles since we're going to use >48mhz core freq
	FLASH -> ACR |= FLASH_ACR_LATENCY_2;

	//turn on external high speed
	RCC -> CR |= RCC_CR_HSEON_Msk;

	//change pll multiplication to x9(8mhz x9 = 72mhz)
	RCC -> CFGR &= ~(RCC_CFGR_PLLMULL16_Msk);
	RCC -> CFGR |= RCC_CFGR_PLLMULL9_Msk;

	//change pll source to HSE
	RCC -> CFGR |= RCC_CFGR_PLLSRC_Msk;

	//change apb1 prescaler to x2, cause apb1 must be <=36mhz
	RCC -> CFGR |= RCC_CFGR_PPRE1_DIV2;

	//turn on pll
	RCC -> CR |= RCC_CR_PLLON;

	//set pll as system clock
	RCC -> CFGR |= RCC_CFGR_SW_PLL;

	SystemCoreClockUpdate();
	//HSE is 8Mhz, pll is x9, clock source is pll, so current core frequency now will be 72Mhz
	current_core_frequency = BASE_CORE_FREQUENCY * 9;
	//re-set systick configuration values for 1ms
	tilib_set_systick(current_core_frequency/FREQ_DIV_SYSTICK);
}

static void _GPIO_init(void)
{
	//enable port B and A clock
	RCC -> APB2ENR |= ((RCC_APB2ENR_IOPAEN_Msk) | (RCC_APB2ENR_IOPBEN_Msk));
	if((RCC -> APB2ENR) & RCC_APB2ENR_IOPBEN_Msk) ;

	//set PB12 to output push-pull @50Mhz max
	GPIOB -> CRH |= (uint32_t)(0b00 << GPIO_CRH_CNF12_Pos) | (uint32_t)(0b11 << GPIO_CRH_MODE12_Pos);

	//configure pin PA1 as an alternate function push-pull @50Mhz max 0b10 | 0b11
	GPIOA -> CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
	GPIOA -> CRL |= (uint32_t)(0b10 << GPIO_CRL_CNF1_Pos) | (uint32_t)(0b11	 << GPIO_CRL_MODE1_Pos);
}
