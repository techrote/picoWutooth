#include "usb_descriptor_data.h"

#include <stddef.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

uint8_t const *tud_descriptor_device_cb(void) {
    return pwt_usb_device_descriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return pwt_usb_configuration_descriptor;
}

enum {
    PWT_STRING_LANGID = 0,
    PWT_STRING_MANUFACTURER,
    PWT_STRING_PRODUCT,
    PWT_STRING_SERIAL,
    PWT_STRING_INTERFACE,
};

static const char *const string_descriptors[] = {
    NULL,
    "picoWutooth",
    "picoWutooth Bluetooth HCI",
    NULL,
    "Bluetooth HCI",
};

static uint16_t string_buffer[33];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    size_t count = 0;
    if (index == PWT_STRING_LANGID) {
        string_buffer[1] = 0x0409;
        count = 1;
    } else if (index == PWT_STRING_SERIAL) {
        count = board_usb_get_serial(&string_buffer[1], 32);
    } else {
        if (index >= (sizeof(string_descriptors) / sizeof(string_descriptors[0])) ||
            string_descriptors[index] == NULL) {
            return NULL;
        }

        const char *source = string_descriptors[index];
        count = strlen(source);
        if (count > 32) {
            count = 32;
        }
        for (size_t i = 0; i < count; ++i) {
            string_buffer[1 + i] = (uint16_t)(uint8_t)source[i];
        }
    }

    string_buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8u) | (2u * count + 2u));
    return string_buffer;
}
