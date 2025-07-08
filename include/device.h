#include "constants.h"

#ifndef DEVICE_CONFIGURATION
#define DEVICE_CONFIGURATION

// CPU settings
#ifndef TARGET_CPU
    #define TARGET_CPU m2560p
#endif

#ifndef F_CPU
    #define F_CPU 16000000
#endif

#ifndef FREQUENCY_CORRECTION
    #define FREQUENCY_CORRECTION 0
#endif

// Sampling & timer setup3
#define CONFIG_AFSK_DAC_SAMPLERATE 9600

// Port settings
#if TARGET_CPU == m2560p
    #define DAC_PORT PORTA
    #define DAC_DDR  DDRA
    //#define LED_PORT PORTB
    //#define LED_DDR  DDRB
    #define ADC_PORT PORTF
    #define ADC_DDR  DDRF
#endif

#endif
