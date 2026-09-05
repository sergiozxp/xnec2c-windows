from pathlib import Path
import re


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"expected text not found in {path}: {old[:80]!r}")
    text = text.replace(old, new, 1)
    p.write_text(text)


# Do not show the Batch Math Library notice while startup code is merely
# restoring/constructing the radio menu. Keep the notice for a real click.
replace_once(
    "src/mathlib.c",
    "\tif (!FORKED)\n\t\tNotice(GTK_BUTTONS_OK, _(\"Batch Math Library\"),",
    "\tif (widget != NULL && !FORKED)\n\t\tNotice(GTK_BUTTONS_OK, _(\"Batch Math Library\"),",
)

# Start every Windows session with Main only. Persist settings and geometry,
# but require explicit user action to open analytical/auxiliary windows.
p = Path("src/rc_config.c")
text = p.read_text()
pattern = re.compile(
    r"  static gboolean\nRestore_Windows\( gpointer dat \)\n\{.*?  return\( FALSE \);\n\}",
    re.S,
)
replacement = """  static gboolean
Restore_Windows( gpointer dat )
{
  (void)dat;

  /* Windows baseline policy: every interactive session starts with only
   * the Main window. Geometry and widget preferences remain persistent,
   * but secondary windows are opened only by an explicit user action. */
  return( FALSE );
}"""
text, count = pattern.subn(replacement, text, count=1)
if count != 1:
    raise SystemExit("Restore_Windows replacement failed")
p.write_text(text)

# A maximized secondary window must not overwrite its last normal geometry.
replace_once(
    "src/rc_config.c",
    """  /* Get geometry of radiation patterns window */
  rc_config.rdpattern_is_open = Get_Window_Geometry( rdpattern_window,
      &(rc_config.rdpattern_x), &(rc_config.rdpattern_y),
      &(rc_config.rdpattern_width), &(rc_config.rdpattern_height) );""",
    """  /* Preserve the last normal geometry. A maximized window is a temporary
   * display state and must not overwrite the geometry used next time. */
  if( rdpattern_window != NULL &&
      gtk_window_is_maximized(GTK_WINDOW(rdpattern_window)) )
    rc_config.rdpattern_is_open = 1;
  else
    rc_config.rdpattern_is_open = Get_Window_Geometry( rdpattern_window,
        &(rc_config.rdpattern_x), &(rc_config.rdpattern_y),
        &(rc_config.rdpattern_width), &(rc_config.rdpattern_height) );""",
)
replace_once(
    "src/rc_config.c",
    """  /* Get geometry of frequency plots window */
  rc_config.freqplots_is_open = Get_Window_Geometry( freqplots_window,
      &(rc_config.freqplots_x), &(rc_config.freqplots_y),
      &(rc_config.freqplots_width), &(rc_config.freqplots_height) );""",
    """  /* Preserve the last normal geometry instead of recording the maximized
   * frame as the next requested window size. */
  if( freqplots_window != NULL &&
      gtk_window_is_maximized(GTK_WINDOW(freqplots_window)) )
    rc_config.freqplots_is_open = 1;
  else
    rc_config.freqplots_is_open = Get_Window_Geometry( freqplots_window,
        &(rc_config.freqplots_x), &(rc_config.freqplots_y),
        &(rc_config.freqplots_width), &(rc_config.freqplots_height) );""",
)

# Closing from View and closing with the title-bar X should preserve the same
# last normal geometry before destroying the window.
replace_once(
    "src/callbacks.c",
    """  else if( isFlagSet(DRAW_ENABLED) )
    Gtk_Widget_Destroy( &rdpattern_window );""",
    """  else if( isFlagSet(DRAW_ENABLED) )
  {
    get_rdpattern_window_state();
    Gtk_Widget_Destroy( &rdpattern_window );
  }""",
)
replace_once(
    "src/callbacks.c",
    """  else if( isFlagSet(PLOT_ENABLED) )
    Gtk_Widget_Destroy( &freqplots_window );""",
    """  else if( isFlagSet(PLOT_ENABLED) )
  {
    get_freqplots_window_state();
    Gtk_Widget_Destroy( &freqplots_window );
  }""",
)

# The NEC2 Editor showed a GTK/Windows cascade-like drift. Keep its persisted
# size but center it deterministically whenever it is newly created.
replace_once(
    "src/callback_func.c",
    """  Set_Window_Geometry( nec2_edit_window,
      rc_config.nec2_edit_x, rc_config.nec2_edit_y,
      rc_config.nec2_edit_width, rc_config.nec2_edit_height );
  gtk_widget_show( nec2_edit_window );""",
    """  Set_Window_Geometry( nec2_edit_window,
      -1, -1,
      rc_config.nec2_edit_width, rc_config.nec2_edit_height );
  gtk_window_set_position( GTK_WINDOW(nec2_edit_window), GTK_WIN_POS_CENTER );
  gtk_widget_show( nec2_edit_window );""",
)

print("baseline UI fixes applied")
