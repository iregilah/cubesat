/**
 * @file mode_manager.h
 * @brief Autonomous satellite mode state machine.
 *
 * Owns the policy for moving between BOOT/SAFE/NOMINAL/PAYLOAD/FAULT. Ground
 * can request a mode (via CDH), but the mode manager always retains authority
 * to demote to SAFE on a critical fault — autonomy must win over a stale
 * ground command when the spacecraft is in danger.
 */
#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t mode_manager_init(void);
obc_err_t mode_manager_start(void);

#ifdef __cplusplus
}
#endif

#endif /* MODE_MANAGER_H */
