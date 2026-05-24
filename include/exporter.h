#ifndef EXPORTER_H
#define EXPORTER_H
#include <stdbool.h>

/* Exports highlights: columns = snippet, document, tags, memo */
bool export_highlights_csv  (int project_id, const char *path);
bool export_codebook_csv    (int project_id, const char *path);

#ifdef HAVE_XLSXWRITER
bool export_highlights_xlsx (int project_id, const char *path);
#endif

#endif
