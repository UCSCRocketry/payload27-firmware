//*****************************************************************************
//
// SPDX-FileCopyrightText: Copyright (c) 2026, UCSC Rocket Team
//
// SPDX-License-Identifier: BSD-3-Clause
//
//*****************************************************************************

//*****************************************************************************
//
//! @file main.c
//!
//! @brief A simple "Hello World" example.
//!
//! @addtogroup payload_examples Payload Examples
//
//! @defgroup hello_world Hello World Example
//! @ingroup payload_examples
//! @{
//!
//! Purpose: This example demonstrates basic Zephyr startup on the STM32
//! Nucleo boards. The application prints various device information 
//! over the SEGGER RTT console for real-time monitoring and debugging.
//!
//! @section hello_world_functionality Functionality
//!
//! The application performs the following operations:
//! - Prints the "Hello World!" banner with the board target
//! - Reads and displays the device ID, revision, package, flash size and
//!   unique ID
//! - Reads and displays the Cortex-M CPUID, core clock, and FPU, cache, MPU
//!   and security state
//! - Reads and clears the reset cause flags
//! - Displays the Zephyr, STM32Cube HAL, and compiler versions
//! - Returns from main() so the idle thread can sleep
//!
//! @section hello_world_usage Usage
//!
//! 1. Plug a J-Link into the MIPI20 connector (CN1)
//! 2. Build with west build -p always
//! 3. Load and start the image with scripts/jlink.py load
//! 4. Monitor the RTT console with JLinkRTTClient
//!
//! @section hello_world_configuration Configuration
//!
//! - @b CONFIG_HWINFO: Enables the unique device ID and reset cause
//! - @b CONFIG_RTT_CONSOLE: Sends console output over RTT (set per board in
//!   boards/*.conf)
//! - @b BOARD: Target board, set in CMakeLists.txt or with west build -b
//
//*****************************************************************************

//
// Standard C library
//
#include <stdio.h>                  // printf()

//
// Zephyr kernel and drivers
//
#include <zephyr/kernel.h>          // Kernel services and clock rates
#include <zephyr/version.h>         // KERNEL_VERSION_STRING and BUILD_VERSION
#include <zephyr/drivers/hwinfo.h>  // Unique device ID and reset cause

//
// Zephyr utilities
//
#include <zephyr/sys/util.h>        // ARRAY_SIZE(), IS_ENABLED() and STRINGIFY()

//
// STM32Cube HAL and LL drivers
//
#ifdef CONFIG_SOC_FAMILY_STM32
#include <soc.h>                    // Device registers and HAL_GetHalVersion()
#include <stm32_ll_bus.h>           // Peripheral clock enables
#include <stm32_ll_system.h>        // Device ID, revision and flash latency
#include <stm32_ll_utils.h>         // Flash size and package type
#endif

//
// CMSIS core
//
#ifdef CONFIG_CPU_CORTEX_M
#include <cmsis_core.h>             // SCB, MPU and CoreDebug registers
#endif

//*****************************************************************************
//
// Main.
//
//*****************************************************************************
int
main(void)
{
    uint8_t uid[16];
    ssize_t uid_len;
    uint32_t cause;
    int ret;

    //
    // ========================================================================
    // Enable required clocks
    // ========================================================================
    //

#if defined(CONFIG_SOC_SERIES_STM32N6X)
    //
    // Enable the BSEC clock. The device ID, package and unique ID are read
    // from the BSEC fuses.
    //
    LL_APB4_GRP2_EnableClock(LL_APB4_GRP2_PERIPH_BSEC);
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
    //
    // Enable the DBGMCU clock, so IDCODE can be read without a debugger
    // attached.
    //
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_DBGMCU);
#endif // CONFIG_SOC_SERIES_STM32N6X


    printf("Hello World! %s\n\n", CONFIG_BOARD_TARGET);

    //
    // ========================================================================
    // Print the device info.
    // ========================================================================
    //
    printf("Device Info:\n");
#ifdef CONFIG_SOC_SERIES
    printf("\tSoC:            %s (%s series)\n", CONFIG_SOC, CONFIG_SOC_SERIES);
#else
    printf("\tSoC:            %s\n", CONFIG_SOC);
#endif

#if defined(CONFIG_SOC_SERIES_STM32N6X)
#ifdef CPU_IN_SECURE_STATE
    //
    // The LL only provides the device ID and revision in the secure state.
    //
    uint32_t rev_id = LL_GetRevisionID();

    printf("\tDevice ID:      0x%04X\n", LL_GetDeviceID());
    printf("\tRevision:       0x%04X (Cut %u.%u)\n", rev_id, rev_id >> 8, rev_id & 0xFF);
#endif // CPU_IN_SECURE_STATE

    //
    // Decode the package type.
    //
    const char *package_name;

    switch (LL_GetPackageType())
    {
        case LL_UTILS_PACKAGETYPE_BGA142:
            package_name = "BGA142";
            break;
        case LL_UTILS_PACKAGETYPE_BGA169:
            package_name = "BGA169";
            break;
        case LL_UTILS_PACKAGETYPE_BGA178:
            package_name = "BGA178";
            break;
        case LL_UTILS_PACKAGETYPE_BGA198:
            package_name = "BGA198";
            break;
        case LL_UTILS_PACKAGETYPE_BGA223:
            package_name = "BGA223";
            break;
        case LL_UTILS_PACKAGETYPE_BGA264:
            package_name = "BGA264";
            break;
        default:
            package_name = "Unknown";
            break;
    }
    printf("\tPackage:        %s\n", package_name);

#elif defined(CONFIG_SOC_SERIES_STM32F4X) || defined(CONFIG_SOC_SERIES_STM32L0X)
    printf("\tDevice ID:      0x%03X\n", LL_DBGMCU_GetDeviceID());
    printf("\tRevision:       0x%04X\n", LL_DBGMCU_GetRevisionID());
    printf("\tFlash size:     %u KB\n", LL_GetFlashSize());
#endif // CONFIG_SOC_SERIES_STM32N6X

    //
    // Print the unique device ID.
    //
    uid_len = hwinfo_get_device_id(uid, sizeof(uid));
    if (uid_len > 0)
    {
        printf("\tUnique ID:      ");
        for (ssize_t i = 0; i < uid_len; i++)
        {
            printf("%02X", uid[i]);
        }
        printf("\n");
    }
    else
    {
        printf("\tUnique ID:      unavailable (%d)\n", (int)uid_len);
    }

    //
    // Print the SRAM size and base address from the devicetree.
    //
#if defined(CONFIG_SRAM_SIZE) && defined(CONFIG_SRAM_BASE_ADDRESS)
    printf("\tSRAM:           %u KB @ 0x%08X\n", CONFIG_SRAM_SIZE, CONFIG_SRAM_BASE_ADDRESS);
#endif
    printf("\n");

    //
    // ========================================================================
    // Print the CPU info.
    // ========================================================================
    //
    printf("CPU Info:\n");

#ifdef CONFIG_CPU_CORTEX_M
    //
    // Decode the core and its revision from the CPUID register.
    //
    uint32_t cpuid = SCB->CPUID;
    uint32_t partno = (cpuid & SCB_CPUID_PARTNO_Msk) >> SCB_CPUID_PARTNO_Pos;
    uint32_t variant = (cpuid & SCB_CPUID_VARIANT_Msk) >> SCB_CPUID_VARIANT_Pos;
    uint32_t revision = (cpuid & SCB_CPUID_REVISION_Msk) >> SCB_CPUID_REVISION_Pos;
    const char *core_name;

    switch (partno)
    {
        case 0xC60:
            core_name = "Cortex-M0+";
            break;
        case 0xC24:
            core_name = "Cortex-M4";
            break;
        case 0xD22:
            core_name = "Cortex-M55";
            break;
        default:
            core_name = "Cortex-M (unknown)";
            break;
    }
    printf("\tCore:           %s r%up%u (CPUID 0x%08X)\n", core_name, variant, revision, cpuid);
#endif // CONFIG_CPU_CORTEX_M

    //
    // Print the core clock.
    //
#ifdef CONFIG_SOC_FAMILY_STM32
    printf("\tCore clock:     %u Hz\n", SystemCoreClock);
#else
    printf("\tHW cycle clock: %u Hz\n", sys_clock_hw_cycles_per_sec());
#endif

#ifdef CONFIG_CPU_HAS_FPU
    //
    // CP10 and CP11 full access means the FPU has been enabled.
    //
    printf("\tFPU:            %s\n",
           ((SCB->CPACR >> 20) & 0xF) == 0xF ? "enabled" : "disabled");
#endif

#if defined(CONFIG_ARMV8_1_M_MVEF)
    printf("\tMVE (Helium):   integer + float\n");
#elif defined(CONFIG_ARMV8_1_M_MVEI)
    printf("\tMVE (Helium):   integer\n");
#endif

    //
    // Print the I-Cache and D-Cache state.
    //
#ifdef CONFIG_CPU_HAS_ICACHE
    printf("\tI-Cache:        %s\n", (SCB->CCR & SCB_CCR_IC_Msk) ? "enabled" : "disabled");
#endif
#ifdef CONFIG_CPU_HAS_DCACHE
    printf("\tD-Cache:        %s\n", (SCB->CCR & SCB_CCR_DC_Msk) ? "enabled" : "disabled");
#endif

#if defined(CONFIG_SOC_SERIES_STM32F4X) || defined(CONFIG_SOC_SERIES_STM32L0X)
    //
    // Print the flash wait states and accelerator configuration.
    //
    uint32_t acr = FLASH->ACR;

    printf("\tFlash latency:  %u wait states\n", LL_FLASH_GetLatency());
    printf("\tFlash prefetch: %s\n", (acr & FLASH_ACR_PRFTEN) ? "enabled" : "disabled");
#ifdef CONFIG_SOC_SERIES_STM32F4X
    printf("\tART I-Cache:    %s\n", (acr & FLASH_ACR_ICEN) ? "enabled" : "disabled");
    printf("\tART D-Cache:    %s\n", (acr & FLASH_ACR_DCEN) ? "enabled" : "disabled");
#endif
#endif // CONFIG_SOC_SERIES_STM32F4X || CONFIG_SOC_SERIES_STM32L0X

#ifdef CONFIG_CPU_HAS_ARM_MPU
    //
    // Print the number of MPU regions.
    //
    uint32_t mpu_regions = (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;

    printf("\tMPU regions:    %u\n", mpu_regions);
#endif

#ifdef CONFIG_ARMV8_M_SE
    //
    // Print the TrustZone security state.
    //
    printf("\tSecurity state: %s\n",
           IS_ENABLED(CONFIG_TRUSTED_EXECUTION_SECURE) ? "Secure" : "Non-secure");
#endif
    printf("\n");

    //
    // ========================================================================
    // Print the reset info.
    // ========================================================================
    //
    printf("Reset Info:\n");

    ret = hwinfo_get_reset_cause(&cause);
    if (ret == 0)
    {
        printf("\tReset flags:    0x%08X\n", cause);
    }
    else
    {
        printf("\tReset flags:    unavailable (%d)\n", ret);
    }
    printf("\n");

    //
    // ========================================================================
    // Print the software info.
    // ========================================================================
    //
    printf("Software Info:\n");
    printf("\tZephyr:         %s (%s)\n", KERNEL_VERSION_STRING, STRINGIFY(BUILD_VERSION));

#ifdef CONFIG_SOC_FAMILY_STM32
    uint32_t hal_version = HAL_GetHalVersion();

    printf("\tSTM32Cube HAL:  %u.%u.%u\n", hal_version >> 24, (hal_version >> 16) & 0xFF,
           (hal_version >> 8) & 0xFF);
#endif

#if defined(__clang__)
    printf("\tCompiler:       %s\n", __VERSION__);
#elif defined(__GNUC__)
    printf("\tCompiler:       GCC %s\n", __VERSION__);
#endif
    printf("\n");


    hwinfo_clear_reset_cause();

    //
    // We are done printing.
    //
    return 0;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
