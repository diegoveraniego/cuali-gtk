#include "importer.h"
#include <poppler.h>
#include <stdio.h>

char* importer_pdf_to_text(const char *filename) {
    GError *error = NULL;
    char *uri = g_filename_to_uri(filename, NULL, &error);
    if (!uri) {
        fprintf(stderr, "Error converting filename to URI: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    PopplerDocument *doc = poppler_document_new_from_file(uri, NULL, &error);
    g_free(uri);
    if (!doc) {
        fprintf(stderr, "Error opening PDF: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    int n_pages = poppler_document_get_n_pages(doc);
    GString *content = g_string_new("");

    for (int i = 0; i < n_pages; i++) {
        PopplerPage *page = poppler_document_get_page(doc, i);
        if (page) {
            char *text = poppler_page_get_text(page);
            g_string_append(content, text);
            g_string_append(content, "\n\n");
            g_free(text);
            g_object_unref(page);
        }
    }

    g_object_unref(doc);
    return g_string_free(content, FALSE);
}
