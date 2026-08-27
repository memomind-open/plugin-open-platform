# Device LVGL rendering core

This directory contains the LVGL 8.3.11 rendering sources used by the glasses.
They were synchronized from glasses revision
`40c9884776dc950de43335a07e4fbaa9245e416a`.
The Studio build replaces only the RTOS clock and JBD display port with host
adapters. Color packing, layout, text measurement, themes, and software drawing
remain aligned with the device fork.

The upstream LVGL code is distributed under the MIT license in `LICENSE.txt`.
The two XBF font resources are the same redistributable fonts used by the
device runtime. Their SHA-256 values are:

- `lv_font_xgimi_17.bin`: `83d105756874af7dd63e9463086cc24ad9e9ba2715cc5ed5cfd6da618fedecda`
- `lv_font_xgimi_20.bin`: `74aa6f09bd401c94e322d448f11468322c4768b2bb802cd4393f9c0ec0b342a0`

Do not add firmware display drivers, RTOS services, or application code here.
Device updates should be synchronized as a reviewed source update so Studio
and firmware stay pinned to the same LVGL revision.
