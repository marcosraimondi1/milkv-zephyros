/*
 * Copyright (c) 2024 GP Orcullo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

SECTION_PROLOGUE (.resource_table,,ALIGN(8))
    {
        KEEP(*(.resource_table))
    }

