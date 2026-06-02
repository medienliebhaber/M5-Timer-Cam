#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/* POST a JPEG buffer to the server's /api/frames endpoint.
 * trigger: "timer" | "test"
 */
esp_err_t http_client_post_frame(const uint8_t *buf, size_t len,
                                  const char *trigger);
