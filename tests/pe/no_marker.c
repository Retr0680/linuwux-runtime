/*
 * Control case: no marker file beside the binary.
 *
 * The library must treat this process as a non-game helper:
 *   - no version banner
 *   - no WINEDLLOVERRIDES injection
 *   - action leaves are not intercepted (or at least no protocol arm)
 *
 * We still issue the special leaves so the runner can confirm the
 * library stayed quiet.
 */

#include "common.h"

int main(void)
{
    banner("no_marker");

    printf("special leaves (should not arm the library):\n");
    issue_action(LEAF_LEGACY_INIT, 0, "LEGACY_INIT");
    issue_action(LEAF_ARM, FAKE_HANDLER, "ARM");
    issue_action(LEAF_LEGACY_KUSER, 0, "LEGACY_KUSER");

    printf("ordinary leaves:\n");
    issue_static(0, "leaf 0 (vendor)");
    issue_static(1, "leaf 1");

    printf("done.\n");
    return 0;
}
