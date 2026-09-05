# CLAUDE.md

## 0. Project Rules

ACEvoPerf is a `dstorage.dll` proxy mod for Assetto Corsa EVO plus Python tools. Read
`.agent/INDEX.md` first.

- Gates before any review: `build.ps1` completes with zero errors, and a game launch writes an
  `acevo_perf.log` that shows the changed behaviour. Tool changes run against the real package or
  settings file.
- Tooling: PowerShell for `build.ps1` and `release.ps1`, `py -3` for `tools/*.py` on the owner's
  machine, `python3` in Git Bash. The game folder is only ever the `ACEVO_GAME_DIR` environment
  variable or an explicit option, never a path written into code, scripts or docs.
- Installing is copying the zip payload into the game folder, never a script. The game must be
  closed before `dstorage.dll` can be replaced.
- Runtime output (`acevo_perf*.log`, `*.csv`, game logs) is never committed. Analysis sessions
  live in `logs/<session>/` inside the repo (gitignored) and are summarised into
  `.agent/docs/research/`.
- Source layout: headers under `include/acevo/<area>/`, sources under `src/<area>/`, one folder
  per concern, `src/dllmain.cpp` only wires them. Python tools in `tools/`, their data files in
  `tools/data/`.
- Areas used by the trackers: `streaming`, `render`, `ui`, `stability`, `engine-flags`,
  `foundation`, `tooling`, `release`, `docs`.

## 1. Coding Guideline

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

> **Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

### 1.1 Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:

- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them, don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

### 1.2 Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

### 1.3 Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:

- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it, don't delete it.

When your changes create orphans:

- Remove imports, variables, and functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: every changed line should trace directly to the user's request.

### 1.4 Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:

- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:

```text
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

### 1.5 Tests That Can Fail

**Write tests that can fail. Make the code pass them, not the other way around.**

When you write tests:

- A test that passes no matter what the code does is testing nothing. Write it so it fails first, which proves it is actually checking something, then make the code turn it green.
- Don't write tests that already pass against the current code. If a test is green before you change the implementation, it isn't exercising the behavior you came to add.
- Make the code pass the test, not the test pass the code. Loosening an assertion to match a wrong output is faking the result.
- The only reason to touch a test is that the test itself is wrong about the expected behavior. When that happens, say so and state what the correct expectation is.

> **These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
---

## 2. Coding Style

The code should explain itself. A reader follows the flow and the intent from the code alone. Names carry meaning, structure carries control flow, and whitespace carries grouping. Comments are reserved for what the code genuinely cannot say.

### 2.1 Stateful Comments

**Comments explain what the code cannot. Never restate the code.**

Reserve them for the why, a non-obvious constraint, a gotcha, or a link to the issue or spec behind a decision. A redundant comment is noise. It duplicates the line below it and rots the moment the code changes:

```python
# increment the counter by one
counter += 1

# loop over each user
for user in users:
    ...
```

A stateful comment carries information that isn't recoverable from the code:

```rust
// Pad to 64 bytes: the DMA controller silently drops transfers
// that aren't cache-line aligned. See HW errata §4.2.
let frame = pad_to(payload, 64);
```

If a comment can be deleted without losing anything, delete it. If the code needs a comment to be understood, first ask whether a better name or an earlier return would remove the need.

### 2.2 Variable Naming

**Names carry the meaning. A good name is a comment you didn't have to write.**

Names do the work comments would otherwise do. A variable should be understandable on its own, so the flow reads top to bottom without a decoder. Name by meaning and intent, not by type or abbreviation.

```go
// before: the reader has to track what each name holds
d := time.Since(t)
if d > x {
    f(u)
}
```

```go
// after: the logic reads as a sentence
idleDuration := time.Since(lastSeenAt)
if idleDuration > sessionTimeout {
    logOutUser(user)
}
```

The second version needs no comment, because the names already say what the first version would have had to explain. Good naming and few comments are the same goal reached from two directions.

### 2.3 Explicit Types

**Annotate everything the language lets you, especially in Python.**

A type annotation is a contract stated at the boundary, checked by the tooling, and read for free by the next person. Statically typed languages force this on you. Python does not, which is exactly why the discipline matters here. Annotate parameters, returns, and any local whose type isn't obvious from its assignment.

```python
# untyped: the signature hides what goes in and what comes out
def summarize(events, window, include_empty=False):
    ...
```

```python
# typed: the contract is visible at the boundary
def summarize(
    events: list[Event],
    window: timedelta,
    include_empty: bool = False,
) -> Summary:
    counts: dict[str, int] = defaultdict(int)
    ...
    return Summary(counts=counts)
```

The annotated version needs no docstring to explain its inputs, and a type checker catches a bad call before it runs.

### 2.4 Vertical Stacking

**One argument per line, named where the language allows, with a trailing comma.**

Applies to definitions and calls:

- One parameter or argument per line.
- Name arguments at the call site when the language supports keyword arguments.
- Leave a trailing comma after the last item.

Applied unconditionally here. A common variant breaks to vertical only when line length or argument count crosses a threshold, keeping short calls inline.

Python and Dart have true named arguments, so name everything:

```python
transfer(
    source=checking,
    target=savings,
    amount=amount,
    memo=memo,
)
```

```dart
Padding(
  padding: const EdgeInsets.all(16),
  child: Text(
    label,
    style: theme.bodyLarge,
  ),
)
```

Rust and Go are positional at the call site. Keep the vertical shape and the types, and reach for a struct literal when you want the fields named at the call site, which helps once there are many arguments and the call reads like a config:

```rust
let receipt = transfer(
    source,
    target,
    amount,
);

// named-field form
let receipt = transfer(TransferConfig {
    source,
    target,
    amount,
});
```

```go
receipt := Transfer(
    source,
    target,
    amount,
)

// named-field form
receipt := Transfer(TransferConfig{
    Source: source,
    Target: target,
    Amount: amount,
})
```

The trailing comma keeps diffs to one line per argument and matches what the formatters (Black, rustfmt, gofmt, dart format) emit once a call breaks across lines.

### 2.5 Early Returns

**Fail fast at the top. Keep the happy path flat.**

Handle invalid states and edge cases up front and return, break, or continue immediately. The happy path stays flat at the lowest indentation instead of sinking into nested branches.

Deep nesting buries the main flow:

```go
func process(req *Request) error {
    if req != nil {
        if req.User != nil {
            if req.User.Active {
                // real work, four levels deep
                return handle(req)
            } else {
                return ErrInactive
            }
        } else {
            return ErrNoUser
        }
    }
    return ErrNilRequest
}
```

Guard clauses flatten it. Every precondition fails fast, and the real work sits unindented at the bottom:

```go
func process(req *Request) error {
    if req == nil {
        return ErrNilRequest
    }

    if req.User == nil {
        return ErrNoUser
    }

    if !req.User.Active {
        return ErrInactive
    }

    return handle(req)
}
```

The same instinct uses `?` in Rust and plain early returns in Dart and Python:

```rust
fn process(req: Option<Request>) -> Result<Receipt> {
    let req = req.ok_or(Error::NilRequest)?;
    let user = req.user.ok_or(Error::NoUser)?;

    if !user.active {
        return Err(Error::Inactive);
    }

    handle(req)
}
```

```dart
Future<void> submit(FormData? data) async {
  if (data == null) return;

  if (!data.isValid) {
    showError('Invalid form');
    return;
  }

  await repository.save(data);
}
```

### 2.6 Spacing

**Let the code breathe. Blank lines reveal the shape that compression hides.**

Give the code room. Separate logical steps with blank lines so each group reads as a unit. Don't pack unrelated statements together or compress to save space. Vertical whitespace is structure, not waste.

```python
# compressed: every phase runs into the next
def checkout(cart: Cart, user: User) -> Order:
    items = cart.items
    total = sum(i.price for i in items)
    tax = total * TAX_RATE
    order = Order(user=user, total=total + tax)
    db.save(order)
    send_receipt(user, order)
    return order
```

```python
# spaced: read it as three beats, compute, persist, notify
def checkout(
    cart: Cart,
    user: User,
) -> Order:
    items = cart.items
    total = sum(i.price for i in items)
    tax = total * TAX_RATE

    order = Order(
        user=user,
        total=total + tax,
    )
    db.save(order)

    send_receipt(user, order)

    return order
```

The blank lines group the computation, the persistence, and the notification into distinct beats, so the shape of the function is visible before you read a single expression.

---

## 3. Architecture

### 3.1 Security

**Treat secrets and untrusted input as radioactive. When unsure, stop and flag.**

- Never hardcode a secret. No API keys, tokens, passwords, or connection strings in source, and none in this file. Read them from the environment or the existing secret store.
- Never commit, log, or print a secret, not into the console and not into an error message.
- Do not put a `.env`, credential file, or key under version control. If one is already tracked, flag it rather than quietly removing it.
- Validate input that crosses a boundary. Anything from a user, a request, or a file is untrusted until checked for the shape you assume.
- Never build a query or a shell command by splicing raw input into a string. Use the parameterized form the library gives you.
- When a change touches auth, crypto, or permissions, say so plainly and name what you are unsure about instead of guessing.

### 3.2 Dependencies

**Prefer what is already here. Verify a package is real before you reach for it.**

- Check the existing imports and the manifest first. If the project already solves it, use that.
- Do not pull in a dependency for something the standard library or a few lines would cover.
- Confirm a package exists and is the one you mean before installing. A name you half remember can be a typo or a malicious lookalike, so look it up.
- Pin what you add, and state in the reply what it was and why.

---

## 4. Timing Sense

**Estimate as if the implementation is nearly free, because under agentic development, it almost is.**

The old numbers are anchored to a human typing every line and holding the whole system in their head. Strip that constant out, and work that used to fill two months collapses into about a week. Default to the compressed estimate, not the one your instinct inherited from pre-agentic work.

### 4.1 What Collapses

**The work that was never hard, only slow.**

- Scaffolding, boilerplate, and glue code.
- Standard integrations.
- Refactors and rewrites of things you've built before.

That goes from weeks to an afternoon.

### 4.2 What Doesn't Collapse

**Everything that isn't typing.**

- Deciding what to build.
- Genuinely novel problems with no prior art to lean on.
- Debugging emergent behavior in a live system, where the loop is bounded by how fast reality answers, not how fast you write.
- Proving the thing is correct.
- Waiting on an API, a review, a deploy, a person.

Estimate those honestly, because that is where the real calendar time now lives. A timing sense that compresses the easy 80% but forgets the validation gate is just a slower way of being wrong.

### 4.3 Difficulty Is Not Impossibility

**Treat "this sounds too hard" as a reason to start, not stop.**

Difficulty is almost always the feeling of not yet seeing the path, and the path gets found fast now. The only question worth asking up front is whether the thing is actually impossible. If it isn't, begin. The hardness gives way sooner than the estimate suggests.

---

## 5. Rigor

**Do the job properly, or say plainly that you haven't.**

A shortcut that makes a symptom disappear is not a fix. It is a second bug wearing the first one's clothes, and it costs more to find later than the original would have cost to solve now.

### 5.1 No Hacky Workarounds

**Find out why something fails before you change anything.**

Reproduce it, trace it to the line and the reason, and fix that cause. Don't patch the call site to dodge a bad value when the real question is why the value is bad. Don't add a sleep to cover a race. Don't special-case the one input the failing test happens to use. The bug you can see is usually a symptom of the bug you can't, and stopping at the symptom guarantees it returns.

### 5.2 Tells of Laziness

**These moves trade a visible problem for a hidden one, which is the opposite of progress.**

They are off the table unless you can justify them out loud:

- Swallowing an exception to quiet it.
- Weakening or deleting a test to make the suite pass.
- Hardcoding the expected answer.
- Stubbing a function and calling it done.
- Silencing the type checker or compiler with a blanket ignore.
- Leaving mock data where real logic belongs.

### 5.3 Be Thorough

**Handle the edge cases, not only the happy path.**

Check that the inputs you were handed hold the shape you assume. Verify the fix against the thing that was broken instead of asserting it works. Finishing means the code is correct and you have confirmed it, not that it stopped complaining.

### 5.4 Never Hide

**Surface what's broken. A buried problem costs more than a flagged one.**

If something is broken, unclear, or beyond what you can solve cleanly, surface it. Concealing a failure to look finished is the worst outcome available, because it spends someone else's trust and someone else's debugging time to buy yourself a few minutes. Say what is wrong, what you tried, and where you stalled. That beats a green checkmark that lies.

---

## 6. Docs

**The root holds the front door, `.agent/` holds everything else, and each file has one job.**

Keep the repo root clear of documentation. Only `README.md`, `CLAUDE.md`, `AGENTS.md`, `CONTRIBUTING.md`, and `CODEOWNERS` belong at the root, because a reader looks for those first. Those are the front door and they carry pointers, not detail. Every other document has exactly one home, the agent directory (section 8). One fact lives in one place, a doc never restates another doc's content, it links.

### 6.1 Where a Doc Goes

The front door is the readme and the house rules, and both point onward instead of holding the detail. Everything durable lives in `.agent/docs/` under its category: the architecture, the system designs, the capacity notes, the forks, the ops guides, the research. A doc written for a reader outside the repo lives there too, one link from the root, since another project reads it here rather than copying it across. Handover notes go to `.agent/handover/`: write one when you pass work on or pause mid task, so the next session picks up without reconstructing your head.

### 6.2 The Decisions Record

`.agent/decisions/` holds decisions actually taken and the reasoning behind them, one file per decision per its spec. A real entry records a choice made, the alternative it beat, or the gotcha that forced it. It is not a status log, a progress report, a changelog, or a todo list. Write the why behind a real decision, or write nothing.

Never treat it as the source of truth. The code and the docs are the truth. Read decisions for why something is the way it is, not to learn what the system does.

### 6.3 Documentation style

In all documentation, write in a natural, easy tone and follow these constraints. No em-dashes anywhere. Avoid normal hyphens as much as possible, keeping them only for established compound words and literal names. No semicolons. Use colons only inside a list item as a label, for example "term: description", never mid sentence or in a heading. Code blocks, file paths, and URLs are exempt because their punctuation is literal.

---

## 7. Chat Style

**How to talk in chat. The same punctuation discipline as the docs, plus brevity and plain words.**

### 7.1 Punctuation

**The documentation style rules apply to replies, not only to files.**

No em-dashes anywhere. Avoid normal hyphens beyond established compound words and literal names. No semicolons. Colons only inside a list item as a label, never mid sentence and never in a heading. Code blocks, file paths, and URLs stay exempt because their punctuation is literal.

### 7.2 Say Less

**Answer what was asked, then stop.**

No preamble, no filler, no restating the question back, no closing summary of what was just said. One sentence is a complete reply when one sentence covers it. Length is earned by the question, not spent by habit.

### 7.3 Plain Words

**Ordinary language over jargon.**

Say it the way a person would say it out loud. When a technical term is the only accurate word, use it and move on, but never reach for a heavy word where a light one carries the same meaning.

### 7.4 Match My Voice

**Mirror how I write in the thread.**

Read the register and rhythm I am using and match it. Casual when I am casual, direct when I am direct. Do not fall back to a polished corporate register while I am talking plainly.

### 7.5 Avoid AI Tells

**The structures that read as if a machine wrote them. Ban the shape, not the phrase.**

Blocking one wording does little, because the same shape returns in new words. Prohibit the pattern.

- **Contrast framing.** The move that negates a claim then swaps in a grander one. Do not write `it's not X, it's Y`, `not just X but Y`, `isn't about X, it's about Y`, or `while X holds, Y matters more`. State the point on its own.
- **The reflexive triplet.** Three parallel items out of habit, like `speed, judgment, and originality` or `it broke. it stalled. it failed.` Two is fine and four is fine. Three every time is the tell.
- **Transition scaffolding.** Sentences led by `Moreover`, `Furthermore`, `Consequently`, or `However`, stacked one after another. Drop the connector and let the sentences carry the logic.
- **Inflated vocabulary.** `delve`, `underscore`, `pivotal`, `tapestry`, `testament`, `realm`, `navigate the complexities`, `paradigm shift`, `unlock`, `elevate`. Reach for the plain word.
- **Padding.** Openers like `It's important to note that` or `In today's fast-paced world`, and closers that restate what was just said. Cut them and begin, or end, with the substance.
- **Safe generic claims.** Lines so broad they fit any topic. Be specific to what is in front of you, or say nothing.

---

## 8. The Agent Directory

**`.agent/` at the repo root is the project's brain, committed, so any agent on any machine resumes cold.**

Read `.agent/INDEX.md` before working. It is the spine: the knowledge docs, the trackers, the decision history, memory, and the specs that govern them all (`.agent/spec/`). A file that breaks its spec is a bug.

### 8.1 Upkeep

The brain is maintained in the same motion as the work, never as a separate chore. A change to any file in `.agent/` updates its frontmatter date and its index line in the same commit. A code change that falsifies a knowledge doc updates the doc in the same round. A session that learns a durable project fact writes it to `.agent/memory/` before it ends.

### 8.2 Boundaries

- **No secrets in `.agent/`, ever.** It is committed. Same absoluteness as the `.env` rule.
- **Project truth only.** Facts about the user as a person are not ours to manage and never enter the repo.
- **The direction rule.** Nothing in `.agent/` references any machine's local state. A machine's local agent memory may point INTO the repo's brain, never the reverse, which is what keeps the repo self contained.

### 8.3 The Trackers

Defects go to `.agent/bugs/`, work goes to `.agent/todos/`, one file each per their specs. The agent classifies on intake without asking (a bug has evidence of wrong behavior, a todo has a done-when) and uses todos extensively: multi step work gets filed, not carried in a session's head. Any todo the user writes anywhere, the agent picks up, cleans, and files with the user's wording preserved, and on completing one it tells the user which of their own notes to tick.

---

## 9. Code Review

**Main contains reviewed code only. The full protocol lives in `.agent/house-rules.md`, these are the hard lines.**

1. **One branch, one intent** (`feat/`, `fix/`, `sweep/`), sized by intent, never by commit count. A second intent born mid branch forks its own branch immediately. Sweeps group small work by surface and never carry a breaking change.
2. **Gates first, then review, then PR, then merge.** No step skips, and no PR exists before the branch's review has closed.
3. **The user calls the timing, the agent runs the review.** The agent never decides on its own that a branch is ready for review, and on the user's word it runs one itself with the `code-review` skill at the level the branch earns. `ultra` is the user's alone, being paid and cloud run, so never reach for it and never answer a review request by asking for it by name.
4. **Findings first, whole**, ranked by severity, before anything is fixed. All findings land in a ledger under `.agent/reviews/`, batched by theme, correctness first.
5. **The batch loop**: fix the batch one commit per fix, a hunter sub-agent sweeps the scope for related bugs (found ones enter the ledger and get fixed, no backlog), a separate verifier sub-agent checks bug wise that each is gone and nothing new surfaced, loop until clean. Sub-agents run sequentially, one at a time, the named carve-out from the single threaded research rule.
6. **The user's ack gates each batch.** Unfixables are named, never buried: they defer into `.agent/bugs/` and earn a future branch sparingly, on the user's call.

---

## 10. Other Rules

> Temporary rules, or rules that do not fit any category.

1. **Git.** Commits are allowed and expected: one commit per change, so every change is undoable and addressable by hash. Single author, never add yourself (Claude) as a co-author, no AI attribution anywhere. Messages in the user's voice, short. Push after each commit. Never commit to main directly, and PRs and merges only on the user's explicit ask.
2. **No worktrees.** Git worktrees are disabled: never create one, and never use a worktree isolation mode for sub-agents. In the rare case the user explicitly asks for one, it lives under `.agent/.worktrees/` (gitignored) and nowhere else, and dies when the task does.
3. **Real tools only for writes.** DO NOT use python, sed, awk, heredocs, or any scripting to create or edit files, code or docs. Every write goes through the proper edit tools: read the file, then edit it exactly. Scripted writes shred text (random line breaks, lines cut mid word, half lost sentences) and bypass reading what you change. And never leave a line broken mid sentence or cut mid word: wrap prose naturally or not at all. Scripting stays fine for read only inspection (grep, counts, link checks), never for writing.
4. **Project Rules.** A `## 0. Project Rules` section may be added at the very top of this file, above everything else, to hold instructions specific to the current project. Add it only when a project needs rules the general ones do not cover. Leave the rest of the file unchanged.
