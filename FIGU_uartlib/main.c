//FIGU 430 w uart lib
/*
 * protokol
 *
 * FMN    - Figu Mirror oN
 * FMF    - Figu Mirrof oF
 * FG00 - FG99 - Figu Gain 00 - 99 %
 * FE0 - FEF - Figu Exposure 0 - F
 *
 * odpovedá FIGU + to čo dostal
 *
 * ako dostal iné, odpovedá ERRf, ERRm, ERRg, ERRe
 *
 *
 */

#include <msp430.h> 
#include <msp430g2553.h>
#include <string.h>
#include "uart.h"
#include "ringbuf.h"

#define RXD        BIT1 //Check your launchpad rev to make sure this is the case. Set jumpers to hardware uart.
#define TXD        BIT2
//#define BUTTON    BIT3
#define PWM_OUT_G   BIT6    // 1.6  - pwm pre gain
#define EXP_OUT_0   BIT0    //1.0 , 1.3 - 1.5  pre expoziciu  - BIT1 - 2.1 na test na vyvojovej doske - 2.1, 2.3, 2.5 - RGB led - na vyvojovu nezabudnuť opraviť aj ďalej v kode porty 2
#define EXP_OUT_1   BIT3
#define EXP_OUT_2   BIT4
#define EXP_OUT_3   BIT5
#define MIR_OUT   BIT7  //p.1.7 pre zrkadlo //1.0 na vyvojovej doske

#define PEG 20;           // perioda pre gain pwm


char serial_in[9]={0};
unsigned int serial_ar = 0;
int xflag = 0;
int dec =0;
int jed =0;

int gain = 0;
//char * data_in;
char * eof = "#";

char g[2];
char *g_out;

void FIGU_run();

int main(void)

{
    //clock setup
    WDTCTL = WDTPW + WDTHOLD; //Disable the Watchdog timer for our convenience.
    BCSCTL1 = CALBC1_16MHZ;            // Set DCO to 8 MHz
    DCOCTL = CALDCO_16MHZ;
    //BCSCTL2 = DIVS_3;                   //deliÄ�ka na SMCLK 1:8 - pre Timer
    //UART setup
    P1SEL = RXD + TXD ;                // Select TX and RX functionality for P1.1 & P1.2
    P1SEL2 = RXD + TXD ;              //
    UCA0CTL1 |= UCSSEL_2;             // Have USCI use System Master Clock: AKA core clk 8MHz
    UCA0BR0 = 130;                    // 8MHz 9600, see user manual
    UCA0BR1 = 6;                      //pre 8MHZ je UCA0BR0 833, je to viac ako 8 bit, delÃ­ sa to do tÃ½ch dvoch registrov 65,3 pre 16M 130,6
    UCA0MCTL = UCBRS1;                // Modulation UCBRSx = 1 - hmmm? - funguje to aj tak, asi je to teda ok (predtÃ½m bolo UCBRS0)
    UCA0CTL1 &= ~UCSWRST;
    IE2 |= UCA0RXIE;                    //uart RX interrupt enable

    // out bit setup
    P1DIR |= MIR_OUT;
    P1OUT |= MIR_OUT;

    P1DIR |= EXP_OUT_0 + EXP_OUT_1 + EXP_OUT_2 + EXP_OUT_3;
    P1OUT |= EXP_OUT_0 + EXP_OUT_1 + EXP_OUT_2 + EXP_OUT_3;

    //timer setup
    P1DIR |= PWM_OUT_G; //+ PWM_OUT_M; //Set pin 2.4 to the output direction.
    P1SEL |= PWM_OUT_G; //+ PWM_OUT_M; //Select pin 2.4 as our PWM output.
    TA0CCR0 = 99; //Set the period in the Timer A1 Capture/Compare 0 register to daÄ�o daÄ�o. CCR0 na nastavenie f ,ostatne CCRx na ine
                    // 99 - nastavené aby sa to jednoducho rátalo  a aby mohla byť aj nula, lebo logika je naopak pre optočleny
    //TA0CCTL2 = OUTMOD_7; //+ CCIE; //+ CCIS_1;
    TA0CCTL1 = OUTMOD_7;
    //TA1CCR2 = PER; //The period in ?? that the power is ON.
    TA0CCR1 = PEG;      //čas kedy je pin PWM_OUT_G na 1 - teda šírka pulzu
    TA0CTL = TASSEL_2 + MC_1; //TASSEL_2 selects SMCLK as the clock source, and MC_1 tells it to count up to the value in TA0CCR0.

    //TA0CTL = TASSEL_2 + ID_3 + TAIE ;   // ID_3 - delička hodín­n 1:8
    //TA0CCR0 = 50000;
    //TA0CCR1 = 40000;
    //TA0CCTL0 = CCIE;
    //TA0CCTL1 = CAP + CCIE;
    //TA0CTL |= MC_1;
    __bis_SR_register(GIE); //+ LPM0_bits); //Switch to low power mode 0.

    while (1)
    {
        serial_in[serial_ar] = uart_getc();

        if (serial_in[serial_ar]==*eof)
           {//xflag=1;
            serial_ar=0;
            uart_puts("FIGU#\r\n");
            uart_flush();
            FIGU_run();

            }
        else
            {    serial_ar++;
            }
    }
}

    // if ((xflag == 1) &( serial_in[0] == 'F'))
    //  { xflag = 0;
    //  uart_puts("FIGU#\r\n");
void FIGU_run()
{
        switch  (serial_in[1])
        {
        default:uart_puts("ERRf#\r\n");
                xflag = 0;
               break;
        case 'M':

            if (serial_in[2]=='N')
            {
                   P1OUT &= ~MIR_OUT;               //na FIGU doske su optočleny čo otačaju logiku  ...teda 0 = High level
                   uart_puts("FMN#\r\n");
            }
            else if (serial_in[2]=='F')
            {
                P1OUT |= MIR_OUT;

                uart_puts("FMF#\r\n");
            }
            else {uart_puts("ERRm#\r\n");}
            break;

        case 'G':

            dec = serial_in[2]-'0';
            jed = serial_in[3]-'0';
            if (((dec - 9)<1)&((jed - 9)<1))
            {TA0CCR1 = 99-(10 * dec + jed);  //optočlen obracia logiku, preto keď príde 99 nebude to 99% gainu, ale nula,
            gain = TA0CCR1;
            uart_puts("FG");
            //uart_puts(gain);

            g[0]= (char)(dec+48);
            g[1]= (char)(jed+48);
                            g_out = g;
                            uart_puts(g_out);
                            uart_puts("#\r\n");
                            memset(g_out, 0, sizeof(g_out));
            }
            else
            {uart_puts("ERRg#\r\n");
            }
            break;

        case 'E':
            //uart_puts("WATEC exposure changed to \r\n");
            switch (serial_in[2])                   //prepísané poradie v hex aby to sedelo s výstupom - viď vyššie logika opačna
            {
            default: uart_puts("ERRe#\r\n");
                    break;

            case 'F': P1OUT &= ~EXP_OUT_0;
                    P1OUT &= ~EXP_OUT_1;
                    P1OUT &= ~EXP_OUT_2;
                    P1OUT &= ~EXP_OUT_3;
                    uart_puts("FEF#\r\n");
                    break;
            case 'E': P1OUT |= EXP_OUT_0;
                                P1OUT &= ~EXP_OUT_1;
                                P1OUT &= ~EXP_OUT_2;
                                P1OUT &= ~EXP_OUT_3;
                                uart_puts("FEE#\r\n");
                                break;
            case 'D': P1OUT &= ~EXP_OUT_0;
                                P1OUT |= EXP_OUT_1;
                                P1OUT &= ~EXP_OUT_2;
                                P1OUT &= ~EXP_OUT_3;
                                uart_puts("FED#\r\n");
                                break;
            case 'C': P1OUT |= EXP_OUT_0;
                                P1OUT |= EXP_OUT_1;
                                P1OUT &= ~EXP_OUT_2;
                                P1OUT &= ~EXP_OUT_3;
                                uart_puts("FEC#\r\n");
                                break;
            case 'B': P1OUT &= ~EXP_OUT_0;
                                P1OUT &= ~EXP_OUT_1;
                                P1OUT |= EXP_OUT_2;
                                P1OUT &= ~EXP_OUT_3;
                                uart_puts("FEB#\r\n");
                                break;
            case 'A': P1OUT |= EXP_OUT_0;
                                            P1OUT &= ~EXP_OUT_1;
                                            P1OUT |= EXP_OUT_2;
                                            P1OUT &= ~EXP_OUT_3;
                                            uart_puts("FEA#\r\n");
                                            break;
            case '9': P1OUT &= ~EXP_OUT_0;
                                            P1OUT |= EXP_OUT_1;
                                            P1OUT |= EXP_OUT_2;
                                            P1OUT &= ~EXP_OUT_3;
                                            uart_puts("FE9#\r\n");
                                            break;
            case '8': P1OUT |= EXP_OUT_0;
                                            P1OUT |= EXP_OUT_1;
                                            P1OUT |= EXP_OUT_2;
                                            P1OUT &= ~EXP_OUT_3;
                                            uart_puts("FE8#\r\n");
                                            break;
            case '7': P1OUT &= ~EXP_OUT_0;
                                            P1OUT &= ~EXP_OUT_1;
                                            P1OUT &= ~EXP_OUT_2;
                                            P1OUT |= EXP_OUT_3;
                                            uart_puts("FE7#\r\n");
                                            break;
            case '6': P1OUT |= EXP_OUT_0;
                        P1OUT &= ~EXP_OUT_1;
                        P1OUT &= ~EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE6#\r\n");
                                                        break;
            case '5': P1OUT &= ~EXP_OUT_0;
                                                        P1OUT |= EXP_OUT_1;
                                                        P1OUT &= ~EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE5#\r\n");
                                                        break;
            case '4': P1OUT |= EXP_OUT_0;
                                                        P1OUT |= EXP_OUT_1;
                                                        P1OUT &= ~EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE4#\r\n");
                                                        break;
            case '3': P1OUT &= ~EXP_OUT_0;
                                                        P1OUT &= ~EXP_OUT_1;
                                                        P1OUT |= EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE3#\r\n");
                                                        break;
            case '2': P1OUT |= EXP_OUT_0;
                                                        P1OUT &= ~EXP_OUT_1;
                                                        P1OUT |= EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE2#\r\n");
                                                        break;
            case '1': P1OUT &= ~EXP_OUT_0;
                                                        P1OUT |= EXP_OUT_1;
                                                        P1OUT |= EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE1#\r\n");
                                                        break;
            case '0': P1OUT |= EXP_OUT_0;
                                                        P1OUT |= EXP_OUT_1;
                                                        P1OUT |= EXP_OUT_2;
                                                        P1OUT |= EXP_OUT_3;
                                                        uart_puts("FE0#\r\n");
                                                        break;
             }


            break;

        }
        //memset(data_in, 0, sizeof(data_in));
        memset(serial_in, 0, sizeof(serial_in));
     }
    // else
    // { }// uart_puts("kompjuter, poslal si dobry prikaz???"); }
     //IFG2 &= ~UCA0RXIFG;

 //   } //koniec while
//}// konec main

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER_A (void)
    {

    P1OUT ^= PWM_OUT_G;

    TA0CTL &= ~TAIFG;

    }








