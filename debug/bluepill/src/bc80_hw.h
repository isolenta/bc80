#pragma once

void bc80_write_mem(uint16_t addr, uint8_t byte);
uint8_t bc80_read_mem(uint16_t addr);
void bc80_write_io(uint16_t addr, uint8_t byte);
uint8_t bc80_read_io(uint16_t addr);
void bc80_lcd_ctrl(uint8_t is_data, uint8_t val);
