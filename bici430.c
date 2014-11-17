/*

    Projeto BICI430
    registrador de dados para bicicletas, integrado a um celular móvel

    Autores:
        Marilson Reque - Física @UFPR
        Wendeurick Silverio - Eng. Elétrica @UFPR

    A fazer:
        - formatação do tempo: hh:mm:ss
        - display SPI
        - traduzir para o inglês (?)
        - comentar as funções
*/

#include <msp430.h>
#include <string.h>

// protótipo das funções
void Serial_config(void);
void Serial_escreve_dado(char dado);
void Serial_escreve_texto(char *caracter);
void itoa(long unsigned int inteiro, char* string);
void ftoa(float n, char *res, int casas);
float _pow(float base, float expoente);
void delay_ms(unsigned int ms);

// definição das constantes
// menu
#define NOTIFICACAO 0
#define DATA_LOGGER 1
// bicicleta
#define R 0.45
#define PI 3.1416
#define CIRC 1000.0 * 2 * PI * R // 1000: dt -> ms
// periféricos
#define BOTAO BIT3 // botao conectado ao pino P1.3
#define SENSOR BIT4 // sensor conectado ao P1.4
#define CHAVE BIT5 // chave conectada ao P1.5
#define LED0 BIT0 // led vermelho conectado ao P1.0
#define LED1 BIT6 // led verde conectado ao P1.6
#define RX_pin BIT1 // pino de recepção serial P1.1
#define TX_pin BIT2 // pino de transmissão serialP1.2

// definição das variáveis
float vel = 0;
unsigned int voltas = 0; // número de voltas da roda; reset em 65535 ~= 185km
unsigned int dt = 0; // intervalo de tempo 'dt'; reset em ~= 65s
unsigned long tempo = 0; // tempo percorrido, em ms; reset em ~= 50 dias
unsigned char timeout = 0; // timeout para a velocidade

unsigned char menu = DATA_LOGGER;
unsigned char pausa = 1;

char valor_str[10]; // string de uso geral

char notificacao[84]; // 6*14
char *string = notificacao;

unsigned char notif_receive = 0; // indicador de "pronto para nova notificação"

void main(void){
      
    // desativa watchdog timer
    WDTCTL = WDTPW + WDTHOLD;
  
    // configura o clock @8MHz
    BCSCTL1 = CALBC1_8MHZ;
    DCOCTL = CALDCO_8MHZ;
  
    // configura as saídas do P1
    P1DIR = LED0 + LED1;

    // habilita o resistor pullup
    P1REN = SENSOR + BOTAO + CHAVE;
    
    // ativa o resistor
    P1OUT = SENSOR + BOTAO + CHAVE;
    
    // configura a interrupção por borda de descida
    P1IE = P1IES = SENSOR + BOTAO;
    
    // limpa as interrupções do P1
    P1IFG = 0;
    
    // configura o timer A0
    TA0CTL = TASSEL_2 + ID_3 + MC_1; // smclk, div 8, up CCR0
    TACCTL0 |= CCIE; // interrupção por comparação
    TACCR0 = 1000; // @8MHz -> 1ms

    // configura a comunicação serial uart
    // 9600 bps, 8 bits, sem paridade
    Serial_config();

    // LCD print pressione o botao
    // Aguarda o usuário
    while(P1IN & BOTAO);

    // Pisca os leds
    for (char i = 0; i < 5; ++i){
        P1OUT |= LED0 + LED1;
        delay_ms(100);
        P1OUT &= ~(LED0 + LED1);
        delay_ms(100);
    }

    // habilita as interrupções
    __enable_interrupt();

    // solicita nova notificação
    Serial_escreve_dado('1');

    // loop
    while(1){
        if (menu == NOTIFICACAO && !(P1IN & CHAVE)){
            if (notif_receive){ // notificação recebida
                Serial_escreve_texto(notificacao);
                notif_receive = 0;
				string = notificacao; // retorna o ponteiro para a primeira posição
            }
        }else if (menu == DATA_LOGGER && !pausa){
			
			if(timeout > 5){
                // zera a velocidade após 5s se não houver leitura do sensor
				vel = timeout = 0;
			}
            
			// cálculo da velocidade média
            float vel_med = CIRC * (voltas)/tempo;
        
            // envia pela UART o valor da velocidade instantânea
            Serial_escreve_texto("\n\nvel: ");
            ftoa(vel, valor_str, 2);
            Serial_escreve_texto(valor_str);

            // envia pela UART o valor da velocidade média
            Serial_escreve_texto("\nvmed: ");
            ftoa(vel_med, valor_str, 2);
            Serial_escreve_texto(valor_str);

            // envia pela UART o valor da distância percorrida (em m)
            Serial_escreve_texto("\ndist: ");
            ftoa(CIRC * voltas / 1000.0, valor_str, 2);
            Serial_escreve_texto(valor_str);
        
            // envia pela UART o valor do tempo percorrido (em s)
            Serial_escreve_texto("\ntempo: ");
            itoa(tempo/1000, valor_str);
            Serial_escreve_texto(valor_str);
			
            // alterna o led verde
            P1OUT ^= LED1;
            delay_ms(1000);            
        }
    }
}

/* rotina de interrupção do P1 */
__attribute__((interrupt(PORT1_VECTOR)))
void int_P1(void){
//#pragma vector = PORT1_VECTOR
//__interrupt void int_P1(void){

    // interrupção do sensor
    if(P1IFG & SENSOR && !pausa){
        
        // cálculo da velocidade instantânea
        vel = CIRC/dt;
        dt = 0;
		
		voltas++;
		
		timeout = 0;

        // debouncing
        //__delay_cycles(8000); // 1ms
        //while(!(P1IN & SENSOR));
        //__delay_cycles(8000);
                
        // limpa flag de interrupção do sensor
        P1IFG &= ~SENSOR;

    }else if(P1IFG & BOTAO){

        if (menu == DATA_LOGGER){
            // inverte a 'pausa' e o led indicador
            P1OUT = (pausa = !pausa)?(P1OUT & ~LED0):(P1OUT | LED0);

        }else if(menu == NOTIFICACAO){
            // LCD limpa tela

            menu = DATA_LOGGER;
            
            // solicita nova notificação
            Serial_escreve_dado('1');
        }

        __delay_cycles(8000); // 1ms
        while(!(P1IN & BOTAO));
        __delay_cycles(8000);

        P1IFG &= ~BOTAO;
    }
}

/* rotina de interrupção do Timer A0, chamada a cada 1ms */
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void int_timer_A(void){
//#pragma vector = TIMER0_A0_VECTOR
//__interrupt void int_timer_A(void){

    if(!pausa){
        dt++;
        tempo++;
		
		if(!(tempo % 1000)) timeout++; // incrementa a cada 1s
    }

    // limpa flag de imterrupção do timer A0
    TACCTL0 &= ~CCIFG;
}

/* rotina de interrupção da serial, chamada a cada byte recebido */
__attribute__((interrupt(USCIAB0RX_VECTOR)))
void Serial_receive(void){
//#pragma vector = USCIAB0RX_VECTOR
//__interrupt void Serial_receive(void){
    // lê o dado recebido
    char dado = UCA0RXBUF;

    if (menu == NOTIFICACAO){
        if (dado == '\0'){ // fim da notificação
            notif_receive = 1;
			*string = '\0';
		}else{
			*string = dado; // concatena o caracter
			string++;
		}
    }
		
	//if(!(P1IN & CHAVE)) menu = NOTIFICACAO; //*******************
}

/* configuração da UART */
void Serial_config(void){
    // ativa as funções UART nos pinos
    P1SEL |= (RX_pin + TX_pin);
    P1SEL2 |= (RX_pin + TX_pin);
    
    // USCI reset: desabilitado para operacao
    UCA0CTL1 |= UCSWRST;
    
    // modo UART assíncrono
    UCA0CTL0 &= ~(UCMODE1 + UCMODE0 + UCSYNC);

    // USCI clock: modo 2 (SMCLK)
    UCA0CTL1 |= UCSSEL_2;

    // Oversampling desabilitado
    UCA0MCTL &= ~UCOS16;

    // configura baud rate para 9600 bps
    UCA0BR1 = 0x03;
    UCA0BR0 = 0x41;
    UCA0MCTL |= 0x06;

    // USCI reset: liberado para operação
    UCA0CTL1 &= ~UCSWRST;

    // habilita interrupção de recepção
    IE2 |= UCA0RXIE;
}

void Serial_escreve_dado(char dado){
    // aguarda buffer vazio
    while(!(IFG2 & UCA0TXIFG));
    
    // escreve o dado
    UCA0TXBUF = dado;
}

void Serial_escreve_texto(char *caracter){
    // percorre a string 'caracter' byte a byte
    while(*caracter){
        Serial_escreve_dado(*caracter);
        caracter++;
    }
}

/* converte o número inteiro em string */
// baseada em http://www.strudel.org.uk/itoa/
void itoa(long unsigned int inteiro, char* string){
    
    char* ptr = string, *ptr1 = string, tmp_char;
    int tmp_inteiro;

    do{
        tmp_inteiro = inteiro;
        inteiro /= 10;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_inteiro - inteiro * 10)];
    }while(inteiro);
    
    if(tmp_inteiro < 0) *ptr++ = '-';
    
    *ptr-- = '\0';
    
    while(ptr1 < ptr){
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
}

/* converte o número de ponto flutuante em string */
// baseada em http://www.geeksforgeeks.org/convert-floating-point-number-string/
void ftoa(float n, char *res, int casas){

    // parte inteira do número
    int iparte = (int) n;
    
    // parte flutuante (após a vírgula)
    float fparte = n - (float) iparte;
 
    // carrega a parte inteira na string
    itoa(iparte, res);
 
    if (casas != 0){
        // adiciona o 'ponto'
        strcat(res, ".");
 
        // tranforma a parte flutuante
        fparte = fparte * _pow(10, casas);
 
        // concatena a parte flutuante à parte inteira
        itoa((int)fparte, res + strlen(res));
    }
}

/* cálculo de potência: retorna o valor da base elevada ao expoente */
float _pow(float base, float expoente){
    float n = base;
    while(--expoente) n *= base;
    return n;
}

void delay_ms(unsigned int ms) {
    while (ms--) {
        __delay_cycles(8000);
    }
}