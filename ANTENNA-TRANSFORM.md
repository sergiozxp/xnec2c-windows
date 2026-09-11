# Whole antenna transformations

The main window contains a new **Transform** menu with three operations:

- **Move antenna** inserts a `GM` card with X, Y, and Z translation. The
  dialog displays the current and resulting minimum height and warns before
  placing geometry below the ground plane.
- **Rotate antenna** inserts a `GM` card for rotation around the global X, Y,
  or Z axis. Positive counter-clockwise rotation follows the NEC convention;
  clockwise rotation is stored as a negative angle.
- **Scale antenna** inserts a `GS` card. The scale factor follows
  `reference frequency / new frequency`. When **Preserve minimum height** is
  selected, a following `GM` card restores the original lowest Z coordinate.

The generated cards are inserted immediately before `GE` and are shown in the
standard NEC2 input editor. Saving or applying the editor commits the change
to the input file and rebuilds the model through the existing xnec2c flow.

NEC `GS` scales coordinates and wire radii together. For that reason the
wire-radius option is displayed as selected and read-only; separating those
operations would require destructive rewriting of individual geometry cards.

Existing low-level `GM` and `GS` card editors remain available and unchanged.
