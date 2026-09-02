/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-02-21
Date Last Modified: 2026-02-22
Module: Extras & Info
File: source_display.c
About: Placeholder for source display.
Revisions:
- 2026-02-22  Simplified to placeholder
- 2026-08-26  BUG FIX: was drawing at the hardcoded (10, 5), which
              sits inside the logo's exclusive zone (rows 6-12,
              cols 4-19 per ui_coordinates.h) — the message ran
              straight through the ASCII-art logo. Moved to
              UI_PLACEHOLDER_ROW/COL, the same spot the app_*
              placeholders in prompt.c already use for this exact
              "not implemented" case.
------------------------------------------------------------
*/

#include "pal.h"
#include "ui_setup.h"
#include "ui_elements.h"
#include "extras.h"

void extras_show_source(void)
{
    ui_init();
    ui_title(UI_PLACEHOLDER_ROW, UI_PLACEHOLDER_COL, bold yellow, "Source display not implemented in this build.");
    pal_pause();
}