/*
 * Legacy dual-handler protocol path (full query sequence).
 *
 * Marker expected beside this binary: DenuvOwO.ini
 *
 * Sequence:
 *   - LEGACY_INIT 0x69696969
 *   - QUERY_SYSTEM_ID 0x336943       → promote to LEGACY_DUAL, store system_id
 *   - QUERY_FULL_HANDLER 0x336934    → store full_handler
 *   - QUERY_FULL_ID 0x336944         → store full_id
 *   - ARM 0x336933                   → store single handler
 *   - LEGACY_KUSER 0x1337            → apply legacy-dual KUSER profile
 */

#include "common.h"

int main(void)
{
    banner("legacy_dual");

    printf("init:\n");
    issue_action(LEAF_LEGACY_INIT, 0, "LEGACY_INIT");

    printf("queries (promote to dual):\n");
    issue_action(LEAF_LEGACY_QUERY_SYSTEM_ID, FAKE_SYSTEM_ID, "QUERY_SYSTEM_ID");
    issue_action(LEAF_LEGACY_QUERY_FULL_HANDLER, FAKE_FULL_HANDLER, "QUERY_FULL_HANDLER");
    issue_action(LEAF_LEGACY_QUERY_FULL_ID, FAKE_FULL_ID, "QUERY_FULL_ID");

    printf("arm:\n");
    issue_action(LEAF_ARM, FAKE_HANDLER, "ARM (legacy dual)");

    printf("kuser:\n");
    issue_action(LEAF_LEGACY_KUSER, 0, "LEGACY_KUSER");

    printf("post static identity:\n");
    issue_static(1, "leaf 1");
    issue_static(0x80000002u, "brand[0]");

    printf("done.\n");
    return 0;
}
