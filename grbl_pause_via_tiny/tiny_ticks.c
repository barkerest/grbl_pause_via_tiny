/*
 * tiny_ticks.c
 *
 * Some very basic timing functions using Timer/Counter 0.
 * Also includes a simple event handling mechanism.
 *
 *  License: MIT
 *  Version: 1.0
 */ 

#include "tiny_ticks.h"

#if TINY_TICKS_EVENT_ENABLE

typedef struct tinyTicksEvent_t {
	tinyTicksEventProc proc;
	uint32_t timeout;
} tinyTicksEvent_t;

static tinyTicksEvent_t tinyEventQueue[TINY_TICKS_EVENT_QUEUE_MAX];

#endif

void (*tinyTicksEventLoopCallback)(tick_t) = 0;
volatile tick_t _t0_overflow;
static tick_t curTick;
static tick_t lastTick;

#if defined(TIMSK0)
#define MYTIMSK TIMSK0
#elif defined(TIMSK)
#define MYTIMSK TIMSK
#else
#error The TIMSK constant is not defined.
#endif


void tinyTicksInit()
{
#if TINY_TICKS_EVENT_ENABLE
	uint8_t i;
#endif
	
	// setup timer0 with a prescalar of 64 and enable overflow interrupt for timer 0.
	TCCR0B |= (1 << CS01) | (1 << CS00);
	MYTIMSK |= (1 << TOIE0);
	
	// clear the overflow counter.
	_t0_overflow = 0;
	
#if TINY_TICKS_EVENT_ENABLE
	
	// clear the event queue.
	for (i = 0; i < TINY_TICKS_EVENT_QUEUE_MAX; i++)
	{
		tinyEventQueue[i].proc = 0;
		tinyEventQueue[i].timeout = 0;
	}
	
#endif
	
	// clear the tick counter
	lastTick = 0;
	curTick = 0;
}

// Timer 0 Overflow Interrupt Handler
// Simply increments the overflow value.
// Note that the overflow value will overflow about every 4,000 seconds.
// Since we are looking for timing and not uptime, we'll leave that overflow alone.
ISR(TIM0_OVF_vect)
{
	_t0_overflow = _t0_overflow + 256;
}

#if TINY_TICKS_EVENT_ENABLE

// registers an event to fire after tickTimeout ticks have elapsed.
uint8_t tinyTicksSetTimeoutInTicks(tinyTicksEventProc proc, uint32_t tickTimeout)
{
	uint8_t i;
	
	for (i = 0; i < TINY_TICKS_EVENT_QUEUE_MAX; i++)
	{
		if (!tinyEventQueue[i].proc || tinyEventQueue[i].proc == proc)
		{
			// we have an empty slot, or the current slot is for the same proc.
			tinyEventQueue[i].proc = proc;
			tinyEventQueue[i].timeout = tickTimeout;
			return 1;
		}
	}
	
	return 0;
}

// gets the number of events currently waiting for execution.
uint8_t tinyTicksEventQueueDepth()
{
	uint8_t i;
	uint8_t ret = 0;
	
	for (i = 0; i < TINY_TICKS_EVENT_QUEUE_MAX; i++)
	{
		if (tinyEventQueue[i].proc) ret++;
	}
	
	return ret;
}

// removes all currently registered events from the event queue.
void tinyTicksClearEventQueue()
{
	uint8_t i;
	
	// clear the event queue.
	for (i = 0; i < TINY_TICKS_EVENT_QUEUE_MAX; i++)
	{
		tinyEventQueue[i].proc = 0;
		tinyEventQueue[i].timeout = 0;
	}
}

#endif

// process the event queue for any expired timeouts and execute the events.
void tinyTicksEventLoop()
{
#if TINY_TICKS_EVENT_ENABLE
	uint8_t i;
	tinyTicksEventProc proc;
#endif
	tick_t diff;
	
	curTick = tinyTicks();
	if (curTick != lastTick)
	{

		// figure out how many ticks have elapsed.
		if (curTick > lastTick)
		{
			diff = curTick - lastTick;
		}
		else
		{
			diff = (((tick_t)0xFFFFFFFF) - lastTick) + curTick;
		}
		
		// log the tick reading from this loop.
		lastTick = curTick;
		
		// run the callback if set.
		if (tinyTicksEventLoopCallback)
		{
			(tinyTicksEventLoopCallback)(diff);
		}
		
#if TINY_TICKS_EVENT_ENABLE

		// process the event queue.
		for (i = 0; i < TINY_TICKS_EVENT_QUEUE_MAX; i++)
		{
			// if there is an event registered.
			if (tinyEventQueue[i].proc)
			{
				// and the timeout has elapsed.
				if (tinyEventQueue[i].timeout <= diff)
				{
					// grab the function pointer.
					proc = tinyEventQueue[i].proc;
					
					// clear the event from the queue (this allows tinyTicksEventLoop to be called within the event).
					tinyEventQueue[i].proc = 0;
					tinyEventQueue[i].timeout = 0;
					
					// execute the event.
					(proc)();
				}
				else
				{
					// otherwise decrement the timeout for this event.
					tinyEventQueue[i].timeout -= diff;
				}
			}
		}
		
#endif
	}
}

// block for at least the specified timeout.
void tinyTicksDelayInTicks(uint32_t tickTimeout)
{
	tick_t diff;
#if TINY_TICKS_EVENT_ENABLE
	tick_t localLast;
	uint8_t i;
	tinyTicksEventProc proc;
#endif
	
	while (1)
	{
		curTick = tinyTicks();
		if (curTick != lastTick)
		{
			// figure out how many ticks have elapsed.
			if (curTick > lastTick)
			{
				diff = curTick - lastTick;
			}
			else
			{
				diff = (((tick_t)0xFFFFFFFF) - lastTick) + curTick;
			}
	
			// log the tick reading from this loop.
			lastTick = curTick;
			
			// determine if we have timed out yet.
			if (diff >= tickTimeout)
			{
				return;
			}
			else
			{
				tickTimeout -= diff;
			}
			
			// run the callback if set.
			if (tinyTicksEventLoopCallback)
			{
				(tinyTicksEventLoopCallback)(diff);
			}
			
#if TINY_TICKS_EVENT_ENABLE
			
			// backup the last tick value.
			localLast = lastTick;
			
			// process the event queue.
			for (i = 0; i < TINY_TICKS_EVENT_QUEUE_MAX; i++)
			{
				// if there is an event registered.
				if (tinyEventQueue[i].proc)
				{
					// and the timeout has elapsed.
					if (tinyEventQueue[i].timeout <= diff)
					{
						// grab the function pointer.
						proc = tinyEventQueue[i].proc;
				
						// clear the event from the queue (this allows tinyTicksEventLoop to be called within the event).
						tinyEventQueue[i].proc = 0;
						tinyEventQueue[i].timeout = 0;
				
						// execute the event.
						(proc)();
					}
					else
					{
						// otherwise decrement the timeout for this event.
						tinyEventQueue[i].timeout -= diff;
					}
				}
			}
			
			// use the new lastTick value to determine how much time has elapsed during the event processing.
			if (lastTick != localLast)
			{
				if (lastTick > localLast)
				{
					diff = lastTick - localLast;
				}
				else
				{
					diff = (((tick_t)0xFFFFFFFF) - localLast) + lastTick;
				}
				
				// once again, determine if we have timed out.
				if (diff >= tickTimeout)
				{
					return;
				}
				else
				{
					tickTimeout -= diff;
				}
				
				// note we don't need to check the events again since lastTick only gets updated during event processing.
			}
			
#endif
		}
	}
}
