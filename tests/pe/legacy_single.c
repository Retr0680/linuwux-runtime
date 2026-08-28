/*
 * Legacy single-handler protocol path.
 *
 * Marker expected beside this binary: DenuvOwO.ini
 * (winmm-loader style; also works with DenuvOwO.dll)
 *
 * Sequence:
 *   - LEGACY_INIT 0x69696969     → LEGACY_SINGLE, rax_is_resume=0
 *   - ARM 0x336933               → store handler (KUSER deferred)
 *   - LEGACY_KUSER 0x1337        → apply legacy-single KUSER profile
 *   - static leaf 1              → legacy identity (Ryzen brand path)
 */

#include "common.h"

int main(void)
{
    banner("legacy_single");

    printf("init:\n");
    issue_action(LEAF_LEGACY_INIT, 0, "LEGACY_INIT");

    printf("arm:\n");
    issue_action(LEAF_ARM, FAKE_HANDLER, "ARM (legacy single)");

    printf("kuser:\n");
    issue_action(LEAF_LEGACY_KUSER, 0, "LEGACY_KUSER");

    printf("post static identity:\n");
    issue_static(1, "leaf 1");
    issue_static(0x80000002u, "brand[0]");

    printf("done.\n");
    return 0;
}
