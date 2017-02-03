/*
 * tiny_ticks.h
 *
 * Some very basic timing functions using Timer/Counter 0.
 * Also includes a simple event handling mechanism.
 *
 *  License: MIT
 *  Version: 1.0
 */ 

#ifndef __TINY_TICKS_H__
#define __TINY_TICKS_H__

#include <avr/io.h>
#include <avr/interrupt.h>

// default to 8 MHz timing.
// this really should be defined on the command line.
#ifndef F_CPU
#define F_CPU	(8000000L)
#endif

// define TINY_TICKS_EVENT_ENABLE as (0) to disable events
#ifndef TINY_TICKS_EVENT_ENABLE
#define TINY_TICKS_EVENT_ENABLE		(1)
#endif

// how big do we want the tick counter to be (16 or 32 bit)?
typedef uint16_t tick_t;

// select the appropriate register to check for overflows
// this allows the code to work with either the ATtiny84 or ATtiny85
#if defined(TIFR0)
#define MYTIFR	TIFR0
#elif defined(TIFR)
#define MYTIFR	TIFR
#else
#error The TIFR constant is not defined.
#endif

#if F_CPU==16000000L
	// 4 microseconds per tick
	#define TINY_MICROS_PER_TICK				(4)
	#define TINY_TICKS_FROM_MICROS(MICROS_VAL)	((MICROS_VAL) >> 2)
	#define TINY_GET_MICROS()					(tinyTicks() << 2)
#elif F_CPU==8000000L
	// 8 microseconds per tick
	#define TINY_MICROS_PER_TICK				(8)
	#define TINY_TICKS_FROM_MICROS(MICROS_VAL)	((MICROS_VAL) >> 3)
	#define TINY_GET_MICROS()					(tinyTicks() << 3)
#elif F_CPU==1000000L
	// 64 microseconds per tick
	#define TINY_MICROS_PER_TICK				(64)
	#define TINY_TICKS_FROM_MICROS(MICROS_VAL)	((MICROS_VAL) >> 6)
	#define TINY_GET_MICROS()					(tinyTicks() << 6)
#else
	// variable microseconds per tick
	#define TINY_MICROS_PER_TICK				(64 / (F_CPU / 1000000L))
	#define TINY_TICKS_FROM_MICROS(MICROS_VAL)	((MICROS_VAL) / (TINY_MICROS_PER_TICK))
	#define TINY_GET_MICROS()					(tinyTicks() * (TINY_MICROS_PER_TICK))
#endif

// overflow counter for timer/counter 0.
volatile extern tick_t _t0_overflow;

// initializes the timer and event system.
void tinyTicksInit();

#if TINY_TICKS_EVENT_ENABLE

// the signature for event procedures.
// event procedures should not have blocking waits and should return quickly.
typedef void (*tinyTicksEventProc)();

// the maximum number of events that can be queued for execution at one time.
#define TINY_TICKS_EVENT_QUEUE_MAX			(8)

// registers an event to run in the specified number of microseconds.
// the event will run when *at least* the specified timeout value has elapsed.
// returns 0 if the queue is full.
uint8_t tinyTicksSetTimeoutInTicks(tinyTicksEventProc proc, uint32_t tickTimeout);
#define tinyTicksSetTimeout(proc, usTimeout)			tinyTicksSetTimeoutInTicks((proc), (TINY_TICKS_FROM_MICROS((usTimeout))))
#define tinyTicksSetTimeoutInMillis(proc, msTimeout)	tinyTicksSetTimeoutInTicks((proc), (TINY_TICKS_FROM_MICROS((msTimeout) * 1000L)))

// gets the number of events currently waiting for execution.
uint8_t tinyTicksEventQueueDepth();

// removes all currently registered events from the event queue.
void tinyTicksClearEventQueue();

#endif

// a callback function called for every loop iteration.
extern void (*tinyTicksEventLoopCallback)(tick_t diff);

// process the event queue for any expired timeouts and execute the events.
void tinyTicksEventLoop();

// block for until the specified timeout has elapsed.
// this can be used without the event system, but it will process events as it waits.
void tinyTicksDelayInTicks(uint32_t tickTimeout);
#define tinyTicksDelay(usTimeout)			tinyTicksDelayInTicks((TINY_TICKS_FROM_MICROS((usTimeout))))
#define tinyTicksDelayInMillis(msTimeout)	tinyTicksDelayInTicks((TINY_TICKS_FROM_MICROS((msTimeout) * 1000L)))

// gets the current number of ticks
inline tick_t tinyTicks()
{
	tick_t ret;
	uint8_t tmr;
	uint8_t flags;
	uint8_t oldSREG;
	
	// disable interrupts briefly
	oldSREG = SREG;
	asm volatile ("cli"::);
	
	// grab the volatile information.
	ret = _t0_overflow;
	tmr = TCNT0;
	flags = MYTIFR;
	
	// enable interrupts
	SREG = oldSREG;
	
	// if the overflow flag is currently set...
	if (flags & (1<<TOV0))
	{
		// and the timer count is not MAX...
		if (tmr < 255)
		{
			// add 256 to the overflow counter (just like the interrupt will do).
			ret += 256;
		}
	}
	
	// add the non-overflowed counter value.
	return ret + tmr;
}

#endif /* __TINY_TICKS_H__ */
