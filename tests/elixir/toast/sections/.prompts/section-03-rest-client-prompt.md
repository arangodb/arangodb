## Task

Generate content for section `section-03-rest-client` (filename: `section-03-rest-client.md`).

## Context Files

Read these files first to understand the full implementation plan:

1. `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/claude-plan.md` - Full implementation plan
2. `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/claude-plan-tdd.md` - Test stubs for each section
3. `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/sections/index.md` - Section manifest and descriptions

## Your Section

In `index.md`, locate the `<!-- SECTION_MANIFEST ... -->` block. Find the entry
for `section-03-rest-client` to understand what this section should contain.

## Output

Output ONLY the markdown content for this section. No JSON wrapper, no code
blocks around the output. Just the raw markdown content.

The hook system will automatically write your output to:
`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/sections/section-03-rest-client.md`

## Content Requirements

The section content must be **completely self-contained**. An implementer should be able to:

1. Read only this section
2. Create a TODO list
3. Start implementing immediately

**Include in the content:**

- Tests FIRST (extract relevant tests from `claude-plan-tdd.md`)
- Implementation details (extract relevant sections from `claude-plan.md`)
- All necessary background and context
- File paths for any code to be created/modified
- Dependencies on other sections (reference only, don't duplicate content)
- **CRITICAL** Remember that tests and code should only be fully specified if absolutely necessary. Stub definitions and docstrings are fine.

**Do NOT:**

- Reference other documents - copy relevant info into the section
- Assume the reader has seen the original plan
- Include content from other sections
- Leave placeholders or TODOs for the implementer to figure out
- Write full code implementations - sections are prose with stubs/signatures only when necessary to clarify intent

## Important Notes

- Extract ONLY the content relevant to your assigned section
- The section should be implementable in isolation (given its dependencies are met)
- Be thorough - better to include too much context than too little
- Preserve code formatting and indentation exactly as shown in the source files