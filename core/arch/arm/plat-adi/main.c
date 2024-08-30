/**
 * @todo spdx identifier
 * Copyright (c) 2022, Analog Devices, Inc.
 */

#include <console.h>
#include <drivers/gic.h>
#include <drivers/serial.h>
#include <io.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <platform_config.h>

#define ADI_UART4_STATUS 0x08

#define ADI_UART4_STATUS_THRE (1 << 5)
#define ADI_UART4_STATUS_DR (1 << 0)

#define ADI_UART4_RBR 0x20
#define ADI_UART4_THR 0x24

register_phys_mem(MEM_AREA_IO_NSEC, ADSP_SC5XX_UART0_BASE, ADSP_SC5XX_UART_SIZE);

static void adsp_serial_flush(struct serial_chip *chip __unused) {
}

static int adsp_serial_getchar(struct serial_chip *chip __unused) {
	vaddr_t uart_base = core_mmu_get_va(ADSP_SC5XX_UART0_BASE, MEM_AREA_IO_NSEC,
		ADSP_SC5XX_UART_SIZE);

	while (!(io_read32(uart_base + ADI_UART4_STATUS) & ADI_UART4_RBR))
		;

	io_write32(uart_base + ADI_UART4_STATUS, 0xffffffff);

	return io_read32(uart_base + ADI_UART4_RBR);
}

static void adsp_serial_putc(struct serial_chip *chip __unused, int ch) {
	vaddr_t uart_base = core_mmu_get_va(ADSP_SC5XX_UART0_BASE, MEM_AREA_IO_NSEC,
		ADSP_SC5XX_UART_SIZE);

	if ('\n' == ch)
		adsp_serial_putc(chip, '\r');

	while (!(io_read32(uart_base + ADI_UART4_STATUS) & ADI_UART4_STATUS_THRE))
		;

	io_write32(uart_base + ADI_UART4_THR, ch);
}

static const struct serial_ops uart_ops = {
	.flush = adsp_serial_flush,
	.getchar = adsp_serial_getchar,
	.putc = adsp_serial_putc,
};

static struct serial_chip uart_chip __nex_bss = {
	.ops = &uart_ops,
};

/**
 * Inherit serial configuration from previous bootloaders
 */
void console_init(void) {
	vaddr_t uart_base = core_mmu_get_va(ADSP_SC5XX_UART0_BASE, MEM_AREA_IO_NSEC,
		ADSP_SC5XX_UART_SIZE);

	register_serial_console(&uart_chip);
}
