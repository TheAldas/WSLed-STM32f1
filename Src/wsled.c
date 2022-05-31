/*
 * wsled.c
 *
 */

#include <wsled.h>

//[x][3] array for holding all LEDs data in RGB format
static uint8_t led_buffer[WS_NUMBER_OF_LEDS][3];
//DMA buffer from which DMA will be sending data to LED strip
static uint8_t dma_buffer[WS_DMA_BUFFERED_LEDS*24];
//current index of LED data that is going to be put into DMA buffer next
static uint32_t current_led_index = 0UL;


//Variables below are set to default values(DMA1 Channel 7, TIM2 Channel 2 - Pin PA1)

//DMA channel for LED data sending
static DMA_Channel_TypeDef *dma_channel = DMA1_Channel7;
//DMA channel index offset from first DMA channel ((Selected DMA channel index - DMA channel 1 index) - 1)
static uint8_t dma_channel_index_offset = 6U;
//(dma_channel_index_offset * 4) can be used instead of (dma_channel_interrupt_offset)
//but dma_channel_interrupt_offset is used in DMA IRQ handler to save clock cycles in IRQ handler
static uint8_t dma_channel_interrupt_offset = 6U * 4U;
//Timer used for DMA control
static TIM_TypeDef *timer = TIM2;
//Timer used for DMA control channel
static uint8_t t_channel = 2U;

static uint32_t core_clock_freq = 0UL;

static const uint8_t gamma_correction[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

static void wsled_fill_led_DMA_buffer(uint32_t DMA_buffer_length, uint32_t offset_leds);
static void timer_init(void);
static void dma_init(void);
static void DMA_Channel_IRQHandler(void);

/*
 * @brief Initializes wsled library, sets required variables.
 *
 * @note Doesn't initialize TIMx channel pin, it has to be initialized beforehand.
 *
 * @param dma_channel_ptr pointer to DMA_Channel_TypedDef struct
 * @param timer_ptr pointer to TIMx TIM_TypeDef struct
 * @param timer_channel TIMx channel. Passes TIMx channel must be in the request list of the passed DMA channel.
 * 						1 <= timer_channel <= 4
 * @param core_clock_frequency System core clock frequency. 8000'000 <= core_clock_frequency
 *
 * @retval int: 0 - function executed successfully
 * 				-1 - passed argument is incorrect
 */
int wsled_init(DMA_Channel_TypeDef *dma_channel_ptr,
		TIM_TypeDef *timer_ptr,
		uint8_t timer_channel,
		uint32_t core_clock_frequency)
{
	if(8000000UL < core_clock_frequency) core_clock_freq = core_clock_frequency;
	else return -1;

	dma_channel = dma_channel_ptr;
	dma_channel_index_offset =  (((uint32_t)dma_channel - (uint32_t)DMA1_Channel1) / ((uint32_t)DMA1_Channel2 - (uint32_t)DMA1_Channel1));
	dma_channel_interrupt_offset = dma_channel_index_offset * 4;
	timer = timer_ptr;

	if(0 < timer_channel && timer_channel < 5) t_channel = timer_channel;
	else return -1;

	timer_init();
	dma_init();

	//clear all leds and display cleared led strip data
	wsled_clear_all();
	wsled_display();

	return 0;
}



/*
 * @brief Sets led value at given index
 *
 * @param index Led index
 * @param red red color value of an led
 * @param green green color value of an led
 * @param blue blue color value of an led
 *
 * @retval None
 */
void wsled_set_led(uint16_t index, uint8_t red, uint8_t green, uint8_t blue)
{
	led_buffer[index][0] = red;
	led_buffer[index][1] = green;
	led_buffer[index][2] = blue;

#if defined(USE_GAMMA_CORRECTION)
	led_buffer[index][0] = gamma_correction[red];
	led_buffer[index][1] = gamma_correction[green];
	led_buffer[index][2] = gamma_correction[blue];
#else
	led_buffer[index][0] = red;
	led_buffer[index][1] = green;
	led_buffer[index][2] = blue;
#endif
}


/*
 * @brief Resets led at index
 *
 * @param index Led index
 *
 * @retval None
 */
void wsled_clear_led(uint16_t index)
{
	led_buffer[index][0] = 0;
	led_buffer[index][1] = 0;
	led_buffer[index][2] = 0;
}


/*
 * @brief Resets all leds to 0, 0, 0 values
 *
 * @param None
 *
 * @retval None
 */
void wsled_clear_all(void)
{
	for(uint32_t i = 0; i < (sizeof(led_buffer)/sizeof(led_buffer[0])); i++){
		led_buffer[i][0] = 0;
		led_buffer[i][1] = 0;
		led_buffer[i][2] = 0;
	}
}

/*
 * @brief Activates DMA channel and starts exporting led data from buffer to DMA buffer till whole led strip is refreshed
 *
 * @param None
 *
 * @retval None
*/
void wsled_display(void)
{
	dma_channel -> CCR &= ~(1UL << DMA_CCR_EN_Pos);
	current_led_index = 0;
	//number of data to transfer
	dma_channel -> CNDTR = WS_DMA_BUFFERED_LEDS*24;
	wsled_fill_led_DMA_buffer(sizeof(dma_buffer), 0);
	wsled_fill_led_DMA_buffer(sizeof(dma_buffer), sizeof(dma_buffer)/2);
	dma_channel -> CCR |= (0b1UL << DMA_CCR_EN_Pos);
}


/*
 * @brief Initializes Timer
 *
 * @retval None
 */
static void timer_init(void)
{
	uint8_t timer_offset = (timer - TIM2) / (TIM3 - TIM2);
	//enable timer(2-4)
	RCC -> APB1ENR |= (1UL << (RCC_APB1ENR_TIM2EN_Pos + timer_offset));
	//enable counter
	timer -> CR1 |= TIM_CR1_CEN_Msk;
	//set tim2 freq, tim2 frequency is (core freq / ARR)
	timer -> ARR = core_clock_freq/LED_PWM_FREQUENCY;
	//set tim2 channel x pulse width((CCRx/ARR) * 100%)
	*(&(timer -> CCR1) + t_channel) = 0;
	//set timer mode to pwm mode 1 and enable output compare preload
	uint32_t ccmr_pwm_msk = ((0b110UL << TIM_CCMR1_OC1M_Pos) | (TIM_CCMR1_OC1PE_Msk)) << (8 * ((t_channel + 1) & 0b1));
	if(t_channel < 3) timer -> CCMR1 = (((timer -> CCMR1) & (0x00FF << (8 * (t_channel & 1)))) | ccmr_pwm_msk);
	else timer -> CCMR2 = ((timer -> CCMR2) & (0x00FF << (8 * (t_channel & 0b1)))) | ccmr_pwm_msk;
	//enable auto-reload preload
	timer -> CR1 |= TIM_CR1_ARPE_Msk;
	//generate update of the counter registers
	timer -> EGR |= TIM_EGR_UG_Msk;
	//set pwm channel polarity to active high and enable output
	timer -> CCER = (timer -> CCER & (0x000F << (4 * (t_channel-1))))
			| (((0UL << TIM_CCER_CC1P_Pos) | (1UL << TIM_CCER_CC1E_Pos)) << ((t_channel-1)*4));
}


/*
 * @brief Initializes DMA
 *
 * @retval None
 */
static void dma_init(void)
{
	RCC -> AHBENR |= RCC_AHBENR_DMA1EN_Msk;

	//disable DMA channel
	dma_channel -> CCR &= ~(0b1UL << DMA_CCR_EN_Pos);

	//enable IRQ for DMA1 channel x
	NVIC_EnableIRQ((DMA1_Channel1_IRQn + (dma_channel_index_offset)));

	//number of data to transfer
	dma_channel -> CNDTR = WS_DMA_BUFFERED_LEDS*24;

	//channel priority - high, memory size - 1 byte, peripheral size - 2 bytes,
	//memory increment mode - enabled, peripheral increment mode - disabled,
	//dma circular mode - enabled, data transfer direction - from memory to peripheral
	dma_channel -> CCR = ((0b10UL << DMA_CCR_PL_Pos) | (0b00UL << DMA_CCR_MSIZE_Pos) | (0b01UL << DMA_CCR_PSIZE_Pos) | (0b1UL << DMA_CCR_MINC_Pos) \
			| (0b0UL << DMA_CCR_PINC_Pos) | (0b1UL << DMA_CCR_CIRC_Pos) | (0b1UL << DMA_CCR_DIR_Pos));


	//enable Half transfer and Transfer Complete interrupts
	dma_channel -> CCR |= ((0b1 << DMA_CCR_HTIE_Pos) | (0b1 << DMA_CCR_TCIE_Pos));

	//dma channel x peripheral address
	dma_channel -> CPAR = (uint32_t) (&(timer -> CCR1)+(t_channel-1));
	//dma channel x memory address
	dma_channel -> CMAR = (uint32_t) &dma_buffer;

	//number of data to transfer
	dma_channel -> CNDTR = sizeof(dma_buffer);

	//turn on DMA requests for TIMx channel
	timer -> DIER |= ((1UL << TIM_DIER_UDE_Pos) | (1UL << (TIM_DIER_CC1DE_Pos + t_channel - 1)));
	// CCx DMA requests sent when update event occurs
	timer -> CR2 |= TIM_CR2_CCDS_Msk;
	//Enable URS so only counter overflow or underflow can generate DMA request
	timer -> CR1 |= TIM_CR1_URS_Msk;
}

/*
 * @brief Fills DMA buffer with new led data
 *
 * @param DMA_buffer_length length of a DMA buffer in bytes
 * @param offset_bytes first byte index in dma_buffer from where the function starts filling DMA buffer since this funciton only fills half a buffer.
 * 					Variable that's passed should be either 0 or (DMA buffer size) / 2
 *
 * @retval None
 */
static void wsled_fill_led_DMA_buffer(uint32_t DMA_buffer_length, uint32_t offset_bytes)
{
	DMA_buffer_length /= 48; //divide buffer by 2 and divide by bits it takes to represent 1 led data(3 colors, 8 bytes for each color)
	for(uint32_t i = 0; i < DMA_buffer_length; i++)
	{
		if(current_led_index >= (sizeof(led_buffer)/sizeof(led_buffer[0])))
		{
			for(uint8_t j = 0; j < 24; j++) dma_buffer[offset_bytes + j] = 0;
			offset_bytes += 24;
			current_led_index++;
			continue;
		}
		//green
		for(uint8_t j = 0; j < 8; j++)
			dma_buffer[(offset_bytes + j)] = ((led_buffer[current_led_index][1] << j) & 0x80) ? LED_HIGH_BIT : LED_LOW_BIT;
		offset_bytes += 8;
		//red
		for(uint8_t j = 0; j < 8; j++)
			dma_buffer[(offset_bytes + j)] = ((led_buffer[current_led_index][0] << j) & 0x80) ? LED_HIGH_BIT : LED_LOW_BIT;
		offset_bytes += 8;
		//blue
		for(uint8_t j = 0; j < 8; j++)
			dma_buffer[(offset_bytes + j)] = ((led_buffer[current_led_index][2] << j) & 0x80) ? LED_HIGH_BIT : LED_LOW_BIT;
		offset_bytes += 8;
		current_led_index++;
	}
}


void DMA1_Channel7_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}

void DMA1_Channel6_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}

void DMA1_Channel5_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}

void DMA1_Channel4_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}

void DMA1_Channel3_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}

void DMA1_Channel2_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}

void DMA1_Channel1_IRQHandler(void)
{
	DMA_Channel_IRQHandler();
}


static void DMA_Channel_IRQHandler()
{
	//disable DMA channel if all of the data has been surely sent to LEDs
	if(current_led_index >= (WS_NUMBER_OF_LEDS+WS_DMA_BUFFERED_LEDS))
	{
		dma_channel -> CCR &= ~(1UL << DMA_CCR_EN_Pos);
		DMA1 -> IFCR |= (1UL << (DMA_IFCR_CGIF1_Pos + dma_channel_interrupt_offset));
		return;
	}

	//if half transfer is complete
	if((DMA1 -> ISR) & (1UL << (DMA_ISR_HTIF1_Pos + dma_channel_interrupt_offset)))
	{
		wsled_fill_led_DMA_buffer(sizeof(dma_buffer), 0);
		DMA1 -> IFCR |= (1UL << (DMA_IFCR_CHTIF1_Pos + dma_channel_interrupt_offset));
	}
	//if transfer is complete
	else if((DMA1 -> ISR) & (1UL << (DMA_ISR_TCIF1_Pos + dma_channel_interrupt_offset)))
	{
		wsled_fill_led_DMA_buffer(sizeof(dma_buffer), sizeof(dma_buffer)/2);
		DMA1 -> IFCR |= (1UL << (DMA_IFCR_CTCIF1_Pos + dma_channel_interrupt_offset));
	}
}
