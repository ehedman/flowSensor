#pragma once

/**
 * C to C++ interface
 */
#ifndef _WATERCTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oneWire {
    uint _data_pin;
    uint _parasite_pin;
    bool _parasite_power{};
    bool _power_mosfet;
    bool _power_polarity;
} _oneWire;

static _oneWire oneWire;


void one_wire_init(uint dataPin)
{
    oneWire._data_pin = dataPin;
    One_wire obj = One_wire(oneWire._data_pin);

    obj.init();  
}

int one_wire_find_and_count_devices_on_bus(void)
{
    One_wire obj = One_wire(oneWire._data_pin);
   
    return obj.find_and_count_devices_on_bus();    
}

int one_wire_convert_temperature(rom_address_t &address, bool wait, bool all)
{
    One_wire obj = One_wire(oneWire._data_pin);

    return obj.convert_temperature(address, wait, all);
}


float one_wire_temperature(rom_address_t &address)
{
    One_wire obj = One_wire(oneWire._data_pin);
    return obj.temperature(address, false);
}

bool one_wire_set_resolution(rom_address_t &address, unsigned int resolution)
{
    One_wire obj = One_wire(oneWire._data_pin);

    return obj.set_resolution(address, resolution);
}

rom_address_t one_wire_address_from_hex(const char *hex_address)
{
    One_wire obj = One_wire(oneWire._data_pin);

    return obj.address_from_hex(hex_address);
}

rom_address_t &one_wire_get_address(int index)
{
    One_wire obj = One_wire(oneWire._data_pin);
    return obj.get_address(index);

}

void one_wire_single_device_read_rom(rom_address_t &rom_address)
{
    One_wire obj = One_wire(oneWire._data_pin);

    obj.single_device_read_rom(rom_address);

}


#ifdef __cplusplus
}
#endif

#else /* _WATERCTRL_H_ */

#define ROMSize 8

typedef struct _rom_address_t {
    uint8_t rom[ROMSize];
} rom_address_t;

void            one_wire_init(uint dataPin);
int             one_wire_find_and_count_devices_on_bus(void);
int             one_wire_convert_temperature(rom_address_t *address, bool wait, bool all);
float           one_wire_temperature(rom_address_t *address);
bool            one_wire_set_resolution(rom_address_t *address, unsigned int resolution);
rom_address_t   one_wire_address_from_hex(const char *hex_address);
rom_address_t   one_wire_get_address(int index);
void            one_wire_single_device_read_rom(rom_address_t *rom_address);

#endif


