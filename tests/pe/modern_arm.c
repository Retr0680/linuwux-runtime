/*
 * Modern protocol path.
 *
 * Marker expected beside this binary: reflex.dll (or reflex64.dll)
 *
 * Sequence:
 *   - static leaf 1          (identity; bit 31 set until armed)
 *   - ARM 0x336933           (RCX = TargetSysHandler) → modern + KUSER
 *   - static 0x40000000      (hypervisor vendor string)
 *   - static leaf 1 again    (bit 31 should clear after arm)
 */

#include "common.h"

int main(void)
{
    banner("modern_arm");

    printf("pre-arm static identity:\n");
    issue_static(1, "leaf 1 (pre)");
    issue_static(0x40000000u, "hypervisor info");

    printf("arm:\n");
    issue_action(LEAF_ARM, FAKE_HANDLER, "ARM (modern)");

    printf("post-arm static identity:\n");
    issue_static(1, "leaf 1 (post)");
    issue_static(0x40000000u, "hypervisor info");
    issue_static(0x80000002u, "brand[0]");

    printf("done.\n");
    return 0;
}
