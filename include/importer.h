#ifndef IMPORTER_H
#define IMPORTER_H

#include <glib.h>

/* PDF → Taguette-compatible HTML (paragraphs as <p> blocks) */
char* importer_pdf_to_html (const char *filename);

/* Plain text → Taguette-compatible HTML */
char* importer_text_to_html (const char *text);

#endif
