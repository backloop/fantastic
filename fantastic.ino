#include <assert.h>
#include <limits.h>
/*
  fantastic: ATmega fan tachometer multiplier

  Purpose:
  - Read the low-frequency tachometer signal from a Noctua PWM fan.
  - Multiply measured tachometer frequency by 4.
  - Re-generate a stable 50% tachometer waveform on OC1A (pin 9) for the motherboard.

  Why:
  - Some server motherboards intermittently read 0 RPM for very low-speed fans.
  - The multiplied tach signal moves effective RPM into a range with fewer false alarms.

  See README.md for system background, IPMI threshold examples, measurements,
  and hardware-level integration notes.
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
