#ifndef WIFI_COMPONENT_H
#define WIFI_COMPONENT_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_connect(void);

bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_COMPONENT_H */
