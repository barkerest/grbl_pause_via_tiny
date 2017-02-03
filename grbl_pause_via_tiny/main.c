/*
 * grbl_pause_via_tiny.c
 *
 *  Version: 1.0
 *  License: MIT
 */ 

#include <avr/io.h>
#include "tiny_ticks.h"

#define MASK_PAUSE		0x01
#define MASK_HOLD		0x02
#define MASK_RESUME		0x04
#define MASK_ABORT		0x08

#define PAUSE_TRIGGERED		((PINB & MASK_PAUSE) == 0)
#define TRIGGER_HOLD()		PORTB &= ~MASK_HOLD
#define CLEAR_HOLD()		PORTB |= MASK_HOLD
#define TRIGGER_RESUME()	PORTB &= ~MASK_RESUME
#define CLEAR_RESUME()		PORTB |= MASK_RESUME
#define TRIGGER_ABORT()		PORTB &= ~MASK_ABORT
#define CLEAR_ABORT()		PORTB |= MASK_ABORT

#if DEBUG
#define MASK_STATUS			0x10
#define ENABLE_STATUS_LED()	DDRB |= MASK_STATUS
#define TOGGLE_STATUS_LED()	PINB |= MASK_STATUS
#endif

// 100ms (0.1s)
#define HOLD_TIMEOUT	100000L

// 1000ms (1s)
#define ABORT_TIMEOUT	1000000L

// 250ms (0.25s)
#define TRIGGER_TIMEOUT	250000L


// when the button is held down for HOLD_TIMEOUT milliseconds, the HOLD pin is triggered.  hold_state is set to 1 when the button is released unless an abort is triggered.
// if the button is released before ABORT_TIMEOUT milliseconds and hold_state is 1, then the RESUME pin is triggered and hold_state is set to 0.
// if the button is held down for ABORT_TIMEOUT milliseconds, the ABORT pin is triggered. hold_state is set to 0 when the button is released.

typedef struct pause_state_t {
	uint8_t		triggered : 1;
	uint8_t		hold_state : 1;
	uint8_t		hold_complete : 1;
	uint8_t		abort_complete : 1;
	uint16_t	interruption;
	uint32_t	elapsed;
} pause_state_t;

pause_state_t pause_state = { 0, 0, 0, 0, 0, 0 };

#define RESET_PAUSE_STATE()	{ pause_state.triggered = 0; pause_state.hold_complete = 0; pause_state.abort_complete = 0; pause_state.elapsed = 0; pause_state.interruption = 0; }

void clear_hold() { CLEAR_HOLD(); }
void clear_resume() { CLEAR_RESUME(); }
void clear_abort() { CLEAR_ABORT(); }


void check_pause_state(tick_t ticks)
{
	if (pause_state.triggered)
	{
		ticks = ticks * TINY_MICROS_PER_TICK;
		
		// pause button is currently being held down.
		if (PAUSE_TRIGGERED)
		{
			// remove interruption and continue accumulating.
			pause_state.interruption = 0;
			pause_state.elapsed += ticks;
		}
		else
		{	// pause button was recently held down but appears not to be held down right now.
			pause_state.interruption += ticks;
			// if more than 1ms has passed since the pause button was definitely last triggered...
			if (pause_state.interruption >= 1000)
			{
				// if an abort occurred, reset the hold state.
				if (pause_state.abort_complete)
				{
					pause_state.hold_state = 0;
				}
				else if (pause_state.hold_complete)
				{
					// if a hold state was processed and was previously processed since the last abort/resume, trigger a resume.
					if (pause_state.hold_state == 1)
					{
						pause_state.hold_state = 0;
						CLEAR_HOLD();
						TRIGGER_RESUME();
						tinyTicksSetTimeout(clear_resume, TRIGGER_TIMEOUT);
					}
					else
					{
						// otherwise queue up a resume if no abort occurs.
						pause_state.hold_state = 1;
					}
				}
				
				// reset the pause state.
				RESET_PAUSE_STATE();
			}
			else
			{
				// otherwise continue accumulating.
				pause_state.elapsed += ticks;
			}
		}
		
		if (pause_state.elapsed > HOLD_TIMEOUT && pause_state.hold_complete == 0)
		{
			TRIGGER_HOLD();
			tinyTicksSetTimeout(clear_hold, TRIGGER_TIMEOUT);
			
			// mark the action as complete.
			pause_state.hold_complete = 1;
		}
		
		if (pause_state.elapsed > ABORT_TIMEOUT && pause_state.abort_complete == 0)
		{
			TRIGGER_ABORT();
			tinyTicksSetTimeout(clear_abort, TRIGGER_TIMEOUT);
			
			// mark the abort as complete and reset the next action to hold.
			pause_state.abort_complete = 1;
		}
	}
	else
	{
		if (PAUSE_TRIGGERED)
		{
			// begin timing the pause button.
			RESET_PAUSE_STATE();
			pause_state.triggered = 1;
		}
	}
}

#if DEBUG
void toggle_status_led()
{
	TOGGLE_STATUS_LED();
	tinyTicksSetTimeoutInMillis(toggle_status_led, 1000);
}
#endif

int main(void)
{
	// setup timing.
	tinyTicksInit();
	tinyTicksEventLoopCallback = check_pause_state;
	
	// enable outputs.
	DDRB |= (MASK_HOLD | MASK_RESUME | MASK_ABORT);
	// set all pins high (pause uses input pullup resistor)
	PORTB |= (MASK_PAUSE | MASK_HOLD | MASK_RESUME | MASK_ABORT);
	
	// enable interrupts.
	sei();
	
	#if DEBUG
	ENABLE_STATUS_LED();
	toggle_status_led();
	#endif
	
	// loop
    while (1)
	{
		// all the magic happens in the callbacks.
		tinyTicksEventLoop();
	}
}

