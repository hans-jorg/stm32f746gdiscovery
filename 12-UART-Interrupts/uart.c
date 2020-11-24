
/**
 ** @file     uart.h
 ** @brief    Hardware Abstraction Layer (HAL) for UART
 ** @version  V1.0
 ** @date     23/01/2016
 **
 ** @note     Direct access to registers
 ** @note     No library except CMSIS is used
 ** @note     Only asynchronous communication
 **
 **/

/**
 ** @brief UART Configuration
 **
 ** @note No interrupts are used
 ** Oversampling    : 8
 ** Clock source    : SystemCoreClock
 ** Handshaking     : No
 **/


#include "stm32f746xx.h"
#include "system_stm32f746.h"
#include "gpio.h"
#include "uart.h"

/**
 ** @brief Bit manipulation macros
 **/
//@{
#define BIT(N)                      (1UL<<(N))
#define BITMASK(M,N)                ((BIT((M)-(N)+1)-1)<<(N))
#define BITVALUE(V,N)               ((V)<<(N))
#define SETBITFIELD(VAR,MASK,VAL)   (VAR)=(((VAR)&~(MASK))|(VAL))
//@}

/**
 ** @brief Info and data area for UARTS
 **/
typedef struct {
    USART_TypeDef           *device;
    GPIO_PinConfiguration   txpinconf;
    GPIO_PinConfiguration   rxpinconf;
    int                     irqlevel;
    int                     irqn;
    char                    inbuffer;
    char                    outbuffer;
} UART_Info;

/**
 * @brief   Interrupt level for UARTs
 */
#define INTLEVEL 6

/**
 ** @brief  List of known UARTs
 **
 ** @note   There are pin alternatives for most of UARTs
 **/
//@{
static UART_Info uarttab[] = {
    /* Device       txconfig        rxconfig   */
    /*            Port  Pin AF     Port  Pin AF */
    { USART1,    { GPIOA, 9, 7 }, { GPIOB, 7, 7 }, INTLEVEL,  USART1_IRQn },
    { USART2,    { GPIOA, 2, 7 }, { GPIOA, 3, 7 }, INTLEVEL,  USART2_IRQn },
    { USART3,    { GPIOD, 8, 7 }, { GPIOD, 9, 7 }, INTLEVEL,  USART3_IRQn },
    { UART4,     { GPIOC,10, 8 }, { GPIOC,11, 8 }, INTLEVEL,  UART4_IRQn  },
    { UART5,     { GPIOC,12, 7 }, { GPIOD, 2, 8 }, INTLEVEL,  UART5_IRQn  },
    { USART6,    { GPIOC, 6, 8 }, { GPIOC, 7, 8 }, INTLEVEL,  USART6_IRQn },
    { UART7,     { GPIOE, 8, 8 }, { GPIOE, 7, 8 }, INTLEVEL,  UART7_IRQn  },
    { UART8,     { GPIOE, 1, 8 }, { GPIOE, 0, 8 }, INTLEVEL,  UART8_IRQn  }
};
static const int uarttabsize = sizeof(uarttab)/sizeof(UART_Info)-1;
//@}

/**
 * @brief   Enable UART
 */
void UART_Enable(USART_TypeDef *uart) {

    if ( uart == USART1 )       RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    else if ( uart == USART2 )  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    else if ( uart == USART3 )  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    else if ( uart == UART4 )   RCC->APB1ENR |= RCC_APB1ENR_UART4EN;
    else if ( uart == UART5 )   RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
    else if ( uart == USART6 )  RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
    else if ( uart == UART7 )   RCC->APB1ENR |= RCC_APB1ENR_UART7EN;
    else if ( uart == UART8 )   RCC->APB1ENR |= RCC_APB1ENR_UART8EN;
}

/**
 ** @brief  Interrupt routines for USART and UART
 **/
///@{

/// IRQ Handler for USART1
void USART1_IRQHandler(void) {
USART_TypeDef  *uart = USART1;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}

/// IRQ Handler for USART2
void USART2_IRQHandler(void) {
USART_TypeDef  *uart = USART2;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}

/// IRQ Handler for USART3
void USART3_IRQHandler(void) {
USART_TypeDef  *uart = USART3;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}

/// IRQ Handler for UART4
void UART4_IRQHandler(void) {
USART_TypeDef  *uart = UART4;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}

/// IRQ Handler for UART5
void UART5_IRQHandler(void) {
USART_TypeDef  *uart = UART5;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}


/// IRQ Handler for USART6
void USART6_IRQHandler(void) {
USART_TypeDef  *uart = USART6;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}


/// IRQ Handler for UART7
void UART7_IRQHandler(void) {
USART_TypeDef  *uart = UART7;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}

/// IRQ Handler for UART7
void UART8_IRQHandler(void) {
USART_TypeDef  *uart = UART8;

    if( uart->ISR & USART_ISR_RXNE  ) { // RX not empty
        uarttab[0].inbuffer = uart->RDR;
    }
    if( uart->ISR & USART_ISR_TXE  ) { // TX not empty
        if ( uarttab[0].outbuffer ) {
            uart->TDR = uarttab[0].outbuffer;
            uarttab[0].outbuffer = 0;
        } else {
            uart->RQR = USART_RQR_TXFRQ;
            uart->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    uart->ICR = 0x00021B5F;     // Clear all pending interrupts
}
///@}

/**
 ** @brief UART Initialization
 **
 ** @note  Use defines in uart.h to configure the uart, or'ing the parameters
 **/
int
UART_Init(int uartn, unsigned config) {
uint32_t baudrate,div,t,over;
USART_TypeDef * uart;
uint32_t uartfreq;

    if( uartn >= uarttabsize ) return -1;

    uart = uarttab[uartn].device;

    // Configure pins
    GPIO_ConfigureSinglePin(&uarttab[uartn].txpinconf);
    GPIO_ConfigureSinglePin(&uarttab[uartn].rxpinconf);

    // Configure RCC DCKCFGR2
    t = RCC->DCKCFGR2;

    // Select clock source

    uartfreq = 0;
    t &= ~BITVALUE(3,uartn*2);
    switch(config&UART_CLOCK_M) {
    case UART_CLOCK_APB:
        t |= BITVALUE(0,uartn*2);
        uartfreq = SystemGetAPB1Frequency();
        break;
    case UART_CLOCK_SYSCLK:
        t |= BITVALUE(1,uartn*2);
        uartfreq = SystemCoreClock;
        break;
    case UART_CLOCK_HSI:
        t |= BITVALUE(2,uartn*2);
        uartfreq = HSI_FREQ;
        break;
    case UART_CLOCK_LSE:
        t |= BITVALUE(3,uartn*2);
        uartfreq = LSE_FREQ;
        break;
    }
    RCC->DCKCFGR2 = t;

    // Enable Clock
    UART_Enable(uart);

    // Configure UART CR1
    t = uart->CR1;

    // data length
    t &= ~(USART_CR1_M|USART_CR1_OVER8|USART_CR1_PCE|USART_CR1_PS|USART_CR1_UE);
    switch( config&UART_SIZE_M ) {
    case UART_8BITS:                  ; break;
    case UART_7BITS: t |= USART_CR1_M0; break;
    case UART_9BITS: t |= USART_CR1_M1; break;
    default:
        return 2;
    }
    // parity
    t |= USART_CR1_TE|USART_CR1_RE;
    switch( config&UART_PARITY_M ) {
    case UART_NOPARITY:                                  ; break;
    case UART_ODDPARITY:  t |= USART_CR1_PCE|USART_CR1_PS; break;
    case UART_EVENPARITY: t |= USART_CR1_PCE;              break;
    }
    if( config&UART_OVER8 ) {
        t |= USART_CR1_OVER8;
        over = 8;
    } else {
        t &= ~USART_CR1_OVER8;
        over = 16;
    }
    uart->CR1 = t;

    // Configure UART CR2
    t = uart->CR2;

    // parity
    t &= ~USART_CR2_STOP;

    switch( config&UART_STOP_M ) {
    case UART_STOP_1:
        t |= 0;
        break;
    case UART_STOP_0_5:
        t |= USART_CR2_STOP_0;
        break;
    case UART_STOP_2:
        t |= USART_CR2_STOP_1;
        break;
    case UART_STOP_1_5:
        t |= USART_CR2_STOP_0|USART_CR2_STOP_1;
        break;
    default:
        return 3;
    }
    uart->CR2 = t;

    // Configure Baudrate
    baudrate = ((config&UART_BAUD_M)>>UART_BAUD_P);

    if( over == 16 ) {
        div = uartfreq/baudrate;
        uart->BRR = div;
    } else {
        div = 2*uartfreq/baudrate;
        uart->BRR = (div&~0xF)|((div&0xF)>>1);
    }

    // Clear buffers
    uarttab[uartn].inbuffer = 0;
    uarttab[uartn].outbuffer = 0;

    // Enable interrupts (only TCIE and RX)
    uart->CR1 |= USART_CR1_RXNEIE;      // Enable interrupt when RX not empty
 //   uart->CR1 |= USART_CR1_TXEIE;       // Enable interrupt when TX is empty

    // Enable interrupts on NVIC
    NVIC_SetPriority(uarttab[uartn].irqn,uarttab[uartn].irqlevel);
    NVIC_ClearPendingIRQ(uarttab[uartn].irqn);
    NVIC_EnableIRQ(uarttab[uartn].irqn);


    // Enable UART
    uart->CR1 |= USART_CR1_UE;
    return 0;
}

/**
 ** @brief UART Send a character
 **
 **/
int
UART_WriteChar(int uartn, unsigned c) {
USART_TypeDef *uart;

    if( uartn >= uarttabsize ) return -1;

    uart = uarttab[uartn].device;
#if 1
    while ( uarttab[uartn].outbuffer != 0 ) {}
    uarttab[uartn].outbuffer = c;
    uart->CR1 |= USART_CR1_TXEIE;
#else
    // Polling
    while( (uart->ISR&USART_ISR_TXE)==0 ) {}
    uart->TDR = c;
#endif
    return 0;
}

/**
 ** @brief UART Send a string
 **
 ** @note  It uses UART_WriteChar
 **
 **/
int
UART_WriteString(int uartn, char s[]) {

    if( uartn >= uarttabsize ) return -1;

    while(*s) {
        UART_WriteChar(uartn,*s++);
    }
    return 0;
}

/**
 ** @brief Read a character from UART
 **
 ** @note  It blocks until a character is entered
 **
 **/
int
UART_ReadChar(int uartn) {
USART_TypeDef *uart;
uint32_t c;

    if( uartn >= uarttabsize ) return -1;

    uart = uarttab[uartn].device;

    if( uarttab[uartn].inbuffer ) {
        c = uarttab[uartn].inbuffer;
        uarttab[uartn].inbuffer = 0;
    }

    if( uart->ISR & USART_ISR_ORE )  {  // overun error
        uart->ICR |= USART_ICR_ORECF;
    }

    return uart->RDR;
}


/**
 ** @brief UART Send a string
 **
 ** @note  It block until "n" characters are entered or
 **        a newline is entered
 **
 ** @note  It uses UART_ReadChar
 **
 **/
int
UART_ReadString(int uartn, char *s, int n) {
int i;

    if( uartn >= uarttabsize ) return -1;

    for(i=0;i<n-1;i++) {
        s[i] = UART_ReadChar(uartn);
        if( s[i] == '\n' || s[i] == '\r' )
            break;
    }
    s[i] = '\0';
    return i;
}

/**
 ** @brief UART Get Status
 **
 ** @note  Return status
 **
 **/
int
UART_GetStatus(int uartn) {
USART_TypeDef *uart;
uint32_t status;

    if( uartn >= uarttabsize ) return -1;

    uart = uarttab[uartn].device;

    status = uart->ISR;
    if ( uarttab[uartn].inbuffer )
        status |= UART_RXNOTEMPTY;
    if ( uarttab[uartn].outbuffer == 0 )
        status |= UART_TXEMPTY;

    return status;

}

