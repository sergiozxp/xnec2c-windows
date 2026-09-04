# Windows UI fixes applied

Initial UI/recovery pass applied on 2026-09-04 in commit `eaf9b10e3211806d329f3ec5e1f1162599d39369`.

Second Windows validation pass applied in commit `7d827d51c2b382b3658b82f723acfb918c502bea`.

Third Windows validation pass applied in commit `35886ccd07149b55a5a9d5057d6da997791f6550`.

This pass now also:
- avoids reopening the NEC editor after a malformed deck, preventing repeated error-dialog loops and returning the app to a blank state ready for File → Open;
- uses native Windows ShellExecuteW for Help → Recursos NEC so the URL opens in the default browser;
- preserves the earlier dark-orange loaded-wire contrast fix on the light canvas.
