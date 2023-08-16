#include "AppEvent.h"
#include "SerialConsole.h"
#include "timer.h"

#include "gpio.h"

#include "assertion.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "AppDebugConfig.h"

#define LOG_LEVEL        DEBUG_APP_EVENT

#define MAX_EVENTS          32

#if 1
/* TO INVESTIGATE:
 * For some unidentified reason, the timer event can be missed if the timeout is set too low.
 * This was reproducible when we configured a 1 to 5ms timer, then printed a debug message just entering LpmEnterLowPower().
 * Tracing the code showed that HAL_RTC_SetAlarm_IT() to setup the timer, but TimerIrqHandler() never fires once the
 * system is in any of the sleep modes.  Unfortunately, we were unable to determine the root cause.
 * Recommended practice is to use timer values under EVENT_TIMEOUT_MIN for timers defined in the MAIN context. */
static const uint32_t EVENT_TIMEOUT_MIN = 6;  /* ms */
#endif

typedef enum
{
    EVENT_TYPE_GENERAL,
    EVENT_TYPE_TIMER,
    EVENT_TYPE_INTERRUPT
} EVENT_TYPES;

typedef struct
{
    bool debugEnable;
    TimerTime_t startTime;
    uint16_t triggerCount;
    uint16_t processCount;
} EventDiagnostics_t;

typedef struct
{
    /* General fields */
    const char* name;
    APP_EVENT_CONTEXTS context;
    AppEvent_Callback callback;
    volatile bool triggered;
    volatile bool paused;
    bool single;
    uint8_t id;
    EVENT_TYPES type;

    /* Timer event fields */
    TimerEvent_t timer;
    uint32_t timeout;
    uint32_t timeoutRandomizer;

    /* Interrupt event fields */
    Gpio_t* irqGpio;
    IrqModes irqMode;
    IrqPriorities irqPriority;

    EventDiagnostics_t diagnostics;
} AppEvent_t;

static uint8_t m_numEvents;
static AppEvent_t m_events[MAX_EVENTS];
static bool m_initialized = false;

static void OnEvent( void* context );
static void OnInterruptEvent( void* context );

static const char* GetEventName(const AppEvent_t* const event)
{
    static char genericName[16];

    if (event->name != NULL)
    {
        return event->name;
    }

    sprintf(genericName, "EVENT%u", event->id);
    return genericName;
}

static void StartEvent(AppEvent_t* const event)
{
    if (event->irqMode == NO_IRQ)
    {
        if (event->diagnostics.debugEnable)
        {
            DEBUG_PRINTF("Event %s start (%lums, %s)\r\n", GetEventName(event), event->timeout,
                         event->single ? "single" : "continuous");

            if (event->timeout < EVENT_TIMEOUT_MIN)
            {
                PRINTF("#WARN: %s timeout set below minimum recommended threshold of %lu ms\r\n", event->name, EVENT_TIMEOUT_MIN );
                PRINTF("#WARN: There is evidence timer may fail to trigger with extensive debug logging under this value.\r\n");
            }
        }
        if (event->timeoutRandomizer > 0)
        {
            const uint16_t randomOffset = (uint16_t)( ( rand() / (double)RAND_MAX ) * event->timeoutRandomizer );
            TimerSetValue( &event->timer, event->timeout + randomOffset );
        }
        TimerStartWithParam( &event->timer, event );
    }
    else
    {
        GpioSetInterrupt( event->irqGpio, event->irqMode, event->irqPriority, &OnInterruptEvent );
        GpioMcuSetContext( event->irqGpio, (uint8_t*)&event->irqGpio->intNo );
        if (event->diagnostics.debugEnable)
        {
            DEBUG_PRINTF("Event %s activated (irq %u)\r\n", GetEventName(event), event->irqGpio->intNo);
        }
    }
}



static void DoTrigger( AppEvent_t* event )
{
    switch (event->context)
    {
    case APP_EVENT_CONTEXT_MAIN:
        event->triggered = true;
        ++event->diagnostics.triggerCount;
        break;

    case APP_EVENT_CONTEXT_IMMEDIATE:
        ++event->diagnostics.triggerCount;
        ++event->diagnostics.processCount;
        if (event->callback != NULL)
        {
            (*(event->callback))();
        }
        break;
    }
}

/* CONTEXT: Executes in the RTCC timer interrupt context with interrupts disabled */
static void OnEvent( void *context )
{
    AppEvent_t* event = (AppEvent_t*)context;

    DoTrigger(event);

    if (!event->single)
    {
        StartEvent(event);
    }
}
/* CONTEXT: Executes in the interrupt context */
static void OnInterruptEvent( void* context )
{
    uint8_t intNo = *((uint8_t*)context);

    for (uint8_t i = 0; i < m_numEvents; ++i)
    {
        AppEvent_t* event = &m_events[i];
        if (event->irqMode != NO_IRQ && event->irqGpio->intNo == intNo)
        {
            DoTrigger(event);

            if (event->single)
            {
                GpioRemoveInterrupt( event->irqGpio );
            }

            break;;
        }
    }
}

void AppEvent_Trigger( uint8_t id )
{
    for (uint8_t i = 0; i < m_numEvents; ++i)
    {
        AppEvent_t* event = &m_events[i];
        if (event->id == id)
        {
            DoTrigger(event);
        }
    }
}

void AppEvent_Init(void)
{
    if (m_initialized)
        return;

    m_numEvents = 0;
    m_initialized = true;
}

AppEvent_t* CreateEvent(const char* const name, AppEvent_Callback callback, APP_EVENT_CONTEXTS context)
{
    uint8_t id = m_numEvents++;
    AppEvent_t* event = &m_events[id];
    event->id = id;
    event->triggered = false;
    event->context = context;
    event->callback = callback;
    event->irqMode = NO_IRQ;
    event->diagnostics.debugEnable = true;
    event->diagnostics.triggerCount = 0;
    event->diagnostics.processCount = 0;
    event->type = EVENT_TYPE_GENERAL;
    event->paused = false;
    if (name != NULL)
    {
        event->name = name;
    }

    return event;
}

void AppEvent_RegisterEvent(uint8_t* const eventId, const char* const name, AppEvent_Callback callback,
                            APP_EVENT_CONTEXTS context)
{
    ASSERT(m_initialized);
    ASSERT(m_numEvents < MAX_EVENTS);
    ASSERT(eventId != NULL);
    ASSERT(callback != NULL);

    AppEvent_t* event = CreateEvent(name, callback, context);

    DEBUG_PRINTF("Event %s[%u] registered (general)\r\n", GetEventName(event), event->id);

    *eventId = event->id;

}

void AppEvent_RegisterTimerWithRandomizer(uint8_t* const eventId, const char* const name, AppEvent_Callback callback,
                                          uint32_t timeout, uint32_t timeoutRandomizer, APP_EVENT_CONTEXTS context)
{
    ASSERT(m_initialized);
    ASSERT(m_numEvents < MAX_EVENTS);
    ASSERT(eventId != NULL);
    ASSERT(callback != NULL);

    AppEvent_t* event = CreateEvent(name, callback, context);
    TimerInit( &event->timer, OnEvent );
    TimerSetValue( &event->timer, timeout );
    event->timeout = timeout;
    event->timeoutRandomizer = timeoutRandomizer;
    event->type = EVENT_TYPE_TIMER;

    DEBUG_PRINTF("Event %s[%u] registered (timer)\r\n", GetEventName(event), event->id);

    *eventId = event->id;
}

void AppEvent_RegisterTimer(uint8_t* const id, const char* const name, AppEvent_Callback callback,
                            uint32_t timeout, APP_EVENT_CONTEXTS context)
{
    ASSERT(m_initialized);
    AppEvent_RegisterTimerWithRandomizer(id, name, callback, timeout, 0, context);
}

void AppEvent_RegisterInterrupt( uint8_t* const eventId, const char* const name, AppEvent_Callback callback,
                                 Gpio_t* gpio, IrqModes irqMode, IrqPriorities irqPriority, APP_EVENT_CONTEXTS context)
{
    ASSERT(m_initialized);
    ASSERT(m_numEvents < MAX_EVENTS);
    ASSERT(eventId != NULL);
    ASSERT(callback != NULL);

    AppEvent_t* event = CreateEvent(name, callback, context);
    event->irqGpio = gpio;
    event->irqMode = irqMode;
    event->irqPriority = irqPriority;
    event->type = EVENT_TYPE_INTERRUPT;

    DEBUG_PRINTF("Event %s[%u] registered (interrupt)\r\n", GetEventName(event), event->id);

    *eventId = event->id;
}

/* In the current simple implementation, we cannot unregister an event. */
/* More complex work with identifying the events would need to be done in order to */
/* be able to support unregistering events. */
#if 0
void AppEvent_Unregister(uint8_t id)
{
}
#endif

void AppEvent_Start(uint8_t id, bool single)
{
    AppEvent_t* event = &m_events[id];

    event->triggered = false;
    event->single = single;
    event->diagnostics.startTime = TimerGetCurrentTime();

    StartEvent(event);
}

void AppEvent_Stop(uint8_t id)
{
    AppEvent_t* event = &m_events[id];

    if (event->diagnostics.debugEnable)
    {
        DEBUG_PRINTF("Event %s stop\r\n", GetEventName(event));
    }
    if (event->irqMode == NO_IRQ)
    {
        TimerStop( &event->timer );
    }
    else
    {
        GpioRemoveInterrupt( event->irqGpio );
    }
}

uint32_t AppEvent_GetTimeout(uint8_t id)
{
    AppEvent_t* event = &m_events[id];
    ASSERT(event->irqGpio == NULL);         /* This function is only valid for timer events */

    return event->timeout;
}

void AppEvent_SetTimeout(uint8_t id, uint32_t timeout)
{
    AppEvent_t* event = &m_events[id];
    ASSERT(event->irqGpio == NULL);         /* This function is only valid for timer events */

    if (event->diagnostics.debugEnable)
    {
        DEBUG_PRINTF("Event %s set timeout (%lums)\r\n", GetEventName(event), timeout);
    }
    TimerSetValue(&event->timer, timeout);
    event->triggered = false;
    event->timeout = timeout;
}

uint32_t AppEvent_TimeRemaining(uint8_t id)
{
    return TimerTimeRemaining( &m_events[id].timer );
}

bool AppEvent_Running(uint8_t id)
{
    return TimerRunning( &m_events[id].timer );
}

void AppEvent_ProcessMainEvents(void)
{
    for (uint8_t id = 0; id < m_numEvents; ++id)
    {
        AppEvent_t* event = &m_events[id];
        if (event->triggered && !event->paused)
        {
            event->triggered = false;
            if ((event->irqMode != NO_IRQ)              /* Process all events from interrupt context */
                || event->single                        /* Process all single trigger events */
                || TimerRunning(&event->timer)          /* Process continuous timer events that are still active */
                || event->type == EVENT_TYPE_GENERAL)   /* Process all general events */
            {
                if (event->diagnostics.debugEnable)
                {
                    DEBUG_PRINTF("Event %s process\r\n", GetEventName(event));
                }
                ++event->diagnostics.processCount;
                if (event->callback != NULL)
                {
                    (*(event->callback))();
                }
            }
        }
    }
}

void AppEvent_Pause(uint8_t id)
{
    ASSERT(m_events[id].context == APP_EVENT_CONTEXT_MAIN);
    m_events[id].paused = true;
}

void AppEvent_Resume(uint8_t id)
{
    ASSERT(m_events[id].context == APP_EVENT_CONTEXT_MAIN);
    m_events[id].paused = false;
}

bool AppEvent_IsIdle(void)
{
    for (uint8_t id = 0; id < m_numEvents; ++id)
    {
        if (m_events[id].triggered)
            return false;
    }

    return true;
}

void AppEvent_PrintDiagnostics(void)
{
    DEBUG_PRINTF("--------------------\r\n");
    DEBUG_PRINTF("AppEvent Diagnostics\r\n");
    for (uint8_t id = 0; id < m_numEvents; ++id)
    {
        AppEvent_t* event = &m_events[id];
        DEBUG_PRINTF("%s: triggered=%u, processed=%u, elapsedSinceStart=%.1fs\r\n",
                     GetEventName(event), event->diagnostics.triggerCount, event->diagnostics.processCount,
                     TimerGetElapsedTime(event->diagnostics.startTime) / 1000.0);
    }
    DEBUG_PRINTF("--------------------\r\n");
}

void AppEvent_EnableDebug(uint8_t id)
{
    AppEvent_t* event = &m_events[id];
    event->diagnostics.debugEnable = true;
    DEBUG_PRINTF("Event %s: Debug logging enabled\r\n", GetEventName(event));
}

void AppEvent_DisableDebug(uint8_t id )
{
    AppEvent_t* event = &m_events[id];
    event->diagnostics.debugEnable = false;
    DEBUG_PRINTF("Event %s: Debug logging disabled\r\n", GetEventName(event));
}

