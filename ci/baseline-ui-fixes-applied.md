# Baseline UI test build

Source baseline: `baseline/windows-upstream-4.4.18-installer`

Applied behavior changes:

- Start with Main only; do not restore secondary windows automatically.
- Suppress programmatic `Batch Math Library` notices during startup.
- Preserve the last normal geometry when Radiation Pattern or Frequency Plots is maximized.
- Make View-menu close and title-bar close preserve geometry consistently.
- Center the NEC2 Editor deterministically when newly opened while retaining its size.

No rendering colors, NEC solver behavior, menus, or model-loading logic were changed in this pass.
