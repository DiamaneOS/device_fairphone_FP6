// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// The one PAL type volume_listener.c uses. The volume listener passes a table
// of these to the stock audio HAL (audio_hw_get_gain_level_mapping), which
// hands it to the stock libar-pal. The layout must match inc/PalDefs.h of
// Fairphone's published PAL (arpal-lx 6d6f50dba430a80b7eaf2c59cc9d63ced5125e99),
// which the stock HAL and libar-pal were built from.

#ifndef FP6_AUDIO_EFFECTS_PAL_DEFS_H
#define FP6_AUDIO_EFFECTS_PAL_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct pal_amp_db_and_gain_table {
    float amp;
    float db;
    uint32_t level;
};

_Static_assert(sizeof(struct pal_amp_db_and_gain_table) == 12, "PAL gain table entry size");
_Static_assert(offsetof(struct pal_amp_db_and_gain_table, db) == 4, "PAL gain table db offset");
_Static_assert(offsetof(struct pal_amp_db_and_gain_table, level) == 8, "PAL gain table level offset");

#endif  // FP6_AUDIO_EFFECTS_PAL_DEFS_H
