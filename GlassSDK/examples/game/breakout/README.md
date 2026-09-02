# GM Breakout plugin

This is a plugin port of the firmware `breakout_game_app.c`. It uses only the
public GM Plugin ABI and the Host-owned LVGL runtime, so it is not linked into
the glasses firmware ROM.

Controls:

- turn the head left/right: move the paddle;
- press the accessory left/right buttons: move the paddle one step (holding a
  repeatable button continues moving it);
- single-click: pause/resume, or restart after win/game over;
- keep the head raised for three seconds: exit the plugin application;
- long-press: show a two-second exit countdown; release to cancel.
