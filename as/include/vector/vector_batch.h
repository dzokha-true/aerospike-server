/*
 * vector_batch.h
 *
 * EC528: VECTOR_DISTANCE batch handler entry point.
 */
#pragma once

#include "base/transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

int
as_vector_batch_handle(as_transaction* btr);

#ifdef __cplusplus
}
#endif
