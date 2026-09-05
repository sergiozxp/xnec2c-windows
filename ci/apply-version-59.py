from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    s = p.read_text(encoding="utf-8")
    count = s.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}")
    p.write_text(s.replace(old, new, 1), encoding="utf-8")

# 1) Batch Math Library: suppress notices caused by automatic/config-driven
# selection while preserving the notice for an actual menu widget action.
replace_once(
    "src/mathlib.c",
    "\tif (!FORKED)\n\t\tNotice(GTK_BUTTONS_OK, _(\"Batch Math Library\"),",
    "\tif (widget != NULL && !FORKED)\n\t\tNotice(GTK_BUTTONS_OK, _(\"Batch Math Library\"),",
)

# 2) Frequency Plots startup restoration: use the same state model as
# Radiation Pattern.  Restore only when it was open and has valid geometry;
# main_loop_start must not itself open the window.
replace_once(
    "src/rc_config.c",
    "  if( rc_config.main_loop_start || isFlagSet(SUPPRESS_INTERMEDIATE_REDRAWS) ||\n\t  (rc_config.freqplots_is_open && rc_config.freqplots_width && rc_config.freqplots_height))",
    "  if( rc_config.freqplots_is_open && rc_config.freqplots_width && rc_config.freqplots_height)",
)

# 3) NEC editor: retain saved size but use deterministic centered placement
# instead of replaying saved coordinates that drift under Windows/GTK.
replace_once(
    "src/callback_func.c",
    "  Set_Window_Geometry( nec2_edit_window,\n      rc_config.nec2_edit_x, rc_config.nec2_edit_y,\n      rc_config.nec2_edit_width, rc_config.nec2_edit_height );\n  gtk_widget_show( nec2_edit_window );",
    "  Set_Window_Geometry( nec2_edit_window,\n      -1, -1,\n      rc_config.nec2_edit_width, rc_config.nec2_edit_height );\n  gtk_window_set_position( GTK_WINDOW(nec2_edit_window), GTK_WIN_POS_CENTER );\n  gtk_widget_show( nec2_edit_window );",
)

# 4) Frequency Plots initial placement: validate its own Y coordinate, not
# Radiation Pattern's Y coordinate.  Its derived first position remains to the
# right of Main: x=main_x+main_width, y=main_y.
replace_once(
    "src/callbacks.c",
    "      if (rc_config.freqplots_x < 0 || rc_config.rdpattern_y < 0)",
    "      if (rc_config.freqplots_x < 0 || rc_config.freqplots_y < 0)",
)
