/* A module whose global defrag callback never finishes: it consumes the whole deadline on every
 * invocation and always reports outstanding work. Used together with defragtest to check that such
 * a module does not starve the global defrag callbacks of other modules, and that the server
 * discards a cursor left behind when a defrag cycle is aborted (see defrag.tcl).
 */

#include "valkeymodule.h"

/* Number of times our global defrag callback was invoked. Exposed via INFO so the test can confirm
 * the busy module actually ran. */
unsigned long long busy_calls = 0;

/* Invocations that began a fresh pass, i.e. found a cursor with no recorded position. Because this
 * module records a position on every call and never reports completion, it can only see position 0
 * again if the server discarded the previous cursor. That makes this a direct probe for the cursor's
 * lifetime being managed by the server. */
unsigned long long busy_fresh_starts = 0;

static int defragBusyGlobal(ValkeyModuleDefragCtx *ctx) {
    busy_calls++;

    ValkeyModuleDefragCursor *cursor = ValkeyModule_DefragCursor(ctx);
    if (ValkeyModule_DefragCursorGetPosition(cursor) == 0) {
        busy_fresh_starts++;
        /* Take a scan cursor too, so an abandoned pass leaves a nested server-owned resource that
         * must be destroyed along with the defrag cursor. */
        ValkeyModule_DefragCursorScanCursor(cursor);
    }
    ValkeyModule_DefragCursorSetPosition(cursor, busy_calls);

    /* Burn the rest of the deadline, then report that work still remains. This models a module that
     * never drains within a single defrag cycle. */
    while (!ValkeyModule_DefragShouldStop(ctx)) {
        /* spin until the deadline is reached */
    }
    return 1;
}

static void BusyInfo(ValkeyModuleInfoCtx *ctx, int for_crash_report) {
    VALKEYMODULE_NOT_USED(for_crash_report);
    ValkeyModule_InfoAddSection(ctx, "stats");
    ValkeyModule_InfoAddFieldULongLong(ctx, "busy_calls", busy_calls);
    ValkeyModule_InfoAddFieldULongLong(ctx, "busy_fresh_starts", busy_fresh_starts);
}

int ValkeyModule_OnLoad(ValkeyModuleCtx *ctx, ValkeyModuleString **argv, int argc) {
    VALKEYMODULE_NOT_USED(argv);
    VALKEYMODULE_NOT_USED(argc);

    if (ValkeyModule_Init(ctx, "defragglobalbusy", 1, VALKEYMODULE_APIVER_1) == VALKEYMODULE_ERR)
        return VALKEYMODULE_ERR;

    ValkeyModule_RegisterInfoFunc(ctx, BusyInfo);
    ValkeyModule_RegisterDefragFunc(ctx, defragBusyGlobal);

    return VALKEYMODULE_OK;
}
