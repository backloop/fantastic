#include <assert.h>
#include <limits.h>
/*
    Problem:
    ========
    The Supermicro X10slm-f motherboard fan controller is initially configured to support high speed
    fans in the 800-25000 RPM range. Fans reporting RPM rates outside these limits will trigger the
    fan controller to revive the fan and/or generate warnings with varying degrees of urgency.

    IPMI can be used to reconfigure the fan controller to adjust its range to between 100-1600 RPM for low-RPM fans
    like the Noctua F/P12 with ranges of 150-1500 RPM. But these fans aren't well supported as the motherboard
    has a tendency to preriodically detect 0 RPM from fans that are spinning at less than 300 RPM. The fan controller
    responds by running all fans at 100% duty until a valid response the the fans' tachometer signal is read. Then it
    gradually spins down the fans, until reading 0 RPM again.

    $ while true; do ipmitool sensor | grep "FAN[2,4]"; sleep 1; done
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 200.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 200.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 200.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 0.000      | RPM        | nr    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 0.000      | RPM        | nr    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 1400.000   | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 1200.000   | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 1300.000   | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 1000.000   | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN4             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000
    FAN2             | 300.000    | RPM        | ok    | 0.000     | 0.000     | 100.000   | 1700.000  | 1800.000  | 1900.000


    IPMI CONFIGURATION (not sufficient):
    ====================================
    Use "ipmitool" from a Linux machine connected to the IPMI NIC or ipmitool on the actual machine itself through /dev/ipmi0
    to edit the thresholds:

    Edit sensor thresholds on a remote machine
    $ ipmitool -H <ip address> -U ADMIN -P ADMIN  sensor thresh "FAN2" lower 0 0 100

    Edit sensor thresholds on a local machine
    $ ipmitool sensor thresh "FAN2" lower 0 0 100
    Locating sensor record 'FAN2'...
    Setting sensor "FAN2" Lower Non-Recoverable threshold to 0,000
    Setting sensor "FAN2" Lower Critical threshold to 0,000
    Setting sensor "FAN2" Lower Non-Critical threshold to 100,000

    List IPMI sensor information
    $ ipmitool sensor
    CPU Temp         | 49,000     | degrees C  | ok    | 0,000     | 0,000     | 0,000     | 95,000    | 98,000    | 100,000
    System Temp      | 33,000     | degrees C  | ok    | -9,000    | -7,000    | -5,000    | 80,000    | 85,000    | 90,000
    Peripheral Temp  | 39,000     | degrees C  | ok    | -9,000    | -7,000    | -5,000    | 80,000    | 85,000    | 90,000
    PCH Temp         | 54,000     | degrees C  | ok    | -11,000   | -8,000    | -5,000    | 90,000    | 95,000    | 100,000
    P1-DIMMA1 Temp   | 32,000     | degrees C  | ok    | 1,000     | 2,000     | 4,000     | 80,000    | 85,000    | 90,000
    P1-DIMMA2 Temp   | na         |            | na    | na        | na        | na        | na        | na        | na
    P1-DIMMB1 Temp   | 31,000     | degrees C  | ok    | 1,000     | 2,000     | 4,000     | 80,000    | 85,000    | 90,000
    P1-DIMMB2 Temp   | na         |            | na    | na        | na        | na        | na        | na        | na
    FAN1             | 1000,000   | RPM        | ok    | 400,000   | 600,000   | 800,000   | 25300,000 | 25400,000 | 25500,000
    FAN2             | 1100,000   | RPM        | ok    | 0,000     | 0,000     | 100,000   | 25300,000 | 25400,000 | 25500,000
    FAN3             | na         |            | na    | na        | na        | na        | na        | na        | na
    FAN4             | 200,000    | RPM        | ok    | 0,000     | 0,000     | 100,000   | 25300,000 | 25400,000 | 25500,000
    FANA             | na         |            | na    | na        | na        | na        | na        | na        | na
    Vcpu             | 1,836      | Volts      | ok    | 1,242     | 1,260     | 1,395     | 1,899     | 2,088     | 2,106
    VDIMM            | 1,488      | Volts      | ok    | 1,092     | 1,119     | 1,200     | 1,641     | 1,722     | 1,749
    12V              | 12,128     | Volts      | ok    | 10,144    | 10,272    | 10,784    | 12,960    | 13,280    | 13,408
    5VCC             | 5,135      | Volts      | ok    | 4,244     | 4,487     | 4,730     | 5,378     | 5,540     | 5,594
    3.3VCC           | 3,299      | Volts      | ok    | 2,789     | 2,823     | 2,959     | 3,554     | 3,656     | 3,690
    VBAT             | 2,972      | Volts      | ok    | 2,384     | 2,496     | 2,580     | 3,476     | 3,588     | 3,672
    5V Dual          | 5,000      | Volts      | ok    | 4,244     | 4,379     | 4,487     | 5,378     | 5,540     | 5,594
    3.3V AUX         | 3,333      | Volts      | ok    | 2,789     | 2,891     | 2,959     | 3,554     | 3,656     | 3,690
    1.2V BMC         | 1,269      | Volts      | ok    | 1,080     | 1,107     | 1,152     | 1,404     | 1,431     | 1,458
    1.05V PCH        | 1,050      | Volts      | ok    | 0,870     | 0,897     | 0,942     | 1,194     | 1,221     | 1,248
    Chassis Intru    | 0x1        | discrete   | 0x0100| na        | na        | na        | na        | na        | na


    THE FINAL SOLUTION:
    ===================
    Create an ATmega-based inline frequency multiplier, that intercepts the fan tachometer signal and multiplies it by a factor of 4
    so that when fed into the fan controller the lower limit fan RPM warnings are never triggered. Also reconfigure the fan controller
    using IPMI for this adjusted RPM range.

    This transforms the Noctua fan tachometer signal from 150-1500 RPM -> 600-6000 RPM, which the fran controller manages without issues.

    $ ipmitool sensor thresh "FAN2" lower 300 400 500
    Locating sensor record 'FAN2'...
    Setting sensor "FAN2" Lower Non-Recoverable threshold to 300,000
    Setting sensor "FAN2" Lower Critical threshold to 400,000
    Setting sensor "FAN2" Lower Non-Critical threshold to 500,000

    $ ipmitool sensor thresh "FAN2" upper 6500 6600 6700
    Locating sensor record 'FAN2'...
    Setting sensor "FAN2" Lower Non-Critical threshold to 6500,000
    Setting sensor "FAN2" Lower Critical threshold to 6600,000
    Setting sensor "FAN2" Lower Non-Recoverable threshold to 6700,000

    $ while true; do ipmitool sensor | grep "FAN[2,4]"; sleep 1; done
    FAN2             | 800.000    | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN4             | 1100.000   | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN2             | 800.000    | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN4             | 1100.000   | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN2             | 800.000    | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN4             | 1100.000   | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN2             | 800.000    | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN4             | 1100.000   | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN2             | 800.000    | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN4             | 1100.000   | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN2             | 800.000    | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000
    FAN4             | 1100.000   | RPM        | ok    | 300.000   | 400.000   | 500.000   | 6500.000  | 6600.000  | 6700.000

    FAN2 settles at 200 RPM originally which is 800 RPM when transformed
    FAN4 settles at 275 RPM originally which is 1100 RPM when transformed.

    Example serial output from the device:

    Input :                 Freq (mHz): 9174        Freq (Hz): 9    RPM: 275            <- calculated input
    Output:                 Freq (mHz): 36696       Freq (Hz): 37   RPM: 1100           <- calculated output

    Input :                 Freq (mHz): 9174        Freq (Hz): 9    RPM: 275            <- calculating input, but no change
    Input :                 Freq (mHz): 9174        Freq (Hz): 9    RPM: 275            <- and again without change
    Input :                 Freq (mHz): 9174        Freq (Hz): 9    RPM: 275            <- and again without change
    Input :                 Freq (mHz): 9091        Freq (Hz): 9    RPM: 273            <- input has now changed
    Output:                 Freq (mHz): 36364       Freq (Hz): 36   RPM: 1092           <- new calculated output

    Input :                 Freq (mHz): 9091        Freq (Hz): 9    RPM: 273            <- same input as before
    Input :                 Freq (mHz): 9174        Freq (Hz): 9    RPM: 275            <- input changed
    Output:                 Freq (mHz): 36696       Freq (Hz): 37   RPM: 1100           <- thus change output

    Design goals:
    =============

    Input:
    RPM: 0 - 2000
    Freq, Hz: 0 - 33

    Output: Some multiple of Input
    RPM: 0 - 8000
    Freq, Hz: 0 - 133


    Lessons learned:
    ================
    * MOST IMPORTANT: Use a common ground if using different DC power supplies to power the ATmega (5V) and the fan (12V).
      Without common ground the tachometer signal will behave very odd (irrespective of it being read with Atmega or an oscilloscope)
    * Use an oscilloscope for verifying the input and output chracteristics of the signals without relying on functional code.
    * Fan RPM control uses a 25kHz PWM signal which propagates into the ground/tachometer signal. Use an RC circuit to filter
      the tachometer signal before reaching the ATmega input pin.
    * Use a series resistor on the open-collect NPN transistor to limit current draw from base->emitter so that upper voltage level
      can be kept at 5V.


    Noctua fan details:
    ===================

    https://noctua.at/pub/media/wysiwyg/Noctua_PWM_specifications_white_paper.pdf

    Measurements of Noctua F/P12 fans:
    PWM duty       RPM
    ==================
        100%      1364
         90%      1200
         80%      1111
         70%      1000
         60%       882
         50%       750
         40%       588
         30%       417
         20%       291
         10%       137
          0%         0
*/

//
// Miscellaneous defines for switching behavior
//
//#define DEBUG
//#define USE_FLOAT
#define USE_INTERRUPT

#define SERIAL_INPUT_TACH_OUT (1)
#define SERIAL_INPUT_FAN_CONTROL (2)
#define SERIAL_INPUT (SERIAL_INPUT_FAN_CONTROL)

#define UNUSED(x) ((void)x)

// Integer division equals to floor()
// to make round() equivalent, add 1/2 denominator to numerator
// (this really is recursive though, as adding 1/2 denominator
// should also be rounded for better precision (lets skip that here)
#define round_division(dividend, divisor) (((unsigned long)(dividend) + (unsigned long)(divisor) / 2) / (unsigned long)(divisor))

// Where we connected the fan sense pin. Must be an interrupt capable pin.
#define PIN_SENSE 2

#define PIN_TACH_OUT 9
#define PIN_PWM_OUT 3

// https://docs.arduino.cc/tutorials/generic/secrets-of-arduino-pwm
//
// TIMER1: PHASE AND FREQUENCY CORRECT MODE, MODE9
// see "Table 15-5" in the ATmega328 manual for mode descriptions
// https://docs.arduino.cc/tutorials/generic/secrets-of-arduino-pwm
// https://wolles-elektronikkiste.de/en/timer-and-pwm-part-2-16-bit-timer1
//
// A frequency (with 50% duty cycle) waveform output in
// "15.9.5 phase and frequency correct" PWM mode can be achieved by
// setting OC1A to toggle its logical level on each compare match (COM1n1:0 = 1).
// This applies only if OCR1A is used to define the TOP value (WGM13:0 = 15 or 9).
// The waveform generated will have a maximum frequency of fOC1A = fclk_IO / 2.
// This feature is similar to the OC1A toggle in CTC mode, except the double buffer
// feature of the output compare unit is enabled in the this PWM mode.
//
// 15.9.5 Phase and frequency correct PWM mode
// * dual slope operation
// * double buffered
// * half max_freq due to use of OCR1A to get 50% duty cycle
//
// OC1A (pin9):  freqA, 50% duty
// OC1B (pin10): disconnected
//
// freq = (uint16_t) (F_CPU / (tccr1b_prescaler_n * top * 2 * 2));
// top_max = UINT16_MAX; // results lowest frequency
// top_min = 1; // results highest frequency
// top_min = 0; // not used, because it sticks to current level
// when top is set, be it high or low...
// n = (1, 8, 64, 256, 1024)
// frequency ranges:
//    N   right shifts  freq_max(Hz)(top_min=1)  freq_min(top_max_UINT16_MAX)
//    1        0            4000000.0            61.04
//    8        3             500000.0             7.63
//   64        6              62500.0             0.95
//  256        8              15625.0             0.24
// 1024       10               3906.25            0.06  <-- best match of interval and thus accuracy

//
// TIMER1 PWM MODE9
//
void tachometer_out(unsigned long mfreq) {
  static const unsigned int tccr1b_prescaler_n = 1024;
  static unsigned int initialized = 0;
  unsigned int ocr1a_top;

  if (mfreq == 0) {  // prevent division by zero
    Serial.println("TODO: set output constant low");
    ocr1a_top = UINT16_MAX;
  } else {
#ifdef USE_FLOAT
    // Floating point artithmetic; prevents overflow,
    // but costs in flash size and execution
    double ocr1a_tmp = F_CPU * (double)1000 / ((unsigned long)tccr1b_prescaler_n * mfreq * 2 * 2);
    ocr1a_top = (unsigned int)ocr1a_tmp;
#else
    // Large numbers. Be careful of overflows...
    // cast to (unsigned long) in calculation so that intermediate steps do not default to (int)
    unsigned long dividend = 1000 * ((unsigned long)F_CPU >> 2);
    unsigned long divisor = (unsigned long)tccr1b_prescaler_n * mfreq;
    ocr1a_top = round_division(dividend, divisor);
#endif
    // handle underflow, prevent setting TOP == 0...
    ocr1a_top = max(1, (unsigned int)(ocr1a_top));
  }

  if (!initialized) {

    // WAVEFORM GENERATION MODE
    // Table 15-5. Enable "Phase and frequency correct PWM"
    // Use Mode 9 because using OCR1A as TOP is double buffered and is
    // updated in the next cycle.
    TCCR1B = _BV(WGM13);
    TCCR1A = _BV(WGM10);

    // COMPARE OUTPUT MODE
    // Table 15-4.
    // Enable special
    // "50% fixed duty cycle, p.105, chapter 15.9.5, last paragraph"
    // on channel A
    TCCR1A &= ~_BV(COM1A1);
    TCCR1A |= _BV(COM1A0);

    switch (tccr1b_prescaler_n) {
      case (1):
        TCCR1B &= ~_BV(CS12);
        TCCR1B &= ~_BV(CS11);
        TCCR1B |= _BV(CS10);
        break;
      case (8):
        TCCR1B &= ~_BV(CS12);
        TCCR1B |= _BV(CS11);
        TCCR1B &= ~_BV(CS10);
        break;
      case (64):
        TCCR1B &= ~_BV(CS12);
        TCCR1B |= _BV(CS11);
        TCCR1B |= _BV(CS10);
        break;
      case (256):
        TCCR1B |= _BV(CS12);
        TCCR1B &= ~_BV(CS11);
        TCCR1B &= ~_BV(CS10);
        break;
      case (1024):
        TCCR1B |= _BV(CS12);
        TCCR1B &= ~_BV(CS11);
        TCCR1B |= _BV(CS10);
        break;
      default:
        Serial.print("Prescaler not supported...");
        assert(0);
    }

    // TOP
    OCR1A = ocr1a_top;
    //Serial.println(ocr1a_top);

    // OC1A
    // Set pin direction, could have used Data Direction Register directly
    //DDRB |= _BV(DDB1); //pin  9, PB1
    pinMode(PIN_TACH_OUT, OUTPUT);

    // OC1B not available in 50% duty mode
    //DDRB |= _BV(DDB2); //pin  10, PB2
    //pinMode(10, OUTPUT);

    initialized = 1;
  } else {
    // TOP
    OCR1A = ocr1a_top;
  }

#ifdef DEBUG
  Serial.print("freq (mHz):");
  Serial.print(mfreq);
  Serial.print(" -> TOP:");
  Serial.println(ocr1a_top);
#endif
}

// TIMER2: PHASE CORRECT MODE, MODE5
// According to manual:
//     f_clk_I/O        = 16MHz
//     f_OCnxPCPWM(max) = f_clk_I/O / (N * 510)
//                      = f_clk_I/O / (N * (255/*max counter*/ * 2))
//                      = f_clkI/O / (tccr2b_prescaler_n * (255/*max counter*/ * 2))
//     f_OCnxPCPWM(x)   = f_clkI/O / (tccr2b_prescaler_n * x * 2)
//     x = top(n,f)
//     x <= 255 (max_counter)
//     ->  x = top(n,f) = f_clk_I/O / ((tccr2b_prescaler_n * f_OCnxPCPWM * 2);
//
//     N = tccr2b_prescaler_n
//
//     For Noctua fan:
//          f_OCnxPCPWM = 25000 Hz
//     ->  x = top(n,f) = f_clk_I/O / (tccr2b_prescaler_n * f_OCnxPCPWM * 2);'
//                      = 16e6 / (x * 25e3 * 2)
//                      = 320 / n
//
//    n        x
//    1      320        N/A, x<=255
//    8       40        <-- highest counter, i.e. best accuracy
//   32       10
//   64        5
//  128        2.5      N/A
//  256        1.25     N/A
// 1024        0.3125   N/A
//
// OC2A (pin11): freqA, 50% duty
// OC2B (pin3):  freqB = 2*freqA, controllable duty: OCR1B = duty_percent * OCR1A
//

//
// TIMER2 PWM MODE5
//
void fan_pwm_control(unsigned int duty_percent) {
  static const unsigned int tccr2b_prescaler_n = 8;
  static unsigned int initialized = 0;
  unsigned int ocr2a_top;

  //25kHz, as defined by Noctua White Paper
  unsigned long freq = 25000;

#ifdef USE_FLOAT
  // Floating point artithmetic; prevents overflow,
  // but costs in flash size and execution
  double ocr2a_tmp = (double)F_CPU / ((unsigned long)tccr2b_prescaler_n * (freq / 2) * 2 * 2);
  ocr2a_top = (unsigned int)ocr2a_tmp;
#else
  // Large numbers. Be careful of overflows...
  unsigned long dividend = ((unsigned long)F_CPU >> 1);
  unsigned long divisor = (unsigned long)tccr2b_prescaler_n * freq;
  ocr2a_top = round_division(dividend, divisor);
#endif
  // handle underflow, prevent setting TOP == 0...
  ocr2a_top = max(1, ocr2a_top);
  //Serial.println(ocr2a_top);

  unsigned int ocr2b_duty = round_division(ocr2a_top * duty_percent, 100);
  ocr2b_duty = max(1, ocr2b_duty);
  //Serial.println(ocr2b_duty);

  if (!initialized) {

    // WAVEFORM GENERATION MODE
    // Table 17-8. Waveform Generation Mode Bit Description
    // Mode 5: PWM, phase correct
    TCCR2A = _BV(WGM20);
    TCCR2B = _BV(WGM22);

    // COMPARE OUTPUT MODE
    // Table 17-4. Compare Output Mode, Phase Correct PWM Mode
    // Table 17-7. Compare Output Mode, Phase Correct PWM Mode
    TCCR2A |= _BV(COM2A0);
    TCCR2A |= _BV(COM2B1);

    switch (tccr2b_prescaler_n) {
      case (1):
        TCCR2B &= ~_BV(CS22);
        TCCR2B &= ~_BV(CS21);
        TCCR2B |= _BV(CS20);
        break;
      case (8):
        TCCR2B &= ~_BV(CS22);
        TCCR2B |= _BV(CS21);
        TCCR2B &= ~_BV(CS20);
        break;
      case (32):
        TCCR2B &= ~_BV(CS22);
        TCCR2B |= _BV(CS21);
        TCCR2B |= _BV(CS20);
        break;
      case (64):
        TCCR2B |= _BV(CS22);
        TCCR2B &= ~_BV(CS21);
        TCCR2B &= ~_BV(CS20);
        break;
      case (128):
        TCCR2B |= _BV(CS22);
        TCCR2B &= ~_BV(CS21);
        TCCR2B |= _BV(CS20);
        break;
      case (256):
        TCCR2B |= _BV(CS22);
        TCCR2B |= _BV(CS21);
        TCCR2B &= ~_BV(CS20);
        break;
      case (1024):
        TCCR2B |= _BV(CS22);
        TCCR2B |= _BV(CS21);
        TCCR2B |= _BV(CS20);
        break;
      default:
        Serial.print("Prescaler not supported...");
        assert(0);
    }

    // TOP - defines PWM frequency on PortA with 50% duty
    OCR2A = ocr2a_top;

    // DUTY - defines PWM frequency on PortB = 2xPortA freq and
    // user defined duty cycle.
    OCR2B = ocr2b_duty;

    // OC2A
    // not used
    //pinMode(11, OUTPUT);

    // OC2B
    // variable duty cycle
    pinMode(PIN_PWM_OUT, OUTPUT);


    initialized = 1;
  } else {
    // DUTY - relative to TOP
    OCR2B = ocr2b_duty;
  }

#ifdef DEBUG
  Serial.print("duty (%): ");
  Serial.print(duty_percent);
  Serial.print(" -> TOP:");
  Serial.print(ocr2a_top);
  Serial.print(" DUTY:");
  Serial.println(ocr2b_duty);
#endif
}

//
// Some logic to average history
//
#define MAX_DIFFS (5)
struct diffs {
  unsigned int vals[MAX_DIFFS];
  unsigned int sum;
  unsigned int pos;
  unsigned int len;
  unsigned int avg;
};

void diff_reset(struct diffs *d) {
  for (unsigned int i = 0; i < MAX_DIFFS; i++) {
    d->vals[i] = 0;
  }
  d->pos = 0;
  d->len = 0;
  d->sum = 0;
  d->avg = 0;
}

void diff_add(struct diffs *d, unsigned int diff) {
  if (d->len == MAX_DIFFS) {
    // remove the oldest
    d->sum -= d->vals[d->pos];
    // diff_len will equal to MAX_DIFFS forever from now
    // because all slots are taken
  } else {
    // don't remove any items until all slots are taken
    d->len++;
  }
  // add the new value
  d->sum += diff;
  // store the new value
  d->vals[d->pos] = diff;
  // restart counter if needed
  d->pos = ++d->pos < MAX_DIFFS ? d->pos : 0;

  d->avg = round_division(d->sum, d->len);
}

//
// Interrupt handler.
// Stores the timestamps of the last 2 interrupts and handles debouncing
//

// 0 is fine for most fans, crappy fans may require 10 or 20 to filter out noise
#define DEBOUNCE 0

// if no interrupts were received for 500ms,
// consider the fan as stuck and report 0 RPM
#define FANSTUCK_THRESHOLD 500

//
// Interrupt handler routine for determining tachometer frequency
//
unsigned long volatile ts1 = 0, ts2 = 0;
void tachometer_isr() {
  unsigned long m = millis();
  if ((m - ts2) > DEBOUNCE) {
    ts1 = ts2;
    ts2 = m;
  }
}

//
// Calculates the RPM/freq.
//
void tachometer_calculate(struct diffs *d, unsigned int *rpm, unsigned long *mfreq) {
  // RPM: 2 pulses per rotation as per
  // https://noctua.at/pub/media/wysiwyg/Noctua_PWM_specifications_white_paper.pdf
  *rpm = round_division((unsigned long)60 * 1000, 2 * d->avg);
  // mfreq = milliHz
  *mfreq = round_division((unsigned long)1000 * 1000, d->avg);

#ifdef DEBUG
  Serial.print(" (");
  Serial.print(d->len);
  Serial.print(", ");
  Serial.print(d->sum);
  Serial.print(", ");
  Serial.print(d->avg);
  Serial.print(") ");
  Serial.print("RPM:");
  Serial.print(*rpm);
  Serial.print(", mfreq:");
  Serial.println(*mfreq);
#endif
}


static struct diffs isr_diffs;
static struct diffs pulsein_diffs;

void setup() {
  // enable serial so we can see the RPM in the serial monitor
  Serial.begin(115200);
  Serial.println("Hello World");

  // DEBUG code:
  //unsigned long mfreq;
  //tachometer_out(mfreq = 25000);
  //unsigned int duty_percent;
  //fan_pwm_control(duty_percent = 50);

  // set the sense pin as input with pullup resistor as the sense
  // signal is "open drain" type.
  //pinMode(PIN_SENSE, INPUT_PULLUP);

#ifdef USE_INTERRUPT
  // Set ISR to be triggered when the signal on the sense pin goes low
  // Keeps IRQ/ISR running continously, this affects power.
  attachInterrupt(digitalPinToInterrupt(PIN_SENSE), tachometer_isr, FALLING);
#endif

  diff_reset(&isr_diffs);
  diff_reset(&pulsein_diffs);
}


static unsigned long old_mfreq = 0;
void loop() {
  // Sleep between measurements...
  delay(1000);

  unsigned int new_rpm = 0;
  unsigned long new_mfreq = 0;

#ifdef USE_INTERRUPT
  Serial.print("Input :");

  // Calculating the input frequency
  noInterrupts();
  unsigned long _ts2 = ts2;
  unsigned long _ts1 = ts1;
  interrupts();
  if (((millis() - _ts2) < FANSTUCK_THRESHOLD) && (_ts2 != 0)) {
    unsigned long diff = _ts2 - _ts1;
    diff_add(&isr_diffs, diff);
    tachometer_calculate(&isr_diffs, &new_rpm, &new_mfreq);
  }
  Serial.print("\t\t");

  Serial.print("\tFreq (mHz): ");
  Serial.print(new_mfreq);

  Serial.print("\tFreq (Hz): ");
  Serial.print(round_division(new_mfreq, 1000));

  Serial.print("\tRPM: ");
  Serial.println(new_rpm);

#else
  // Method of calculating the input frequency without interrupts
  unsigned int fan_stuck = 0;

  unsigned long high = 0;
  if (!fan_stuck) {
    // timeout in us, defines minimum measurable frequency
    high = pulseIn(PIN_SENSE, HIGH, 100000 /*us*/);
    if (high == 0) {
      fan_stuck = 1;
    }
  }

  unsigned long low = 0;
  if (!fan_stuck) {
    // timeout in us, defines minimum measurable frequency
    low = pulseIn(PIN_SENSE, LOW, 100000 /*us*/);
    if (low == 0) {
      fan_stuck = 1;
    }
  }

  if (fan_stuck) {
    Serial.println("fan stuck...");
    new_rpm = new_mfreq = 0;
    //    } else if (high < 1000 || low < 1000) {
  } else if ((high + low) < 1000) {
    // this implies frequencies of >1kHz which is not expected
    // from a PC fan.
    Serial.print("spurious fan tachometer readings (us): ");
    Serial.print(high);
    Serial.print(", ");
    Serial.println(low);
    new_rpm = new_mfreq = 0;
  } else {
    // round() convert from us to ms
    unsigned long diff = round_division(high + low, 1000);
    diff_add(&pulsein_diffs, diff);
    tachometer_calculate(&pulsein_diffs, &new_rpm, &new_mfreq);
  }
#endif


#if defined(SERIAL_INPUT)
  if (Serial.available()) {
    // clear averaging history
    diff_reset(&isr_diffs);
    diff_reset(&pulsein_diffs);

#if SERIAL_INPUT == SERIAL_INPUT_TACH_OUT
    uint32_t mfreq = Serial.readString().toInt();
    tachometer_out(mfreq);
#elif SERIAL_INPUT == SERIAL_INPUT_FAN_CONTROL
    uint32_t duty_percent = Serial.readString().toInt();
    fan_pwm_control(duty_percent);
#endif
  }
#endif

#if !defined(SERIAL_INPUT) || SERIAL_INPUT == SERIAL_INPUT_FAN_CONTROL

  if (old_mfreq != new_mfreq) {
    old_mfreq = new_mfreq;

    // multiply the output frequency
    new_mfreq *= 4;
    new_rpm *= 4;

    Serial.print("Output:\t\t");

    Serial.print("\tFreq (mHz): ");
    Serial.print(new_mfreq);

    Serial.print("\tFreq (Hz): ");
    // NOTE:
    // Due to rounding output_freq will not always appear
    // to be an exakt multiple of input_freq
    Serial.print(round_division(new_mfreq, 1000));

    Serial.print("\tRPM: ");
    Serial.println(new_rpm);
    Serial.println("");

    tachometer_out(new_mfreq);
  }
#endif
}
