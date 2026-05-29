#include "obc_errors.h"

const char *obc_err_str(obc_err_t err)
{
    switch (err) {
        case OBC_OK:              return "OK";
        case OBC_ERR_FAIL:        return "FAIL";
        case OBC_ERR_TIMEOUT:     return "TIMEOUT";
        case OBC_ERR_NO_DEVICE:   return "NO_DEVICE";
        case OBC_ERR_INVALID_ARG: return "INVALID_ARG";
        case OBC_ERR_CRC:         return "CRC";
        case OBC_ERR_NO_MEM:      return "NO_MEM";
        case OBC_ERR_BUSY:        return "BUSY";
        default:                  return "UNKNOWN";
    }
}
