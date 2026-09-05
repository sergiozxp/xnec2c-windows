from pathlib import Path

p = Path('src/freqplots/freqplots_core.c')
s = p.read_text()
old = '''  if( isFlagClear(PLOT_ENABLED) ) return;\n\n  fstep = fp_selected_fstep();\n'''
new = '''  if( isFlagClear(PLOT_ENABLED) ) return;\n  if( isFlagSet(INPUT_PENDING) )\n  {\n    freqplots_clear_data_display();\n    return;\n  }\n\n  fstep = fp_selected_fstep();\n'''
if old not in s:
    raise SystemExit('Display_Frequency_Data guard insertion point not found')
s = s.replace(old, new, 1)
old = '''  if( isFlagClear(PLOT_ENABLED) ||\n      isFlagClear(ENABLE_EXCITN) )\n    return;\n'''
new = '''  if( isFlagClear(PLOT_ENABLED) ||\n      isFlagSet(INPUT_PENDING) ||\n      isFlagClear(ENABLE_EXCITN) )\n    return;\n'''
if old not in s:
    raise SystemExit('_Plot_Frequency_Data guard insertion point not found')
s = s.replace(old, new, 1)
p.write_text(s)
print('input-pending render guard applied')
