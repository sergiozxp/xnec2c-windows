from pathlib import Path
import re

p = Path('src/rc_config.c')
s = p.read_text(encoding='utf-8')

patterns = {
    'Create_Default_Config': '''gboolean\nCreate_Default_Config\\( void \\)\n\\{.*?\n\\} /\\* Create_Default_Config\\(\\) \\*/''',
    'Read_Config': '''gboolean\nRead_Config\\( void \\)\n\\{.*?\n\\} /\\* Read_Config\\(\\) \\*/''',
    'Save_Config': '''gboolean\nSave_Config\\( void \\)\n\\{.*?\n\\} /\\* Save_Config\\(\\) \\*/''',
}

repls = {
    'Create_Default_Config': '''gboolean\nCreate_Default_Config( void )\n{\n  /* Windows stateless policy: always start from compiled defaults and never\n   * inspect a persisted xnec2c configuration file. */\n  rc_config_apply_defaults();\n  rc_config.first_run = 1;\n  return( TRUE );\n} /* Create_Default_Config() */''',
    'Read_Config': '''gboolean\nRead_Config( void )\n{\n  /* Windows stateless policy: do not create, read, stat, or otherwise touch\n   * ~/.xnec2c or xnec2c.conf.  Create_Default_Config() already established\n   * the compiled defaults before command-line parsing; preserving the live\n   * values here keeps command-line options intact while restoring the GUI\n   * strictly from this process's fresh in-memory state. */\n  rc_config.first_run = 1;\n  Restore_GUI_State();\n  return( TRUE );\n} /* Read_Config() */''',
    'Save_Config': '''gboolean\nSave_Config( void )\n{\n  /* Windows stateless policy: session state is intentionally ephemeral.\n   * Never write xnec2c.conf or any other file under ~/.xnec2c. */\n  return( TRUE );\n} /* Save_Config() */''',
}

for name, pat in patterns.items():
    ns, n = re.subn(pat, repls[name], s, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f'failed to replace {name}: matches={n}')
    s = ns

p.write_text(s, encoding='utf-8')
print('Applied stateless configuration policy to src/rc_config.c')
