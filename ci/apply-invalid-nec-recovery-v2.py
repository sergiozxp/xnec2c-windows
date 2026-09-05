from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'pattern not found in {path}: {old[:120]!r}')
    s = s.replace(old, new, 1)
    p.write_text(s)

# main.c: make a new load a transaction that invalidates the previous model
# before parsing and always releases INPUT_PENDING on a failed load.
replace_once(
    'src/main.c',
    '  gboolean ok, new;\n  GtkWidget *widget;\n',
    '  gboolean ok, new;\n  int previous_steps_total;\n  GtkWidget *widget;\n'
)

replace_once(
    'src/main.c',
'''  /* Invalidate freq loop preconditions before Stop_Frequency_Loop so that
   * the GTK event flush inside Stop_Frequency_Loop cannot re-entrantly
   * start a new freq loop via Start_Frequency_Loop_Greenline.  The
   * existing steps_total < 1 guard in freq_loop_start_internal rejects
   * any call that arrives during the flush. */
  g_rec_mutex_lock(&freq_data_lock);
  calc_data.FR_cards    = 0;
  calc_data.steps_total = 0;
  g_rec_mutex_unlock(&freq_data_lock);

  /* Always call Stop_Frequency_Loop: the thread retires the sweep state
   * on normal exit, so checking that state alone would skip cleanup,
   * leaking pth_freq_loop and floop_state.  Stop_Frequency_Loop is
   * idempotent — it checks pth_freq_loop internally and no-ops safely. */
  Stop_Frequency_Loop();

  /* Close open files if any */
  Close_File( &input_fp );


  /* Open NEC2 input file */
  if( strlen(rc_config.input_file) == 0 )
    return( FALSE );
''',
'''  /* Snapshot the old sweep size, then invalidate the loop preconditions
   * before Stop_Frequency_Loop so no nested GTK iteration can restart it. */
  g_rec_mutex_lock(&freq_data_lock);
  previous_steps_total = calc_data.steps_total;
  calc_data.FR_cards    = 0;
  calc_data.steps_total = 0;
  calc_data.freq_step   = -1;
  g_rec_mutex_unlock(&freq_data_lock);

  /* Retire any running sweep before clearing its retained-result state. */
  Stop_Frequency_Loop();

  /* A new load invalidates the old model before any parser runs.  This is
   * essential for geometry/read errors, because Read_Commands() is normally
   * the function that clears these validity flags and it may never be reached. */
  g_rec_mutex_lock(&freq_data_lock);
  ClearFlag( ENABLE_RDPAT | ENABLE_NEAREH | ENABLE_EXCITN );
  freq_sweep_results_clear();
  if( save.fstep != NULL )
    for( int i = 0; i <= previous_steps_total; i++ )
      save.fstep[i] = 0;
  mem_array_free(&freqplots_main_view()->fr_plots);
  g_rec_mutex_unlock(&freq_data_lock);

  /* Present the invalidated state immediately.  Frequency Plots clears its
   * canvas even with ENABLE_EXCITN unset, so no previous-deck pixels survive. */
  Queue_Structure_Rebuild( TRUE );
  if( rdpattern_window != NULL )
    Queue_Radiation_Redraw( TRUE );
  if( freqplots_window != NULL )
  {
    freqplots_clear_data_display();
    freqplots_redraw_all( TRUE );
  }

  /* Close open files if any */
  Close_File( &input_fp );

  /* Open NEC2 input file */
  if( strlen(rc_config.input_file) == 0 )
  {
    ClearFlag( INPUT_PENDING );
    return( FALSE );
  }
'''
)

replace_once(
    'src/main.c',
'''  calc_data.freq_step = -1;

  mem_array_free(&freqplots_main_view()->fr_plots);

  Open_File( &input_fp, rc_config.input_file, "r");

  /* Read input file, record failures */
  ok = Read_Comments() && Read_Geometry() && Read_Commands();
''',
'''  calc_data.freq_step = -1;

  mem_array_free(&freqplots_main_view()->fr_plots);

  /* Opening the file is part of the load transaction.  Never enter a parser
   * with an invalid FILE handle. */
  ok = Open_File( &input_fp, rc_config.input_file, "r" );

  /* Read input file only when the open itself succeeded. */
  if( ok )
    ok = Read_Comments() && Read_Geometry() && Read_Commands();
'''
)

replace_once(
    'src/main.c',
'''  if( !ok )
  {
    /* Close plot/rdpat windows if open */
    Gtk_Widget_Destroy( &rdpattern_window );
    Gtk_Widget_Destroy( &freqplots_window );

    /* Batch mode has no operator to dismiss the editor; Stop() already
''',
'''  if( !ok )
  {
    /* Parsing may have partially repopulated model state before the error.
     * Collapse it to an unambiguously empty model and release the load guard
     * before any editor or file-chooser callback can run. */
    g_rec_mutex_lock(&freq_data_lock);
    ClearFlag( ENABLE_RDPAT | ENABLE_NEAREH | ENABLE_EXCITN );
    freq_sweep_results_clear();
    if( save.fstep != NULL )
      for( int i = 0; i <= calc_data.steps_total; i++ )
        save.fstep[i] = 0;
    calc_data.FR_cards    = 0;
    calc_data.steps_total = 0;
    calc_data.freq_step   = -1;
    data.n = 0;
    data.m = 0;
    mem_array_free(&freqplots_main_view()->fr_plots);
    g_rec_mutex_unlock(&freq_data_lock);

    Close_File( &input_fp );
    ClearFlag( INPUT_PENDING );

    /* Keep all presentation windows open, but make every one show the same
     * empty/no-valid-model state. */
    Queue_Structure_Rebuild( TRUE );
    if( rdpattern_window != NULL )
      Queue_Radiation_Redraw( TRUE );
    if( freqplots_window != NULL )
    {
      freqplots_clear_data_display();
      freqplots_redraw_all( TRUE );
    }

    /* Batch mode has no operator to dismiss the editor; Stop() already
'''
)

# Frequency Plots: clear the canvas before validity guards and expose a small
# helper that blanks the scalar readouts when a model is invalidated.
replace_once(
    'src/freqplots/freqplots_core.c',
'''  if( isFlagClear(PLOT_ENABLED) ) return;

  fstep = fp_selected_fstep();

  if( fstep < 0 )
    return;
''',
'''  if( isFlagClear(PLOT_ENABLED) ) return;

  fstep = fp_selected_fstep();

  if( fstep < 0 )
  {
    freqplots_clear_data_display();
    return;
  }
'''
)

replace_once(
    'src/freqplots/freqplots_core.c',
'''/*-----------------------------------------------------------------------*/

/* Return a newly allocated terse identity "P<n>: T<tag>/S<seg>" for port @p;
''',
'''/*-----------------------------------------------------------------------*/

/* Clear scalar readouts that belong to the previously valid model. */
void
freqplots_clear_data_display( void )
{
  static const char *entry_ids[] = {
    "freqplots_maxgain_entry",
    "freqplots_fmhz_entry",
    "freqplots_vswr_entry",
    "freqplots_zreal_entry",
    "freqplots_zimag_entry",
    "freqplots_ant_temp_tot_entry",
    "freqplots_ant_temp_entry",
    "freqplots_gt_entry"
  };

  if( freqplots_window_builder == NULL )
    return;

  for( unsigned i = 0; i < G_N_ELEMENTS(entry_ids); i++ )
  {
    GtkWidget *entry = Builder_Get_Object(freqplots_window_builder, entry_ids[i]);
    if( entry != NULL )
      gtk_entry_set_text(GTK_ENTRY(entry), "—");
  }
}

/*-----------------------------------------------------------------------*/

/* Return a newly allocated terse identity "P<n>: T<tag>/S<seg>" for port @p;
'''
)

replace_once(
    'src/freqplots/freqplots_core.c',
'''_Plot_Frequency_Data( freqplots_view_t *v, cairo_t *cr )
{
  /* Abort plotting if main window is to be closed
   * or when plots drawing area not available */
  if( isFlagClear(PLOT_ENABLED) ||
      isFlagClear(ENABLE_EXCITN) )
    return;
''',
'''_Plot_Frequency_Data( freqplots_view_t *v, cairo_t *cr )
{
  /* Always erase the previous frame first.  A failed NEC load deliberately
   * clears ENABLE_EXCITN/results, so the validity guard below must not leave
   * the old antenna's pixels on screen. */
  const theme_t *th = theme_active();
  cairo_set_source_rgb( cr, (double)th->colors[THEME_ROLE_BACKGROUND].r,
      (double)th->colors[THEME_ROLE_BACKGROUND].g,
      (double)th->colors[THEME_ROLE_BACKGROUND].b );
  cairo_rectangle(cr, 0.0, 0.0, (double)v->width, (double)v->height);
  cairo_fill(cr);

  /* Abort plotting if main window is to be closed or no valid excitation
   * exists for the current model. */
  if( isFlagClear(PLOT_ENABLED) ||
      isFlagClear(ENABLE_EXCITN) )
    return;
'''
)

replace_once(
    'src/freqplots/freqplots_core.c',
'''  /* Clear drawingarea to the active theme's background surface; foreground
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

''',
''
)

replace_once(
    'src/common.h',
'''void freqplots_redraw_all(gboolean force);
int freqplots_count_selected(void);
''',
'''void freqplots_redraw_all(gboolean force);
void freqplots_clear_data_display(void);
int freqplots_count_selected(void);
'''
)

print('invalid NEC recovery v2 applied')
