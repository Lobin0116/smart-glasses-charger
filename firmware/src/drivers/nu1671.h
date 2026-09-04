#ifndef NU1671_H
#define NU1671_H

#include <stdbool.h>

/* NuVolta NU1671 wireless power receiver (Board1_V2, replacing the V1
 * MT5706). Family reference: NU1677 datasheet (docs/datasheet/) — pin map,
 * electrical limits and the PDET flow below all come from it.
 *
 * The part has NO supply rail of its own and NO enable pin: as a receiver it
 * powers itself from the coil field, so off the pad it draws nothing and its
 * I2C/nINT/PDETB interface is simply dead. USB 5V and the RX output are
 * OR'd onto VBUS_IN in hardware (Q1), so wireless charging of the case
 * battery needs no software enable — the IP5353 charge state already
 * reflects it. What firmware can do:
 *   - probe the bus to learn whether the case is sitting on a TX pad
 *   - latch nINT edges (status/fault change) and re-probe on them
 *   - run the ping-detect loop (PDET_EN) to watch for pad removal at ~20µA
 *
 * SKU register map: the family datasheet documents the interface but not the
 * register table; status decoding waits for the NU1671 programming guide. */

/* 7-bit I2C address, NU1677 datasheet §8.9. */
#define NU1671_I2C_ADDR 0x34U

/* Call from the EXTI callback on an nINT (PA7) falling edge. Latches a
 * pending event; the main loop consumes it via nu1671_poll(). */
void nu1671_on_interrupt(void);

/* Main-loop side: consume any pending nINT event (re-probing the bus when
 * one occurred) and return the cached pad-presence flag. No periodic probe —
 * off-pad the chip just NACKs, and nINT covers every state change on-pad. */
bool nu1671_poll(void);

/* Address-only I2C transaction: ACK means the chip is powered, i.e. the case
 * is on a transmitter pad. Updates the cached flag read by is_present(). */
void nu1671_probe(void);
bool nu1671_is_present(void);

/* Ping-detect control (datasheet §8.12): PDET_EN also powers the on-chip
 * detect block (<20µA). While enabled, the NU1671 holds PDETB low as long
 * as it keeps seeing transmitter pings; high means the pad is gone. */
void nu1671_pdet_enable(bool enable);
bool nu1671_tx_pad_present(void);

#endif /* NU1671_H */
