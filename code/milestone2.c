/*
Stephen Glass
milestone2
MSP430FR6989
*/

#include <msp430.h>
#include "LCDDriver.h"

/* [Damon@19:06_11/25/17]: See line 44-46.
#define FRSERIES        1   // Using an FR series msp430
*/

#define MCLK_FREQ_MHZ   8   // MCLK = 8MHz

void initGPIO();
void initializeTimer(int capture);
void initADC();
void LCDInit();
void buttonInit();
void initLeds(void);
void initPWM(void);
void setTemperature(int temp);
int getTemperatureDuty(int temp);
void updatePWM(float currentTemp, int desiredTemp);

int ADC_check;
int Pot_check;
int ADCdisplay = 1;

unsigned int pauseUpdate = 0;
long ADC_Result;
long Pot_Result;

float currentTemp;
int desiredTemp;

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                                 // Stop WDT

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    #if defined(FRSERIES)
        PM5CTL0 &= ~LOCKLPM5;
    #endif

    initGPIO();                                              // Configure GPIO
    LCDInit();
    initADC();
    buttonInit();
    initLeds();
    initPWM();
    __delay_cycles(400);                                     // Delay for reference settling
    initializeTimer(4);                                     // 4 Hertz timer

    //setTemperature(62);

    __bis_SR_register(LPM0_bits | GIE);                     // LPM0, ADC_ISR will force exit
    __no_operation();                                       // Debugging
}

void initGPIO(void)
{
    // GPIO Setup
    P1OUT &= ~BIT0;                         // Clear LED to start
    P1DIR |= BIT0;                          // Set P1.0/LED to output
}

void buttonInit(void)
{
    P1REN |= (BIT1);         // puller-resistor on the button pin P1.1
    P1OUT |= (BIT1);         // writes "1" to portpin, telling the resistor to pullup
    P1IES |= (BIT1);         // triggers when you press the button, "~BIT3" for when release
    P1IE |= (BIT1);          // enables selector-mask for generating interrupts on the pin
    __enable_interrupt();    // enable interrupts
}

// Port 1 interrupt service routine
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void) // function called when the port is interrupted (button pressed)
{
    pauseUpdate = !pauseUpdate;
    P1IFG &= ~BIT1; // P1.1 (button) IFG cleared
}

void initADC(void)
{
    // Configure ADC A4 (P8.7) pin
    P8SEL0 |= BIT7;
    P8SEL1 |= BIT7;
	
	// Configure ADC A5 (P8.6) pin
    P8SEL0 |= BIT6;
    P8SEL1 |= BIT6;

    // By default, REFMSTR=1 => REFCTL is used to configure the internal reference
    while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
    REFCTL0 |= REFVSEL_0 | REFON;           // Select internal ref = 1.2V
                                            // Internal Reference ON

    // Configure ADC12
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON + ADC12MSC;
    ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1;                   // ADCCLK = MODOSC; sampling timer
    ADC12CTL2 |= ADC12RES_2;                // 12-bit conversion results
    ADC12IER0 |= ADC12IE0;                  // Enable ADC conv complete interrupt
    ADC12MCTL0 |= ADC12INCH_4 | ADC12VRSEL_1; // A4 ADC input select; Vref=1.2V
    ADC12MCTL1 |= ADC12INCH_5 + ADC12EOS | ADC12VRSEL_0; // A5 ADC input select; Vref=1.2V
    //ADC12IE = 0x08; // Enable ADC12IFG.3
    ADC12CTL0 |= ADC12ENC;

    while(!(REFCTL0 & REFGENRDY));          // Wait for reference generator to settle
}

void initializeTimer(int hertz) // Seconds = 1/Hertz, 10Hz = 0.1s
{
    TA1CCTL0 = CCIE;
    TA1CTL = TASSEL_2 + MC_1 + ID_3; // SMCLK/8, UPMODE
    // CLK/HERTZ = CAPTURE
    // CLK = 1MHZ/8 = 125kHz
    int capture = (125000)/hertz;
    TA1CCR0 = capture; // (1000000/8)/(12500) = 10 Hz = 0.1 seconds
}

void updatePWM(float currentTemp, int desiredTemp)
{
    if (desiredTemp >= 75 && currentTemp <=75)
        TA0CCR1 = 0;
    else if(desiredTemp < currentTemp + 3)
    {
        // Desired = 70, Current = 50
        if(currentTemp - desiredTemp >= 1) TA0CCR1 = getTemperatureDuty(desiredTemp) + (currentTemp-desiredTemp)*2.5;
        else TA0CCR1 = getTemperatureDuty(desiredTemp) + (currentTemp-desiredTemp)*0.5;
    }
    else
    {
        if(desiredTemp - currentTemp > 2) TA0CCR1 = 0;
        else TA0CCR1 = 10;
    }
}

// Timer A0 interrupt service routine
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer0_A1 (void)
{
    if(pauseUpdate) // IF the button was pressed to pause updating
    {
        ADC12CTL0 |= ADC12SC;    // Sampling and conversion start

        /* Reset the LCD display before updating again */
        showChar(' ', 1);
        showChar(' ', 2);
        showChar(' ', 3);
        showChar(' ', 4);
        showChar(' ', 5);
        showChar(' ', 6);

        //T = ADC_Result * [(1.2/4096) * (1/0.01)]

        currentTemp = (ADC_Result*0.02929688);
        ADC_check = (int)currentTemp;
        ADCdisplay = 2;
        while (ADC_check > 0)
        {
            int digit = ADC_check % 10;
            if(digit == 0) showChar('0', ADCdisplay);
            else if(digit == 1) showChar('1', ADCdisplay);
            else if(digit == 2) showChar('2', ADCdisplay);
            else if(digit == 3) showChar('3', ADCdisplay);
            else if(digit == 4) showChar('4', ADCdisplay);
            else if(digit == 5) showChar('5', ADCdisplay);
            else if(digit == 6) showChar('6', ADCdisplay);
            else if(digit == 7) showChar('7', ADCdisplay);
            else if(digit == 8) showChar('8', ADCdisplay);
            else if(digit == 9) showChar('9', ADCdisplay);
            ADCdisplay--;
            ADC_check /= 10;
        }

        Pot_check = (int)(Pot_Result*0.0244);
        desiredTemp = Pot_check;
        ADCdisplay = 6;
        if(Pot_check == 0)
        {
            showChar('0', 6);
        }
        else
        {
            while (Pot_check > 0)
            {
                int digit = Pot_check % 10;
                if(digit == 0) showChar('0', ADCdisplay);
                else if(digit == 1) showChar('1', ADCdisplay);
                else if(digit == 2) showChar('2', ADCdisplay);
                else if(digit == 3) showChar('3', ADCdisplay);
                else if(digit == 4) showChar('4', ADCdisplay);
                else if(digit == 5) showChar('5', ADCdisplay);
                else if(digit == 6) showChar('6', ADCdisplay);
                else if(digit == 7) showChar('7', ADCdisplay);
                else if(digit == 8) showChar('8', ADCdisplay);
                else if(digit == 9) showChar('9', ADCdisplay);
                ADCdisplay--;
                Pot_check /= 10;
            }
        }

        updatePWM(currentTemp, desiredTemp);
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG))
    {
        case ADC12IV_NONE:        break;    // Vector  0:  No interrupt
        case ADC12IV_ADC12OVIFG:  break;    // Vector  2:  ADC12MEMx Overflow
        case ADC12IV_ADC12TOVIFG: break;    // Vector  4:  Conversion time overflow
        case ADC12IV_ADC12HIIFG:  break;    // Vector  6:  ADC12BHI
        case ADC12IV_ADC12LOIFG:  break;    // Vector  8:  ADC12BLO
        case ADC12IV_ADC12INIFG:  break;    // Vector 10:  ADC12BIN
        case ADC12IV_ADC12IFG0:             // Vector 12:  ADC12MEM0 Interrupt
            ADC_Result = ADC12MEM0;
            Pot_Result = ADC12MEM1;
            break;
        case ADC12IV_ADC12IFG1:   break;    // Vector 14:  ADC12MEM1
        case ADC12IV_ADC12IFG2:   break;    // Vector 16:  ADC12MEM2
        case ADC12IV_ADC12IFG3:   break;    // Vector 18:  ADC12MEM3
        case ADC12IV_ADC12IFG4:   break;    // Vector 20:  ADC12MEM4
        case ADC12IV_ADC12IFG5:   break;    // Vector 22:  ADC12MEM5
        case ADC12IV_ADC12IFG6:   break;    // Vector 24:  ADC12MEM6
        case ADC12IV_ADC12IFG7:   break;    // Vector 26:  ADC12MEM7
        case ADC12IV_ADC12IFG8:   break;    // Vector 28:  ADC12MEM8
        case ADC12IV_ADC12IFG9:   break;    // Vector 30:  ADC12MEM9
        case ADC12IV_ADC12IFG10:  break;    // Vector 32:  ADC12MEM10
        case ADC12IV_ADC12IFG11:  break;    // Vector 34:  ADC12MEM11
        case ADC12IV_ADC12IFG12:  break;    // Vector 36:  ADC12MEM12
        case ADC12IV_ADC12IFG13:  break;    // Vector 38:  ADC12MEM13
        case ADC12IV_ADC12IFG14:  break;    // Vector 40:  ADC12MEM14
        case ADC12IV_ADC12IFG15:  break;    // Vector 42:  ADC12MEM15
        case ADC12IV_ADC12IFG16:  break;    // Vector 44:  ADC12MEM16
        case ADC12IV_ADC12IFG17:  break;    // Vector 46:  ADC12MEM17
        case ADC12IV_ADC12IFG18:  break;    // Vector 48:  ADC12MEM18
        case ADC12IV_ADC12IFG19:  break;    // Vector 50:  ADC12MEM19
        case ADC12IV_ADC12IFG20:  break;    // Vector 52:  ADC12MEM20
        case ADC12IV_ADC12IFG21:  break;    // Vector 54:  ADC12MEM21
        case ADC12IV_ADC12IFG22:  break;    // Vector 56:  ADC12MEM22
        case ADC12IV_ADC12IFG23:  break;    // Vector 58:  ADC12MEM23
        case ADC12IV_ADC12IFG24:  break;    // Vector 60:  ADC12MEM24
        case ADC12IV_ADC12IFG25:  break;    // Vector 62:  ADC12MEM25
        case ADC12IV_ADC12IFG26:  break;    // Vector 64:  ADC12MEM26
        case ADC12IV_ADC12IFG27:  break;    // Vector 66:  ADC12MEM27
        case ADC12IV_ADC12IFG28:  break;    // Vector 68:  ADC12MEM28
        case ADC12IV_ADC12IFG29:  break;    // Vector 70:  ADC12MEM29
        case ADC12IV_ADC12IFG30:  break;    // Vector 72:  ADC12MEM30
        case ADC12IV_ADC12IFG31:  break;    // Vector 74:  ADC12MEM31
        case ADC12IV_ADC12RDYIFG: break;    // Vector 76:  ADC12RDY
        default: break;
    }
}

void LCDInit()
{
    PJSEL0 = BIT4 | BIT5;                   // For LFXT

    // Initialize LCD segments 0 - 21; 26 - 43
    LCDCPCTL0 = 0xFFFF;
    LCDCPCTL1 = 0xFC3F;
    LCDCPCTL2 = 0x0FFF;

    // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    // Configure LFXT 32kHz crystal
    CSCTL0_H = CSKEY >> 8;                  // Unlock CS registers
    CSCTL4 &= ~LFXTOFF;                     // Enable LFXT
    do
    {
      CSCTL5 &= ~LFXTOFFG;                  // Clear LFXT fault flag
      SFRIFG1 &= ~OFIFG;
    }while (SFRIFG1 & OFIFG);               // Test oscillator fault flag
    CSCTL0_H = 0;                           // Lock CS registers

    // Initialize LCD_C
    // ACLK, Divider = 1, Pre-divider = 16; 4-pin MUX
    LCDCCTL0 = LCDDIV__1 | LCDPRE__16 | LCD4MUX | LCDLP;

    // VLCD generated internally,
    // V2-V4 generated internally, v5 to ground
    // Set VLCD voltage to 2.60v
    // Enable charge pump and select internal reference for it
    LCDCVCTL = VLCD_1 | VLCDREF_0 | LCDCPEN;

    LCDCCPCTL = LCDCPCLKSYNC;               // Clock synchronization enabled

    LCDCMEMCTL = LCDCLRM;                   // Clear LCD memory
    //Turn LCD on
    LCDCCTL0 |= LCDON;
}

void initLeds(void)
{
    P1DIR |= (BIT6);     // P2.0 output
    P1SEL0 |= (BIT6);     // P2.0 to TA1.1
    P1SEL1 |= (BIT6);     // P2.0 to TA1.1
}

void initPWM(void)
{
    TA0CTL = (MC__UP + TASSEL__SMCLK); // Configure TA0: Upmode using 1MHz clock / 4 = 250k
    TA0CCR0 = 100; // 250k / 255 = ~1kHz, set compare to 255

    TA0CCTL1 = OUTMOD_7;
    TA0CCR1  = 0; // Fan PWM @ P1.6
}

void setTemperature(int temp)
{
    int duty = 0;
    //y = -36.485x + 75.267
    if(temp >= 54) // first piecewise function
    {
        duty = (int)(-1.754386*(temp-81.3));
    }
    else
    {
        duty = (int)(-5.263158*(temp-61.4));
    }
    TA0CCR1 = duty;
}

int getTemperatureDuty(int temp)
{
    int duty = 0;
    //y = -36.485x + 75.267
    if(temp >= 54) // first piecewise function
    {
        duty = (int)(-1.754386*(temp-81.3));
    }
    else
    {
        duty = (int)(-5.263158*(temp-61.4));
    }
    return duty;
}
