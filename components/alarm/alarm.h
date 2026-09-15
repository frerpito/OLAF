#pragma once

#include <stdbool.h>

#include "esp_err.h"


typedef struct
{
    bool door_open;
    bool door_timeout_alarm;
    bool temperature_warning;
    bool temperature_alarm;
    bool recovery_active;
    float recovery_time_s;
} alarm_status_t;


esp_err_t alarm_init(void);


void alarm_notify_door_changed(void);


void alarm_set_status(
    const alarm_status_t *status
);


void alarm_update(void);
