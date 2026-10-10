# Describing a development task

Start the AI coding tool at the repository root and have it read `AGENTS.md`. Tools with skill support can invoke `$memomind-example-app` directly. For tools without `$` invocation, ask them to read `.agents/skills/memomind-example-app/SKILL.md`.

Developers do not need to choose the ABI or fill in every protocol field first. Describe where the application runs, its main interactions, and acceptance criteria; the AI should propose a default approach based on the examples and implement it. Adapt the application names and behavior in the requests below.

## Phone configuration with a glasses focus timer

> Use $memomind-example-app to build focus-timer. Configure the duration on the phone and support start, pause, resume, and reset. Show the remaining time and state on the glasses. Save the selected duration and restore that setting on reopening; background countdown is not required. Use the existing web_bridge and create a new directory while preserving the original examples. Deliver complete source, preview commands, pairing instructions, and an MMPKG. Test pause/resume, completion, and reload.

Expected route: Web starter, state machine and storage, text rendering, subscription/timer cleanup, Browser Studio, then MMPKG. Calculate elapsed time from timestamps rather than treating timer callback counts as an accurate clock. Define paused and non-foreground behavior according to the request.

## Standalone glasses game

> Use $memomind-example-app to build a glasses-only catch game. Refer to breakout for input and lifecycle handling. Turn the head left/right to move the paddle, single-click to start/pause, and double-click to exit. Show the score and support restarting. Run independently on the glasses without a phone page. Create a new directory and identity, and preserve telephone-UI yielding. Deliver source, GMP, build commands, and Studio instructions, with unperformed physical-device checks identified.

Expected route: native game reference, input and nonblocking state machine, layout based on runtime display dimensions, drawing/call-UI policy, targeted build, then Desktop Studio. Do not blindly copy a reference game's fixed asset or display-size assumptions.

## Phone control of a native glasses application

> Use $memomind-example-app to build a paired remote-control demo. Phone left/right buttons move a square on the glasses, and the glasses reply with the position to display on the phone. Use fighter-controller's state-synchronization approach with a simpler custom protocol. Support input release, disconnection, and reconnection, and use new application identities. Implement both plugins, document message fields and pairing manifests, and deliver their packages and Desktop Studio integration steps.

Expected route: dedicated Web/Glass directories, a short message specification, send/receive permissions and pairing, codecs on both sides, stale-input reset, then Desktop Studio. Simulated Browser Studio replies do not establish that the GMP has passed integration tests.

## Simplified reader

> Use $memomind-example-app to build a TXT reader based on novel-reader. Support only user-selected UTF-8 TXT files, glasses pagination, bookmarks, and reading-position restoration; omit EPUB and illustrations. Implement it in new directories while preserving bounded reads and the file-permission model. Deliver both plugins' source, packages, and verification steps, and identify file-persistence behavior that still requires testing in the App on a physical device.

Expected route: Host files/storage, bounded text windows, glasses layout/page turns, progress and cleanup, then dedicated GMP pairing. A smaller feature scope must not introduce whole-book RAM storage, cached short-lived tickets, or raw file URLs.

## Continuing after the first delivery

Describe observable issues such as "the countdown text is too small," "the button still reports success after disconnection," or "reopening after double-click exit restores the wrong state." Have the AI reproduce, edit, and verify the existing application rather than regenerating its directory or only providing advice.

The handoff should make application state, rendering, protocol code, the manifest, and startup commands easy to find. If a dependency, API, or device is unavailable, the AI should complete the independent work and identify the specific blocker rather than treating a placeholder UI as a complete application.
