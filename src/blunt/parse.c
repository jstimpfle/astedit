#if 0

#include <stddef.h>

enum {
        BLUNTASTNODE_FILE,  // root node
        BLUNTASTNODE_DIRECTIVE,
        BLUNTASTNODE_TOKEN,
};

enum BluntDirectiveKind {
        BLUNTDIRECTIVE_REQUIRE,
        NUM_BLUNTDIRECTIVE_KINDS,
};


struct BluntAstTag {
        /* we need to be able to go up, at least. */
        struct BluntAstTag *parent;
        int nodeKind;  /* BLUNTASTNODE_?? */
};

struct BluntAstFile {
        struct BluntAstTag tag;
        struct BluntAstDirective *directives;
        int numDirectives;
};

struct BluntAstDirective {
        struct BluntAstTag tag;
        int directiveKind;
};





static struct BluntAstFile *create_BluntAstFile(void)
{
        struct BluntAstFile *fileNode;
        ALLOC_MEMORY(fileNode, 1);
        fileNode->tag.nodeKind = BLUNTASTNODE_FILE;
        fileNode->directives = NULL;
        fileNode->numDirectives = 0;
}

void blunt_parse_file(void)
{
        struct BluntAstFile *fileNode = create_BluntAstFile();
}

#endif