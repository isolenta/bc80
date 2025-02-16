#pragma once

void bc80_write_mem(uint16_t addr, uint8_t byte);
uint8_t bc80_read_mem(uint16_t addr);
void bc80_bus_acquire();
void bc80_bus_release();
