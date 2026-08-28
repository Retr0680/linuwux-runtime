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

/*
 * Robust WINEDLLOVERRIDES parse / merge / format.
 *
 * Format (wine(1)):
 *   entry  := name[,name...] = order
 *   list   := entry[;entry...]
 *   order  := n,b | b,n | n | b | <empty> | ...
 *
 * Grouped names ("winmm,version=n,b") are expanded to per-name entries
 * on parse. Serialization emits one name=order per entry — equivalent
 * for Wine, simpler to merge without collisions.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "linuwux.h"
#include "overrides.h"

/* Lutris/Proton can ship long override lists; keep headroom. */
#define OV_MAX_ENTRIES  128
#define OV_NAME_MAX     128
#define OV_ORDER_MAX    32
#define OV_OUT_MAX      8192

struct ov_entry {
    char name[OV_NAME_MAX];
    char order[OV_ORDER_MAX];
};

static char *trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t'))
        e--;
    *e = '\0';
    return s;
}

static int ov_find(const struct ov_entry *tab, int n, const char *name)
{
    int i;

    for (i = 0; i < n; i++) {
        if (!strcasecmp(tab[i].name, name))
            return i;
    }
    return -1;
}

/*
 * Parse WINEDLLOVERRIDES into tab[0..*out_n).
 * First occurrence of a name wins; later duplicates are skipped.
 * Returns 0 on success, -1 if the table ran out of room (partial parse).
 */
static int ov_parse(const char *src, struct ov_entry *tab, int cap, int *out_n)
{
    char *copy = NULL;
    char *save_semi = NULL;
    char *frag;
    int n = 0;
    int overflow = 0;

    *out_n = 0;
    if (!src || !src[0])
        return 0;

    copy = strdup(src);
    if (!copy)
        return -1;

    for (frag = strtok_r(copy, ";", &save_semi); frag;
         frag = strtok_r(NULL, ";", &save_semi)) {
        char *eq;
        char *names;
        char *order;
        char *save_comma = NULL;
        char *tok;

        frag = trim(frag);
        if (!frag[0])
            continue;

        eq = strchr(frag, '=');
        if (!eq) {
            /* Malformed fragment — skip rather than invent an order. */
            linuwux_log("WINEDLLOVERRIDES: skip malformed fragment \"%s\"\n", frag);
            continue;
        }
        *eq = '\0';
        names = trim(frag);
        order = trim(eq + 1);
        if (!names[0])
            continue;

        for (tok = strtok_r(names, ",", &save_comma); tok;
             tok = strtok_r(NULL, ",", &save_comma)) {
            tok = trim(tok);
            if (!tok[0])
                continue;
            if (ov_find(tab, n, tok) >= 0)
                continue; /* first wins */
            if (n >= cap) {
                overflow = 1;
                break;
            }
            snprintf(tab[n].name, sizeof(tab[n].name), "%s", tok);
            snprintf(tab[n].order, sizeof(tab[n].order), "%s", order);
            n++;
        }
        if (overflow)
            break;
    }

    free(copy);
    *out_n = n;
    if (overflow) {
        linuwux_log("WINEDLLOVERRIDES: parse hit %d-entry cap — extra entries dropped\n",
                     cap);
        return -1;
    }
    return 0;
}

/* Add name=order only if name is not already present. */
static int ov_set_if_absent(struct ov_entry *tab, int n, int cap,
                            const char *name, const char *order)
{
    if (ov_find(tab, n, name) >= 0)
        return n;
    if (n >= cap) {
        linuwux_log("WINEDLLOVERRIDES: cannot add %s=%s — entry table full\n",
                     name, order);
        return n;
    }
    snprintf(tab[n].name, sizeof(tab[n].name), "%s", name);
    snprintf(tab[n].order, sizeof(tab[n].order), "%s", order);
    return n + 1;
}

static int ov_format(const struct ov_entry *tab, int n, char *out, size_t out_sz)
{
    size_t used = 0;
    int i;

    if (out_sz == 0)
        return -1;
    out[0] = '\0';

    for (i = 0; i < n; i++) {
        int w;

        w = snprintf(out + used, out_sz - used, "%s%s=%s",
                     used ? ";" : "", tab[i].name, tab[i].order);
        if (w < 0 || (size_t)w >= out_sz - used) {
            out[0] = '\0';
            return -1;
        }
        used += (size_t)w;
    }
    return 0;
}

int linuwux_overrides_apply(unsigned present)
{
    struct ov_entry tab[OV_MAX_ENTRIES];
    char out[OV_OUT_MAX];
    const char *existing;
    int n = 0;
    int before;
    unsigned added = 0;

    if (!present) {
        linuwux_log("WINEDLLOVERRIDES: no pack-native winmm/version/reflex beside exe — left unchanged\n");
        return 0;
    }

    existing = getenv("WINEDLLOVERRIDES");
    if (ov_parse(existing ? existing : "", tab, OV_MAX_ENTRIES, &n) < 0 && n == 0) {
        /* strdup failure or empty overflow with nothing useful */
        linuwux_log("WINEDLLOVERRIDES: parse failed — left unchanged\n");
        return -1;
    }

    before = n;

    /*
     * Existence-gated. Order preference matches prior behaviour: n,b
     * (native first, builtin fallback). Never clobber an existing key.
     */
    {
        static const struct {
            unsigned bit;
            const char *name;
        } native_dlls[] = {
            { LINUWUX_NATIVE_WINMM,    "winmm"    },
            { LINUWUX_NATIVE_VERSION,  "version"  },
            { LINUWUX_NATIVE_REFLEX,   "reflex"   },
            { LINUWUX_NATIVE_REFLEX64, "reflex64" },
        };
        size_t i;

        for (i = 0; i < sizeof(native_dlls) / sizeof(native_dlls[0]); i++) {
            int m;

            if (!(present & native_dlls[i].bit))
                continue;
            m = ov_set_if_absent(tab, n, OV_MAX_ENTRIES, native_dlls[i].name, "n,b");
            if (m > n) {
                added |= native_dlls[i].bit;
                n = m;
            }
        }
    }

    if (n == before) {
        linuwux_log("WINEDLLOVERRIDES: pack-native keys already set — left unchanged\n");
        return 0;
    }

    if (ov_format(tab, n, out, sizeof(out)) < 0) {
        linuwux_log("WINEDLLOVERRIDES: merged result exceeds %d bytes — left unchanged\n",
                     OV_OUT_MAX);
        return -1;
    }

    if (setenv("WINEDLLOVERRIDES", out, 1) != 0) {
        linuwux_log("WINEDLLOVERRIDES: setenv failed — left unchanged\n");
        return -1;
    }

    linuwux_log("WINEDLLOVERRIDES: merged (added%s%s%s%s) \"%s\"\n",
                 (added & LINUWUX_NATIVE_WINMM)    ? " winmm"    : "",
                 (added & LINUWUX_NATIVE_VERSION)  ? " version"  : "",
                 (added & LINUWUX_NATIVE_REFLEX)   ? " reflex"   : "",
                 (added & LINUWUX_NATIVE_REFLEX64) ? " reflex64" : "",
                 out);
    return 0;
}
