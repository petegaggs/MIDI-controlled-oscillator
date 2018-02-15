# MIDI-controlled-oscillator
an arduino based MIDI controlled analogue oscillator
This is a novel approach - it's a hybrid design, with a standard op-amp integrator to generate a sawtooth much like other analogue oscillator designs.
However, the reset of the integrator comes from the microcontroller, this avoids needing any tempco, matched pairs, calibration etc.
I use the 16 bit timer to generate a square wave of the correct frequency. The rising edge of this will reset the integrator.
So as well as a sawtooth, there is a square wave available as well, I'm not using this yet. If I were to use this, I would suggest buffering with an opamp before taking it off-board.
The integrator drive comes from an SPI DAC. The exponential conversion is done in software (originally by lookup table, now calculated on the fly). The integrator drive level does not need to be precise, any error here will result in a small change of amplitude of the sawtooth, rather than a frequency error, which would be the case with traditional oscillator circuits.
The sawtooth is about 4V pk-pk, and the design works over a 7 octave range.

Other features:
MIDI control
supports MIDI pitchbend
Optional analogue control voltage on pin A0, to modulate pitch. Can be disabled if not required.
powered from +9V supply (I have a local inverter to generate -9V for the op-amp).
Nice and simple, not too many parts.
I used an Arduino nano in my prototype, since that was what I had to hand, but it was originally going to be a pro mini 5V 16MHz.
Note that you need the arduino MIDI library installed. Also the timer is used directly, so it won't work on all Arduinos. It will work on any '328p based hardware. 
