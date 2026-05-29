/**
 * @file obc_errors.h
 * @brief Project-wide error codes and helpers.
 */
#ifndef OBC_ERRORS_H
#define OBC_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OBC_OK = 0,
    OBC_ERR_FAIL = -1,          /**< Unspecified failure. */
    OBC_ERR_TIMEOUT = -2,       /**< Operation timed out. */
    OBC_ERR_NO_DEVICE = -3,     /**< Hardware not present (sim fallback used). */
    OBC_ERR_INVALID_ARG = -4,
    OBC_ERR_CRC = -5,           /**< Frame/checksum mismatch. */
    OBC_ERR_NO_MEM = -6,
    OBC_ERR_BUSY = -7,
} obc_err_t;

/** @return Human-readable name for an OBC error code. */
const char *obc_err_str(obc_err_t err);

#ifdef __cplusplus
}
#endif

#endif /* OBC_ERRORS_H */
