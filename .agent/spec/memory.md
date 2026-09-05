---
title: Spec for project memory
updated: 2026-09-05
---

# Memory spec

Applies to every file under `.agent/memory/`. Memory holds durable project facts that are not derivable from the code or the docs, so a cold session does not rediscover them.

- Project truth only. Nothing about any person, and nothing about any machine's local state (paths, user names, hardware serials).
- One topic per file, frontmatter with `title` and `updated`.
- Each fact carries how it was established (measured, read from a binary, confirmed by a run) so a later session knows how much to trust it and how to re check it.
- Facts that turn out wrong are corrected in place, not appended to.
