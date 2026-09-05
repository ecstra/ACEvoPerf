---
name: conventions
kind: doc
description: the shared rules every file in .agent obeys, frontmatter, naming, links, indexes
updated: 2026-08-12
links: [agent-readme]
---

# Conventions

The shared law. Each folder's own spec adds to this, never contradicts it.

## Frontmatter

Every markdown file in .agent opens with:

```
---
name: <unique-kebab-slug>
kind: doc | memory | bug | todo | decision | review
description: <one line, what this file holds>
updated: <YYYY-MM-DD, the last content change>
links: [<names of related files>]
---
```

Kinds add their own fields (status, severity, area, by, date, parent),
defined in that kind's spec. `name` is unique across the whole directory
and normally equals the filename without extension.

## Naming and ids

Files are kebab case. Tracked kinds carry ids: `BUG-###`, `TODO-###`,
`DEC-###`, zero padded, assigned as the next free number in that folder,
never reused. The filename is `<ID>-<short-slug>.md`.

## Links

Two forms, each with its job:

- Frontmatter `links:` lists names. They resolve through the indexes and
  survive nothing, so they are kept true: whoever moves or renames a file
  greps its name and fixes every reference in the same commit.
- Body links are real markdown relative links that actually resolve, to
  files in .agent and to project files both. A link that does not
  resolve is a bug.

A frontmatter link to a name that does not exist yet is allowed: it marks
something worth writing, not an error.

## Indexes

`.agent/INDEX.md` is the spine: one line per file, grouped by folder.
Folders with many files carry their own INDEX.md too, and the spine may
summarize those groups with counts plus the open items. An index line is
`- [name](path), hook` where the hook is the description or a shorter cut
of it.

The upkeep rule: any change to a file updates its `updated:` date and its
index line in the same commit. An index that disagrees with its folder is
a bug.

## Prose

House style: no em-dashes, no semicolons, plain words, colons only as
list labels. Terse beats complete. A claim about the code names the file
it is true in, so a sync round can verify it mechanically. Never leave a
line broken mid sentence or cut mid word: wrap prose naturally or not at
all.

## Boundaries

- No secrets, ever. This directory is committed.
- No machine paths, no user personal facts. Project truth only.
