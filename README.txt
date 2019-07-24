The goal of this project is to experiment a little with syntax aware editing
and basic IDE functionality.

A bare bones text editor is already in a working state:

- Rope data structure that can efficiently insert or remove chunks of text
- UTF-8 encoding/decoding.
- Basic graphics
- Cursor can only go 1 character left/right at a time, currently.

Now I want to include a parser for a simple programming language that has a
valid parse tree for any text that is buffered in the editor.

That means that the parser needs to insert "INVALID" nodes in the AST where it
failed to parse the input, and it must try to recover as soon as possible to a
known state, to avoid having the rest of the input subsumed under an INVALID
node.

The syntax of the programming language must be explicitly designed to support
reliable and early recovery.

Most importantly, I want to make everything such that the AST is updated
incrementally as the buffer is edited. I don't want to re-parse the whole
buffer on every little change. And I don't want to re-create all defined
objects, identifiers, checked types, and so on, that weren't changed by the
edit in the first place.
