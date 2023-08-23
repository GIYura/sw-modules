#ifndef DIGITAL_PATTERN_H
#define DIGITAL_PATTERN_H

/*
 * A pattern is composed of a set of phases, which (in turn) are composed
 * of a set of input states.
 *
 * - Pattern
 *      - Phase(s)
 *          - State(s)
 *
 * For a pattern to be successful, every phase in the pattern must be completed
 * successfully. If one phase is performed incorrectly, the pattern is incorrect
 * and the pattern struct is reset.
 *
 * For a phase to be completed successfully, the digital input must go through
 * the states (HIGH or LOW) in the specified sequence (stored in the array), within
 * the time alloted for the phase (stored as phaseDuration in the PatternPhase_t struct).
 * Once the input successfully performs the state sequence, the phase is deemed
 * 'complete'. If input does not perform the sequence in the alloted time OR the
 * input changes state after the phase has already completed, the phase is invalid
 * and the phase is deemed incorrect.
 *
 * Todo (Implement if needed?)
 *      - Heap memory allocation?
 *      - Functions for removing phases and states.
 * Note: Current implementation is probably convoluted and can be improved upon or
 * redone completely.
 */

#include "timer.h"
#include "gpio.h"

/* Max number of phases in a pattern */
#define PHASE_MAX      3
/* Max number of states in a phase */
#define STATE_MAX      8

typedef enum 
{
    INPUT_HIGH,
    INPUT_LOW
} INPUT_STATE;

typedef enum 
{
    PHASE_IDLE,
    PHASE_INPROGRESS,
    PHASE_COMPLETE,
    PHASE_INVALID
} PHASE_STATE;

/* Note: Typically the structs would not be modified outside of
 * the function provided.
 */
typedef struct 
{
    INPUT_STATE states[STATE_MAX];
    uint8_t numStates;
    uint32_t phaseDuration;
    bool fixedDuration;
    int stateIndex;
    PHASE_STATE phaseStatus;
} PatternPhase_t;

typedef struct 
{
    Gpio_t * inputPin;
    PatternPhase_t phases[PHASE_MAX];
    uint8_t numPhases;
    TimerEvent_t phaseTimer;
    int phaseIndex;
    bool patternComplete;
} DigitalPattern_t;

/*
 * @brief Checks the state of the input updates the appropriate
 * PatternPhase_t struct
 *
 * Should typically be invoked by an interrupt handler
 * (directly or indirectly).
 *
 * @param   pattern, pointer to struct containing the pattern
 * */
void OnDigitalPattern_tEvent(DigitalPattern_t* pattern);

/*
 * @brief Initializes a DigitalPattern_t struct
 *
 * @param inputPin, gpio input pin to examine for the pattern
 *
 *@return DigitalPattern_t struct
 */
DigitalPattern_t DigitalPattern_t_Init(Gpio_t* inputPin);

/*
 * @brief Creates a phase struct with the specified duration, to be used in
 * the construction of a DigitalPattern_t
 *
 * @param duration, phase duration
 * @param fixedDuration, bool to indicate whether the phase has a fix
 *        Essentially, whether a phase terminates when the phase timer
 *        expires (fixed) or on the last input state (not fixed).
 *
 * @return PatternPhase_t struct
 */
PatternPhase_t DigitalPattern_t_CreatePhase(uint32_t duration, bool fixedDuration);

/*
 * @brief Adds a pattern phase struct
 *
 * @param pattern, pointer to the pattern struct that will store the phase
 * @param phase, phase struct that will be added to @p pattern
 */
void DigitalPattern_t_AddPhase(DigitalPattern_t* pattern, PatternPhase_t phase);

/*
 * @brief Adds a state to the phase struct
 *
 * @param phase, pointer to phase struct that will store the state
 * @param state, input state to add to @p phase
 */
void DigitalPattern_t_AddState(PatternPhase_t* phase, INPUT_STATE state);

/*
 * @brief Checks to see if pattern has been successfully performed
 * Note: This function checks the patternComplete field in the phase
 * struct. If this is true, the value true is returned AND the pattern
 * struct is reset.
 *
 * @param patter, pointer to pattern struct to examine
 *
 * @return true if the pattern was performed successfully, false if not
 */
bool DigitalPattern_t_Check(DigitalPattern_t* pattern);

#endif /* DIGITAL_PATTERN_H */

