#include <gnome-xml/parser.h>

int copy_line(const char **srcpp, char *buf, int maxlen)
{
    const char *srcp;
    char *dstp;

    /* Skip leading whitespace */
    srcp = *srcpp;
    while ( *srcp && isspace(*srcp) ) {
        ++srcp;
    }

    /* Copy the line */
    dstp = buf;
    while ( *srcp && (*srcp != '\r') && (*srcp != '\n') ) {
        if ( (dstp-buf) >= maxlen ) {
            break;
        }
        *dstp++ = *srcp++;
    }

    /* Trim whitespace */
    while ( (dstp > buf) && isspace(*dstp) ) {
        --dstp;
    }
    *dstp = '\0';

    /* Update line pointer */
    *srcpp = srcp;

    /* Return the length of the line */
    return strlen(buf);
}

void PrefixLevel(int level)
{
    int i;

    for ( i=0; i<level; ++i ) {
        printf(" ");
    }
}

void ParseNode(xmlDocPtr doc, xmlNodePtr cur, int level)
{
    const char *data;
    char buf[BUFSIZ];
    int i;

    while ( cur ) {
        if ( strcmp(cur->name, "option") != 0 ) {
            xmlSetProp(cur, "checked", "true");
        }
        if ( strcmp(cur->name, "text") != 0 ) {
            PrefixLevel(level);
            printf("Parsing %s node at level %d { \n", cur->name, level);
            data =  xmlNodeListGetString(doc, cur->childs, 1);
            if ( data ) {
                while ( copy_line(&data, buf, BUFSIZ) ) {
                    PrefixLevel(level);
                    printf(" Data: %s\n", buf);
                }
            }
            if ( cur->childs ) {
                ParseNode(doc, cur->childs, level+1);
            }
            PrefixLevel(level);
            printf("}\n");
        }
        cur = cur->next;
    }
}

main()
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile("setup.xml");
    if ( doc ) {
        cur = doc->root;
        if ( cur ) {
            printf("Root node name: %s\n", cur->name);
            ParseNode(doc, cur, 0);
        }
    }
    xmlSaveFile("foo.xml", doc);
}
