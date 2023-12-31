#include "../system/include/cmsis/stm32f4xx.h"
#include <stdint.h>		
void softdelay(volatile unsigned long N);

void I2C1_init(void);
void I2C_send(uint8_t *pack, uint8_t pack_num);
void I2C_send_byte(uint8_t byte);

void PCF8574AT_send(uint8_t data, uint8_t led_flag, uint8_t rw_flag, uint8_t rs_flag, uint8_t cycles, uint32_t delay_ms);

void LCD1602_backlight_on(void);
void LCD1602_backlight_off(void);
void LCD1602_send_char(uint8_t character);
void LCD1602_init(void);
void LCD1602_set_cursor(uint8_t position);
void LCD1602_send_char_position(uint8_t character, uint8_t position);
void LCD1602_position_rst(void);
void LCD1602_display_clear(void);
void LCD1602_send_string(uint8_t *string, uint8_t position);
void LCD1602_send_integer(uint16_t number, uint8_t position);

void LCD1602_send_arith_results(uint8_t X, uint8_t Y, uint8_t mode, uint32_t result);
void arith_display(uint8_t X, uint8_t Y, uint8_t mode);

#define frequency	16000000UL		// 16 MHz high-speed internal (RC)

/*	I2C CODE DEFINES && VARIABLES && MACRO START	*/

#define I2C_MODE_TRANSMIT 0
#define I2C_ADDR(address, mode)	(((address) << 1) | (mode))	// [[addr[7:0]],[R/!W bit]]

#define PCF8574AT_ADDRESS 0x3F	// tested with Arduino 

#define LCD_READ 1	// R / !W
#define LCD_WRITE 0	// R / !W
#define LCD_INSTR 0	// RS
#define LCD_DATA 1	// RS
#define ENA 1	// latching
#define N_ENA 0	// latching	

/*	DISPLAY INSTRUCTIONS				*/

#define INTERFACE_8_BIT 0x30			// 0011 XXXX -> 001 [DATA 8/4] XXXX	
#define INTERFACE_4_BIT_2_ROWS_FONT_5X8 0x28	// 0010 10XX -> 001 [DATA 8/4] [Number of lines 1/2] [Font size] XX
#define INTERFACE_4_BIT	0x20
#define DISPLAY_ON_CURSOR_ON_BLINK_DISABLE 0x0E	// 0000 1110 -> 0000 1 [Display on/off] [Cursor] [Blink]	
#define DISPLAY_ON_CURSOR_ON_BLINK_ENABLE 0x0F	// 0000 1111 -> 0000 1 [Display on/off] [Cursor] [Blink]	
#define DISPLAY_CLEAR 0x01			// 0000 0001
#define DIRECTION_L_TO_R_SHIFT_DISABLE 0x06	// 0000 0110 -> 0000 01 [DDRAM address Increment/Decrement] [Shift]
#define DDRAM_ADDRESS_0X0 0x80
#define DISPLAY_ON_CURSOR_OFF_BLINK_DISABLE 0x0C
#define DISPLAY_OFF_CURSOR_OFF_BLINK_DISABLE 0x8

/*	I2C CODE DEFINES && VARIABLES && MACRO END	*/

void softdelay(volatile unsigned long N)
{
	volatile unsigned long inner;
	
	while (N--) 
	{
		for (inner = 100; inner > 0; inner--);
	}
}

// I2C initialization

void I2C1_init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;    
	
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;	// enable GPIOB
	
	GPIOB->AFR[1] |= (4 << (4 * 0));	// PB8 - SCL - for AF4
	GPIOB->AFR[1] |= (4 << (4 * 1));	// PB9 - SCL - for AF4
	
	GPIOB->MODER &= ~(GPIO_MODER_MODER9 |GPIO_MODER_MODER8);	// AF open drain mode, external pull-ups
	GPIOB->MODER |= (GPIO_MODER_MODER9_1 |  GPIO_MODER_MODER8_1);
	GPIOB->OTYPER |=(GPIO_OTYPER_OT_9 |GPIO_OTYPER_OT_8);

	I2C1->CR2 &= ~I2C_CR2_FREQ;	
	I2C1->CR2 |= 16;		// 16 MHz - PB1
	
	I2C1->CCR &= ~I2C_CCR_CCR;
	I2C1->CCR |= 80;		// 100 kHz SCL frequency
	
	I2C1->CCR &= ~I2C_CCR_FS;
	
	I2C1->TRISE = 17;		// (1000ns / Tpclk1) + 1
	
	I2C1->CR1 |= I2C_CR1_PE;	// peripheral enable
}

void I2C_send(uint8_t *pack, uint8_t pack_num)
{
	// Sends N = pack_num bytes of the data to the Slave.
	// Check "Figure 164. Transfer sequence diagram for master transmitter" to see the events flow.
	
	I2C1->CR1 |= I2C_CR1_START;			// start bit 
	
	while (!(I2C1->SR1 & I2C_SR1_SB));		// wait for SB (event 5)
	(void) I2C1->SR1;				// clear SB flag - EV5: SB = 1, cleared by reading SR1 register followed by writing DR 
							// register with Address
	I2C1->DR = 0x7E;
	
	while (!(I2C1->SR1 & I2C_SR1_ADDR));		// wait for ADDR
	(void) I2C1->SR1;				// clear ADDR flag - EV6: ADDR = 1, cleared by reading SR1 register followed by 
	(void) I2C1->SR2;				// reading SR2
	
	I2C1->DR = *pack;
	while (!(I2C1->SR1 & I2C_SR1_TXE));		// wait for data register empty
	
	pack++;
	
	for(uint8_t num = 0; num < pack_num-1; num++)
	{
		  I2C1->DR = *pack++;			// send and go to the next cell of the input array
		  while (!(I2C1->SR1 & I2C_SR1_TXE));
	}
	
	while (!(I2C1->SR1 & I2C_SR1_BTF));		// byte transfer finished
		
	I2C1->CR1 |= I2C_CR1_STOP;			// stop bit
}

void I2C_send_byte(uint8_t byte)
{
	I2C1->CR1 |= I2C_CR1_START;
    	
	while (!(I2C1->SR1 & I2C_SR1_SB));
	(void) I2C1->SR1;

	I2C1->DR = 0x7E;
           	
	while (!(I2C1->SR1 & I2C_SR1_ADDR));
	
	(void) I2C1->SR1;
	(void) I2C1->SR2;
    	
	I2C1->DR = byte;
	while (!(I2C1->SR1 & I2C_SR1_TXE));

	while (!(I2C1->SR1 & I2C_SR1_BTF));
	
	I2C1->CR1 |= I2C_CR1_STOP;
}

// implementation of latching inputting way
void PCF8574AT_send(uint8_t data, uint8_t led_flag, uint8_t rw_flag, uint8_t rs_flag, uint8_t cycles, uint32_t delay_ms)
{
	// latching timings (delays) were not taken in account
	
	// RS | R/!W | DB7 | DB6 | DB5 | DB4 | DB3 | DB2 | DB1 | DB0	- instruction format from datasheet
	// DB7 | DB6 | DB5 | DB4 | LED | E | R/!W | RS			- I2C (PCF8547AT instruction format)
	
	// cycles - 2 (instr -> latched_instr) or 4 (MSB_instr -> latched_MSB_instr -> LSB_instr -> latched_LSB_instr)
	// RS: 0 = instr, 1 = data
	// rw - R / !W
	
	uint8_t led, ena, rw, rs, data_msbits, data_lsbits;	// data's 4 most significant bits, and 4 least significant bits
	
	led = led_flag << 3;					// LED: 0 - backlight OFF, 1 - backlight ON
	ena = ENA << 2;
	rw = rw_flag << 1;					// R/!W: 0 - write, 1 - read
	rs = rs_flag;						// RS: 0 - instruction, 1 - data
	
	data_msbits = data & 0xF0;				// 1111 1111 & F0 = 1111 0000
	data_lsbits = (data << 4) & 0xF0;			// 1111 0001 << 4 = 0001 0000
	
	uint8_t package[4];
	
	for(uint8_t cells = 0; cells < 4; cells++)
	{
		package[cells] = 0;
	}
	
	package[0] = data_msbits | led | ena | rw | rs;		// ENA = 1
	package[1] = package[0] ^ 4; 				// ENA = 0 --> LATCHED!
	package[2] = data_lsbits | led | ena | rw | rs;		// ENA = 1
	package[3] = package[2] ^ 4;				// ENA = 0 --> LATCHED!
		
	I2C_send(package, cycles);				// send ready instruction
	
	softdelay(delay_ms);					// wait some time, required delay after instruction was sent
}

void LCD1602_backlight_on(void)
{
	PCF8574AT_send(0x0, 1, LCD_WRITE, LCD_INSTR, 2, 1);	// 1 ms delay, 2 cycles
}

void LCD1602_backlight_off(void)
{
	PCF8574AT_send(0x0, 0, LCD_WRITE, LCD_INSTR, 2, 1);	// 1 ms delay, 2 cycles
}

void LCD1602_send_char(uint8_t character)
{
	PCF8574AT_send(character, 1, LCD_WRITE, LCD_DATA, 4, 3);	// 4 cycles, 3 ms delay
}

void LCD1602_init()
{
	// RS | R/!W | DB7 | DB6 | DB5 | DB4 | DB3 | DB2 | DB1 | DB0 - instruction format from datasheet
	// DB7 | DB6 | DB5 | DB4 | LED | E | R/!W | RS	- I2C (PCF8547AT instruction format)
	
	// softdelay(5);	// some (40 ms, for the best way) delay before initialization start
	
	/*
	uint8_t instruction[] = 
	{
		INTERFACE_8_BIT, 
		INTERFACE_8_BIT, 
		INTERFACE_4_BIT, 
		INTERFACE_4_BIT_2_ROWS_FONT_5X8, 
		DISPLAY_OFF_CURSOR_OFF_BLINK_DISABLE,
		DISPLAY_CLEAR, 
		DIRECTION_L_TO_R_SHIFT_DISABLE, 
		DISPLAY_ON_CURSOR_OFF_BLINK_DISABLE
	};
	*/
	
	uint8_t instruction[] = {0x33, 0x32, 0x28, 0x0c, 0x01, 0x06, 0x80};	// working version from the forum 
	
	// 0x33,0x32,INTERFACE_4_BIT_2_ROWS_FONT_5X8,DISPLAY_ON_CURSOR_OFF_BLINK_DISABLE, DISPLAY_CLEAR, DIRECTION_L_TO_R_SHIFT_DISABLE, DDRAM_ADDRESS_0X0
	
	// uint8_t instr_count = sizeof(instruction) / sizeof(instruction[0]);		// number of initialization instructions
	uint8_t instr_count = 7;	// piece of hardcode
	
	uint8_t instr;
	
	for(instr = 0; instr < 3; instr++)
	{
		PCF8574AT_send(instruction[instr], 1, LCD_WRITE, LCD_INSTR, 2, 3);	// 3 ms delay, 2 cycles
	}
	for(instr = 3; instr < instr_count; instr++)
	{
		PCF8574AT_send(instruction[instr], 1, LCD_WRITE, LCD_INSTR, 4, 3);	// 3 ms delay, 4 cycles
	}
}

void LCD1602_set_cursor(uint8_t position)
{
	/*
		00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 0A | 0B | 0C | 0D | 0E | 0F
		40 | 41 | 42 | 43 | 44 | 45 | 46 | 47 | 48 | 49 | 4A | 4B | 4C | 4D | 4E | 4F
	*/
	
	uint8_t data;
	
	data = (1 << 7) | position;
	
	PCF8574AT_send(data, 1, LCD_WRITE, LCD_INSTR, 4, 3);	// 4 cycles, 3 ms delay
}

void LCD1602_send_char_position(uint8_t character, uint8_t position)
{
	LCD1602_set_cursor(position);
	LCD1602_send_char(character);
}

void LCD1602_position_rst(void)
{
	PCF8574AT_send(0x2, 1, LCD_WRITE, LCD_INSTR, 4, 1);	// 1 ms delay, 2 cycles
}

void LCD1602_display_clear(void)
{
	LCD1602_set_cursor(0x00);
	
	for(uint8_t position = 0x0; position <= 0x67; position++)
	{
		LCD1602_send_char(' ');
	}
	
	LCD1602_set_cursor(0x00);			// go to the start
}

void LCD1602_send_string(uint8_t *string, uint8_t position)
{
	LCD1602_set_cursor(position);
	
	while(*string)
	{
		LCD1602_send_char((uint8_t) *string);
		
		string++;
	}
}

void LCD1602_send_integer(uint16_t number, uint8_t position)
{
	// uint16_t tens_of_thousands, thousands, hundreds, tens, units;
	uint8_t output[5];
	
	output[0] = number / 10000;
	output[1] = (number - (output[0] * 10000)) / 1000;
	output[2] = (number - (output[0] * 10000) - (output[1] * 1000)) / 100;	
	output[3] = (number - (output[0] * 10000) - (output[1] * 1000) - (output[2] * 100)) / 10;
	output[4] = (number - (output[0] * 10000) - (output[1] * 1000) - (output[2] * 100) - (output[3] * 10));
	
	LCD1602_set_cursor(position);
	
	for(uint8_t i = 0; i < 5; i++)
	{
		LCD1602_send_char(output[i] + 48);
	}
}


void LCD1602_send_arith_results(uint8_t X, uint8_t Y, uint8_t mode, uint32_t result)
{
	// 00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 0A | 0B | 0C | 0D | 0E | 0F
	// 40 | 41 | 42 | 43 | 44 | 45 | 46 | 47 | 48 | 49 | 4A | 4B | 4C | 4D | 4E | 4F

	// X | : | X4 | X3 | X2 | X1 | X0 | 07 | 08 | 09 | 0A | 0B | 0C | 0D | 0E | 0F
	// Y | : | Y4 | Y3 | Y2 | Y1 | Y0 | 47 | 48 | R  | :  | R4 | R3 | R2 | R1 | R0
	
	// LCD1602_display_clear();
	
	LCD1602_send_string("X:", 0x00);
	LCD1602_send_integer(X, 0x02);
	
	LCD1602_send_string("Y:", 0x40);
	LCD1602_send_integer(Y, 0x42);
	
	LCD1602_send_string("Z:", 0x49);
	LCD1602_send_integer(result, 0x4B);	
	
	switch(mode)
	{
		case 0:
			LCD1602_send_string("Z=(X+Y)", 0x09);
			break;
		case 1:
			LCD1602_send_string("Z=(X-Y)", 0x09);
			break;
		case 2:
			LCD1602_send_string("Z=(X*Y)", 0x09);
			break;
		case 3:	
			LCD1602_send_string("Z=(X/Y)", 0x09);
			break;
		default:
			LCD1602_send_string("ERRMODE", 0x09);
	}
}

void arith_display(uint8_t X, uint8_t Y, uint8_t mode)
{
	uint32_t result = 0;
	
	result = arithmetic_function(X, Y, mode);
	
	LCD1602_send_arith_results(X, Y, mode, result);
}

#define frequency	16000000UL	// 16 MHz high-speed internal (RC)

/*	GLOBAL VARIABLES START				*/

uint8_t LCD_show_ready = 0, debounce_ms_enable = 0, pressure_flag;
uint8_t in_X = 1, in_Y = 1, in_mode = 0;	// ADD by default
uint16_t pressure_10_ms_ticks = 0;

/*	GLOBAL VARIABLES END				*/

/************************************************** SYSTICK **************************************************/

void init_systick(void)
{
	SysTick->CTRL = 0;						// disable SysTick
	SysTick->LOAD = frequency - 1;					// set the initial reload value
	SysTick->VAL = 0;						// reset the curent SysTick counter value
	
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk;
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;	// switch off

	// use proc.clock (1 = processor clock, 0 = external clock (HCLK/8 = 2 MHz)), enable interrupts (1 = enable)
}

void SysTick_Handler(void)
{
	if(debounce_ms_enable)
	{
		EXTI->IMR |= (EXTI_IMR_IM13 | EXTI_IMR_IM14 | EXTI_IMR_IM15);	// enable disabled interrupts
		
		debounce_ms_enable = 0;
		
		SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;  	
	}
}

void systick_debounce_ms(uint32_t debounce_ms)
{
	// default value is 250 ms debounce
	uint32_t time_ms = (debounce_ms >= 1 && debounce_ms <= 1000) ? debounce_ms : 250;	
	
	SysTick->VAL = 0;					// clear current CNT value
	SysTick->LOAD = ((frequency / 1000) * time_ms) - 1;	// (time * 10^-3 * frequency) - 1 = (time * 10^-3 * 16 000 000) - 1 = (time * 16 000) - 1

	debounce_ms_enable = 1;
	
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;	// enable SysTick
}

/**********************************************************************************************************/

/************************************************** TIM2 **************************************************/
void tim2_init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;				// TIM2 clock
	TIM2->PSC = 16000 - 1;						// 1000 ticks per second - 1ms - prescaler 
	TIM2->ARR = 10-1;						// T = 10 ms = 10 ticks -> {0, 1, 2, 3, 4, 5, 6, 7, 8, 9} - auto-reload-register
	TIM2->DIER |= TIM_DIER_UIE;					// TIM2 DMA/interrupt enable register - allow events by timer -  Update interrupt enable 		
	TIM2->CR1 &= ~TIM_CR1_DIR;					// upcounting, DIR = 0
		
	NVIC_EnableIRQ(TIM2_IRQn);	// enable interrupts by TIM2
	__enable_irq();	// global interrupts enable
}

void TIM2_IRQHandler(void)
{
	TIM2->SR &= ~TIM_SR_UIF;	// clear pending bit in the interrupt handler
			
	pressure_10_ms_ticks = ((GPIOB->IDR & GPIO_IDR_ID15) == pressure_flag) ? (pressure_10_ms_ticks + 1) : 0;
	
	if(pressure_10_ms_ticks >= 200)	// 200 x 10ms = 2000ms = 2s --> set default values
	{
		in_X = 1;	
		in_Y = 1;
		in_mode = 0;	
		
		LCD_show_ready = 1;
		
		pressure_10_ms_ticks = 0;	// reset cnt 
		
		TIM2->CR1 &= ~TIM_CR1_CEN;	// switch off TIM2
	}
}

void btn_pressure_check(void)
{
	// set pressure flag:
	pressure_flag = (GPIOB->IDR & GPIO_IDR_ID15);	// PB15 == 1 --> pressure_flag = 1
	
	TIM2->CR1 |= TIM_CR1_CEN;			// switch on TIM2
}

/**********************************************************************************************************/

/************************************************** GPIO **************************************************/

void init_GPIO(void)
{
	// OSPEEDR - 2 MHz by default after reset
	
	// RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;	// enable GPIOA
	// GPIOA->MODER |= GPIO_MODER_MODE5_0;	// PA5 for output - Nucleo onboard LED
	// GPIOA->MODER &= ~GPIO_MODER_MODE5_1;	// push-pull mode is by default
	
	
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;	// enable GPIOB
	
	GPIOB->MODER &= ~GPIO_MODER_MODE13;			// PB13 for input 
	GPIOB->MODER &= ~GPIO_MODER_MODE14;			// PB14 for input
	GPIOB->MODER &= ~GPIO_MODER_MODE15;			// PB15 for input
	
	GPIOB->PUPDR |= GPIO_PUPDR_PUPD13_0;			// pull-up for PB13 - [01]
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD13_1;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPD14_0;			// pull-up for PB14 - [01]
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD14_1;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPD15_0;			// pull-up for PB15 - [01]
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD15_1;
}

/**********************************************************************************************************/

/************************************************** EXTI **************************************************/

void btn_irq_init(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;	// SYSCFG (System configuration controller) clock through APB2 bus enable
	
	SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PB;	// interrupt source selection, PB13 -> EXTI13
	SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI14_PB;	// interrupt source selection, PB14 -> EXTI14
	SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI15_PB;	// PB15 -> EXTI15
	
	EXTI->IMR |= (EXTI_IMR_IM13 | EXTI_IMR_IM14 | EXTI_IMR_IM15);	// interrupt request mask - IM11 and IM12 are not masked now
	EXTI->FTSR |= (EXTI_FTSR_TR13 | EXTI_FTSR_TR14 | EXTI_FTSR_TR15);	// falling trigger
	
	NVIC_EnableIRQ(EXTI15_10_IRQn);
	NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
	NVIC_SetPriority(EXTI15_10_IRQn, 0);	// highest priority
	
	__enable_irq();	// enable interrupts, PRIMASK reset
}

void EXTI15_10_IRQHandler(void)
{
	EXTI->IMR &= ~EXTI_IMR_IM13;	// disable EXTI13 interrupts
	EXTI->IMR &= ~EXTI_IMR_IM14;	// disable EXTI14 interrupts
	EXTI->IMR &= ~EXTI_IMR_IM15;	// disable EXTI15 interrupts
	
	systick_debounce_ms(500);	// start debouncing 
	
	if(EXTI->PR & EXTI_PR_PR13)	// PB13 - in_X
	{
		in_X++;
						
		EXTI->PR |= EXTI_PR_PR13;	// clear pending flag by writing 1 (clear event flag after work)
	}
	else if(EXTI->PR & EXTI_PR_PR14)	// PB14 - in_Y
	{
		in_Y++;

		EXTI->PR |= EXTI_PR_PR14;	// clear pending flag by writing 1 	
	}
	else if(EXTI->PR & EXTI_PR_PR15)	// PB15 - MODE/RST
	{
		btn_pressure_check();		// save PB15 IDR value --> start TIM2 10ms counting for 3 seconds
		
		in_mode = (in_mode <= 2) ? (in_mode + 1) : 0;
		
		EXTI->PR |= EXTI_PR_PR15;	// clear pending flag by writing 1
	}
	
	LCD_show_ready = 1;
}

/*********************************************************************************************************/

/************************************************** ASM **************************************************/

__asm uint32_t arithmetic_function(uint8_t X, uint8_t Y, uint8_t mode) 
{
	// embedded assembly function	
	
	// X - RO, Y - R1, mode - R2
	
	// mode == 0 --> Z = (A + B) 
	// mode == 1 --> Z = (A - B) 
	// mode == 2 --> Z = (A * B) 
	// mode == 3 --> Z = (A / B) 
	
	CMP	R2, #0		// mode == 1?
	BEQ	add_label 
	
	CMP	R2, #1		// mode == 2?
	BEQ	sub_label
	
	CMP	R2, #2		// mode == 3?
	BEQ	mul_label
	
	CMP	R2, #3		// mode == 4?
	BEQ	div_label
	
add_label	
	ADD	R0, R0, R1	// R0 = R0 + R1 = X + Y
	BAL	end_label
	
sub_label
	SUB	R0, R0, R1	// R0 = R0 - R1 = X - Y
	BAL	end_label
	
mul_label
	MUL	R0, R0, R1	// R0 = R0 * R1 = X * Y
	BAL	end_label
	
div_label
	UDIV	R0, R0, R1	// R0 = R0 / R1 = X / Y
	BAL	end_label
	
end_label
	BX 	LR
}

/***************************************************************************************************************/

/************************************************** main loop **************************************************/

int main(void)
{	
	init_GPIO();
	btn_irq_init();
	init_systick();
	tim2_init();
	
	I2C1_init();
	LCD1602_init();	

	while(1)
	{		
		if(LCD_show_ready)
		{
			arith_display(in_X, in_Y, in_mode);
			
			LCD_show_ready = 0;
		}
		else
		{
			__NOP;
		}
	}
}


