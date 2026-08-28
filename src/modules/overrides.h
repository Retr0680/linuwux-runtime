/*
 * Copyright (C) 2026 brcly
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef LINUWUX_OVERRIDES_H
#define LINUWUX_OVERRIDES_H

/*
 * WINEDLLOVERRIDES merge for pack-native DLLs.
 *
 * Wine resolves DLL names with a load order (native vs builtin). We do not
 * LoadLibrary the pack's winmm/version/reflex ourselves — we only ensure
 * that when something requests those names, native (next to the exe) wins.
 *
 * Policy:
 *   - Parse the existing env into a name→order map (handles grouped
 *     entries like "winmm,version=n,b").
 *   - For each pack-native DLL that actually exists beside the game exe,
 *     add "name=n,b" only if that name is not already present (user /
 *     Proton / Lutris values are left alone).
 *   - Serialize back; on truncation leave the env unchanged and log.
 */

/* Bitflags: native PE files seen next to the Windows target exe. */
#define LINUWUX_NATIVE_WINMM     (1u << 0)
#define LINUWUX_NATIVE_VERSION   (1u << 1)
#define LINUWUX_NATIVE_REFLEX    (1u << 2)
#define LINUWUX_NATIVE_REFLEX64  (1u << 3)

/*
 * Merge existence-gated overrides into WINEDLLOVERRIDES and setenv(3).
 * present: OR of LINUWUX_NATIVE_* for files found in the game directory.
 * Returns 0 on success (including "nothing to add"), -1 if the result
 * would not fit and the environment was left unchanged.
 */
int linuwux_overrides_apply(unsigned present);

#endif /* LINUWUX_OVERRIDES_H */
