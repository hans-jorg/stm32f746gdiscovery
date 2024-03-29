
/**
 * @file     system_stm32l476.c
 * @brief    utilities code according CMSIS
 * @version  V1.0
 * @date     03/10/2020
 *
 * @note     Includes standard SystemInit
 * @note     Includes non standard SystemCoreClockSet
 * @note
 * @note     Calls SystemInit
 * @note     Calls _main (It provides one, but it is automatically redefined)
 * @note     Calls main
 * @note     This code must be adapted for processor and compiler
 *
 ******************************************************************************/

#include "stm32f746xx.h"
#include "system_stm32f746.h"



/**
 *  @brief  Standard configuration for 200 MHz using HSE as clock source
 */
const PLLConfiguration_t  MainPLLConfiguration_200MHz = {
    .source = CLOCKSRC_HSE,
    .M = HSE_OSCILLATOR_FREQ/1000000,       // f_INT = 1 MHz
    .N = 400,                               // f_VCO = 400 MHz
    .P = 2,                                 // f_OUT = 200 MHz
    .Q = 2,                                 // not used
    .R = 2                                  // not used
};

/**
 *  @brief  Standard configuration for 216 MHz using HSE as clock source
 */
const PLLConfiguration_t  MainPLLConfiguration_216MHz = {
    .source = CLOCKSRC_HSE,
    .M = HSE_OSCILLATOR_FREQ/1000000,       // f_INT = 1 MHz
    .N = 432,                               // f_VCO = 432 MHz
    .P = 2,                                 // f_OUT = 216 MHz
    .Q = 2,                                 // not used
    .R = 2                                  // not used
};

/**
 *  @brief  Standard configuration for maximal frequency (216 MHz) using HSE as clock source
 */
const PLLConfiguration_t  MainPLLConfiguration_Max = {
    .source = CLOCKSRC_HSE,
    .M = HSE_OSCILLATOR_FREQ/1000000,       // f_INT = 1 MHz
    .N = 432,                               // f_VCO = 432 MHz
    .P = 2,                                 // f_OUT = 216 MHz
    .Q = 2,                                 // not used
    .R = 2                                  // not used
};

/**
 *  @brief  SAI PLL standard configuration for 48 MHz frequency (used by USB)
 *          using HSE as clock source
 *
 *
 * @note    Assumes PLL Main will use HSE (crystal) and have a 1 MHz input for PLL
 *
 * @note    LCD_CLK should be in range 5-12, with typical value 9 MHz.
 *
 * @note    There is an extra divisor in PLLSAIDIVR[1:0] of RCC_DCKCFGR, that can
 *          have value 2, 4, 8 or 16.
 *
 * @note    So the R output must be 18, 36, 72 or 144 MHz.
 *          But USB, RNG and SDMMC needs 48 MHz. The LCM of 48 and 9 is 144.
 *
 *          f_LCDCLK  = 9 MHz        PLLSAIRDIV=8
 *
 */
const PLLConfiguration_t  PLLSAIConfiguration_48MHz = {
    .source         = RCC_PLLCFGR_PLLSRC_HSI,
    .M              = HSE_FREQ/1000,                        // f_IN = 1 MHz
    .N              = 144,                                  // f_VCO = 144 MHz
    .P              = 3,                                    // f_P = 48 MHz
    .Q              = 3,                                    // f_Q = 48 MHz
    .R              = 2                                     // f_R = 72 MHz
};


/**
 *  internal functions
 */
static uint32_t FindHPRE(uint32_t divisor);

/**
 * @brief   SystemCoreClock
 * @note    Global variable holding System Clock Frequency (HCLK)
 * @note    It is part of CMSIS
 */
uint32_t SystemCoreClock = HSI_FREQ;


//////////////// Clock Management /////////////////////////////////////////////

/**
 * @brief   Flag to indicate that the Main PLL was configured
 */
static uint32_t MainPLLConfigured = 0;


/**
 * @brief   AHB prescaler table
 * @note    It is a power of 2 in range 1 to 512 but different to 32
 */
static const uint32_t hpre_table[] = {
    1,1,1,1,1,1,1,1,                /* 0xxx: No division */
    2,4,8,16,64,128,256,512         /* 1000-1111: division by */
};


/**
 * @brief   APB prescaler table
 * @note    It is a power of 2 in range 1 to 16
 */
static const uint32_t ppre_table[] = {
    1,1,1,1,                        /* 0xxx: No division */
    2,4,8,16                        /* 1000-1111: division by */
};


/**
 * @brief   Clock Configuration for 200 MHz
 * @note    It is a power of 2 in range 1 to 16
 */
static PLLConfiguration_t ClockConfiguration200MHz = {
    .source = CLOCKSRC_HSE,     /* Clock source = HSE */
    .M = HSE_FREQ/1000000,      /* f_IN = 1 MHz   */
    .N = 400,                   /* f_PLL = 400 MHz*/
    .P = 2,                     /* f_OUT = 200 MHz*/
    .Q = 2,                     /* Not used */
    .R = 2                      /* Not used */
};


/**
 * @brief   Tables relating Flash Wait States to Clock Frequency and Supply Voltage
 *
 * @note    Is used the info on Table 5 of Section 3.3.2 of RM
 */
///@{
typedef struct {
        uint32_t    vmin;          /* minimum voltage in mV */
        uint32_t    freqmax[11];   /* maximal frequency in MHz for the number of WS */
} FlashWaitStates_Type;

FlashWaitStates_Type const flashwaitstates_tab[] = {
    /*  minimum                 Maximum frequency for Wait states                   */
    /*  voltage      0    1     2     3      4     5     6     7     8     9        */
    {   2700,     { 30,   60,   90,  120,  150,  180,  210,  216,    0,    0,   0}  },
    {   2400,     { 24,   48,   72,   96,  120,  144,  168,  192,  216,    0,   0}  },
    {   2100,     { 22,   44,   66,   88,  110,  132,  154,  176,  198,  216,   0}  },
    {   1800,     { 20,   40,   60,   80,  100,  120,  140,  160,  180,    0,   0}  },
    {      0,     {  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}  }
};

/// Used when increasing clock frequency
#define MAXWAITSTATES 9
///@}

/**
 * @brief iabs: find absolute value of an integer
 */
static inline int iabs(int k) { return k<0?-k:k; }


/**
 * @brief   UnlockFlashRegisters
 **/
static inline void UnlockFlashRegisters(void) {
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;
}

/**
 * @brief   LockFlashRegisters
 **/
static inline void LockFlashRegisters(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

/**
 * @brief   SetFlashWaitStates
 *
 * @note    Set FLASH to have n wait states
 **/

static void inline SetFlashWaitStates(int n) {

    FLASH->ACR = (FLASH->CR&~FLASH_ACR_LATENCY)|((n)<<FLASH_ACR_LATENCY_Pos);

}


/**
 * @brief   Find number of Wait States according
 *
 * @note    Given Core Clock Frequency and Voltage, find the number of Wait States
 *          needed for correct access to flash memory
 **/
static int
FindFlashWaitStates(uint32_t freq, uint32_t voltage) {
int i,j;

    /* Look for a line with tension not greater than voltage parameter */
    for(i=0;flashwaitstates_tab[i].vmin && voltage<flashwaitstates_tab[i].vmin;i++) {}
    if( flashwaitstates_tab[i].vmin == 0 )
        return -1;

    for(j=0;flashwaitstates_tab[i].freqmax[j]&&freq>flashwaitstates_tab[i].freqmax[j];j++) {}
    if( flashwaitstates_tab[i].freqmax[j] == 0 )
        return -1;

    return j;
}


/**
 * @brief   Configure Flash Wait State according core frequency and voltage
 *
 **/
static void inline ConfigureFlashWaitStates(uint32_t freq, uint32_t voltage) {
uint32_t ws;

    ws = FindFlashWaitStates(freq,voltage);

    if( ws < 0 )
        return;

    SetFlashWaitStates(ws);

}


/**
 * @brief   Get HPRE Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the HCLK clock signal
 *
 */

uint32_t SystemGetHPRE(void) {
uint32_t hpre;

    hpre = (RCC->CFGR&RCC_CFGR_HPRE)>>RCC_CFGR_HPRE_Pos;
    return hpre;
}


/**
 * @brief   Set HPRE Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the HCLK clock signal
 *
 */

uint32_t SystemSetHPRE(uint32_t hpre) {

    RCC->CFGR = (RCC->CFGR&~RCC_CFGR_HPRE)|(hpre<<RCC_CFGR_HPRE_Pos);
    return 0;
}


/**
 * @brief   Get AHB Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the HCLK clock signal
 *
 */

uint32_t SystemGetAHBPrescaler(void) {
uint32_t hpre,prescaler;

    /* Get HCLK prescaler */
    hpre = (RCC->CFGR&RCC_CFGR_HPRE_Msk)>>RCC_CFGR_HPRE_Pos;
    prescaler = hpre_table[hpre];
    return prescaler;
}

/**
 * @brief   Set AHB Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the HCLK clock signal
 *
 */

uint32_t SystemSetAHBPrescaler(uint32_t div) {
uint32_t hpre;

    hpre = FindHPRE(div);
    RCC->CFGR = (RCC->CFGR&~RCC_CFGR_HPRE)|(hpre<<RCC_CFGR_HPRE_Pos);

    return 0;
}

/**
 * @brief   Get APB1 Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the APB1 clock, the high speed
 *          peripheral clock
 *
 * @note    It must be set so the APB1 frequency is not greater than 54 MHz.
 *
 */

uint32_t SystemGetAPB1Prescaler(void) {
    return ppre_table[(RCC->CFGR&RCC_CFGR_PPRE1_Msk)>>RCC_CFGR_PPRE1_Pos];
}


/**
 * @brief   SetAPB1 Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the APB1, that is
 *          the slow speed peripheral bus
 *
 * @note    It must be set so the APB1 frequency is not greater than 54 MHz.
 *
 */

void SystemSetAPB1Prescaler(uint32_t div) {
uint32_t ppre1;
uint32_t p2;


    if( SystemCoreClock/div > 54000000 )
        return;
        
    p2 = SystemFindLargestPower2Exp(div);

    if (p2 == 0)
        ppre1 = 0;
    else {
        ppre1 = 4+p2-1;
    }

    RCC->CFGR =  (RCC->CFGR&~RCC_CFGR_PPRE1_Msk)|(ppre1)<<RCC_CFGR_PPRE1_Pos;

}

/**
 * @brief   Get APB2 Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the APB1 clock, the high speed
 *          peripheral clock
 *
 * @note    It must be set so the APB1 frequency is not greater than 108 MHz.
 *
 */

uint32_t SystemGetAPB2Prescaler(void) {
    return ppre_table[(RCC->CFGR&RCC_CFGR_PPRE2_Msk)>>RCC_CFGR_PPRE2_Pos];
}


/**
 * @brief   SetAPB2 Prescaler
 *
 * @note    This prescaler divides the HCLK to generate the APB1, the high speed
 *          peripheral clock
 *
 * @note    It must be set so the APB1 frequency is not greater than 108 MHz.
 *
 */

void SystemSetAPB2Prescaler(uint32_t div) {
uint32_t ppre2;
uint32_t p2;

    if( SystemCoreClock/div > 54000000 )
        return;
        
    p2 = SystemFindLargestPower2Exp(div);

    if (p2 == 0)
        ppre2 = 0;
    else {
        ppre2 = 4+p2-1;
    }

    RCC->CFGR =  (RCC->CFGR&~RCC_CFGR_PPRE2_Msk)|(ppre2)<<RCC_CFGR_PPRE2_Pos;

}
///@}

/**
 * @brief   CalculateMainPLLOutFrequency
 *
 * @note    BASE_FREQ = HSE_FREQ or HSI_FREQ or MSI_FREQ
 *          PLL_VCO = (BASE_FREQ / PLL_M) * PLL_N
 *          SYSCLK = PLL_VCO / PLL_R
 */
static uint32_t
CalculateMainPLLOutFrequency(const PLLConfiguration_t *pllconfig) {
uint64_t outfreq,infreq;
uint32_t clocksource;

    clocksource = pllconfig->source;
    
    if( clocksource == CLOCKSRC_HSI) {
        infreq = HSI_FREQ;
    } else if ( clocksource == CLOCKSRC_HSE ){
        infreq = HSE_FREQ;
    } else {
        return 0;
    }
    outfreq  = (infreq*pllconfig->N)/pllconfig->M/pllconfig->P;  // Overflow possible ?
    return (uint32_t) outfreq;
}

/**
 * @brief   CalculatePLLI2SOutFrequency
 *
 * @note    BASE_FREQ = HSE_FREQ or HSI_FREQ or MSI_FREQ
 *          PLL_VCO = (BASE_FREQ / PLL_M) * PLL_N
 *          OUTP = PLL_VCO / PLL_P
 *          OUTQ = PLL_VCO / PLL_Q
 *          OUTR = PLL_VCO / PLL_R
 *
 *          PLLI2SQ = PLLSAIQ = PLLQ = OUTQ
 *          PLLI2SR = PLLSAIR = OUTR
 *          MAINOUT = PLLCLK = PLLSAIP = OUTP
 */
int
SystemCalcPLLFrequencies(const PLLConfiguration_t *pllconfig, PLLOutputFrequencies_t *pllfreq) {
uint64_t outfreq,infreq,vcofreq;
uint32_t clocksource;
    
    clocksource = pllconfig->source;

    if( clocksource == CLOCKSRC_HSI) {
        infreq = HSI_FREQ;
    } else if ( clocksource == CLOCKSRC_HSE ){
        infreq = HSE_FREQ;
    } else {
        return 0;
    }
    pllfreq->infreq = infreq;
    pllfreq->pllinfreq = infreq/pllconfig->M;
    vcofreq = (infreq*pllconfig->N)/pllconfig->M;
    pllfreq->vcofreq = vcofreq;

    pllfreq->poutfreq = 0;
    pllfreq->qoutfreq = 0;
    pllfreq->routfreq = 0;
    if( pllconfig->P )
        pllfreq->poutfreq  = vcofreq/pllconfig->P;
    if( pllconfig->Q )
        pllfreq->qoutfreq  = vcofreq/pllconfig->Q;
    if( pllconfig->R )
        pllfreq->routfreq  = vcofreq/pllconfig->R;

    return (uint32_t) pllfreq->poutfreq;
}

/**
 * @brief   SystemGetPLLConfiguration
 *
 * @note    Fill the struct apointed by pllconfig with the PLL parameters
 */
int  SystemGetPLLConfiguration(uint32_t whichone, PLLConfiguration_t *pllconfig) {

    /* Common to all */
    if( RCC->PLLCFGR&RCC_PLLCFGR_PLLSRC )
        pllconfig->source = CLOCKSRC_HSE;
    else
        pllconfig->source = CLOCKSRC_HSI;
    pllconfig->M = (RCC->PLLCFGR&RCC_PLLCFGR_PLLM_Msk)>>RCC_PLLCFGR_PLLM_Pos;

    switch(whichone) {
    case PLL_MAIN:
        pllconfig->N = (RCC->PLLCFGR&RCC_PLLCFGR_PLLN_Msk)>>RCC_PLLCFGR_PLLN_Pos;
        pllconfig->P = (RCC->PLLCFGR&RCC_PLLCFGR_PLLP_Msk)>>RCC_PLLCFGR_PLLP_Pos;
        pllconfig->Q = (RCC->PLLCFGR&RCC_PLLCFGR_PLLP_Msk)>>RCC_PLLCFGR_PLLQ_Pos;
        pllconfig->R = 0;
        break;
    case PLL_SAI:
        pllconfig->N = (RCC->PLLSAICFGR&RCC_PLLSAICFGR_PLLSAIN_Msk)>>RCC_PLLSAICFGR_PLLSAIN_Pos;
        pllconfig->P = (RCC->PLLSAICFGR&RCC_PLLSAICFGR_PLLSAIP_Msk)>>RCC_PLLSAICFGR_PLLSAIP_Pos;
        pllconfig->Q = (RCC->PLLSAICFGR&RCC_PLLSAICFGR_PLLSAIR_Msk)>>RCC_PLLSAICFGR_PLLSAIQ_Pos;
        pllconfig->R = (RCC->PLLSAICFGR&RCC_PLLSAICFGR_PLLSAIR_Msk)>>RCC_PLLSAICFGR_PLLSAIR_Pos;
        break;
    case PLL_I2S:
        pllconfig->N = (RCC->PLLI2SCFGR&RCC_PLLI2SCFGR_PLLI2SN_Msk)>>RCC_PLLI2SCFGR_PLLI2SN_Pos;
        pllconfig->P = (RCC->PLLI2SCFGR&RCC_PLLI2SCFGR_PLLI2SP_Msk)>>RCC_PLLI2SCFGR_PLLI2SP_Pos;
        pllconfig->Q = (RCC->PLLI2SCFGR&RCC_PLLI2SCFGR_PLLI2SR_Msk)>>RCC_PLLI2SCFGR_PLLI2SQ_Pos;
        pllconfig->R = (RCC->PLLI2SCFGR&RCC_PLLI2SCFGR_PLLI2SR_Msk)>>RCC_PLLI2SCFGR_PLLI2SR_Pos;
        break;
    }
    /* Correct P divisor because it is encoded as 0, 1, 2 and 3 */
    pllconfig->P = (pllconfig->P)*2+2;

    return 0;
}

/**
 * @brief   SystemGetPLLFrequencies
 *
 * @note    Fill the struct apointed by pllfreq with corresponding frequencies
 */
int  SystemGetPLLFrequencies(uint32_t whichone, PLLOutputFrequencies_t *pllfreq) {
PLLConfiguration_t pllconfig;

    SystemGetPLLConfiguration(whichone,&pllconfig);
    SystemCalcPLLFrequencies(&pllconfig,pllfreq);

    return 0;
}

/**
 * @brief   SystemCheckPLLConfiguration
 *
 * @note    returns 0 if configuration is OK
 *
 * @note    Since there is no R in the Main PLL Clock generator, a zero value is accepted
 *
 */
int  SystemCheckPLLConfiguration(const PLLConfiguration_t *pllconfig) {

    if( pllconfig->M < 2 || pllconfig->M > 63 )
        return -1;

    if( pllconfig->N < 50 || pllconfig->M > 432 )
        return -2;

    if( (pllconfig->P!=2) && (pllconfig->P!=4) && (pllconfig->P!=6) && (pllconfig->P!=8) )
        return -3;

    if( pllconfig->Q < 2 || pllconfig->Q > 15 )
        return -4;

    if( pllconfig->R && (pllconfig->R < 2 || pllconfig->R > 7) )
        return -4;

    return 0;
}


/**
 * @brief   SystemGetSYSCLKFrequency
 *
 * @note    returns the SYSCLK, i.e., the System Core Clock before the prescaler
 */

uint32_t SystemGetSYSCLKFrequency(void) {
uint32_t rcc_cr, rcc_cfgr, rcc_pllcfgr;
uint32_t src;
uint32_t sysclk_freq;
uint32_t base_freq;
uint32_t pllsrc;
PLLConfiguration_t pllconfig;

    rcc_cr = RCC->CR;
    rcc_cfgr = RCC->CFGR;
    rcc_pllcfgr = RCC->PLLCFGR;
    sysclk_freq = 0;
    
    /* Get source */
    src = rcc_cfgr & RCC_CFGR_SWS;
    switch (src) {
    case RCC_CFGR_SWS_HSI:  /* HSI used as system clock source */
        sysclk_freq = HSI_FREQ;
        break;
    case RCC_CFGR_SWS_HSE:  /* HSE used as system clock source */
        sysclk_freq = HSE_FREQ;
        break;
    case RCC_CFGR_SWS_PLL:  /* PLL used as system clock source */

        pllsrc = (rcc_pllcfgr & RCC_PLLCFGR_PLLSRC);
        if ( (pllsrc & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSI )
            pllsrc = CLOCKSRC_HSI;
        else
            pllsrc = CLOCKSRC_HSE;

        pllconfig.source = pllsrc;
        pllconfig.M = (rcc_pllcfgr & RCC_PLLCFGR_PLLM)>>RCC_PLLCFGR_PLLM_Pos;
        pllconfig.N = (rcc_pllcfgr & RCC_PLLCFGR_PLLN)>>RCC_PLLCFGR_PLLN_Pos;
        pllconfig.P = (rcc_pllcfgr & RCC_PLLCFGR_PLLP)>>RCC_PLLCFGR_PLLP_Pos;
        sysclk_freq = CalculateMainPLLOutFrequency(&pllconfig);
      break;
    }

    return sysclk_freq;
}

/**
 * @brief   SystemGetCoreClock
 *
 * @note    Returns the System Core Clock based on information contained in the
 *          Clock Register Values (RCC)
 */

uint32_t
SystemGetCoreClock(void) {
uint32_t sysclk_freq, prescaler, hpre;

    sysclk_freq = SystemGetSYSCLKFrequency();
    
    prescaler = SystemGetAHBPrescaler();

    /* HCLK frequency */
    return sysclk_freq/prescaler;
}

/**
 * @brief   SystemGetAPB1Frequency
 *
 * @note    Returns the APB1 (low speed peripheral) clock frequency 
 */
uint32_t SystemGetAPB1Frequency(void) {
uint32_t freq;
uint32_t ppre1;

    freq = SystemGetCoreClock();
    ppre1 = SystemGetAPB1Prescaler();
    return freq/ppre1;
}

/**
 * @brief   SystemGetAPB1Frequency
 *
 * @note    Returns the System Core Clock based on information contained in the
 *          Clock Register Values (RCC)
 */

uint32_t SystemGetAPB2Frequency(void) {
uint32_t freq;
uint32_t ppre2;

    freq = SystemGetCoreClock();
    ppre2 = SystemGetAPB2Prescaler();
    return freq/ppre2;
}

/**
 * @brief   SystemGetAHBFrequency
 *
 * @note    It is the same as SystemCoreClock and HCLK
 */

uint32_t SystemGetAHBFrequency(void) {

    return SystemGetCoreClock();

}

/**
 * @brief   SystemGetHCLKFrequency
 *
 * @note    It is the same as SystemCoreClock and HCLK
 */

uint32_t SystemGetHCLKFrequency(void) {

    return SystemGetCoreClock();

}

/**
 * @brief   FindHPRE
 *
 * @note    Given a divisor, find the best HPRE returning its encoding
 *
 * @note    Could use the hpre_table table above, as the alternative below.
 */
static uint32_t FindHPRE(uint32_t divisor) {
uint32_t k;


    if( divisor <= 1 ) {                    // Minimal
        return 0;
    } else if( divisor >= 512 ) {           // Maximum
        return 15;
    }

        
    k = SystemFindLargestPower2(divisor);   // 2 exponent of divisor
#if 1
    if( k <= 1 )
        return 0;
    if( k < 5 ) {
        return 0x8+k-1;
    } else  if ( k == 5 ) { // There is no divisor 32. It is changed to 64
        return 12;
    } else {
        return 0x8+k-2;
    }

#else
    for(int i=0;i<sizeof(hpre_table)/sizeof(uint32_t);i++) {
        if( hpre_table[i]>=divisor)
            return i;
    }
#endif
}

/**
 * @brief   SystemConfigMainPLL
 *
 * @note    Configure Main PLL unit
 *
 * @note    If core clock source (HCLK) is PLL, it is changed to HSI
 *
 * @note    It does not switch the core clock source (HCLK) to PLL
 */

void
SystemConfigMainPLL(const PLLConfiguration_t *pllconfig) {
uint32_t freq,src;
uint32_t rcc_pllcfgr;
uint32_t clocksource;
int      pllwascoreclock = 0;

    clocksource = pllconfig->source;

    // If core clock source is PLL change it to HSI
    if( (RCC->CFGR&RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL ) {
        SystemEnableHSI();
        RCC->CFGR = (RCC->CFGR&RCC_CFGR_SW)|RCC_CFGR_SW_HSI;
        pllwascoreclock = 1;
    }
    // Disable Main PLL
    SystemDisableMainPLL();

    // Configure it
    switch(clocksource) {
    case CLOCKSRC_HSI:
        SystemEnableHSI();
        freq = HSI_FREQ;
        src  = RCC_CFGR_SW_HSI;
        break;
    case CLOCKSRC_HSE:
        SystemEnableHSE();
        freq = HSE_FREQ;
        src  = RCC_CFGR_SW_HSE;
        break;
    default:
        return;
    }
    // Get PLLCFGR and clear fields to be set
    rcc_pllcfgr = RCC->PLLCFGR
             &  ~(
                    RCC_PLLCFGR_PLLM
                   |RCC_PLLCFGR_PLLN
                   |RCC_PLLCFGR_PLLP
                   |RCC_PLLCFGR_PLLQ
                   |RCC_PLLCFGR_PLLSRC
                 );

    rcc_pllcfgr |=
                 (
                   ((pllconfig->M<<RCC_PLLCFGR_PLLM_Pos)&RCC_PLLCFGR_PLLM)
                  |((pllconfig->N<<RCC_PLLCFGR_PLLN_Pos)&RCC_PLLCFGR_PLLN)
                  |((pllconfig->P<<RCC_PLLCFGR_PLLP_Pos)&RCC_PLLCFGR_PLLP)
                  |((pllconfig->Q<<RCC_PLLCFGR_PLLQ_Pos)&RCC_PLLCFGR_PLLQ)
                  |((src<<RCC_PLLCFGR_PLLSRC_Pos)&RCC_PLLCFGR_PLLSRC)
                 );

    RCC->PLLCFGR = rcc_pllcfgr;

    SystemEnableMainPLL();

    MainPLLConfigured = 1;

    /* If it was the core clock, change back */
    if( pllwascoreclock ) {
        RCC->CFGR = (RCC->CFGR&RCC_CFGR_SW)|RCC_CFGR_SW_PLL;
    }
}

/**
 * @brief   SystemConfigPLLSAI
 *
 * @note    Configure SAI PLL unit
 *
 */

void
SystemConfigPLLSAI(const PLLConfiguration_t *pllconfig) {
uint32_t freq,src;
uint32_t rcc_pllsaicfgr;
uint32_t clocksource;

    // Some parameter are shared with the Main PLL.
    // It must be configured first
    if( !MainPLLConfigured )
        return;

    // Disable SAI PLL
    RCC->CR &= ~RCC_CR_PLLSAION;
    
    // Get PLLSAICFGR and clear fields to be set
    rcc_pllsaicfgr = RCC->PLLSAICFGR
                    &~(
                        RCC_PLLSAICFGR_PLLSAIN
                       |RCC_PLLSAICFGR_PLLSAIP
                       |RCC_PLLSAICFGR_PLLSAIQ
                       |RCC_PLLSAICFGR_PLLSAIR
                      );

    rcc_pllsaicfgr |= (
                        ((pllconfig->N<<RCC_PLLSAICFGR_PLLSAIN_Pos)&RCC_PLLSAICFGR_PLLSAIN)
                       |((pllconfig->P<<RCC_PLLSAICFGR_PLLSAIP_Pos)&RCC_PLLSAICFGR_PLLSAIP)
                       |((pllconfig->Q<<RCC_PLLSAICFGR_PLLSAIQ_Pos)&RCC_PLLSAICFGR_PLLSAIQ)
                       |((pllconfig->R<<RCC_PLLSAICFGR_PLLSAIR_Pos)&RCC_PLLSAICFGR_PLLSAIR)
                      );

    RCC->PLLSAICFGR = rcc_pllsaicfgr;

    // Enable SAI PLL
    RCC->CR |= RCC_CR_PLLSAION;

    while ( (RCC->CR&RCC_CR_PLLSAIRDY) == 0 ) {}

}

/**
 * @brief   SystemConfigPLLI2S
 *
 * @note    Configure I2S PLL unit
 *
 */

void
SystemConfigPLLI2S(const PLLConfiguration_t *pllconfig) {
uint32_t freq,src;
uint32_t rcc_plli2scfgr;
uint32_t clocksource;

    // Some parameter are shared with the Main PLL.
    // It must be configured first
    if( !MainPLLConfigured )
        return;

    // Disable SAI PLL
    RCC->CR &= ~RCC_CR_PLLI2SON;
    
    // Get PLLI2SCFGR and clear fields to be set
    rcc_plli2scfgr = RCC->PLLI2SCFGR
                    &~(
                        RCC_PLLI2SCFGR_PLLI2SN
                       |RCC_PLLI2SCFGR_PLLI2SP
                       |RCC_PLLI2SCFGR_PLLI2SQ
                       |RCC_PLLI2SCFGR_PLLI2SR
                      );

    rcc_plli2scfgr |= (
                        ((pllconfig->N<<RCC_PLLI2SCFGR_PLLI2SN_Pos)&RCC_PLLI2SCFGR_PLLI2SN)
                       |((pllconfig->P<<RCC_PLLI2SCFGR_PLLI2SP_Pos)&RCC_PLLI2SCFGR_PLLI2SP)
                       |((pllconfig->Q<<RCC_PLLI2SCFGR_PLLI2SQ_Pos)&RCC_PLLI2SCFGR_PLLI2SQ)
                       |((pllconfig->R<<RCC_PLLI2SCFGR_PLLI2SR_Pos)&RCC_PLLI2SCFGR_PLLI2SR)
                      );

    RCC->PLLI2SCFGR = rcc_plli2scfgr;

    // Enable SAI PLL
    RCC->CR |= RCC_CR_PLLI2SON;

    while ( (RCC->CR&RCC_CR_PLLI2SRDY) == 0 ) {}

}

/**
 * @brief   SystemSetCoreClock
 *
 * @note    Configure to use clock source. If not enabled, enable it and wait for
 *          stabilization
 *
 * @note    If the PLL clock is not configure, it is configured to generate a
 *          200 MHz clock signal
 *
 * @note    To increase the clock frequency (Section 3.3.2 of RM)
 *          1. Program the new number of wait states to the LATENCY bits
 *             in the FLASH_ACR register
 *          2. Check that the new number of wait states is taken into account
 *             to access the Flash memory by reading the FLASH_ACR register
 *          3. Modify the CPU clock source by writing the SW bits in the RCC_CFGR
 *             register
 *          4  If needed, modify the CPU clock prescaler by writing the HPRE bits
 *             in RCC_CFGR
 *          5. Check that the new CPU clock source or/and the new CPU clock prescaler
 *             value is/are taken into account by reading the clock source status
 *             (SWS bits) or/and the AHB prescaler value (HPRE bits), respectively,
 *             in the RCC_CFGR register.
 *
 * @note   To decrease the clock frequency (Section 3.3.2 of RM)
 *         1. Modify the CPU clock source by writing the SW bits in
 *            the RCC_CFGR register
 *         2. If needed, modify the CPU clock prescaler by writing the HPRE
 *            bits in RCC_CFGR
 *         3. Check that the new CPU clock source or/and the new CPU clock
 *            prescaler value is/are taken into account by reading the
 *            clock source status (SWS bits) or/and the AHB prescaler value
 *            (HPRE bits), respectively, in the RCC_CFGR register
 *         4. Program the new number of wait states to the LATENCY bits in FLASH_ACR
 *         5. Check that the new number of wait states is used to access
 *            the Flash memory by reading the FLASH_ACR register
 *
 */
uint32_t SystemSetCoreClock(uint32_t newsrc, uint32_t newdiv) {
uint32_t src,div;
uint32_t hpre,newhpre;
uint32_t ppre1;
uint32_t ppre2;

    src = RCC->CFGR & RCC_CFGR_SW;

    // Save APBx prescaler configuration */
    ppre1 = SystemGetAPB1Prescaler();
    ppre2 = SystemGetAPB2Prescaler();
    
    if( newsrc == src ) {   // Just change the prescaler
        hpre = (RCC->CFGR&RCC_CFGR_HPRE_Msk)>>RCC_CFGR_HPRE_Pos;
        div = hpre_table[hpre];
        newhpre = FindHPRE(newdiv);
        if( newdiv < div ) {                    // Increasing clock frequency
            SetFlashWaitStates(MAXWAITSTATES);  // Worst case
            SystemSetAPB1Prescaler(4);          // Safe
            SystemSetAPB2Prescaler(2);          // Safe
        }
        RCC->CFGR = (RCC->CFGR&~RCC_CFGR_HPRE)|(newhpre<<RCC_CFGR_HPRE_Pos);
    } else {                // There is a change of clock source
        SetFlashWaitStates(MAXWAITSTATES);  // Worst case
        SystemSetAPB1Prescaler(4);          // Safe
        SystemSetAPB2Prescaler(2);          // Safe
        // Set HPRE Prescaler
        newhpre = FindHPRE(newdiv);
        RCC->CFGR = (RCC->CFGR&~RCC_CFGR_HPRE)|(newhpre<<RCC_CFGR_HPRE_Pos);
        // Change clock source
        switch(newsrc) {
        case CLOCKSRC_HSI:
            SystemEnableHSI();
            RCC->CFGR = (RCC->CFGR&~RCC_CFGR_SW)|RCC_CFGR_SW_HSI;
            break;
        case CLOCKSRC_HSE:
            SystemEnableHSE();
            RCC->CFGR = (RCC->CFGR&~RCC_CFGR_SW)|RCC_CFGR_SW_HSE;
            break;
        case CLOCKSRC_PLL:
            if( !MainPLLConfigured ) {
                SystemSetAPB1Prescaler(4);                  // Safe
                SystemSetAPB2Prescaler(2);                  // Safe
                SystemConfigMainPLL(&ClockConfiguration200MHz);
            }
            RCC->CFGR = (RCC->CFGR&~RCC_CFGR_SW)|RCC_CFGR_SW_PLL;
            __DSB();
            __ISB();
        }
    }

    // Set SystemCoreClock to the new frequency and adjust flash wait states
    
    SystemCoreClockUpdate();
    ConfigureFlashWaitStates(SystemCoreClock,VSUPPLY);
    // Try to restore APBx prescalers
    SystemSetAPB1Prescaler(ppre1);
    SystemSetAPB2Prescaler(ppre2); 
    return 0;
 }


/**
 * @brief   SystemSetCoreClockFrequency
 *
 * @note    Configure to use PLL as a clock source and to run at the given
 *          frequency.
  */
uint32_t SystemSetCoreClockFrequency(uint32_t freq) {
PLLConfiguration_t clockconf;

    if( freq >= HCLKMAX ) {
        freq = HCLKMAX;
    }
    clockconf.source = CLOCKSRC_HSE;     /* Clock source   */
    clockconf.M = HSE_FREQ/1000000;      /* f_IN = 1 MHz   */
    clockconf.N = 2*freq;                /* f_PLL = 400 MHz*/
    clockconf.P = 2;                     /* f_OUT = 200 MHz*/
    clockconf.Q = 2;                     /* Not used */
    clockconf.R = 2;                     /* Not used */

    SystemConfigMainPLL(&clockconf);
    SystemSetCoreClock(CLOCKSRC_PLL,1);
    return freq;
}

///////////////////////////Auxiliary functions ////////////////////////////////

/**
 * @brief   FindNearestPower2Divisor
 *
 * @note    Given a number, find a power of 2 nearest to it
 */
uint32_t SystemFindNearestPower2(uint32_t divisor) {
int n = 1;
int err = 10000000;

    for(int i=0;i<20;i++) {
        int k = 1<<i;
        int e = iabs((int)divisor-(int)k);
        if( e >= err)
            break;
        err = e;
        n = k;
    }
    return n;
}

uint32_t SystemFindNearestPower2Exp(uint32_t divisor) {
int n = 1;
int err = 10000000;
int i;

    for(i=0;i<20;i++) {
        int k = 1<<i;
        int e = iabs((int)divisor-(int)k);
        if( e >= err){
            i--;
            break;
        }
        err = e;
        n = k;
    }
    return i;
}

uint32_t SystemFindLargestPower2(uint32_t divisor) {
int n = 1;
int err = 10000000;
int i;

    for(i=0;i<20;i++) {
        int k = 1<<i;
        int e = iabs((int)divisor-(int)k);
        if( e >= err) {
            i--;
            break;
        }
        err = e;
        n = k;
    }
    if( n < divisor )
        n<<=1;
    return n;
}

uint32_t SystemFindLargestPower2Exp(uint32_t divisor) {
int n = 1;
int err = 10000000;
int i;

    for(i=0;i<20;i++) {
        int k = 1<<i;
        int e = iabs((int)divisor-(int)k);
        if( e >= err) {
            i--;
            break;
        }
        err = e;
        n = k;
    }
    if( n < divisor )
        i++;
    return i;
}


//////////////// CMSIS  ///////////////////////////////////////////////////////

/**
 * @brief SystemCoreClockUpdate
 *
 * @note Updates the SystemCoreClock variable using information contained in the
 *       Clock Register Values (RCC)
 *
 * @note This function must be called to update SystemCoreClock variable every time
 *       the clock configuration is modified.
 *
 * @note It is part of CMSIS
 */

void
SystemCoreClockUpdate(void) {

    SystemCoreClock = SystemGetCoreClock();

}


/**
 * @brief SystemInit
 *
 * @note  Resets to default configuration for clock and disables all interrupts
 *
 * @note  It is part of CMSIS
 *
 * @note  Replaces the one (dummy) contained in start_DEVICE.c
 */

void
SystemInit(void) {

    /* Configure FPU when FPU_USED=1 as defined in core_cm7.h */
#if __FPU_USED == 1U
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        /* enable CP10 and CP11 coprocessors */
        SCB->CPACR |= (0x0FUL << 20);
        __DSB();
        __ISB();
    #endif
#endif
    /* Reset HSEON, CSSON and PLLON bits */
    RCC->CR = 0x00000083;

    /* Reset CFGR register */
    RCC->CFGR = 0x00000000;

    /* Reset PLLCFGR register */
    RCC->PLLCFGR = 0x24003010;

    /* Disable all interrupts */
    RCC->CIR = 0x00000000;

    /* Enable HSE but do not switch to it */
    SystemEnableHSE();


    // Enable Peripheral Clocks
    SystemSetAHBPrescaler(1);
    SystemSetAPB1Prescaler(4);          // Safe
    SystemSetAPB2Prescaler(2);          // Safe
            
    /* Update SystemCoreClock */
    SystemCoreClockUpdate();

    /**
     * @note    There is a L1 cache in the CPU core, but only for
     *          access thru AXIM bus (range 0x0800_0000-0x080F_FFFF).
     * 
     * @noter   The is a Adaptive Real-Time (ART) Accelerator, a ST tecnology 
     *          for instruction lookahead. It works for *Flash memory* accesses
     *          thru the ITCM bus (range 0x0020_0000-0x002F_FFFF).  It has
     *          64 cache lines with 256 bits.
     *          
     */

    // Invalidate both caches to avoid using invalid values
    SCB_InvalidateICache();
    SCB_InvalidateDCache();

    // Enable Instruction Cache
    SCB_EnableICache();
    __ISB();                                 // See manual

    // Enable Data Cache (It aauses problem in ETH DMA!!!!!!)
    //SCB_EnableDCache();
    SCB_DisableDCache();

    /* Enable ART (ST technology) */
    FLASH->ACR &= ~FLASH_ACR_ARTEN;         /* Disable ART */
    FLASH->ACR |= FLASH_ACR_ARTRST;         /* Reset ART */
    //FLASH->ACR &= ~FLASH_ACR_ARTRST;      /* Reset ART */

    FLASH->ACR |= FLASH_ACR_ARTEN;          /* Enable ART */
    FLASH->ACR |= FLASH_ACR_PRFTEN;         /* Enable ART Prefetch*/

    /**
     * @note It is possible to relocate Vector Table
     *       It must be a 512 byte boundary. Bits 8:0 = 0
     */
    //SCB->VTOR = FLASH_BASE;    /* Vector Table Relocation in Internal FLASH */


    /*
     * Additional initialization here 
     */

    /*
     * Turn off display, by setting pin 3 of GPIO Port K to zero
     */
    RCC->AHB1ENR |= (1<<10);       
    __DSB();
    // Set mode to output */
    GPIOK->MODER = (GPIOK->MODER&~(0x3<<(3*2)))|(0x1<<(3*2));
    /* Turn off display */
    GPIOK->ODR  &= ~(1<<3);
                                        
}












