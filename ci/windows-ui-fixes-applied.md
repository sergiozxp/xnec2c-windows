# Windows UI fixes applied

Initial UI/recovery pass applied on 2026-09-04 in commit `eaf9b10e3211806d329f3ec5e1f1162599d39369`.

Second Windows validation pass applied in commit `7d827d51c2b382b3658b82f723acfb918c502bea`.

This pass now also:
- lets the fatal NEC error dialog return cleanly instead of entering a nested GTK main loop, so File → Open can be used again after a malformed NEC file;
- changes loaded-wire geometry from pure yellow to dark orange for contrast on the light canvas;
- uses the Windows shell to open Help → Recursos NEC in the default browser.
