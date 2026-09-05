# Windows UI fixes applied

Initial UI/recovery pass applied on 2026-09-04 in commit `eaf9b10e3211806d329f3ec5e1f1162599d39369`.

Second Windows validation pass applied in commit `7d827d51c2b382b3658b82f723acfb918c502bea`.

Third Windows validation pass applied in commit `35886ccd07149b55a5a9d5057d6da997791f6550`.

Fourth Windows recovery/path pass applied in commit `43ffb6f5b9b2605dca78cf82bdd4292b94af9237`.

Clear/clean-state pass applied in commit `7e418ab80910f69ab768ace8b8cc4bb815d91dc2`.

Window/session audit pass applied in commit `ee273b9781b7b32eeee0217925c4eaeede16e8b6`.

This pass now also:
- avoids reopening the NEC editor after a malformed deck, preventing repeated error-dialog loops and returning the app to a blank state ready for File → Open;
- uses native Windows ShellExecuteW for Help → Recursos NEC so the URL opens in the default browser;
- preserves the earlier dark-orange loaded-wire contrast fix on the light canvas;
- opens Windows paths through GLib `g_fopen`, improving support for UTF-8/Unicode names such as `alimentación`;
- never enters the parser after a failed file open, preventing the misleading second `Unexpected EOF` error and keeping File → Open recoverable;
- adds File → Clear, which closes secondary windows, clears the loaded model and session restore state, and returns the main view to the fresh-start screen;
- routes failed NEC loads through the same deterministic clean-state reset used by File → Clear;
- starts each interactive run with only the main window and no automatically restored Radiation Pattern, Frequency Plots, Editor, or Symbol Overrides windows;
- opens secondary windows centered on the main window rather than replaying stale desktop coordinates;
- removes automatic Editor reopening based on previously saved editor size;
- makes File → Open clear the previous session before loading the newly selected NEC file;
- makes File → New start from the same deterministic clean state before opening a fresh editor;
- clears transient chooser/edit/plot/draw/quit flags during File → Clear so previous operations cannot leak into later menu actions.
