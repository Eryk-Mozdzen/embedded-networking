cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE "toolchain.cmake")

project(ppp-serial-spi)

enable_language(ASM C)

add_executable(${CMAKE_PROJECT_NAME}.elf
    "src/main.c"
    "src/fifo.c"
    "src/timer.c"
    "src/sys.c"

    "cubemx/Core/Src/main.c"
    "cubemx/Core/Src/stm32f4xx_hal_msp.c"
    "cubemx/Core/Src/stm32f4xx_it.c"
    "cubemx/Core/Src/syscalls.c"
    "cubemx/Core/Src/sysmem.c"
    "cubemx/Core/Src/system_stm32f4xx.c"
    "cubemx/startup_stm32f411xe.s"

    "../third-party/lwip/src/api/api_lib.c"
    "../third-party/lwip/src/api/api_msg.c"
    "../third-party/lwip/src/api/err.c"
    "../third-party/lwip/src/api/if_api.c"
    "../third-party/lwip/src/api/netbuf.c"
    "../third-party/lwip/src/api/netdb.c"
    "../third-party/lwip/src/api/netifapi.c"
    "../third-party/lwip/src/api/sockets.c"
    "../third-party/lwip/src/api/tcpip.c"
    "../third-party/lwip/src/apps/altcp_tls/altcp_tls_mbedtls_mem.c"
    "../third-party/lwip/src/apps/http/altcp_proxyconnect.c"
    "../third-party/lwip/src/apps/http/fs.c"
    "../third-party/lwip/src/apps/http/http_client.c"
    "../third-party/lwip/src/apps/http/httpd.c"
    "../third-party/lwip/src/apps/lwiperf/lwiperf.c"
    "../third-party/lwip/src/apps/mdns/mdns_domain.c"
    "../third-party/lwip/src/apps/mdns/mdns_out.c"
    "../third-party/lwip/src/apps/mdns/mdns.c"
    "../third-party/lwip/src/apps/mqtt/mqtt.c"
    "../third-party/lwip/src/apps/netbiosns/netbiosns.c"
    "../third-party/lwip/src/apps/smtp/smtp.c"
    "../third-party/lwip/src/apps/snmp/snmp_asn1.c"
    "../third-party/lwip/src/apps/snmp/snmp_core.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_icmp.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_interfaces.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_ip.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_snmp.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_system.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_tcp.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2_udp.c"
    "../third-party/lwip/src/apps/snmp/snmp_mib2.c"
    "../third-party/lwip/src/apps/snmp/snmp_msg.c"
    "../third-party/lwip/src/apps/snmp/snmp_netconn.c"
    "../third-party/lwip/src/apps/snmp/snmp_pbuf_stream.c"
    "../third-party/lwip/src/apps/snmp/snmp_raw.c"
    "../third-party/lwip/src/apps/snmp/snmp_scalar.c"
    "../third-party/lwip/src/apps/snmp/snmp_snmpv2_framework.c"
    "../third-party/lwip/src/apps/snmp/snmp_snmpv2_usm.c"
    "../third-party/lwip/src/apps/snmp/snmp_table.c"
    "../third-party/lwip/src/apps/snmp/snmp_threadsync.c"
    "../third-party/lwip/src/apps/snmp/snmp_traps.c"
    "../third-party/lwip/src/apps/snmp/snmpv3_mbedtls.c"
    "../third-party/lwip/src/apps/snmp/snmpv3.c"
    "../third-party/lwip/src/apps/sntp/sntp.c"
    "../third-party/lwip/src/apps/tftp/tftp.c"
    "../third-party/lwip/src/core/ipv4/acd.c"
    "../third-party/lwip/src/core/ipv4/autoip.c"
    "../third-party/lwip/src/core/ipv4/dhcp.c"
    "../third-party/lwip/src/core/ipv4/etharp.c"
    "../third-party/lwip/src/core/ipv4/icmp.c"
    "../third-party/lwip/src/core/ipv4/igmp.c"
    "../third-party/lwip/src/core/ipv4/ip4_addr.c"
    "../third-party/lwip/src/core/ipv4/ip4_frag.c"
    "../third-party/lwip/src/core/ipv4/ip4.c"
    "../third-party/lwip/src/core/ipv6/dhcp6.c"
    "../third-party/lwip/src/core/ipv6/ethip6.c"
    "../third-party/lwip/src/core/ipv6/icmp6.c"
    "../third-party/lwip/src/core/ipv6/inet6.c"
    "../third-party/lwip/src/core/ipv6/ip6_addr.c"
    "../third-party/lwip/src/core/ipv6/ip6_frag.c"
    "../third-party/lwip/src/core/ipv6/ip6.c"
    "../third-party/lwip/src/core/ipv6/mld6.c"
    "../third-party/lwip/src/core/ipv6/nd6.c"
    "../third-party/lwip/src/core/altcp_alloc.c"
    "../third-party/lwip/src/core/altcp_tcp.c"
    "../third-party/lwip/src/core/altcp.c"
    "../third-party/lwip/src/core/def.c"
    "../third-party/lwip/src/core/dns.c"
    "../third-party/lwip/src/core/inet_chksum.c"
    "../third-party/lwip/src/core/init.c"
    "../third-party/lwip/src/core/ip.c"
    "../third-party/lwip/src/core/mem.c"
    "../third-party/lwip/src/core/memp.c"
    "../third-party/lwip/src/core/netif.c"
    "../third-party/lwip/src/core/pbuf.c"
    "../third-party/lwip/src/core/raw.c"
    "../third-party/lwip/src/core/stats.c"
    "../third-party/lwip/src/core/sys.c"
    "../third-party/lwip/src/core/tcp_in.c"
    "../third-party/lwip/src/core/tcp_out.c"
    "../third-party/lwip/src/core/tcp.c"
    "../third-party/lwip/src/core/timeouts.c"
    "../third-party/lwip/src/core/udp.c"
    "../third-party/lwip/src/netif/ppp/polarssl/arc4.c"
    "../third-party/lwip/src/netif/ppp/polarssl/des.c"
    "../third-party/lwip/src/netif/ppp/polarssl/md4.c"
    "../third-party/lwip/src/netif/ppp/polarssl/md5.c"
    "../third-party/lwip/src/netif/ppp/polarssl/sha1.c"
    "../third-party/lwip/src/netif/ppp/auth.c"
    "../third-party/lwip/src/netif/ppp/ccp.c"
    "../third-party/lwip/src/netif/ppp/chap_ms.c"
    "../third-party/lwip/src/netif/ppp/chap-md5.c"
    "../third-party/lwip/src/netif/ppp/chap-new.c"
    "../third-party/lwip/src/netif/ppp/demand.c"
    "../third-party/lwip/src/netif/ppp/eap.c"
    "../third-party/lwip/src/netif/ppp/ecp.c"
    "../third-party/lwip/src/netif/ppp/eui64.c"
    "../third-party/lwip/src/netif/ppp/fsm.c"
    "../third-party/lwip/src/netif/ppp/ipcp.c"
    "../third-party/lwip/src/netif/ppp/ipv6cp.c"
    "../third-party/lwip/src/netif/ppp/lcp.c"
    "../third-party/lwip/src/netif/ppp/magic.c"
    "../third-party/lwip/src/netif/ppp/mppe.c"
    "../third-party/lwip/src/netif/ppp/multilink.c"
    "../third-party/lwip/src/netif/ppp/ppp.c"
    "../third-party/lwip/src/netif/ppp/pppapi.c"
    "../third-party/lwip/src/netif/ppp/pppcrypt.c"
    "../third-party/lwip/src/netif/ppp/pppoe.c"
    "../third-party/lwip/src/netif/ppp/pppol2tp.c"
    "../third-party/lwip/src/netif/ppp/pppos.c"
    "../third-party/lwip/src/netif/ppp/upap.c"
    "../third-party/lwip/src/netif/ppp/utils.c"
    "../third-party/lwip/src/netif/ppp/vj.c"
    "../third-party/lwip/src/netif/bridgeif_fdb.c"
    "../third-party/lwip/src/netif/bridgeif.c"
    "../third-party/lwip/src/netif/ethernet.c"
    "../third-party/lwip/src/netif/lowpan6_ble.c"
    "../third-party/lwip/src/netif/lowpan6_common.c"
    "../third-party/lwip/src/netif/lowpan6.c"
    "../third-party/lwip/src/netif/slipif.c"
    "../third-party/lwip/src/netif/zepif.c"

    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cec.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_crc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cryp.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cryp_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dcmi.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dcmi_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dfsdm.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma2d.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dsi.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_eth.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_fmpi2c.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_fmpi2c_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_fmpsmbus.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_fmpsmbus_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hash.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hash_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hcd.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2s.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2s_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_irda.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_lptim.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_ltdc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_ltdc_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_mmc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_nand.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_nor.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pccard.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_qspi.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rng.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sai.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sai_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sdram.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_smartcard.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_smbus.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spdifrx.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sram.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_usart.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_wwdg.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_adc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_crc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_dac.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_dma2d.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_dma.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_exti.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fmc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fmpi2c.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fsmc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_gpio.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_i2c.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_lptim.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_pwr.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rcc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rng.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_rtc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_spi.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_tim.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usart.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_utils.c"
)

target_include_directories(${CMAKE_PROJECT_NAME}.elf PRIVATE
	"include"
    "../third-party/lwip/src/include"
    "cubemx/Core/Inc"
    "cubemx/Drivers/STM32F4xx_HAL_Driver/Inc"
    "cubemx/Drivers/CMSIS/Include"
    "cubemx/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
)

target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    USE_FULL_LL_DRIVER
    STM32F411xE
)

target_compile_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
    $<$<COMPILE_LANGUAGE:ASM>:
        -x assembler-with-cpp

        -mthumb
        -mcpu=cortex-m4
        -mhard-float
        -mfpu=fpv4-sp-d16
	>

	$<$<COMPILE_LANGUAGE:C>:
		-O2 -g
		-std=c23
		-Wall
        -Wextra
        -Wpedantic

        -Wno-unused-parameter

		-ffunction-sections
		-fdata-sections

        -mthumb
        -mcpu=cortex-m4
        -mhard-float
        -mfpu=fpv4-sp-d16
	>
)

target_link_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
	-T ${CMAKE_SOURCE_DIR}/cubemx/STM32F411XX_FLASH.ld
	-Wl,-Map=${CMAKE_PROJECT_NAME}.map
	-Wl,--gc-sections
	-Wl,-cref
    -Wl,--start-group -lc -Wl,--end-group

    --specs=nano.specs

    -mthumb
    -mcpu=cortex-m4
	-mhard-float
	-mfpu=fpv4-sp-d16
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -j .text -D ${CMAKE_PROJECT_NAME}.elf > ${CMAKE_PROJECT_NAME}.dump
	COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
)

add_custom_target(flash
    COMMAND STM32_Programmer_CLI -c port=SWD -w ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.elf -rst
    DEPENDS ${CMAKE_PROJECT_NAME}.elf
)
