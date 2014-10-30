#include <msp430.h>
//#include <string.h>

void Serial_config(void);
void Serial_escreve_dado(char dado);
void Serial_escreve_texto(char *caracter);
void itoa(long unsigned int inteiro, char* string);

#define	SENSOR BIT3 // P1.3

#define LED0 BIT0 // P1.0
#define LED1 BIT6 // P1.6

#define RX_pin BIT1 // P1.1
#define TX_pin BIT2 // P1.2

unsigned int timer_cycles = 0;

unsigned int passadas = 0;

char texto[] = "";

void main(void){
	  
	WDTCTL = WDTPW + WDTHOLD;
  
	BCSCTL1 = CALBC1_8MHZ;
	DCOCTL = CALDCO_8MHZ;
  
	P1DIR = LED0 + LED1;

	P1REN = P1OUT = SENSOR;
	
	P1IE = P1IES = SENSOR;
	P1IFG = 0;
	
	TA0CTL = TASSEL_2 + ID_3 + MC_1;
	TACCTL0	|= CCIE;
	TACCR0 = 1000; // 1ms

	Serial_config();

	__enable_interrupt();

	while(1);
}

#pragma vector = PORT1_VECTOR
__interrupt void int_P1(void){

	if(P1IFG & SENSOR){
	
		P1OUT ^= LED1;

		itoa(++passadas, texto);
		Serial_escreve_texto(texto);
		Serial_escreve_dado('\n');

		__delay_cycles(40000); // 5ms
		while(!(P1IN & SENSOR));
		__delay_cycles(40000);

		P1IFG &= ~SENSOR;
	}
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void int_timer_A(void){

	if(++timer_cycles == 1000){ // 1s
		timer_cycles = 0;
		P1OUT ^= LED0;
	}

	TACCTL0 &= ~CCIFG;
}

#pragma vector = USCIAB0RX_VECTOR
__interrupt void Serial_receive(void){

	char dado = UCA0RXBUF;
		
}

void Serial_config(void){
	P1SEL |= (RX_pin + TX_pin);
	P1SEL2 |= (RX_pin + TX_pin);

	UCA0CTL1 |= UCSWRST;

	UCA0CTL0 &= ~(UCMODE1 + UCMODE0 + UCSYNC);

	UCA0CTL1 |= UCSSEL_2;

	UCA0MCTL &= ~UCOS16;

	UCA0BR1 = 0x03;
	UCA0BR0 = 0x41;
	UCA0MCTL |= 0x06;

	UCA0CTL1 &= ~UCSWRST;

	IE2 |= UCA0RXIE;
}

void Serial_escreve_dado(char dado){
	while(!(IFG2 & UCA0TXIFG));
	UCA0TXBUF = dado;
}

void Serial_escreve_texto(char *caracter){
	while(*caracter){
		Serial_escreve_dado(*caracter);
		caracter++;
	}
}

void itoa(long unsigned int inteiro, char* string){
    // http://www.strudel.org.uk/itoa/
    
    char* ptr = string, *ptr1 = string, tmp_char;
    int tmp_inteiro;

    do {
        tmp_inteiro = inteiro;
        inteiro /= 10;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_inteiro - inteiro * 10)];
    } while ( inteiro );
    
    if (tmp_inteiro < 0) *ptr++ = '-';
    
    *ptr-- = '\0';
    
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
}