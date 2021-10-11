#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_MOSI 19

#define FLASH_PAGE_SIZE 256

#define FLASH_CMD_PAGE_PROGRAM 0x02
#define FLASH_CMD_READ 0x03
#define FLASH_CMD_STATUS 0x05
#define FLASH_CMD_WRITE_EN 0x06
#define FLASH_CMD_SECTOR_ERASE 0x20

#define FLASH_STATUS_BUSY_MASK 0x01

static inline void cs_select(uint cs_pin)
{
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop"); // FIXME
}

static inline void cs_deselect(uint cs_pin)
{
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop"); // FIXME
}

void __not_in_flash_func(flash_read)(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t *buf, size_t len)
{
    cs_select(cs_pin);
    uint8_t cmdbuf[4] = {
        FLASH_CMD_READ,
        addr >> 16,
        addr >> 8,
        addr};
    spi_write_blocking(spi, cmdbuf, 4);
    spi_read_blocking(spi, 0, buf, len);
    cs_deselect(cs_pin);
}

void __not_in_flash_func(flash_write_enable)(spi_inst_t *spi, uint cs_pin)
{
    cs_select(cs_pin);
    uint8_t cmd = FLASH_CMD_WRITE_EN;
    spi_write_blocking(spi, &cmd, 1);
    cs_deselect(cs_pin);
}

void __not_in_flash_func(flash_wait_done)(spi_inst_t *spi, uint cs_pin)
{
    uint8_t status;
    do
    {
        cs_select(cs_pin);
        uint8_t buf[2] = {FLASH_CMD_STATUS, 0};
        spi_write_read_blocking(spi, buf, buf, 2);
        cs_deselect(cs_pin);
        status = buf[1];
    } while (status & FLASH_STATUS_BUSY_MASK);
}

void __not_in_flash_func(flash_sector_erase)(spi_inst_t *spi, uint cs_pin, uint32_t addr)
{
    uint8_t cmdbuf[4] = {
        FLASH_CMD_SECTOR_ERASE,
        addr >> 16,
        addr >> 8,
        addr};
    flash_write_enable(spi, cs_pin);
    cs_select(cs_pin);
    spi_write_blocking(spi, cmdbuf, 4);
    cs_deselect(cs_pin);
    flash_wait_done(spi, cs_pin);
}

void __not_in_flash_func(flash_page_program)(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t data[])
{
    uint8_t cmdbuf[4] = {
        FLASH_CMD_PAGE_PROGRAM,
        addr >> 16,
        addr >> 8,
        addr};
    flash_write_enable(spi, cs_pin);
    cs_select(cs_pin);
    spi_write_blocking(spi, cmdbuf, 4);
    spi_write_blocking(spi, data, FLASH_PAGE_SIZE);
    cs_deselect(cs_pin);
    flash_wait_done(spi, cs_pin);
}

void printbuf(uint8_t buf[FLASH_PAGE_SIZE])
{
    for (int i = 0; i < FLASH_PAGE_SIZE; ++i)
    {
        if (i % 16 == 15)
            printf("%02x\n", buf[i]);
        else
            printf("%02x ", buf[i]);
    }
}

int main()
{
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    printf("SPI initialised, let's goooooo\n");

    uint8_t page_buf[FLASH_PAGE_SIZE] = {0};

    const uint32_t target_addr = 0;

    flash_sector_erase(SPI_PORT, PIN_CS, 0);
    flash_sector_erase(SPI_PORT, PIN_CS, 256);

    page_buf[0] = 6;
    page_buf[1] = 8;
    flash_page_program(SPI_PORT, PIN_CS, 0, page_buf);
    flash_read(SPI_PORT, PIN_CS, 0, page_buf, FLASH_PAGE_SIZE);
    printf("After program 0:\n");
    printbuf(page_buf);

    for (int i = 0; i < FLASH_PAGE_SIZE; ++i)
        page_buf[i] = 0;

    page_buf[0] = 5;
    page_buf[1] = 9;
    flash_page_program(SPI_PORT, PIN_CS, 256, page_buf);
    flash_read(SPI_PORT, PIN_CS, 256, page_buf, FLASH_PAGE_SIZE);
    printf("After program 1a:\n");
    printbuf(page_buf);

    flash_read(SPI_PORT, PIN_CS, 0, page_buf, FLASH_PAGE_SIZE);
    printf("After program 1b:\n");
    printbuf(page_buf);

    return 0;
}
