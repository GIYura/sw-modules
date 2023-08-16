#### Description

The repo contains valuable SW modules that might be used as a base code.

##### Event module

The Event module provides a simple, low footprint event-driven design model.
The module provides the following features:
 - Centralized management of timer and interrupt events allowing for reduced code size and streamlined debug logging;
 - Handling of asynchronous events in one of more controlled main context event loops OR immediate handling of critical timer/interrupt events.

##### Digital pattern

This algorithm implements a configurable time-based digital pattern detector.  
It is best used for any digital input, such as a button or switch, where you are trying to trigger off of a temporal defined pattern 
from the source in real time.  

For a pattern to be successful, every phase in that pattern must be completed in sequence.
If any phase in the pattern is not completed, the pattern struct resets. 

Each phase has a configurable duration.
For a phase to be successful, the digital source must go through all the states (High / Low) in the specified sequence and within 
the specified duration.  
If the sequence is not completed in the allotted time or the input sequence is incorrect, then the phase is deemed incorrect. 

##### Debug gpio

The debug-gpio module enables a developer to seamlessly output data over (up to) three GPIO lines in PWM or manchester encoded output 
for debugging and real-time system timing analysis.

