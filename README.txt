The goal of this project is to experiment a little with syntax aware editing
and basic IDE functionality.

A bare bones text editor is already in a working state:

- Rope data structure for efficiency. (see below)
- UTF-8 encoding/decoding.
- Basic windowing and OpenGL access using GLFW library
- Text rendering using FreeType library.

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

ROPE DATASTRUCTURE
==================

This project contains a rope implementation based on a Red-black tree, where
each node of the tree holds up to 1024 bytes. Thanks to that implementation,
most text editing operations, like navigating or inserting/deleting small
chunks of text, take only a few microseconds. Even in a test that I did with a
huge buffer (loaded from a 1.8G text file) most operations took < 20us on my
computer.

In other words, there is no noticeable latency from the internal data
structures. That should enable a very good user experience, even for very
complex operations. (I haven't benchmarked the latency from other parts of the
code, like drawing).

Since the rope data structure has no idea about the shape of the data (binary
files, huge files, files with extremely long lines, etc.), these figures apply
for any kind of text file.

When a buffer is loaded from a file, the memory usage will be only slightly
larger (about 5%) than the text itself. In pathological cases (deleting just
the right spans after loading a large file), I think that overhead could go up
to about 50%.  But these cases are probably not practically relevant, and we
could still figure out an optimization.
