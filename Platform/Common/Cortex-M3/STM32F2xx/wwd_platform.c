/*
 * Copyright 2013, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

/** @file
 *
 */
#include <stdint.h>
#include "stm32f2xx.h"
#include "gpio_irq.h"
#include "platform.h"
#include "platform_sleep.h"
#include "platform_common_config.h"
#include "platform_internal_gpio.h"
#include "MICOPlatform.h"

#ifndef WL_RESET_BANK
#error Missing WL_RESET_BANK definition
#endif
#ifndef WL_REG_ON_BANK
#error Missing WL_REG_ON_BANK definition
#endif

/******************************************************
 *                      Macros
 ******************************************************/
#ifdef __GNUC__
#define TRIGGER_BREAKPOINT() __asm__("bkpt")
#elif defined ( __IAR_SYSTEMS_ICC__ )
#define TRIGGER_BREAKPOINT() __asm("bkpt 0")
#endif

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

static OSStatus platform_reset_wlan_powersave_clock( void );

extern void host_platform_reset_wifi( bool reset_asserted );

extern void host_platform_power_wifi( bool power_enabled );

/******************************************************
 *               Variables Definitions
 ******************************************************/

/******************************************************
 *               Function Definitions
 ******************************************************/

OSStatus host_platform_init( void )
{
    GPIO_InitTypeDef gpio_init_structure;

    platform_reset_wlan_powersave_clock( );

    /* Enable the GPIO peripherals related to the reset and reg_on pins */
    RCC_AHB1PeriphClockCmd( WL_RESET_BANK_CLK | WL_REG_ON_BANK_CLK, ENABLE );

    /* Setup the reset pin */
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init_structure.GPIO_OType = GPIO_OType_PP;
    gpio_init_structure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio_init_structure.GPIO_Pin = WL_RESET_PIN;
    GPIO_Init( WL_RESET_BANK, &gpio_init_structure );
    host_platform_reset_wifi( true ); /* Start wifi chip in reset */

    gpio_init_structure.GPIO_Pin = WL_REG_ON_PIN;
    GPIO_Init( WL_REG_ON_BANK, &gpio_init_structure );
    host_platform_power_wifi( false ); /* Start wifi chip with regulators off */

    return kNoErr;
}

OSStatus host_platform_deinit( void )
{
    GPIO_InitTypeDef gpio_init_structure;

    /* Re-Setup the reset pin and REG_ON pin - these need to be held low to keep the chip powered down */
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init_structure.GPIO_OType = GPIO_OType_PP;
    gpio_init_structure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio_init_structure.GPIO_Pin = WL_RESET_PIN;
    GPIO_Init( WL_RESET_BANK, &gpio_init_structure );
    host_platform_reset_wifi( true ); /* Start wifi chip in reset */

    gpio_init_structure.GPIO_Pin = WL_REG_ON_PIN;
    GPIO_Init( WL_REG_ON_BANK, &gpio_init_structure );
    host_platform_power_wifi( false ); /* Start wifi chip with regulators off */

    platform_reset_wlan_powersave_clock( );

    return kNoErr;
}

uint32_t host_platform_get_cycle_count(void)
{
/* From the ARM Cortex-M3 Techinical Reference Manual
 * 0xE0001004  DWT_CYCCNT  RW  0x00000000  Cycle Count Register */
    return *((volatile uint32_t*)0xE0001004);
}

bool host_platform_is_in_interrupt_context( void )
{
    /* From the ARM Cortex-M3 Techinical Reference Manual
     * 0xE000ED04   ICSR    RW [a]  Privileged  0x00000000  Interrupt Control and State Register */
    uint32_t active_interrupt_vector = (uint32_t)( SCB ->ICSR & 0x3fU );

    if ( active_interrupt_vector != 0 )
    {
        return true;
    }
    else
    {
        return false;
    }
}


OSStatus host_platform_init_wlan_powersave_clock( void )
{
#if ( MICO_WLAN_POWERSAVE_CLOCK_SOURCE == MICO_WLAN_POWERSAVE_CLOCK_IS_PWM )

    MicoPwmInitialize( (mico_pwm_t) MICO_PWM_WLAN_POWERSAVE_CLOCK, WLAN_POWERSAVE_CLOCK_FREQUENCY, WLAN_POWERSAVE_CLOCK_DUTY_CYCLE );
    MicoPwmStart( (mico_pwm_t) MICO_PWM_WLAN_POWERSAVE_CLOCK );
    return WICED_SUCCESS;

#elif ( MICO_WLAN_POWERSAVE_CLOCK_SOURCE == MICO_WLAN_POWERSAVE_CLOCK_IS_MCO )

    GPIO_InitTypeDef gpio_init_structure;

    /* Enable the GPIO peripherals related to the 32kHz clock pin */
    RCC_AHB1PeriphClockCmd( WL_32K_OUT_BANK_CLK, ENABLE );

    /* Setup the 32k clock pin to 0 */
    gpio_init_structure.GPIO_Speed = GPIO_Speed_100MHz;
    gpio_init_structure.GPIO_Mode  = GPIO_Mode_AF;
    gpio_init_structure.GPIO_OType = GPIO_OType_PP;
    gpio_init_structure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    /* configure PA8 in alternate function mode */
    gpio_init_structure.GPIO_Pin   = 1 << WL_32K_OUT_PIN;
    GPIO_Init( WL_32K_OUT_BANK, &gpio_init_structure );
    GPIO_PinAFConfig( WL_32K_OUT_BANK, WL_32K_OUT_PIN, GPIO_AF_MCO );

    /* enable LSE output on MCO1 */
    RCC_MCO1Config( RCC_MCO1Source_LSE, RCC_MCO1Div_1 );

    return kNoErr;

#else

    return platform_reset_wlan_powersave_clock( );

#endif
}

OSStatus host_platform_deinit_wlan_powersave_clock( void )
{
#if ( MICO_WLAN_POWERSAVE_CLOCK_SOURCE == MICO_WLAN_POWERSAVE_CLOCK_IS_PWM )

    MicoPwmStop( (mico_pwm_t) MICO_PWM_WLAN_POWERSAVE_CLOCK );
    platform_reset_wlan_powersave_clock( );
    return kNoErr;

#else

    return platform_reset_wlan_powersave_clock( );

#endif
}

static OSStatus platform_reset_wlan_powersave_clock( void )
{
    /* Tie the pin to ground */
    MicoGpioInitialize( (mico_gpio_t) MICO_GPIO_WLAN_POWERSAVE_CLOCK, OUTPUT_PUSH_PULL );
    MicoGpioOutputLow( (mico_gpio_t) MICO_GPIO_WLAN_POWERSAVE_CLOCK );
    return kNoErr;
}
