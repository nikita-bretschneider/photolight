/******************************************
 * Camera Light / ATTINY13
 * 2020 Nikita Lieselotte Bretschneider
 * ( nikita.bretschneider@gmx.de )
 ******************************************/


#define MCU attiny13 
#define F_CPU 1200000UL
#define __AVR_ATtiny13__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*************************
 * define hardware wiring
 *************************/

	#define B_OUT1			PB2	// 2k8K LED chain
	#define B_OUT2			PB1	// 3k2K LED chain
	#define B_OUT3			PB0	// 6k6K LED chain
	#define P_OUT			PORTB // All outputs are on  port B 
	#define P_OUT_setup		DDRB // ---""---, define setup port

	#define P_KEYIN			PINB // Keyboard input port
	#define P_KEYIN_setup	DDRB // Keyboard input port settings
	#define P_KEYIN_output	PORTB // Keyboard input port output/pullup register
	#define B_KEYIN_UPDWN	PB3 // UP/DOWN keys (pull up/pull down)
	#define B_KEYIN_TEMP	PB4 // Color temperature switch, Hi..6k6, Lo..2k8, HiZ 3k2 (maybe changed in future)

/*********************************************
 * define global variables, bits and constants
 *********************************************/
	volatile unsigned char phase = 0; // Contains actual phase
	
	static unsigned char chain1_on_phase = 0;	// Contains phase of rising edge chain1 signal
	static unsigned char chain1_off_phase = 0; // Contains phase of falling edge of chain 1 signal
	static unsigned char chain2_on_phase = 0;	// dtto but chain 2
	static unsigned char chain2_off_phase = 0; //
	static unsigned char chain3_on_phase = 0;	// dtto bud chain 3
	static unsigned char chain3_off_phase = 0;

	static unsigned char keyboard_reading_trigger_phase = 8;	// Phase of reading keyboard 

	volatile unsigned char P_keyboard = 0;	// Virtual keyboard input port, output of the keyboard routine
	#define B_keyboard_UP			1	//and keys bit definitions
	#define B_keyboard_DOWN			2
	#define B_keyboard_TEMP_Hi		3
	#define B_keyboard_TEMP_Lo		4
	#define B_keyboard_isr_running	6
	#define LOOP_LIMITER			0x1F // Constant defining phase loop length, must be #0..01..1
	
	/* Every rise/edge is located almost symetrically around initial phase.
	   The goal is to achieve situation, when the power consumption
	   is distributed to the whole LOOP cycle */
	
	
	static unsigned char chain1_value = 4; // Chain1 value and initial value
	static unsigned char chain2_value = 4; // dtto but chain2
	static unsigned char chain3_value = 4; // dtto but chain3

	static unsigned char chain1_isr_on_phase = 0xFF; // Internal ISR copy of on_phase, needed by atomic loading
	static unsigned char chain1_isr_off_phase = 0; //Internal ISR copy of off_phase, needed by atomic loading
	static unsigned char chain2_isr_on_phase = 0xFF; // dtto but chain 2
	static unsigned char chain2_isr_off_phase = 0; // dtto but chain 2
	static unsigned char chain3_isr_on_phase = 0xFF; // dtto but chain 3
	static unsigned char chain3_isr_off_phase = 0; // dtto but chain 3
	
	volatile unsigned char P_state = 0x00; // Virtual handshaking port between ISR and main loop and inside main loop
	#define B_state_load_strobe		1	// Tells the ISR to load data, cleared when done by ISR
	#define B_keyboard_load_enable	2	// Tells the ISR to run keyboard load routine at propper time, cleared by ISR
	#define B_running_ISR	3	// Indicates that we are running ISR task
	#define B_state_keyboard_last_UP	4	// Last state of keyboard was UP
	#define B_state_keyboard_last_DOWN	5	// Last state of keyboard was DOWN
	#define B_state_data_is_dirty		6	// Data needs recomputing, cleared by recomputing routine, which sets state_load_strobe, set by UI triggered by keyboard

void keyboard_ui_routine( void)
{
	if ( bit_is_set( P_keyboard,B_keyboard_UP))
	{
		if ( bit_is_set( P_state, B_state_keyboard_last_UP))
		{
			P_state |= _BV( B_keyboard_load_enable); 
			return; // UP is pressed and was pressed, nothing changed, do noting
		}
		P_state |= _BV( B_state_keyboard_last_UP); // Set Last_UP because it was not
		P_state &= ~_BV( B_state_keyboard_last_DOWN); // And unset Last_DOWN, because if we are up, then we are not down
		if ( bit_is_set( P_keyboard, B_keyboard_TEMP_Hi))
		{
			chain3_value++;
			chain3_value &= LOOP_LIMITER;
		}
		else
		{
			if ( bit_is_set( P_keyboard,B_keyboard_TEMP_Lo))
			{
				chain1_value++;
				chain1_value &= LOOP_LIMITER;
			}
			else
			{
				chain2_value++;
				chain2_value &= LOOP_LIMITER;
			}
			
		}
		P_state |= (_BV( B_state_data_is_dirty) | _BV( B_keyboard_load_enable)); // We changed some value in chain parameters, recomputing needed. And kick the keyboard routine
		return; // If UP then not DOWN, respawn faster
	}

	if ( bit_is_set( P_keyboard, B_keyboard_DOWN))
	{
		//and do the exactly same for DOWN key
		if( bit_is_set( P_state, B_state_keyboard_last_DOWN))
		{
			P_state |= _BV( B_keyboard_load_enable);
			return; // DOWN is pressed and was pressed, nothing to do
		}
		P_state |= _BV( B_state_keyboard_last_DOWN);
		P_state &= ~_BV( B_state_keyboard_last_UP);
		if (bit_is_set( P_keyboard, B_keyboard_TEMP_Hi))
		{
			chain3_value--;
			chain3_value &= LOOP_LIMITER;
		}
		else
		{
			if( bit_is_set( P_keyboard, B_keyboard_TEMP_Lo))
			{
				chain1_value--;
				chain3_value &= LOOP_LIMITER;
			}
			else
			{
				chain2_value--;
				chain2_value &= LOOP_LIMITER;
			}
			
		}
		P_state |= (_BV( B_state_data_is_dirty) | _BV( B_keyboard_load_enable));
		return;
	}
	
	// If we are here, then UP nor DOWN was pressed, 
	P_state &= ~( _BV( B_state_keyboard_last_UP) | _BV( B_state_keyboard_last_DOWN));
	P_state |= _BV( B_keyboard_load_enable);
}

void data_recompute_routine( void)
{
	chain1_on_phase = 0;
	chain1_off_phase = chain1_value;
	if (LOOP_LIMITER > chain1_value + chain2_value)
	{
		chain2_on_phase = chain1_off_phase+1;
		chain2_off_phase = chain2_on_phase + chain2_value;
		chain3_on_phase = chain2_off_phase + 1;
		chain3_off_phase = chain3_on_phase + chain3_value;
		return;
	}
	chain3_on_phase = chain1_off_phase +1;
	chain3_off_phase = chain3_on_phase + chain3_value;
	chain2_on_phase = chain3_off_phase +1;
	chain2_off_phase = chain2_on_phase + chain2_value;
}

void main_loop_call( void)
{
	if( bit_is_clear(P_state, B_keyboard_load_enable))
	{
		keyboard_ui_routine();
		P_state |= _BV( B_keyboard_load_enable);
	}
	if( bit_is_set( P_state, B_state_data_is_dirty)) 
	{
		data_recompute_routine();
		chain1_on_phase &= LOOP_LIMITER;
		chain2_on_phase &= LOOP_LIMITER;
		chain3_on_phase &= LOOP_LIMITER;
		chain1_off_phase &= LOOP_LIMITER;
		chain2_off_phase &= LOOP_LIMITER;
		chain3_off_phase &= LOOP_LIMITER;
		P_state |= _BV( B_state_load_strobe);
		P_state &= ~_BV( B_state_data_is_dirty);
	}
}


int main( void)
{
	P_OUT_setup |= _BV( B_OUT1)|_BV( B_OUT2)|_BV( B_OUT3);	// Set Chain 1,2,3 as outputs
	
	TCCR0B |= (1<<CS00);	
	TCCR0B &= ~((1<<CS01) | (1<<CS02));	// Setup timer divider registers
	
	TIMSK0 |= (1<<TOIE0);	// Setup timer IRQs
	sei();	// Enable IRQs to swap to background task

	P_state |= _BV( B_keyboard_load_enable) | _BV( B_state_load_strobe);
	
	while( 1)	// Main loop
		{
			main_loop_call();
			//tests();
		}
}

ISR( TIM0_OVF_vect)		// Timer IRQ vector
{
	if bit_is_set( P_state, B_running_ISR) return; //exit if previous ISR routine is unfinished, preventing stack overflow due some erratic IRQs
	P_state |= B_running_ISR;
	
	phase++; 		// increase counter
	phase &= LOOP_LIMITER;	// and limit maximum

	if ( bit_is_set( P_state, B_state_load_strobe))
	{
		// we are inside ISR, so we are atomic as a kitten
		chain1_isr_off_phase = chain1_off_phase & LOOP_LIMITER;
		chain2_isr_off_phase = chain2_off_phase & LOOP_LIMITER;
		chain3_isr_off_phase = chain3_off_phase & LOOP_LIMITER;

		chain1_isr_on_phase = chain1_on_phase & LOOP_LIMITER;
		chain2_isr_on_phase = chain2_on_phase & LOOP_LIMITER;
		chain3_isr_on_phase = chain3_on_phase & LOOP_LIMITER;

		P_state &= ~_BV( B_state_load_strobe);

	}

	// Chain power switching tasks
	if ( phase == chain1_isr_off_phase) P_OUT &= ~_BV( B_OUT1);
	if ( phase == chain2_isr_off_phase) P_OUT &= ~_BV( B_OUT2);
	if ( phase == chain3_isr_off_phase) P_OUT &= ~_BV( B_OUT3);

	if ( phase == chain1_isr_on_phase) P_OUT |= _BV( B_OUT1);
	if ( phase == chain2_isr_on_phase) P_OUT |= _BV( B_OUT2);
	if ( phase == chain3_isr_on_phase) P_OUT |= _BV( B_OUT3);

	if ( (LOOP_LIMITER >> 2) == phase)
	{
		if ( bit_is_set( P_state, B_keyboard_load_enable))
		{
			P_keyboard = _BV( B_keyboard_isr_running); // reset all keyboard bits and tell ISR we are really reading inputs
			P_KEYIN_output &= ~( _BV( B_KEYIN_UPDWN) | _BV( B_KEYIN_TEMP)); // disable pull-up
			// and return from this part of subroutine, we have to wait to pull-up discharges
		}
		
	}

	if ( (LOOP_LIMITER >> 1) == phase )
	{
		if ( bit_is_set( P_keyboard, B_keyboard_isr_running))
		{
			if ( P_KEYIN & _BV( B_KEYIN_TEMP)) P_keyboard |= _BV( B_keyboard_TEMP_Hi); // is Hi without pull-up
			if ( P_KEYIN & _BV( B_KEYIN_UPDWN)) P_keyboard |= _BV( B_keyboard_UP); // is Hi without pull-up
			P_KEYIN_output |= ( _BV( B_KEYIN_UPDWN) | _BV( B_KEYIN_TEMP)); // enable pull-up
			if ( bit_is_set( P_keyboard, B_keyboard_UP) && bit_is_set( P_keyboard, B_keyboard_TEMP_Hi))
			{
				// in this case we have UP/DOWN and TEMP in Hi position, so we are done
				P_keyboard &= ~_BV( B_keyboard_isr_running); // We are done
				P_state &= ~_BV( B_keyboard_load_enable); // tell the main loop that the keyboard was read
			}
		}
		
	}

	if (((LOOP_LIMITER >> 1) + (LOOP_LIMITER >> 2)) == phase)
	{
		if ( bit_is_set( P_keyboard, B_keyboard_isr_running))
		{
			if ( bit_is_clear( P_KEYIN, B_KEYIN_UPDWN)) P_keyboard |= _BV( B_keyboard_DOWN); // is Lo with pull-up
			if ( bit_is_clear( P_KEYIN, B_KEYIN_TEMP)) P_keyboard |= _BV( B_keyboard_TEMP_Lo); // is Lo with pull-up
			P_KEYIN_output &= ~( _BV( B_KEYIN_UPDWN) | _BV( B_KEYIN_TEMP)); // disable pull-up
			P_keyboard &= ~_BV( B_keyboard_isr_running); // We are done
			P_state &= ~_BV( B_keyboard_load_enable); // tell the main loop that the keyboard was read
		}
	}
	
	P_state &= ~( _BV( B_running_ISR)); // clear the ISR running status
}
