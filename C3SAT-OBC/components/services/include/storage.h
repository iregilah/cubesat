/**
 * @file storage.h
 * @brief Onboard "mass memory" — persists telemetry to a SPIFFS partition.
 *
 * Models a CubeSat mass-memory module: the storage task drains the telemetry
 * log queue and appends compact CSV records to flash, rotating the file when
 * it reaches a size cap (a stand-in for ring-buffered onboard storage).
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t storage_init(void);
obc_err_t storage_start(void);
/** Print the stored telemetry log to the console (TC_OP_DUMP_LOG). */
void      storage_dump(void);
/** Fill used/total bytes of the mass-memory filesystem. */
void      storage_stats(size_t *used, size_t *total);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
