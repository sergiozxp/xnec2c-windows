from pathlib import Path

p = Path('src/callbacks.c')
s = p.read_text(encoding='utf-8')


def replace_c_function(text: str, name: str, replacement: str) -> str:
    marker = name + '('
    start_name = text.find(marker)
    if start_name < 0:
        raise SystemExit(f'function not found: {name}')

    # Include the return type / indentation line immediately before the name.
    start = text.rfind('\n', 0, start_name)
    start = text.rfind('\n', 0, start) + 1

    brace = text.find('{', start_name)
    if brace < 0:
        raise SystemExit(f'opening brace not found: {name}')

    i = brace
    depth = 0
    state = 'code'
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ''

        if state == 'code':
            if c == '/' and n == '/':
                state = 'line_comment'; i += 2; continue
            if c == '/' and n == '*':
                state = 'block_comment'; i += 2; continue
            if c == '"':
                state = 'string'; i += 1; continue
            if c == "'":
                state = 'char'; i += 1; continue
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    end = i + 1
                    return text[:start] + replacement.rstrip() + text[end:]
            i += 1
            continue

        if state == 'line_comment':
            if c == '\n': state = 'code'
            i += 1; continue
        if state == 'block_comment':
            if c == '*' and n == '/': state = 'code'; i += 2
            else: i += 1
            continue
        if state in ('string', 'char'):
            quote = '"' if state == 'string' else "'"
            if c == '\\': i += 2; continue
            if c == quote: state = 'code'
            i += 1; continue

    raise SystemExit(f'closing brace not found: {name}')


freq = r'''  gboolean
on_freqplots_window_delete_event(
    GtkWidget       *widget,
    GdkEvent        *event,
    gpointer         user_data)
{
  /* The title-bar X is only an alternate entry point to the existing
   * View -> Frequency Plots action.  Do not maintain a second close path. */
  GtkWidget *menuitem = Builder_Get_Object(main_window_builder, "main_freqplots");

  if( menuitem != NULL &&
      gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) )
    gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));

  /* The View action owns destruction of the window. */
  return( TRUE );
}'''

rdpat = r'''  gboolean
on_rdpattern_window_delete_event(
    GtkWidget       *widget,
    GdkEvent        *event,
    gpointer         user_data)
{
  /* The title-bar X is only an alternate entry point to the existing
   * View -> Radiation Pattern action.  Do not maintain a second close path. */
  GtkWidget *menuitem = Builder_Get_Object(main_window_builder, "main_rdpattern");

  if( menuitem != NULL &&
      gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) )
    gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));

  /* The View action owns destruction of the window. */
  return( TRUE );
}'''

s = replace_c_function(s, 'on_freqplots_window_delete_event', freq)
s = replace_c_function(s, 'on_rdpattern_window_delete_event', rdpat)

p.write_text(s, encoding='utf-8')
print('Unified title-bar X close paths with View menu actions')
