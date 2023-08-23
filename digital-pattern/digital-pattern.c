#include "assertion.h"
#include <stddef.h>
#include "digital-pattern.h"

#include "AppDebugConfig.h"
#include "SerialConsole.h"

const char* phasePrintTable[] = 
{
    "Idle",
    "In Progress",
    "Complete",
    "Invalid"
};

static void PrintDiagnostics(DigitalPattern_t* pattern)
{
    if (pattern->phaseIndex != -1)
    {
        PatternPhase currentPhase = pattern->phases[pattern->phaseIndex];
        DEBUG_PRINTF("Pattern phase %d of %d:\r\n", pattern->phaseIndex + 1, pattern->numPhases);
        DEBUG_PRINTF("\tTime left in phase: %lu\r\n", TimerTimeRemaining( &pattern->phaseTimer ));
        DEBUG_PRINTF("\tStatus: %s\r\n", phasePrintTable[(uint8_t)currentPhase.phaseStatus] );
        DEBUG_PRINTF("\tState %d of %d\r\n", currentPhase.stateIndex, currentPhase.numStates);
    }
}

static InputState CheckInputState(Gpio_t* inputPin)
{
    if (!GpioRead( inputPin ))
        return INPUT_LOW;
    else
        return INPUT_HIGH;
}

static void ResetPatternStruct(DigitalPattern* pattern)
{
    ASSERT(pattern != NULL);
    for (uint8_t i = 0; i < pattern->numPhases; i++)
    {
        PatternPhase *idxPhase = &pattern->phases[i];
        idxPhase->phaseStatus = PHASE_IDLE;
        idxPhase->stateIndex = -1;
        TimerStop( &pattern->phaseTimer );
    }
    DEBUG_PRINTF("Resetting Pattern\r\n");
    pattern->patternComplete = false;
    pattern->phaseIndex = -1;
}

static void OnPhaseTimerEvent(void* context)
{
    ASSERT(context != NULL);
    DigitalPattern *pattern = (DigitalPattern *) context;
    PatternPhase *activePhase = &pattern->phases[pattern->phaseIndex];

    if (activePhase->phaseStatus == PHASE_COMPLETE
            || activePhase->numStates == 0)            //Phase has no states (phase does not need to see any edges)
    {
        if ((activePhase->numStates == 0 ) && ( activePhase->phaseStatus != PHASE_IDLE))
            ResetPatternStruct( pattern );              //Reset pattern if there are no states and there was input activity

        /* If on last phase, set pattern complete. Else, move on to next phase */
        else if (pattern->phaseIndex == pattern->numPhases -1)
        {
            pattern->patternComplete = true;
            DEBUG_PRINTF("Pattern Complete\r\n");
        }
        else
        {
            pattern->phaseIndex++;
            PatternPhase *nextPhase = &pattern->phases[pattern->phaseIndex];
            DEBUG_PRINTF("Next phase of %lu ms starting\r\n", nextPhase->phaseDuration);
            TimerSetValue( &pattern->phaseTimer, nextPhase->phaseDuration );
            TimerStartWithParam( &pattern->phaseTimer, pattern );
        }
    }
    else
    {
        /* Phase was not completed successfully, reset pattern*/
        DEBUG_PRINTF("Phase Failure, ");
        ResetPatternStruct( pattern );
    }
}

void OnDigitalPatternEvent(DigitalPattern_t* pattern)
{
    ASSERT(pattern != NULL);
    if (pattern->phaseIndex == -1)
       pattern->phaseIndex = 0;  //Pattern has started

    PatternPhase *activePhase = &pattern->phases[pattern->phaseIndex];
    if (activePhase->phaseStatus == PHASE_IDLE)
    {
       DEBUG_PRINTF("Starting Phase\r\n");
       activePhase->stateIndex = 0;
       activePhase->phaseStatus = PHASE_INPROGRESS;
       if (pattern->phaseIndex == 0)
       {
           /* Start timer if active phase is the first phase
            * Note: Phase timers for subsequent phases are started
            * on the timer callback */
           TimerSetValue( &pattern->phaseTimer, activePhase->phaseDuration );
           TimerStartWithParam( &pattern->phaseTimer, pattern );
       }
    }

    if (activePhase->phaseStatus == PHASE_INPROGRESS)
    {
        if (CheckInputState(pattern->inputPin) == activePhase->states[activePhase->stateIndex])
           activePhase->stateIndex++;

        if (activePhase->stateIndex == activePhase->numStates)
        {
            activePhase->phaseStatus = PHASE_COMPLETE;
            if (!activePhase->fixedDuration)
            {
                TimerStop(&pattern->phaseTimer);
                /* Call timer callback */
                OnPhaseTimerEvent(pattern);
            }
        }
    }
    else
    {
        if (activePhase->phaseStatus == PHASE_COMPLETE)
        {
            /* additional input activity invalidates phase, reset pattern */
            DEBUG_PRINTF("Additional invalid input detected: ");
            ResetPatternStruct(pattern);
        }
    }
    PrintDiagnostics(pattern);
}

DigitalPattern DigitalPattern_Init(Gpio_t *inputPin)
{
    ASSERT(inputPin != NULL);

    DigitalPattern digPattern;
    TimerEvent_t phaseTimer = {};   //initialize empty struct

    digPattern.inputPin = inputPin;
    digPattern.numPhases = 0;
    digPattern.phaseIndex = -1;
    digPattern.patternComplete = false;
    digPattern.phaseTimer = phaseTimer;

    TimerInit(&digPattern.phaseTimer, OnPhaseTimerEvent);

    return digPattern;
}

PatternPhase DigitalPattern_CreatePhase(uint32_t timerVal, bool fixedDuration)
{
    PatternPhase phase;

    phase.phaseDuration = timerVal;
    phase.numStates = 0;
    phase.stateIndex = -1; //pattern is idle
    phase.phaseStatus = PHASE_IDLE;
    phase.fixedDuration = fixedDuration;

    return phase;
}

void DigitalPattern_AddPhase(DigitalPattern_t* pattern, PatternPhase phase)
{
    ASSERT(pattern != NULL);
    ASSERT( pattern->numPhases < MAX_PHASES );

    pattern->phases[pattern->numPhases] = phase;
    pattern->numPhases++;
}

void DigitalPattern_AddState(PatternPhase* phase, InputState state)
{
    ASSERT(phase != NULL);
    ASSERT(phase->numStates < MAX_STATES);

    phase->states[phase->numStates] = state;
    phase->numStates++;
}

bool DigitalPattern_Check(DigitalPattern_t* pattern)
{
    ASSERT(pattern != NULL);

    bool patternDetected = false;

    if (pattern->patternComplete)
    {
        patternDetected = true;
        ResetPatternStruct(pattern);
    }

    return patternDetected;
}

