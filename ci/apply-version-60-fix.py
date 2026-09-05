from pathlib import Path

# Version 60: only invalid NEC recovery / stale Frequency Plots cleanup.

main = Path('src/main.c')
s = main.read_text()

old = '''  /* Open NEC2 input file */
  if( strlen(rc_config.input_file) == 0 )
    return( FALSE );
'''
new = '''  /* Open NEC2 input file */
  if( strlen(rc_config.input_file) == 0 )
  {
    ClearFlag( INPUT_PENDING );
    return( FALSE );
  }
'''
assert old in s
s = s.replace(old, new, 1)

old = '''  Open_File( &input_fp, rc_config.input_file, "r");

  /* Read input file, record failures */
  ok = Read_Comments() && Read_Geometry() && Read_Commands();
'''
new = '''  /* Opening the file is part of the load transaction.  Do not enter the
   * parsers with a NULL/invalid FILE handle when the open itself fails. */
  ok = Open_File( &input_fp, rc_config.input_file, "r" );

  /* Read input file, record failures */
  if( ok )
    ok = Read_Comments() && Read_Geometry() && Read_Commands();
'''
assert old in s
s = s.replace(old, new, 1)

old = '''  if( !ok )
  {
    /* Close plot/rdpat windows if open */
    Gtk_Widget_Destroy( &rdpattern_window );
    Gtk_Widget_Destroy( &freqplots_window );

    /* Batch mode has no operator to dismiss the editor; Stop() already
'''
new = '''  if( !ok )
  {
    /* A failed load is a completed transaction too: release the file and
     * INPUT_PENDING before any interactive callbacks run.  Leaving the flag
     * set caused every later Open_Input_File() call to be rejected as reentry. */
    Close_File( &input_fp );
    ClearFlag( INPUT_PENDING );

    /* Keep the existing presentation windows open, but invalidate every view
     * so none of them can continue to present results from the previous deck. */
    Queue_Structure_Rebuild( TRUE );
    if( rdpattern_window != NULL )
      Queue_Radiation_Redraw( TRUE );
    if( freqplots_window != NULL )
      freqplots_redraw_all( TRUE );

    /* Batch mode has no operator to dismiss the editor; Stop() already
'''
assert old in s
s = s.replace(old, new, 1)
main.write_text(s)

fp = Path('src/freqplots/freqplots_core.c')
s = fp.read_text()

clear_block = '''  /* Clear drawingarea to the active theme's background surface; foreground
   * roles are contrast-solved against this same surface. */
  const theme_t *th = theme_active();
  cairo_set_source_rgb( cr, (double)th->colors[THEME_ROLE_BACKGROUND].r,
      (double)th->colors[THEME_ROLE_BACKGROUND].g,
      (double)th->colors[THEME_ROLE_BACKGROUND].b );
  cairo_rectangle(
      cr, 0.0, 0.0,
      (double)v->width,
      (double)v->height );
  cairo_fill( cr );

'''
assert s.count(clear_block) >= 1
s = s.replace(clear_block, '', 1)

needle = '''_Plot_Frequency_Data( freqplots_view_t *v, cairo_t *cr )
{
  /* Abort plotting if main window is to be closed
'''
replacement = '''_Plot_Frequency_Data( freqplots_view_t *v, cairo_t *cr )
{
  /* Clear first, before any validity guard.  A failed NEC load deliberately
   * removes ENABLE_EXCITN/results; returning before this clear left the pixels
   * of the previous antenna visible in Frequency Plots. */
''' + clear_block + '''  /* Abort plotting if main window is to be closed
'''
assert needle in s
s = s.replace(needle, replacement, 1)
fp.write_text(s)

print('Version 60 source patch applied successfully.')
