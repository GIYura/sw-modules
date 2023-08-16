#ifndef EVENT_H
#define EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"

typedef void (* AppEvent_Callback)(void);
typedef enum
{
    APP_EVENT_CONTEXT_MAIN,        /* Process in the main event loop */
    APP_EVENT_CONTEXT_IMMEDIATE,   /* Process as soon as event is triggered, could be timer or interrupt context */
} APP_EVENT_CONTEXTS;


void AppEvent_Init(void);

/**
 * @brief Register a generic (manually triggered event)
 * @param id: returned identifier of the event if it is successfully registered
 * @param name: name of the event (for debugging purposes)
 * @param callback: function to call when the event is triggered
 * @param context: context in which the event callback will execute once triggered
 */
void AppEvent_RegisterEvent(uint8_t* const eventId, const char* const name, AppEvent_Callback callback,
                            APP_EVENT_CONTEXTS context);

/**
 * @brief Manually trigger an event (ie. put it in the event loop)
 * @param id: identifier of the event to be triggered
 */
void AppEvent_Trigger( uint8_t eventId );

/**
 * @brief Register a timer driven event and use a randomizer to add jitter to the event period
 * @param id: returned identifier of the event if it is successfully registered
 * @param name: name of the event (for debugging purposes)
 * @param callback: function to call when the event is triggered
 * @param timeout: event period in ms
 * @param timeoutRandomizer: maximum event period jitter in ms
 * @param context: context in which the event callback will execute once triggered
 */
void AppEvent_RegisterTimerWithRandomizer(uint8_t* id, const char* const name, AppEvent_Callback callback,
                                          uint32_t timeout, uint32_t timeoutRandomizer, APP_EVENT_CONTEXTS context);

/**
 * @brief Register a timer driven event
 * @param id: returned identifier of the event after it is successfully registered
 * @param name: name of the event (for debugging purposes)
 * @param callback: function to call when the event is triggered
 * @param timeout: event period in ms
 * @param context: context in which the event callback will execute once triggered
 */
void AppEvent_RegisterTimer(uint8_t* const id, const char* const name, AppEvent_Callback callback,
                            uint32_t timeout, APP_EVENT_CONTEXTS context);

/**
 * @brief Register an interrupt driven event
 * @param id: returned identifier of the event after it is successfully registered
 * @param name: name of the event (for debugging purposes)
 * @param callback: function to call when the event is triggered
 * @param gpio: interrupt gpio information
 * @param irqMode: desired interrupt mode
 * @param irqPriority: desired interrupt priority
 * @param context: context in which the event callback will execute once triggered
 */
void AppEvent_RegisterInterrupt( uint8_t* const eventId, const char* const name, AppEvent_Callback callback,
                                 Gpio_t* gpio, IrqModes irqMode, IrqPriorities irqPriority, APP_EVENT_CONTEXTS context);
/**
 * @brief Start/activate an  event
 * @param id: event identifier
 * @param single: set true for a single trigger event; false for a continuous trigger event
 */
void AppEvent_Start(uint8_t id, bool single);

/**
 * @brief Stop an event
 * @param id: event identifier
 */
void AppEvent_Stop(uint8_t id);

/**
 * @brief Returns the configured timeout value of the event
 * @param id: event identifier
 * @returns timeout in ms
 */
uint32_t AppEvent_GetTimeout(uint8_t id);

/**
 * @brief Sets the configured timeout of the event
 * @param id: event identifier
 * @param timeout: desired timeout value in ms
 */
void AppEvent_SetTimeout(uint8_t id, uint32_t timeout);

/**
 * @brief Returns the time remaining before the event next triggers
 * @param id: event identifier
 * @returns time remaining in ms
 */
uint32_t AppEvent_TimeRemaining(uint8_t id);

/**
 * @brief Check if an event is running
 * @param id: event identifier
 * @returns true if the event is running; false otherwise
 */
bool AppEvent_Running(uint8_t id);

/**
 * @brief Process all triggered events
 * @notes This method should be called in the main application loop
 */
void AppEvent_ProcessMainEvents(void);

/**
 * @brief Prevent an event from being processed in the main application loop
 * @notes Only applicable to eventss registered in APP_EVENT_CONTEXT_MAIN
 */
void AppEvent_Pause(uint8_t id);

/**
 * @brief Allow event to be processed in the main application loop
 * @notes Only applicable to eventss registered in APP_EVENT_CONTEXT_MAIN
 */
void AppEvent_Resume(uint8_t id);

/**
 * @returns true if the event loop is idle, meaning there are no "triggered" events
 */
bool AppEvent_IsIdle(void);

/**
 * @brief Print diagnostics information about all registered events
 */
void AppEvent_PrintDiagnostics(void);

/**
 * @brief Enable printing of event diagnostics
 * @param id: event identifier
 * @notes Printing of event debug info is enabled by default. However, serial logging
 *        of high frequency events may disrupt MCU operation. In such case,
 *        the event debug info should be disabled during normal operation.
 */
void AppEvent_EnableDebug(uint8_t id);

/**
 * @brief Disable printing of event diagnostics
 * @param id: event identifier
 * @notes Printing of event debug info is enabled by default. However, serial logging
 *        of high frequency events may disrupt MCU operation. In such case,
 *        the event debug info should be disabled during normal operation.
 */
void AppEvent_DisableDebug(uint8_t id);

#endif /* EVENT_H */

