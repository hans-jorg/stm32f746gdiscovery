/**
 * @file    i2c-master.c
 *
 * @brief   I2C implementation of a master interface for STM32F746 using polling
 *
 * @note    Simple implementation. Configured to use 16 MHz HSI as clock source
 *
 * @note    The are three alternatives for the implementation:
 *          * Polling   <- This module
 *          * Interrupt
 *          * Direct Memory Access (DMA)
 *
 * @note    This module uses the gpio module to configure pins.
 *
 * @author  Hans
 * @date    2023/06/04
 */

#include "stm32f746xx.h"
#include "system_stm32f746.h"
#include "i2c-master.h"
#include "gpio.h"



/**
 *
 * @brief I2CCLK frequency
 *
 *
 * @note  I2CCLK Clock is configured in the RCC->DCKCFGR2 register.
 *        They are I2CxSEL fields, one for each I2C unit
 *
 * @note  Clock sources
 *
 *  | Source         | I2CxSEL |
 *  |----------------|---------|
 *  |  APB1 (PCLK1)  |   00    |
 *  |  SYSCLK        |   01    |
 *  |  HSI           |   10    |
 *
 * @note Minimal frequencies
 *
 * | Mode                | Analog filter |  DNF = 1 |
 * |---------------------|---------------|----------|
 * | Standard mode       |    2 MHz      |   2 MHz  |
 * | Fast mode           |   10 MHz      |   9 MHz  |
 * | Fast plus mode      |   22.5 MHz    |  16 MHz  |
 *
 * OBS: Using HSI (=16 MHz) as filter, it is not possible to use Fast plus mode
 *
 * @note  From Table 182 of the STM32F746NG Datasheet
 *
 * | Parameter     |  Standard  |   Fast     | Fast Plus  |
 * |---------------|------------|------------|------------|
 * |  PRESC        |      3     |       1    |       0    |
 * |  SCLL         |     0x13   |     0x9    |     0x4    |
 * |  SCLH         |     0xF    |     0x3    |     0x2    |
 * |  SDADEL       |     0x2    |     0x2    |     0x0    |
 * |  SCLDEL       |     0x4    |     0x3    |     0x2    |
 */

/**
 * @brief   All timing for 16 MHz (=HSI)
 *
 * @note    Generated by STM3CubeMX for a 16 MHz I2CCLK
 *
  * @note The timing is set by
 *       PRESC (31-28:4 bits):   fPRESC = fI2CCLK/(PRESC+1)
 *       SCLDEL(23-20:4 bits):   tSCLDEL= tPRESC*(SCLDEL+1)
 *       SDADEL(19-10:4 bits):   tSDADEL= tPRESC*(SDADEL+1)
 *       SCLH  (15-8:8 bits):    tSCLH = (SCLH+1)*tPRESC
 *       SCLL  ( 7-0:8 bits):    tSCLL = (SCLL+1)*tPRESC
 *
 * @note The restrictions are:
 *
 *       SDADEL >= (tfmax+tHDDATmin-tAFmin-(DNF+3)*tI2CCLK)/(PRESC+1)*tI2CCLK
 *       SDADEL <= (tHDDATmax-fAFmax-(DNF+4)xtI2CCLK)/(PRESC+1)*tI2CCLK
 *       SCLDEL >= (trmax+tSUDAT,om)/(PRESC+1)*tI2CCLK-1
 *
 * @note The I2C standard specifies the following parameters (RM Table 180)
 *
 * | Speed    | fSCL | tHDSTA | tSUSTA | tSUSTO | tBUF | tLOW | tHIGH | tf  | tf  |
 * |----------|------|--------|--------|--------|------|------|-------|-----|-----|
 * | Standard |  100 |   4    |   4,7  |   4.0  |  4.7 |  4.7 |  4,0  | 1.0 | 0.3 |
 * | Fast     |  400 |   0.6  |   0.6  |   0.6  |  1.3 |  1.3 |  0.6  | 0.3 | 0.3 |
 * | Fast plus| 1000 |   0.26 |   0.26 |   0.26 |  0.5 |  0.5 |  0.26 | 0.12| 0.12|
 *
 * @note The I2C standard specifies the following parameters (RM Table 178)
 *
 *
 *| Speed     |tHDDATmin|tVDDATmax|tSUDATmin|trmax |tfmax|
 *|-----------|---------|---------|---------|------|-----|
 *| Standard  |    0    |   3.45  |  0.250  |  1   | 0.3 |
 *| Fast      |    0    |   0.9   |  0.100  |  0.3 | 0.3 |
 *| Fast plus |    0    |   0.45  |  0.050  |  0.12| 0.12|
 *| SMBUS     |    0.3  |     -   |  0.250  |  1   | 0.3 |
 *
 * @note tAF (Maximum pulse width of spikes that are suppressed by the analog
 *       filter) is defined in the STM32F746NG datasheet
 *
 * | Parameter |  Min  |  Max  |
 * |-----------|-------|-------|
 * |  tAF (ns) |   50  |  150  |
 *
 * @note Table with values for TIMINGR generated by STM32CubeMX
 *       tr and tf considered 0.
 *
 * | Speed     |   None     |   Analog   | Digital=1  | Digital=2  |
 * |-----------|------------|------------|------------|------------|
 * |  100 KHz  | 0x00303D5D | 0x00303D5B | 0x00303C5C | 0x00303B5B |
 * |  400 KHz  | 0x0010071B | 0x0010061A | 0x0010061A | 0x00100519 |
 * | 1000 KHz  | 0x00000208 | 0x00000107 | 0x00000107 | 0x00000006 |
 *
 * @note TIMINGR = 0x00303D5D means
 *                  PRESC=0   -> fPRESC = fI2CCLK/(PRESC+1)
 *                  SCLDEL=3
 *                  SDADEL=3
 *                  SCLH=3D   = 61
 *                  SCLL=5D   = 91
 *
 * @note Table 182 of RM shows another set of values for a 16 MHz clock.
 *       It is not clear which kind of filter is beeing used!
 *
 * | Speed          | TIMINGR     | PRESC | SCLDEL | SDADEL | SCLH | SCLL |
 * |----------------|-------------|-------|--------|--------|------|------|
 * |      100 KHz   | 0x30420F13  |  0x3  |   0x4  |   0x2  | 0x0F | 0x13 |
 * |      400 KHz   | 0x10320309  |  0x1  |   0x3  |   0x2  | 0x03 | 0x09 |
 * |     1000 KHz   | 0x00200204  |  0x0  |   0x2  |   0x0  | 0x02 | 0x04 |
 *
 */

/**
 *  @note   Using HSI as I2CCLK clock source (=16 MHz)
 *          Alternatives are:
 *              0: APB1CLK
 *              1: SYSCLK
 *              2: HSICLK
 */
#define I2CCLKSRC (2)

/**
 * @brief
 *
 * The calculation of the timing parameters (PRESC,SCLDEL,SDADEL,SCLH,SCLL)
 * is a PITA.
 *
 * The easiest way is to use STM32CubeMX.
 * Do not forget to specify tr and tf, because they have a bit impact on the
 * timing parameters
 *
 * Below there are some precalculated values for timing according the some combination of speed
 * and the filters used.
 *
 * This can be overided by specifying a non zero timing parameter
 *
 * It is not a good idea to calculate this (constant) parameter during execution.
 * This will demand unnecessarily RAM, Flash and floating point (or at least, a boring
 * fixed point) calculation.
 */

/**
 * Data structure to store precalculated TIMINGR values;
 * They are ordered from largest to smallest sizes.
 */

struct Default_Timing_t {
    uint32_t    timingr;
    uint32_t    freq;           // Frequency (in KHz!!!!)
    uint32_t    speed;
    unsigned    analog:1;       // a bit
    unsigned    digital:1;      // bit field with width = 1 (=bit)
    unsigned    dnf:4;          // bit field with widht = 4 (0-15 value);
};

/**
 * Table of precalculated TIMINGR values;
 */

static struct Default_Timing_t default_timing[] = {
/*      TIMINGR     Freq     Speed   Analog  Digital DNF  */
    {  0x00503D5A,  16000,   100000,   0,      0,     0  },
    {  0x00503D58,  16000,   100000,   1,      0,     0  },
    {  0x00503C59,  16000,   100000,   0,      1,     1  },
    {  0x00503B58,  16000,   100000,   0,      1,     2  },
    {  0x00300718,  16000,   400000,   0,      0,     0  },
    {  0x00300617,  16000,   400000,   1,      0,     0  },
    {  0x00300617,  16000,   400000,   0,      1,     1  },
    {  0x00300912,  16000,   400000,   0,      1,     2  },
    {  0x00200205,  16000,  1000000,   0,      0,     0  },
    {  0x00200105,  16000,  1000000,   1,      0,     0  },
    {  0x00200004,  16000,  1000000,   0,      1,     1  },
    {  0x00200003,  16000,  1000000,   0,      1,     2  },
    {           0,      0,        0,   0,      0,     0  }  // END OF TABLE
};

/**
 *
 *  @brief  Data structure to store information about I2C Pin Configuration
 */
typedef struct {
    I2C_TypeDef             *i2c;
    GPIO_PinConfiguration   sclpin;
    GPIO_PinConfiguration   sdapin;
} I2C_Configuration_t;


/**
 * @brief   Configuration for STM32F746G Discovery Boardd
 *
 * @note
 *
 * |  I2C   |    SCL             |           SDA              |
 * |--------|--------------------|----------------------------|
 * |  I2C1  |  PB6 *PB8*         |  PB7 *PB9*                 |
 * |  I2C2  |  PB10 PF1 PH4      |  PB11 PF0 PH5              |
 * |  I2C3  |  PA8 *PH7*         |  PC9 *PH8*                 |
 * |  I2C4  |  PD12 PF14 PH11    |  PD13 PF15 PH12            |
 *
 * Only I2C1 and I2C3 in the table above are free to use
 * I2C1 at PB8 and PB9 used for EXT I2C (Arduino connectors)
 * I2C3 at PH7 and PH* used for LCD Touch and AUDIO I2C
 * Other I2Cs have pin usage conflicts
 *
 * @note All SCL and SDA pins must be configured as
 *        ALTERNATE FUNCTION = 4, OPEN DRAIN, HIGH SPEED, PULL-UP
 *        This corresponds to the following values in the table below
 *                    AF  mode otype ospeed pupd initial
 *                     4    2     1      2    1     1
 *        The pupd and initial must be verified!
 *        ospeed = high speed or very high speed.
 *        Maybe there is a need to use SYSCFG->PMC fields as in others MCUs of the family
 */
static const I2C_Configuration_t i2c_configuration[] = {
//    I2Cx   GPIO   Pin   AF  mode otype ospeed pupd initial
    { I2C1,
            {GPIOB,  8,   4,    2,   1,    3,    1}, // SCL
            {GPIOB,  9,   4,    2,   1,    3,    1}, // SDA
    },
    { I2C3,
            {GPIOH,  7,   4,    2,   1,    3,    1}, // SCL
            {GPIOH,  8,   4,    2,   1,    3,    1}, // SDA
    },
    { 0,
            {    0,  0,   0,    0,   0,    0,    0}, // SCL
            {    0,  0,   0,    0,   0,    0,    0}, // SDA
    }
};

/**
 * @brief Data structure to store run time info. For now, only status
 */
///@{
typedef struct {
    I2C_TypeDef                *i2c;
    I2C_Status_t                status;
} RunTimeInfo_t;

static RunTimeInfo_t  RunTimeInfo[] = {
    { I2C1,  I2C_UNINITIALIZED },
    { I2C3,  I2C_UNINITIALIZED },
    {    0,  I2C_UNINITIALIZED }
};
///@}

/**
 * @brief  Find run time info for a specific I2C
 *
 * @note   Since i2c is referenced as a pointer to its registers, a search is needed to
 *         find the corresponding index in the Run Time Info table.
           It uses a very simple search algorithm (Linear search). It is possible to hash the
           pointer or to do some pointer arithmetic to find the index, but it is highly non
           portable.
 *
 * @param  Description of parameter
 *
 * @return Description of return parameters
 */

static RunTimeInfo_t *FindRunTimeInfo( I2C_TypeDef *i2c ) {
RunTimeInfo_t *p = RunTimeInfo;

    while( p->i2c && p->i2c != i2c ) {
        p++;
    }

    if( p->i2c )
        return p;
    else
        return 0;
}

/**
 * @brief  Set I2C Status
 *
 * @param  I2C Pointer
 *
 * @return 0 if OK, negative in case of error
 */

static int I2CMaster_SetStatus( I2C_TypeDef *i2c, I2C_Status_t status ) {
RunTimeInfo_t *p;

    p = FindRunTimeInfo(i2c);
    if( !p )
        return -1;

    p->status = status;
    return 0;
}

/**
 * @brief  Get I2C Status
 *
 * @param  I2C Pointer
 *
 * @return Status stored in RunTimeInfo or I2C_ERROR
 */

I2C_Status_t  I2CMaster_GetStatus( I2C_TypeDef *i2c ) {
RunTimeInfo_t *p;
I2C_Status_t  t;

    p = FindRunTimeInfo(i2c);
    if( !p )
        return I2C_ERROR;

    t = p->status;
    if( t == I2C_ERROR )
        p->status = I2C_READY;
    return t;
}

/**
 *  @brief  ConfigurePins
 *
 *  @note   For now, uses GPIO library
 */
static int I2CMaster_ConfigurePins( I2C_TypeDef *i2c ) {
const I2C_Configuration_t *p;

    // Lookup configuration information on table
    p = i2c_configuration;
    while( p->i2c && (i2c!=p->i2c) ) p++;

    // Not found!!
    if( ! p->i2c )
        return -1;

    // Pins not configurable
    if( (p->sclpin.gpio == 0) || (p->sdapin.gpio == 0) )
        return -2;
    // Configure pins when possible
    GPIO_ConfigureSinglePin(&(p->sclpin));
    GPIO_ConfigureSinglePin(&(p->sdapin));
}


/**
 *  @brief  Peripheral Clock Enable for I2C
 *
 *  @note   Using HSI as I2CCLK clock source (=16 MHz)
 */
static void
I2CMaster_PeripheralClockEnable( I2C_TypeDef *i2c ) {

    // Enable Peripheral Clock
    if ( i2c == I2C1 ) {
        RCC->APB1ENR |= RCC_APB1ENR_I2C1EN_Msk;
    } else if ( i2c == I2C2 ) {
        RCC->APB1ENR |= RCC_APB1ENR_I2C2EN_Msk;
    } else if ( i2c == I2C3 ) {
        RCC->APB1ENR |= RCC_APB1ENR_I2C3EN_Msk;
    } else if ( i2c == I2C4 ) {
        RCC->APB1ENR |= RCC_APB1ENR_I2C4EN_Msk;
    }

}

/**
 *  @brief  Kernel Clock Enable for I2C
 *
 *  @note   It uses the I2CCLKSRC symbol define abo
 */

static void
I2CMaster_KernelClockConfig( I2C_TypeDef *i2c ) {

#if I2CCLKSRC == (2)
    RCC->CR |= RCC_CR_HSION;
    // Should I wait for HSIRDY?
#endif

    // Enable I2C clocks
    if ( i2c == I2C1 ) {
        RCC->DCKCFGR2 =  (RCC->DCKCFGR2&~RCC_DCKCFGR2_I2C1SEL_Msk)
                        |(I2CCLKSRC<<RCC_DCKCFGR2_I2C1SEL_Pos);
    } else if ( i2c == I2C2 ) {
        RCC->DCKCFGR2 =  (RCC->DCKCFGR2&~RCC_DCKCFGR2_I2C2SEL_Msk)
                        |(I2CCLKSRC<<RCC_DCKCFGR2_I2C2SEL_Pos);
    } else if ( i2c == I2C3 ) {
        RCC->DCKCFGR2 =  (RCC->DCKCFGR2&~RCC_DCKCFGR2_I2C3SEL_Msk)
                        |(I2CCLKSRC<<RCC_DCKCFGR2_I2C3SEL_Pos);
    } else if ( i2c == I2C4 ) {
        RCC->DCKCFGR2 =  (RCC->DCKCFGR2&~RCC_DCKCFGR2_I2C4SEL_Msk)
                        |(I2CCLKSRC<<RCC_DCKCFGR2_I2C4SEL_Pos);
    }

}

/**
 *  @brief  I2CMaster_Reset
 *
 *  @note   Reset I2C using the SWRST pin on the APB1RSTR
 *
 *  @note   There is another way to reset it by using the PE=0, PE=1
 *          sequence as describe in RM 30.4.4: "A software reset can be
 *          performed by clearing the PE bit in the I2C_CR1 register"
 *
 */
static void I2CMaster_Reset( I2C_TypeDef *i2c ) {
uint32_t mask;

    if ( i2c == I2C1 ) {
        mask =  RCC_APB1RSTR_I2C1RST;
    } else if ( i2c == I2C2 ) {
        mask =  RCC_APB1RSTR_I2C2RST;
    } else if ( i2c == I2C3 ) {
        mask =  RCC_APB1RSTR_I2C3RST;
    } else if ( i2c == I2C4 ) {
        mask =  RCC_APB1RSTR_I2C4RST;
    }
    // Set reset
    RCC->APB1RSTR |=  mask;

    __NOP();

    // Clear reset
    RCC->APB1RSTR &= ~mask;

    // Set flag to uninitialized
    I2CMaster_SetStatus(i2c,I2C_UNINITIALIZED);


}



/**
 *  @brief  I2CMaster_Disable
 *
 *  @note   Disable I2C and at the same time, reset it
 *          See RM 30.4.4
 *
 *  @note   When cleared, PE must be kept low for at
 *          least 3 APB clock cycles. (RM Section 30.7.1)
 *
 *  @note   This is ensured by writing the following software sequence:
 *           - Write PE=0
 *           - Check PE=0
 *           - Write PE=0
 *          (RM Section 30.4.5)
 */
static void I2CMaster_Disable( I2C_TypeDef *i2c ) {

    // Turn off device (Three times, see Note in RM Section 30.7.1 */
    i2c->CR1 &= ~I2C_CR1_PE;
    i2c->CR1 &= ~I2C_CR1_PE;
    i2c->CR1 &= ~I2C_CR1_PE;

    // Set flag to disabled
    I2CMaster_SetStatus(i2c,I2C_DISABLED);

}


/**
 *  @brief  I2CMaster_Enable
 *
 *  @note   Enable I2C and at the same time, reset it
 *          See RM 30.4.4
 *
 *  @note   When set, PE must be kept high for at
 *          least 3 APB clock cycles. (RM Section 30.7.1)
 *
 *  @note   This is ensured by writing the following software sequence:
 *           - Write PE=1
 *           - Check PE=1
 *           - Write PE=1
 *          (RM Section 30.4.5)
 */
static void I2CMaster_Enable( I2C_TypeDef *i2c ) {

    // Turn off device (Three times, see Note in RM Section 30.7.1 */
    i2c->CR1 |= I2C_CR1_PE;
    i2c->CR1 |= I2C_CR1_PE;
    i2c->CR1 |= I2C_CR1_PE;

    // Set flag to enabled
    I2CMaster_SetStatus(i2c,I2C_READY);

}


/**
 * @brief  Get Default Filter Parameter
 *
 * @note   Returns precalculated TIMINGR for certain combinations of clock source, speed and
 *         filter settings.
 *
 * @note   A simple sequential search is used. If the table grows larger, use hash.
 *
 * @param  conf as used by the I2CMaster_Init function.
 *
 * @return TIMINGR register. returns 0 when the right parameter could not be found.
 */



static uint32_t GetPreCalculatedTiming(uint32_t conf) {
uint32_t freq;
uint32_t timingr;
struct Default_Timing_t *pTiming = &default_timing[0];

    int clksrc = conf&I2C_CONF_CLOCK_MASK;
    int dnf = (conf&I2C_CONF_FILTER_DNF_MASK)>>I2C_CONF_FILTER_DNF_Pos;
    int analog = (conf&I2C_CONF_FILTER_ANALOG)||(conf&I2C_CONF_FILTER_BOTH);  // 0 or 1
    int digital = (conf&I2C_CONF_FILTER_DIGITAL)||(conf&I2C_CONF_FILTER_BOTH);;// 0 or 1



    freq = 0;
    switch(clksrc) {
    case I2C_CONF_CLOCK_HSICLK:
        freq = HSI_FREQ;
        break;
    case I2C_CONF_CLOCK_SYSCLK:
        freq = SystemGetSYSCLKFrequency();
        break;
    case I2C_CONF_CLOCK_APB1CLK:
        freq = SystemGetAPB1Frequency();
        break;
    }
    if( freq == 0 )
        return 0;

    timingr = 0;
    while( pTiming->freq ) {
        if(   (freq == pTiming->freq) && (dnf == pTiming->dnf) && (analog == pTiming->analog)
            && (digital == pTiming->digital) ) {
            timingr = pTiming->timingr;
            break;
        }
    }
    return timingr;
}


/**
 *  @brief  I2CMaster_Init
 *
 *  @note   Initializes I2C and configure it
 *
 *  @note   It only accepts one of the filters: None, Analog or Digital.
 */
int
I2CMaster_Init( I2C_TypeDef *i2c, uint32_t conf, uint32_t timing) {
int index;


    // Enable peripheral clock for the I2C
    I2CMaster_PeripheralClockEnable(i2c);

    // Configure clock for the I2C kernel
    I2CMaster_KernelClockConfig(i2c);

    // Is a reset needed?
    I2CMaster_Reset(i2c);

    // Disable I2C
    I2CMaster_Disable(i2c);

    // In the example in CubeF7, there is a 200 ms delay here

    // Configure pins
    I2CMaster_ConfigurePins(i2c);

    // If timing parameter = 0, try to find one in the table
    if( timing == 0 ) {
        timing = GetPreCalculatedTiming(conf);
        if( timing == 0 )
            return 0;  /* Could not find a pre calculated timing */
    }

    uint32_t dnf = (conf&I2C_CONF_FILTER_DNF_MASK)>>I2C_CONF_FILTER_DNF_Pos;
    // Configure filters
    if( (conf&I2C_CONF_FILTER_NONE)!=0 ) {
        // Using no filter
        i2c->CR1 |= I2C_CR1_ANFOFF;                 // Turn off analog filter
        i2c->CR1 = (i2c->CR1&~(I2C_CR1_DNF_Msk));   // Turn off digital filter
        index = 0;
    } else if( (conf&I2C_CONF_FILTER_ANALOG)!=0 )  {
        // Using analog filter
        i2c->CR1 &= ~I2C_CR1_ANFOFF;                // Turn on analog filter
        i2c->CR1 = (i2c->CR1&~(I2C_CR1_DNF_Msk));   // Turn off digital filter
        index = 1;
    } else if( (conf&I2C_CONF_FILTER_DIGITAL)!=0 ) {
        // Disabling analog filter
        i2c->CR1 |= I2C_CR1_ANFOFF;                 // Turn off analog filter
        // Using digital filter
        i2c->CR1 = (i2c->CR1&~(I2C_CR1_DNF_Msk))|(dnf<<I2C_CR1_DNF_Pos);
    } else if ( (conf&I2C_CONF_FILTER_BOTH)!=0 ) {
        // Using analog filter
        i2c->CR1 &= ~I2C_CR1_ANFOFF;                // Turn on analog filter
        // Using digital filter
        i2c->CR1 = (i2c->CR1&~(I2C_CR1_DNF_Msk))|(dnf<<I2C_CR1_DNF_Pos);
    }
    i2c->TIMINGR = timing;

    // Disable interrupts
    i2c->CR1 &= ~(I2C_CR1_ERRIE
                 |I2C_CR1_TCIE
                 |I2C_CR1_STOPIE
                 |I2C_CR1_NACKIE
                 |I2C_CR1_ADDRIE
                 |I2C_CR1_RXIE
                 |I2C_CR1_TXIE
                 );

    // Enable stretch mode, disable SMB mode,
    i2c->CR1 &= ~(I2C_CR1_PE
                 |I2C_CR1_DNF
                 |I2C_CR1_ANFOFF
                 |I2C_CR1_TXDMAEN
                 |I2C_CR1_RXDMAEN
                 |I2C_CR1_SBC
                 |I2C_CR1_NOSTRETCH
                 |I2C_CR1_GCEN
                 |I2C_CR1_SMBHEN
                 |I2C_CR1_SMBDEN
                 |I2C_CR1_ALERTEN
                 |I2C_CR1_PECEN
                 );

    // Only 7 bit address
    i2c->CR2 &= ~(I2C_CR2_ADD10|I2C_CR2_HEAD10R|I2C_CR2_START|I2C_CR2_STOP|I2C_CR2_NACK
                 |I2C_CR2_NBYTES_Msk|I2C_CR2_RELOAD|I2C_CR2_PECBYTE);

    // Enable auto end and
    i2c->CR2 |= (I2C_CR2_AUTOEND | I2C_CR2_NACK);


    // Configure addresses. It is a master. It does not need one.
    i2c->OAR1 &= ~(I2C_OAR1_OA1EN|I2C_OAR1_OA1MODE|I2C_OAR1_OA1_Msk);
    i2c->OAR2 &= ~(I2C_OAR2_OA2EN|I2C_OAR2_OA2_Msk);

    // Turn on device. */
    I2CMaster_Enable(i2c);

    // Set flag to Ready
    I2CMaster_SetStatus(i2c,I2C_READY);

    return 0;
}


/**
 * @brief  I2C Master detects a slave
 *
 * @note   Long description of function
 *
 * @param  Description of parameter
 *
 * @return Description of return parameters
 */

int
I2CMaster_Detect( I2C_TypeDef *i2c, uint16_t addr ) {
    i2c->CR2 =   (i2c->CR2 & ~(
                     I2C_CR2_SADD_Msk
                    |I2C_CR2_NBYTES_Msk
                    |I2C_CR2_ADD10
                    |I2C_CR2_RD_WRN
                    |I2C_CR2_HEAD10R
                    |I2C_CR2_RELOAD
                    |I2C_CR2_AUTOEND
                    |I2C_CR2_STOP
                    )
                 )
                |((addr<<I2C_CR2_SADD_Pos)&I2C_CR2_START|I2C_CR2_STOP)
                |((0<<I2C_CR2_NBYTES_Pos)&I2C_CR2_NBYTES_Msk)
                |I2C_CR2_AUTOEND;

#ifdef USE_POLLING
    // There should be a timeout!!
    while ( (i2c->ISR&I2C_ISR_BUSY) == 1 ) {}
    if( (i2c->ISR&I2C_ISR_NACKF) == 1 ) {
        return -1;
    }
#endif
    return 0;
}


/**
 * @brief I2C Master write to a slave
 *
 * @note  Send the *n* bytes in the *data array* to slave *addr*
 *
 * @note  Should have a timeout

 * @param i2c:      I2C peripheral to be used
 * @param address:  I2C address of slave
 * @param data:     pointer to data to be transmitted
 * @param n:        Number of bytes to be transmitted
 * @return int:     0 if OK, else negative number
 */
int
I2CMaster_Write( I2C_TypeDef *i2c, uint16_t addr, uint8_t *data, uint16_t nbytes) {
uint8_t *p = data;

    if( nbytes > 255 )
        return -1;

    i2c->CR2 =   (i2c->CR2 & ~(
                     I2C_CR2_SADD_Msk
                    |I2C_CR2_NBYTES_Msk
                    |I2C_CR2_ADD10
                    |I2C_CR2_RD_WRN
                    |I2C_CR2_HEAD10R
                    |I2C_CR2_RELOAD
                    |I2C_CR2_AUTOEND
                    |I2C_CR2_STOP
                    )
                 )
                |((addr<<I2C_CR2_SADD_Pos)&I2C_CR2_START)
                |((nbytes<<I2C_CR2_NBYTES_Pos)&I2C_CR2_NBYTES_Msk)
                |I2C_CR2_AUTOEND;
#ifdef USE_POLLING
    i2c->CR2 |= I2C_CR2_START;
    i2c->TXDR = *p++;

    while(1) {
        while( (i2c->ISR&I2C_ISR_TXIE) == 0 ) {}     // Block!!!!!
        // TBD: Test error conditions

        nbytes--;
        if( nbytes == 1 )
            break;
        i2c->TXDR = *p++;
    }
    i2c->CR2 |= I2C_CR2_STOP;  // should be set with the last byte
    i2c->TXDR = *p++;
#endif
    return 0;
}

/**
 * @brief I2CMaster_Read
 *
 * @note  Read *n* bytes into the *data array* from slave *addr*
 *
 * @param i2c
 * @param address
 * @param data
 * @param n
 * @return int
 */
int
I2CMaster_Read( I2C_TypeDef *i2c, uint16_t addr, uint8_t *data, uint16_t nbytes) {
uint8_t *p = data;
int ninitial = nbytes;

    i2c->CR2 =   (i2c->CR2 & ~(
                     I2C_CR2_SADD_Msk
                    |I2C_CR2_NBYTES_Msk
                    |I2C_CR2_ADD10
                    |I2C_CR2_HEAD10R
                    |I2C_CR2_RELOAD
                    )
                 )
                |((addr<<I2C_CR2_SADD_Pos)&I2C_CR2_START_Msk)
                |((nbytes<<I2C_CR2_NBYTES_Pos)&I2C_CR2_NBYTES_Msk)
                |I2C_CR2_AUTOEND
                |I2C_CR2_RD_WRN;
#ifdef USE_POLLING
    i2c->CR2 |= I2C_CR2_START;
    n = 0;
    while( ((i2c->CR2&I2C_CR2_STOPF)==0)&&(n<nbytes) ) {
        while( (i2c->ISR&I2C_ISR_RXNE) == 0 ) {}     // Block!!!!!
        *p++ = i2c->RXDR;
        n++;
    }
#endif

    return 0;
}