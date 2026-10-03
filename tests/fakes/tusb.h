#ifndef PWT_TEST_TUSB_H
#define PWT_TEST_TUSB_H
#include <stdbool.h>
#include <stdint.h>
bool tud_mounted(void);
bool tud_bt_event_send(void *event, uint16_t length);
bool tud_bt_acl_data_send(void *data, uint16_t length);
bool tud_bt_acl_data_receive_ready(void);
#endif
