#ifndef FW_VERSION_H
#define FW_VERSION_H

/* Case firmware version reported by the glasses via at_glass_data.case_version.
 * The case compares this against CASE_FW_VERSION: mismatch triggers OTA.
 * Bump on every release that should propagate to deployed cases. */
#define CASE_FW_VERSION 0x0CU

#endif /* FW_VERSION_H */
