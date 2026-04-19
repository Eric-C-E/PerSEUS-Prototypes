# Source Face Frames

Place source drawings here before converting them to LVGL C assets.

Expected source format:

- PNG
- 320x240 pixels
- one full-screen face frame per image
- no filesystem loading at runtime; these are source files only

`loading_screen.png` is RGB565.

All other face frames are monochrome black-on-white:

- white/background pixels are value 0
- black/active line pixels are HIGH/nonzero

Current required names:

- `loading_screen.png`
- `neutral_idle.png`
- `neutral_blink.png`
- `low_neg_idle.png`
- `low_neg_blink.png`
- `low_pos_idle.png`
- `low_pos_blink.png`
- `high_neg_idle.png`
- `high_neg_blink.png`
- `high_pos_idle.png`
- `high_pos_blink.png`
- `neutral_to_low_neg.png`
- `neutral_to_low_pos.png`
- `neutral_to_high_neg.png`
- `neutral_to_high_pos.png`
