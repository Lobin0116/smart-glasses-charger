/*
 * Mock gd32e23x.h for host-side unit testing.
 * Redirects all SPL definitions to mock_spl.h.
 * This file must appear earlier in the include path than the real header.
 */
#ifndef GD32E23X_H_MOCK
#define GD32E23X_H_MOCK

#include "mock_spl.h"

#endif
