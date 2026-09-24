---
name: session-leak-fix
kind: doc
description: the switch that frees the practice and menu sessions the game keeps in memory for the rest of the process, the reference cycle it breaks, the one call it patches in the exe, its log lines and what it has been run on
updated: 2026-09-24
links: [BUG-016-vram-overhead-grows-across-scene-loads, session-leak-census-2026-09-16, telemetry, memory-creep-2026-09-14, DEC-023-the-session-free-stays-immediate]
---

# Session leak fix

`[engine] session_leak_fix`, on by default, `src/engine/session_leak_fix.cpp`, for BUG-016. The source's
header holds the mechanism and every address.

## What the game does

Every session, a practice at a track or the menu, runs on a `LocalServerConnection` made with
`std::make_shared`. The connection owns its game mode through its `LocalGameServer`, and the game mode's base,
`RemoteGameMode`, keeps the connection in a list of strong pointers. When `GameServerConnectionManager` lets go
of a finished session, that cycle keeps it, with its scene, weather services and physics body, until the
process ends. At the Red Bull Ring that is about 57 MB a visit. How it was found is in
[session-leak-census-2026-09-16](../research/session-leak-census-2026-09-16.md).

## What it patches

- **One call.** The call of the connection's make_shared inside the local server connect (0x124AAE8) goes to a
  hook, which makes the connection as before and then looks at the earlier ones.
- **A weak reference per connection.** The hook adds one to each new connection's weak count, so a control
  block the game frees by itself stays readable until the hook lets go of it.
- **The free.** An earlier connection whose use count is 1 and whose own game mode's list holds it has nothing
  else left. The count goes from 1 to 0 by compare and exchange, the list entry is cleared, and the control
  block's destroy and delete run as they would for a last `std::shared_ptr`.
- **Checks first.** The exe's stamp and size, and seven byte ranges hashed against the 0.9.1 build (the connect,
  the make_shared and its thunk, the destructors of the connection, the local game server and the game mode's
  list, and the list's element release). Any mismatch logs the range and patches nothing.

The manager still holds the previous session when the next one connects, so memory keeps the current session
and the one before it, and each is freed one load later.

## What it rests on

The free runs on the connecting thread inside the connect, and nothing else of the game touches a finished
session there. A plain copy of the list's entry, which raises the count without looking, or a push that moves
the list between the entry being found and cleared, would race it if another thread made one at that moment.
Every free logged so far ran on `GameThread` a whole session after the freed one ended, with no fault, but the
game's code that reads the list was not examined.
[DEC-023](../../decisions/DEC-023-the-session-free-stays-immediate.md) records why the free does not wait a
connect to take that assumption away.

## Log

A `[sessions]` line per free, with the game mode's class and the time it took, and one per connect with the
thread and how many connections are held, see [telemetry](../ops/telemetry.md).

## What it has run on

Six Red Bull Ring practice visits parked in the pit box with the menu between them, eleven sessions freed on
`GameThread` in 0.2 to 5.7 ms each, no crash, the live heap growing about 5 MB a visit instead of 57 (BUG-016
holds the table). Then the owner's own play, laps driven at two tracks, a leaderboard lap, a multiplayer
session and a thirty AI race, whose `InstantRaceRemote` freed in 29.1 ms inside the next load. A session
restarted from the pause menu is the one path that has not run under it.
