from pathlib import Path
import re

# 1. Semantic structure colors. Existing classification is preserved:
# normal = ideal conductor; LD5 = lossy conductor; other LD = localized load.
p = Path('src/prerender/prerender_color.c')
s = p.read_text()
old = '''    case SEG_COLOR_LOADED:\n    case SEG_COLOR_LOADED_RESISTIVITY:\n      *r = 1.0f;\n      *g = 1.0f;\n      *b = 0.0f;\n      break;\n\n    case SEG_COLOR_NORMAL:\n      *r = 0.0f;\n      *g = 0.0f;\n      *b = 1.0f;\n      break;'''
new = '''    case SEG_COLOR_LOADED:\n      /* Localized LD load/trap: dark orange #C45100. */\n      *r = 196.0f / 255.0f;\n      *g =  81.0f / 255.0f;\n      *b =   0.0f / 255.0f;\n      break;\n\n    case SEG_COLOR_LOADED_RESISTIVITY:\n      /* Distributed conductor loss (LD type 5): light orange #F4A261. */\n      *r = 244.0f / 255.0f;\n      *g = 162.0f / 255.0f;\n      *b =  97.0f / 255.0f;\n      break;\n\n    case SEG_COLOR_NORMAL:\n      /* Ideal/perfect conductor: blue #1565C0. */\n      *r =  21.0f / 255.0f;\n      *g = 101.0f / 255.0f;\n      *b = 192.0f / 255.0f;\n      break;'''
if old not in s:
    raise SystemExit('Expected segment color block not found')
p.write_text(s.replace(old, new, 1))

# 2. Default Cairo/frequency-plot theme: white canvas and dark axes/text.
p = Path('resources/themes.ini')
s = p.read_text()
start = s.index('[legacy]')
end = s.index('\n[sunset]', start)
block = s[start:end]
replacements = {
    'background': '#FFFFFF',
    'grid': '#D0D0D0',
    'grid_primary': '#B8B8B8',
    'grid_secondary': '#C4C4C4',
    'grid_emphasis': '#808080',
    'grid_perimeter': '#606060',
    'grid_scale': '#606060',
    'axis': '#404040',
    'series_primary': '#C000C0',
    'series_secondary': '#007A7A',
    'label_primary': '#C000C0',
    'label_secondary': '#007A7A',
    'label_axis': '#303030',
    'text_primary': '#202020',
    'text_muted': '#606060',
    'view_axis': '#303030',
    'view_axis_label': '#202020',
    'marker_extreme': '#202020',
    'cursor': '#008000',
}
for key, val in replacements.items():
    block, n = re.subn(rf'(?m)^{re.escape(key)}\s*=\s*#[0-9A-Fa-f]{{6}}$', f'{key} = {val}', block, count=1)
    if n != 1:
        raise SystemExit(f'legacy theme key not found exactly once: {key}')
p.write_text(s[:start] + block + s[end:])

# 3. Add a top-level "Recursos NEC" menu at runtime.
p = Path('src/main.c')
s = p.read_text()
anchor = '''static gint opt_start_optimizer_thread(void)\n{'''
if anchor not in s:
    raise SystemExit('main.c insertion anchor not found')
helper = r'''
/* Open the CharlyGolf NEC resources site in the user's default browser. */
static void
on_nec_resources_activate(GtkMenuItem *item, gpointer user_data)
{
  GError *error = NULL;
  (void)item;
  (void)user_data;

  if( !gtk_show_uri_on_window(GTK_WINDOW(main_window),
        "https://antenas.charlygolf.com/", GDK_CURRENT_TIME, &error) )
  {
    pr_warn("cannot open NEC resources URL: %s\n",
        error ? error->message : "unknown error");
    g_clear_error(&error);
  }
}

/* Add one top-level menu to the main menubar, immediately before Help. */
static void
install_nec_resources_menu(GtkBuilder *builder)
{
  GSList *objects, *it;
  GtkWidget *menubar = NULL;
  GtkWidget *top, *submenu, *link;
  GList *children, *c;
  int position = -1;
  int index = 0;

  objects = gtk_builder_get_objects(builder);
  for( it = objects; it != NULL; it = it->next )
  {
    if( GTK_IS_MENU_BAR(it->data)
        && gtk_widget_get_toplevel(GTK_WIDGET(it->data)) == main_window )
    {
      menubar = GTK_WIDGET(it->data);
      break;
    }
  }
  g_slist_free(objects);

  if( menubar == NULL )
  {
    pr_warn("cannot locate main menubar for NEC resources menu\n");
    return;
  }

  top = gtk_menu_item_new_with_label("Recursos NEC");
  submenu = gtk_menu_new();
  link = gtk_menu_item_new_with_label("Antenas CharlyGolf");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(top), submenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), link);
  g_signal_connect(link, "activate", G_CALLBACK(on_nec_resources_activate), NULL);

  children = gtk_container_get_children(GTK_CONTAINER(menubar));
  for( c = children; c != NULL; c = c->next, index++ )
  {
    if( GTK_IS_MENU_ITEM(c->data) )
    {
      const char *label = gtk_menu_item_get_label(GTK_MENU_ITEM(c->data));
      if( label != NULL && g_strrstr(label, "Help") != NULL )
      {
        position = index;
        break;
      }
    }
  }
  g_list_free(children);

  if( position >= 0 )
    gtk_menu_shell_insert(GTK_MENU_SHELL(menubar), top, position);
  else
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), top);

  gtk_widget_show_all(top);
}

'''
s = s.replace(anchor, helper + anchor, 1)
call_anchor = '''  main_window = create_main_window( &main_window_builder );\n  gtk_window_set_title( GTK_WINDOW(main_window), PACKAGE_STRING );'''
call_new = '''  main_window = create_main_window( &main_window_builder );\n  gtk_window_set_title( GTK_WINDOW(main_window), PACKAGE_STRING );\n  install_nec_resources_menu( main_window_builder );'''
if call_anchor not in s:
    raise SystemExit('main window call anchor not found')
p.write_text(s.replace(call_anchor, call_new, 1))
