/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No 
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all 
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM 
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES 
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS 
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of 
* this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer 
*
* Copyright (C) 2015 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : resetprg.c
* Device(s)    : RX231
* Description  : Defines post-reset routines that are used to configure the MCU prior to the main program starting. 
*                This is were the program counter starts on power-up or reset.
***********************************************************************************************************************/
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 05.01.2015 1.00     First Release
*         : 30.09.2015 1.01     Changed Minor version to 3.01
*         : 01.11.2017 2.00     Added the bsp startup module disable function.
*                               Added the ILOCO oscillation stabilization wait processing.
*                               Added the setting of IWDT.IWDTCSTPR.BIT.SLCSTP.
*                               Deleted the process to stop ILOCO.
*                               Added the following build condition in lpt_clock_source_select function.
*                               - BSP_CFG_LPT_CLOCK_SOURCE == 2
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
/* Defines MCU configuration functions used in this file */
#include    <_h_c_lib.h>
/* This macro is here so that the stack will be declared here. This is used to prevent multiplication of stack size. */
#define     BSP_DECLARE_STACK
/* Define the target platform */
#include    "platform.h"

/* When using the user startup program, disable the following code. */
#if (BSP_CFG_STARTUP_DISABLE == 0)

/* BCH - 01/16/2013 */
/* 0602: Defect for macro names with '_[A-Z]' is also being suppressed since these are default names from toolchain.
   3447: External linkage is not needed for these toolchain supplied library functions. */
/* PRQA S 0602, 3447 ++ */

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/* If the user chooses only 1 stack then the 'U' bit will not be set and the CPU will always use the interrupt stack. */
#if (BSP_CFG_USER_STACK_ENABLE == 1)
    #define PSW_init  (0x00030000)
#else
    #define PSW_init  (0x00010000)
#endif
#define FPSW_init (0x00000000)  /* Currently nothing set by default. */

/* IWDTCLK Oscillation stabilization time is more than 50us */
/* When the system clock is the sub-clock, warning will occur but there is no problem.
   Because the calculation result of this definition is zero. */
#define IWDTCLK_STABILIZE_LOOP_CNT    ((uint32_t)(0.00005f * BSP_ICLK_HZ / 10))

/***********************************************************************************************************************
Pre-processor Directives
***********************************************************************************************************************/
/* Set this as the entry point from a power-on reset */
#pragma entry PowerON_Reset_PC

/***********************************************************************************************************************
External function Prototypes
***********************************************************************************************************************/
/* Functions to setup I/O library */
extern void _INIT_IOLIB(void);
extern void _CLOSEALL(void);

#if BSP_CFG_USER_WARM_START_CALLBACK_PRE_INITC_ENABLED != 0
/* If user is requesting warm start callback functions then these are the prototypes. */
void BSP_CFG_USER_WARM_START_PRE_C_FUNCTION(void);
#endif

#if BSP_CFG_USER_WARM_START_CALLBACK_POST_INITC_ENABLED != 0
/* If user is requesting warm start callback functions then these are the prototypes. */
void BSP_CFG_USER_WARM_START_POST_C_FUNCTION(void);
#endif

/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
/* Power-on reset function declaration */
void PowerON_Reset_PC(void);

#if BSP_CFG_RUN_IN_USER_MODE==1
    #if __RENESAS_VERSION__ < 0x01010000
        /* Declare the contents of the function 'Change_PSW_PM_to_UserMode' as assembler to the compiler */
        #pragma inline_asm Change_PSW_PM_to_UserMode

    /* MCU user mode switcher function declaration */
    static void Change_PSW_PM_to_UserMode(void);
    #endif
#endif

/* Main program function declaration */
void main(void);
static void operating_frequency_set(void);
static void clock_source_select(void);
static void usb_lpc_clock_source_select(void);

/***********************************************************************************************************************
* Function name: PowerON_Reset_PC
* Description  : This function is the MCU's entry point from a power-on reset.
*                The following steps are taken in the startup code:
*                1. The User Stack Pointer (USP) and Interrupt Stack Pointer (ISP) are both set immediately after entry 
*                   to this function. The USP and ISP stack sizes are set in the file bsp_config.h.
*                   Default sizes are USP=4K and ISP=1K.
*                2. The interrupt vector base register is set to point to the beginning of the relocatable interrupt 
*                   vector table.
*                3. The MCU operating frequency is set by operating_frequency_set.
*                4. Calls are made to functions to setup the C runtime environment which involves initializing all 
*                   initialed data, zeroing all uninitialized variables, and configuring STDIO if used
*                   (calls to _INITSCT and _INIT_IOLIB).
*                5. Board-specific hardware setup, including configuring I/O pins on the MCU, in hardware_setup.
*                6. Global interrupts are enabled by setting the I bit in the Program Status Word (PSW), and the stack 
*                   is switched from the ISP to the USP.  The initial Interrupt Priority Level is set to zero, enabling 
*                   any interrupts with a priority greater than zero to be serviced.
*                7. The processor is optionally switched to user mode.  To run in user mode, set the macro 
*                   BSP_CFG_RUN_IN_USER_MODE above to a 1.
*                8. The bus error interrupt is enabled to catch any accesses to invalid or reserved areas of memory.
*
*                Once this initialization is complete, the user's main() function is called.  It should not return.
* Arguments    : none
* Return value : none
***********************************************************************************************************************/
void PowerON_Reset_PC(void)
{
    /* Stack pointers are setup prior to calling this function - see comments above */    
    
    /* Initialise the MCU processor word and Exception Table Register */
#if __RENESAS_VERSION__ >= 0x01010000    
    set_intb((void *)__sectop("C$VECT"));
    set_extb((void *)__sectop("EXCEPTVECT"));
#else
    set_intb((unsigned long)__sectop("C$VECT"));
    set_extb((unsigned long)__sectop("EXCEPTVECT"));
#endif    


    /* Initialize FPSW for floating-point operations */
#ifdef __ROZ
#define FPU_ROUND 0x00000001  /* Let FPSW RMbits=01 (round to zero) */
#else
#define FPU_ROUND 0x00000000  /* Let FPSW RMbits=00 (round to nearest) */
#endif
#ifdef __DOFF
#define FPU_DENOM 0x00000100  /* Let FPSW DNbit=1 (denormal as zero) */
#else
#define FPU_DENOM 0x00000000  /* Let FPSW DNbit=0 (denormal as is) */
#endif

    set_fpsw(FPSW_init | FPU_ROUND | FPU_DENOM);
    
    /* Switch to high-speed operation */
    operating_frequency_set();
    usb_lpc_clock_source_select();

    /* If the warm start Pre C runtime callback is enabled, then call it. */
#if BSP_CFG_USER_WARM_START_CALLBACK_PRE_INITC_ENABLED == 1
     BSP_CFG_USER_WARM_START_PRE_C_FUNCTION();
#endif

    /* Initialize C runtime environment */
    _INITSCT();

    /* If the warm start Post C runtime callback is enabled, then call it. */
#if BSP_CFG_USER_WARM_START_CALLBACK_POST_INITC_ENABLED == 1
     BSP_CFG_USER_WARM_START_POST_C_FUNCTION();
#endif

#if BSP_CFG_IO_LIB_ENABLE == 1
    /* Comment this out if not using I/O lib */
    _INIT_IOLIB();
#endif

    /* Initialize MCU interrupt callbacks. */
    bsp_interrupt_open();

    /* Initialize register protection functionality. */
    bsp_register_protect_open();

    /* Configure the MCU and board hardware */
    hardware_setup();

    /* Change the MCU's user mode from supervisor to user */
    nop();
    set_psw(PSW_init);      
#if BSP_CFG_RUN_IN_USER_MODE==1
    /* Use chg_pmusr() intrinsic if possible. */
    #if __RENESAS_VERSION__ >= 0x01010000
    chg_pmusr() ;
    #else
    Change_PSW_PM_to_UserMode();
    #endif
#endif

    /* Enable the bus error interrupt to catch accesses to illegal/reserved areas of memory */
    R_BSP_InterruptControl(BSP_INT_SRC_BUS_ERROR, BSP_INT_CMD_INTERRUPT_ENABLE, FIT_NO_PTR);

    /* Call the main program function (should not return) */
    main();
    
#if BSP_CFG_IO_LIB_ENABLE == 1
    /* Comment this out if not using I/O lib - cleans up open files */
    _CLOSEALL();
#endif

    /* BCH - 01/16/2013 */
    /* Infinite loop is intended here. */    
    while(1) /* PRQA S 2740 */
    {
        /* Infinite loop. Put a breakpoint here if you want to catch an exit of main(). */
    }
}

/***********************************************************************************************************************
* Function name: operating_frequency_set
* Description  : Configures the clock settings for each of the device clocks
* Arguments    : none
* Return value : none
***********************************************************************************************************************/
static void operating_frequency_set (void)
{
    /* Used for constructing value to write to SCKCR register. */
    uint32_t temp_clock = 0;
    
    /* 
    Default settings:
    Clock Description              Frequency
    ----------------------------------------
    Input Clock Frequency............  8 MHz
    PLL frequency (/2 x13)............ 54 MHz
    Internal Clock Frequency.........  54 MHz
    Peripheral Clock A Frequency...... 54 MHz
    Peripheral Clock B Frequency...... 27 MHz
    Peripheral Clock D Frequency...... 27 MHz
    External Bus Clock Frequency.....  27 MHz
    Flash IF Clock Frequency.......... 27 MHz
    USB Clock Frequency............... 48 MHz */
            
    /* Protect off. */
    SYSTEM.PRCR.WORD = 0xA50B;

    /* Select the clock based upon user's choice. */
    clock_source_select();

    /* Figure out setting for FCK bits. */
#if   BSP_CFG_FCK_DIV == 1
    /* Do nothing since FCK bits should be 0. */
#elif BSP_CFG_FCK_DIV == 2
    temp_clock |= 0x10000000;
#elif BSP_CFG_FCK_DIV == 4
    temp_clock |= 0x20000000;
#elif BSP_CFG_FCK_DIV == 8
    temp_clock |= 0x30000000;
#elif BSP_CFG_FCK_DIV == 16
    temp_clock |= 0x40000000;
#elif BSP_CFG_FCK_DIV == 32
    temp_clock |= 0x50000000;
#elif BSP_CFG_FCK_DIV == 64
    temp_clock |= 0x60000000;
#else
    #error "Error! Invalid setting for BSP_CFG_FCK_DIV in r_bsp_config.h"
#endif

    /* Figure out setting for ICK bits. */
#if   BSP_CFG_ICK_DIV == 1
    /* Do nothing since ICK bits should be 0. */
#elif BSP_CFG_ICK_DIV == 2
    temp_clock |= 0x01000000;
#elif BSP_CFG_ICK_DIV == 4
    temp_clock |= 0x02000000;
#elif BSP_CFG_ICK_DIV == 8
    temp_clock |= 0x03000000;
#elif BSP_CFG_ICK_DIV == 16
    temp_clock |= 0x04000000;
#elif BSP_CFG_ICK_DIV == 32
    temp_clock |= 0x05000000;
#elif BSP_CFG_ICK_DIV == 64
    temp_clock |= 0x06000000;
#else
    #error "Error! Invalid setting for BSP_CFG_ICK_DIV in r_bsp_config.h"
#endif

    /* Figure out setting for BCK bits. */
#if   BSP_CFG_BCK_DIV == 1
    /* Do nothing since BCK bits should be 0. */
#elif BSP_CFG_BCK_DIV == 2
    temp_clock |= 0x00010000;
#elif BSP_CFG_BCK_DIV == 4
    temp_clock |= 0x00020000;
#elif BSP_CFG_BCK_DIV == 8
    temp_clock |= 0x00030000;
#elif BSP_CFG_BCK_DIV == 16
    temp_clock |= 0x00040000;
#elif BSP_CFG_BCK_DIV == 32
    temp_clock |= 0x00050000;
#elif BSP_CFG_BCK_DIV == 64
    temp_clock |= 0x00060000;
#else
    #error "Error! Invalid setting for BSP_CFG_BCK_DIV in r_bsp_config.h"
#endif

    /* Configure PSTOP1 bit for BCLK output. */
#if BSP_CFG_BCLK_OUTPUT == 0    
    /* Set PSTOP1 bit */
    temp_clock |= 0x00800000;   // disable BCLK output
#elif BSP_CFG_BCLK_OUTPUT == 1
    /* Clear PSTOP1 bit */
    temp_clock &= ~0x00800000;  // enable BCLK output, divide by 1
#elif BSP_CFG_BCLK_OUTPUT == 2
    /* Clear PSTOP1 bit */
    temp_clock &= ~0x00800000;  // enable BCLK output
    /* Set BCLK divider bit */
    SYSTEM.BCKCR.BIT.BCLKDIV = 1;   // and divide by 2
#else
    #error "Error! Invalid setting for BSP_CFG_BCLK_OUTPUT in r_bsp_config.h"
#endif

    /* Figure out setting for PCKA bits. */
#if   BSP_CFG_PCKA_DIV == 1
    /* Do nothing since PCKA bits should be 0. */
#elif BSP_CFG_PCKA_DIV == 2
    temp_clock |= 0x00001000;
#elif BSP_CFG_PCKA_DIV == 4
    temp_clock |= 0x00002000;
#elif BSP_CFG_PCKA_DIV == 8
    temp_clock |= 0x00003000;
#elif BSP_CFG_PCKA_DIV == 16
    temp_clock |= 0x00004000;
#elif BSP_CFG_PCKA_DIV == 32
    temp_clock |= 0x00005000;
#elif BSP_CFG_PCKA_DIV == 64
    temp_clock |= 0x00006000;
#else
    #error "Error! Invalid setting for BSP_CFG_PCKA_DIV in r_bsp_config.h"
#endif

    /* Figure out setting for PCKB bits. */
#if   BSP_CFG_PCKB_DIV == 1
    /* Do nothing since PCKB bits should be 0. */
#elif BSP_CFG_PCKB_DIV == 2
    temp_clock |= 0x00000100;
#elif BSP_CFG_PCKB_DIV == 4
    temp_clock |= 0x00000200;
#elif BSP_CFG_PCKB_DIV == 8
    temp_clock |= 0x00000300;
#elif BSP_CFG_PCKB_DIV == 16
    temp_clock |= 0x00000400;
#elif BSP_CFG_PCKB_DIV == 32
    temp_clock |= 0x00000500;
#elif BSP_CFG_PCKB_DIV == 64
    temp_clock |= 0x00000600;
#else
    #error "Error! Invalid setting for BSP_CFG_PCKB_DIV in r_bsp_config.h"
#endif

    /* Figure out setting for PCKD bits. */
#if   BSP_CFG_PCKD_DIV == 1
    /* Do nothing since PCKD bits should be 0. */
#elif BSP_CFG_PCKD_DIV == 2
    temp_clock |= 0x00000001;
#elif BSP_CFG_PCKD_DIV == 4
    temp_clock |= 0x00000002;
#elif BSP_CFG_PCKD_DIV == 8
    temp_clock |= 0x00000003;
#elif BSP_CFG_PCKD_DIV == 16
    temp_clock |= 0x00000004;
#elif BSP_CFG_PCKD_DIV == 32
    temp_clock |= 0x00000005;
#elif BSP_CFG_PCKD_DIV == 64
    temp_clock |= 0x00000006;
#else
    #error "Error! Invalid setting for BSP_CFG_PCKD_DIV in r_bsp_config.h"
#endif

    /* b7 to b4 should be 0x0.
       b22 to b20 should be 0x0. */

    /* Set SCKCR register. */
    SYSTEM.SCKCR.LONG = temp_clock;

    while (SYSTEM.SCKCR.LONG != temp_clock)
    {
        /* Confirm that previous write to SCKCR register has completed. RX MCU's have a 5 stage pipeline architecture
         * and therefore new instructions can be executed before the last instruction has completed. By reading the
         * value of register we will ensure that the value has been written. Not all registers require this
         * verification but the SCKCR is marked as such in the HW manual.
         */
    }

    /* Actually set selected clock source here. Default for r_bsp_config.h is PLL. */
    SYSTEM.SCKCR3.WORD = ((uint16_t)BSP_CFG_CLOCK_SOURCE) << 8;

#if (BSP_CFG_CLOCK_SOURCE != 0)
    /* We can now turn LOCO off since it is not going to be used. */
    SYSTEM.LOCOCR.BYTE = 0x01;
#endif

    /* Protect on. */
    SYSTEM.PRCR.WORD = 0xA500;          
}

/***********************************************************************************************************************
* Function name: clock_source_select
* Description  : Enables and disables clocks as chosen by the user.
* Arguments    : none
* Return value : none
***********************************************************************************************************************/
static void clock_source_select (void)
{
    volatile uint8_t read_verify;
     /* Declared volatile for software delay purposes. */
    volatile uint32_t i;

    /* Set to High-speed operating mode if clock is > 12MHz. */
    /* Because ICLK may not be maximum frequency, it is necessary to check all clock frequency. */
    if( (BSP_ICLK_HZ  > BSP_MIDDLE_SPEED_MAX_FREQUENCY)||
        (BSP_PCLKA_HZ > BSP_MIDDLE_SPEED_MAX_FREQUENCY)||
        (BSP_PCLKB_HZ > BSP_MIDDLE_SPEED_MAX_FREQUENCY)||
        (BSP_PCLKD_HZ > BSP_MIDDLE_SPEED_MAX_FREQUENCY)||
        (BSP_FCLK_HZ  > BSP_MIDDLE_SPEED_MAX_FREQUENCY)||
        (BSP_BCLK_HZ  > BSP_MIDDLE_SPEED_MAX_FREQUENCY) )
    {
        SYSTEM.OPCCR.BYTE = 0x00;   // set to high-speed mode
        while(SYSTEM.OPCCR.BIT.OPCMTSF == 1)
        {
            /* Wait for transition to finish. */
        }
        }
        /* Set to memory wait if ICLK is > 32MHz. */
        if( BSP_ICLK_HZ > BSP_MEMORY_NO_WAIT_MAX_FREQUENCY )
        {
        SYSTEM.MEMWAIT.BYTE = 0x01; // Use wait states
        while (SYSTEM.MEMWAIT.BYTE != 0x01)
        {
            /* wait for bit to set */
        }
    }

    /* Configure the main clock oscillator drive capability. */
    if ((BSP_CFG_MCU_VCC_MV >= 2400) && (BSP_CFG_XTAL_HZ >= 10000000))
    {
        SYSTEM.MOFCR.BIT.MODRV21 = 1;  // VCC >= 2.4v and BSP_CFG_XTAL_HZ >= 10 MHz
    }
    else  // VCC < 2.4v  -or-  BSP_CFG_XTAL_HZ < 10 MHz
    {
        SYSTEM.MOFCR.BIT.MODRV21 = 0;  // RSKRX231 is 3.3V and uses 8MHz external oscillator
    }

    /* At this time the MCU is still running on the 4MHz LOCO. */
    
#if (BSP_CFG_CLOCK_SOURCE == 1) // HOCO
    /* Make sure HOCO is stopped before changing frequency. */
    SYSTEM.HOCOCR.BYTE = 0x01;

    /* Set frequency for the HOCO. */
    SYSTEM.HOCOCR2.BIT.HCFRQ = BSP_CFG_HOCO_FREQUENCY;

    /* HOCO is chosen. Start it operating. */
    SYSTEM.HOCOCR.BYTE = 0x00;
    while (SYSTEM.OSCOVFSR.BIT.HCOVF != 1)
    {
        ;   // wait for stabilization
    }
#else
    /* HOCO is not chosen. */
    /* Stop the HOCO. */
    SYSTEM.HOCOCR.BYTE = 0x01; 
#endif


#if (BSP_CFG_CLOCK_SOURCE == 2) || (BSP_CFG_CLOCK_SOURCE == 4) || (BSP_CFG_USB_CLOCK_SOURCE == 1) // MAIN or PLL or UPLL

    /* Can set wait time to 0 because clock is externally input. Will leave at default 8192 cycles (2.048ms) */
    SYSTEM.MOSCWTCR.BYTE = 0x04;        

    /* Set the main clock to operating. */
    SYSTEM.MOSCCR.BYTE = 0x00;          
    while (SYSTEM.OSCOVFSR.BIT.MOOVF != 1)
    {
        ;   // wait for stabilization
    }
#else
    /* Set the main clock to stopped. */
    SYSTEM.MOSCCR.BYTE = 0x01;          
#endif


#if (BSP_CFG_CLOCK_SOURCE == 3) || (BSP_CFG_LPT_CLOCK_SOURCE == 0) // SUBCLOCK
    
    SYSTEM.SOSCCR.BYTE = 0x01;      // Make sure sub-clock is initially stopped
    while (SYSTEM.SOSCCR.BYTE != 0x01)
    {
        /* wait for bit to change */
    }
 
    RTC.RCR3.BIT.RTCEN = 0;         // Also set the RTC Sub-Clock disable
    while (RTC.RCR3.BIT.RTCEN != 0)
    {
        /* wait for bit to change */
    }

    for(i = 0; i < 88; i++)          // 153us*4.56/4.00 (LOCO max)
    {
        nop() ;
    }

    RTC.RCR3.BIT.RTCDV = 0x01;      // Init the Sub-Clock Oscillator Drive Capacity = Low CL
    while (RTC.RCR3.BIT.RTCDV != 0x01)
    {
        /* wait for bits to change */
    }
    
    SYSTEM.SOSCCR.BYTE = 0x00;      // Start sub-clock
    while (SYSTEM.SOSCCR.BYTE != 0x00)
    {
        /* wait for bit to change */
    }

    R_BSP_SoftwareDelay(1482, BSP_DELAY_MILLISECS); // 1.3sec*4.56/4.00 (LOCO max)

    RTC.RCR3.BIT.RTCEN = 0x01;      // Also set the RTC Sub-Clock enable
    while (RTC.RCR3.BIT.RTCEN != 0x01)
    {
        /* wait for bit to change */
    }
#else
    /* Set the sub-clock to stopped. */
    SYSTEM.SOSCCR.BYTE = 0x01;          
#endif


#if (BSP_CFG_CLOCK_SOURCE == 4) // PLL

    /* Set PLL Input Divisor. */
    SYSTEM.PLLCR.BIT.PLIDIV = BSP_CFG_PLL_DIV >> 1;

    /* Set PLL Multiplier. */
    SYSTEM.PLLCR.BIT.STC = ((uint8_t)((float)BSP_CFG_PLL_MUL * 2.0)) - 1;

    /* Set the PLL to operating. */
    SYSTEM.PLLCR2.BYTE = 0x00;          
    while (SYSTEM.OSCOVFSR.BIT.PLOVF != 1)
    {
        ;   // wait for stabilization
    }
#else
    /* Set the PLL to stopped. */
    SYSTEM.PLLCR2.BYTE = 0x01;          
#endif

    /* LOCO is saved for last since it is what is running by default out of reset. This means you do not want to turn
       it off until another clock has been enabled and is ready to use. */
#if (BSP_CFG_CLOCK_SOURCE == 0)
    /* LOCO is chosen. This is the default out of reset. */
    SYSTEM.LOCOCR.BYTE = 0x00;
#else
    /* LOCO is not chosen but it cannot be turned off yet since it is still being used. */
#endif

    /* Make sure a valid clock was chosen. */
#if (BSP_CFG_CLOCK_SOURCE > 4) || (BSP_CFG_CLOCK_SOURCE < 0)
    #error "Error! Invalid setting for BSP_CFG_CLOCK_SOURCE in r_bsp_config.h"
#endif 
}


/***********************************************************************************************************************
* Function name: usb_lpc_clock_source_select
* Description  : Enables clock sources for the usb and lpc (if not already done) as chosen by the user.
*                This function also implements the software delays needed for the clocks to stabilize.
* Arguments    : none
* Return value : none
***********************************************************************************************************************/
static void usb_lpc_clock_source_select (void)
{
    /* Declared volatile for software delay purposes. */
    volatile uint32_t i;


    /* Protect off. DO NOT USE R_BSP_RegisterProtectDisable()! (not initialized yet) */
    SYSTEM.PRCR.WORD = 0xA50F;


    /* INITIALIZE AND SELECT UCLK SOURCE */

#if (BSP_CFG_USB_CLOCK_SOURCE == 1)
    /* USB PLL is chosen. Start it operating. */
    /* main clock oscillator already initialized in clock_source_select() or cgc_clock_config() */

    /* Set PLL Input Divisor. */
    SYSTEM.UPLLCR.BIT.UPLIDIV = BSP_CFG_UPLL_DIV >> 1;

    /* Set PLL Multiplier. */
    SYSTEM.UPLLCR.BIT.USTC = (BSP_CFG_UPLL_MUL * 2) - 1;

    /* Set USB PLL as selected UCLK */
    SYSTEM.UPLLCR.BIT.UCKUPLLSEL = 1;

    /* Set the PLL to operating. */
    SYSTEM.UPLLCR2.BYTE = 0x00;

    while (SYSTEM.OSCOVFSR.BIT.UPLOVF != 1)
    {
        /* Make sure clock has stabilized. */
    }
#elif (BSP_CFG_USB_CLOCK_SOURCE == 0)
    /* This is the default config setting in order to save power by disabling the USB PLL.
     * IF the USB clock will be used then the USB PLL should be enabled. It is configured for
     * 48 MHz via the config file settings */

    /* Set system clock as selected UCLK */
    SYSTEM.UPLLCR.BIT.UCKUPLLSEL = 0;           // default from reset
    /* Set the USB PLL to stopped. */
    SYSTEM.UPLLCR2.BYTE = 0x01;                 // default from reset
#else
    #error "ERROR - Valid USB clock source must be chosen in r_bsp_config.h using BSP_CFG_USB_CLOCK_SOURCE macro."
#endif

    /* INITIALIZE AND SELECT LPT CLOCK SOURCE */

#if (BSP_CFG_LPT_CLOCK_SOURCE == 0) || (BSP_CFG_LPT_CLOCK_SOURCE == 2)
    /* Sub-clock is chosen. */
    /* sub-clock oscillator already initialized in clock_source_select() or cgc_clock_config() */

#elif (BSP_CFG_LPT_CLOCK_SOURCE == 1)
    /* IWDTCLK is chosen. Start it operating. */
    SYSTEM.ILOCOCR.BYTE = 0x00;
	
    /* Wait processing for the IWDT clock oscillation stabilization (50us) */
    for(i = 0; i < IWDTCLK_STABILIZE_LOOP_CNT; i++)
    {
        nop() ;                     /* wait for stabilization */
    }

    /* Controls whether to stop the IWDT counter in a low power consumption state.
    IWDTCSTPR - IWDT Count Stop Control Register
    b7     SLCSTP   - Sleep Mode Count Stop Control - Count stop is disabled.
    b6:b1  Reserved - These bits are read as 0. Writing to these bits has no effect. */
    IWDT.IWDTCSTPR.BIT.SLCSTP = 0;
#else
    #error "Error! Invalid setting for BSP_CFG_LPT_CLOCK_SOURCE in r_bsp_config.h"
#endif

    SYSTEM.PRCR.WORD = 0xA500;
    return;
}

/***********************************************************************************************************************
* Function name: Change_PSW_PM_to_UserMode
* Description  : Assembler function, used to change the MCU's usermode from supervisor to user.
* Arguments    : none
* Return value : none
***********************************************************************************************************************/
#if BSP_CFG_RUN_IN_USER_MODE==1
    #if __RENESAS_VERSION__ < 0x01010000
static void Change_PSW_PM_to_UserMode(void)
{
    MVFC   PSW,R1
    OR     #00100000h,R1
    PUSH.L R1
    MVFC   PC,R1
    ADD    #10,R1
    PUSH.L R1
    RTE
    NOP
    NOP
}
    #endif
#endif

#endif /* BSP_CFG_STARTUP_DISABLE == 0 */
