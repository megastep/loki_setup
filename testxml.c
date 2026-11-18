
#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "setup-xml.h"

int copy_line(const char **srcpp, char *buf, int maxlen)
{
    const char *srcp;
    char *dstp;

    /* Skip leading whitespace */
    srcp = *srcpp;
    while ( *srcp && isspace((unsigned char)*srcp) ) {
        ++srcp;
    }

    /* Copy the line */
    dstp = buf;
    while ( *srcp && (*srcp != '\r') && (*srcp != '\n') ) {
        if ( (dstp-buf) >= maxlen - 1 ) {
            break;
        }
        *dstp++ = *srcp++;
    }

    /* Trim whitespace */
    while ( (dstp > buf) && isspace((unsigned char)dstp[-1]) ) {
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

    while ( cur ) {
        if ( strcmp(cur->name, "option") != 0 ) {
            xmlSetProp(cur, "checked", "true");
        }
        if ( ! xmlNodeIsText(cur) ) {
            PrefixLevel(level);
            printf("Parsing %s node at level %d { \n", cur->name, level);
            data =  xmlNodeListGetString(doc, XML_CHILDREN(cur), 1);
            if ( data ) {
                const char *line = data;
                while ( copy_line(&line, buf, BUFSIZ) ) {
                    PrefixLevel(level);
                    printf(" Data: %s\n", buf);
                }
                xmlFree((void *)data);
            }
            if ( XML_CHILDREN(cur) ) {
                ParseNode(doc, XML_CHILDREN(cur), level+1);
            }
            PrefixLevel(level);
            printf("}\n");
        }
        cur = cur->next;
    }
}

xmlNodePtr FindNode(xmlNodePtr node, const char *name)
{
    while ( node ) {
        /* Did we find the node? */
        if ( strcmp(node->name, name) == 0 ) {
            return(node);
        }
        /* Look through the children */
        if ( XML_CHILDREN(node) ) {
            xmlNodePtr found;

            found = FindNode(XML_CHILDREN(node), name);
            if ( found ) {
                return(found);
            }
        }
        /* Keep looking */
        node = node->next;
    }
    return(NULL);
}

const char *GetNodeText(xmlDocPtr doc, xmlNodePtr node, const char *name)
{
    const char *text;

    text = NULL;
    node = FindNode(node, name);
    if ( node ) {
        text = xmlNodeListGetString(doc, XML_CHILDREN(node), 1);
    }
    return(text);
}

int main(void)
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile("setup.xml");
    if ( doc ) {
        const char *desc = GetNodeText(doc, XML_ROOT(doc), "desc");
        if (desc) {
            printf("Description: %s\n", desc);
            xmlFree((void *)desc);
        }
        cur = XML_ROOT(doc);
        if ( cur ) {
            printf("Root node name: %s\n", cur->name);
            ParseNode(doc, cur, 0);
        }
    }
    if ( !doc )
        return 1;
    int result = xmlSaveFile("foo.xml", doc);
    xmlFreeDoc(doc);
    if ( result < 0 )
        return 1;
	return 0;
}
