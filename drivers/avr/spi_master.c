/*  Copyright 2020
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <avr/io.h>

#include "spi_master.h"
#include "quantum.h"
#include "timer.h"

#if defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U4__) || defined(__AVR_ATmega32U4__) || defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB647__) || defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB1287__)
#    define SPI_SCK_PIN B1
#    define SPI_MOSI_PIN B2
#    define SPI_MISO_PIN B3
#elif defined(__AVR_ATmega32A__)
#    define SPI_SCK_PIN B7
#    define SPI_MOSI_PIN B5
#    define SPI_MISO_PIN B6
#elif defined(__AVR_ATmega328P__)
#    define SPI_SCK_PIN B5
#    define SPI_MOSI_PIN B3
#    define SPI_MISO_PIN B4
#endif

#ifndef SPI_TIMEOUT
#    define SPI_TIMEOUT 100
#endif

static pin_t   currentfollowerPin    = NO_PIN;
static uint8_t currentfollowerConfig = 0;
static bool    currentfollower2X     = false;

void spi_init(void) {
    writePinHigh(SPI_SS_PIN);
    setPinOutput(SPI_SCK_PIN);
    setPinOutput(SPI_MOSI_PIN);
    setPinInput(SPI_MISO_PIN);

    SPCR = (_BV(SPE) | _BV(MSTR));
}

bool spi_start(pin_t followerPin, bool lsbFirst, uint8_t mode, uint16_t divisor) {
    if (currentfollowerPin != NO_PIN || followerPin == NO_PIN) {
        return false;
    }

    currentfollowerConfig = 0;

    if (lsbFirst) {
        currentfollowerConfig |= _BV(DORD);
    }

    switch (mode) {
        case 1:
            currentfollowerConfig |= _BV(CPHA);
            break;
        case 2:
            currentfollowerConfig |= _BV(CPOL);
            break;
        case 3:
            currentfollowerConfig |= (_BV(CPOL) | _BV(CPHA));
            break;
    }

    uint16_t roundedDivisor = 1;
    while (roundedDivisor < divisor) {
        roundedDivisor <<= 1;
    }

    switch (roundedDivisor) {
        case 16:
            currentfollowerConfig |= _BV(SPR0);
            break;
        case 64:
            currentfollowerConfig |= _BV(SPR1);
            break;
        case 128:
            currentfollowerConfig |= (_BV(SPR1) | _BV(SPR0));
            break;
        case 2:
            currentfollower2X = true;
            break;
        case 8:
            currentfollower2X = true;
            currentfollowerConfig |= _BV(SPR0);
            break;
        case 32:
            currentfollower2X = true;
            currentfollowerConfig |= _BV(SPR1);
            break;
    }

    SPCR |= currentfollowerConfig;
    if (currentfollower2X) {
        SPSR |= _BV(SPI2X);
    }
    currentfollowerPin = followerPin;
    setPinOutput(currentfollowerPin);
    writePinLow(currentfollowerPin);

    return true;
}

spi_status_t spi_write(uint8_t data) {
    SPDR = data;

    uint16_t timeout_timer = timer_read();
    while (!(SPSR & _BV(SPIF))) {
        if ((timer_read() - timeout_timer) >= SPI_TIMEOUT) {
            return SPI_STATUS_TIMEOUT;
        }
    }

    return SPDR;
}

spi_status_t spi_read() {
    SPDR = 0x00;  // Dummy

    uint16_t timeout_timer = timer_read();
    while (!(SPSR & _BV(SPIF))) {
        if ((timer_read() - timeout_timer) >= SPI_TIMEOUT) {
            return SPI_STATUS_TIMEOUT;
        }
    }

    return SPDR;
}

spi_status_t spi_transmit(const uint8_t *data, uint16_t length) {
    spi_status_t status = SPI_STATUS_ERROR;

    for (uint16_t i = 0; i < length; i++) {
        status = spi_write(data[i]);
    }

    return status;
}

spi_status_t spi_receive(uint8_t *data, uint16_t length) {
    spi_status_t status = SPI_STATUS_ERROR;

    for (uint16_t i = 0; i < length; i++) {
        status = spi_read();

        if (status > 0) {
            data[i] = status;
        }
    }

    return (status < 0) ? status : SPI_STATUS_SUCCESS;
}

void spi_stop(void) {
    if (currentfollowerPin != NO_PIN) {
        setPinOutput(currentfollowerPin);
        writePinHigh(currentfollowerPin);
        currentfollowerPin = NO_PIN;
        SPSR &= ~(_BV(SPI2X));
        SPCR &= ~(currentfollowerConfig);
        currentfollowerConfig = 0;
        currentfollower2X     = false;
    }
}
