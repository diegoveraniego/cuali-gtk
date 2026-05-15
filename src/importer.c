#include "importer.h"
#include <poppler.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

/*
 * Convert a plain-text paragraph into an HTML-escaped <p> block.
 * Taguette offsets are UTF-8 bytes over the plain text (no HTML tags).
 */
static void
append_paragraph (GString *out, const char *text, int len)
{
    if (len <= 0) return;
    g_string_append (out, "<p>");
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '<')       g_string_append (out, "&lt;");
        else if (c == '>')  g_string_append (out, "&gt;");
        else if (c == '&')  g_string_append (out, "&amp;");
        else                g_string_append_c (out, (char)c);
    }
    g_string_append (out, "</p>\n");
}

/*
 * Split a page's plain text into paragraphs on double-newlines,
 * emit each as a <p> block.  Single newlines become a space so words
 * at line-breaks don't merge.
 */
static void
page_text_to_html (GString *out, const char *text)
{
    if (!text || !*text) return;

    /* Normalize: replace \r\n and \r with \n */
    char *norm = g_strdup (text);
    for (char *p = norm; *p; p++) {
        if (*p == '\r') *p = '\n';
    }

    const char *start = norm;
    while (*start) {
        /* find double-newline = paragraph break */
        const char *end = start;
        while (*end) {
            if (end[0] == '\n' && end[1] == '\n') break;
            end++;
        }

        /* build paragraph content: collapse single \n to space */
        GString *para = g_string_new ("");
        for (const char *p = start; p < end; p++) {
            if (*p == '\n') {
                if (para->len > 0 && para->str[para->len - 1] != ' ')
                    g_string_append_c (para, ' ');
            } else {
                g_string_append_c (para, *p);
            }
        }

        /* strip leading/trailing whitespace */
        g_strstrip (para->str);
        if (para->len > 0) {
            append_paragraph (out, para->str, (int)para->len);
        }
        g_string_free (para, TRUE);

        if (*end == '\0') break;
        start = end + 2;   /* skip \n\n */
    }

    g_free (norm);
}

/*
 * PDF → Taguette-compatible HTML.
 * Each page is separated by an empty <p></p> for visual spacing.
 */
char *
importer_pdf_to_html (const char *filename)
{
    GError *error = NULL;
    char *uri = g_filename_to_uri (filename, NULL, &error);
    if (!uri) {
        fprintf (stderr, "importer: URI error: %s\n", error->message);
        g_error_free (error);
        return NULL;
    }

    PopplerDocument *doc = poppler_document_new_from_file (uri, NULL, &error);
    g_free (uri);
    if (!doc) {
        fprintf (stderr, "importer: PDF open error: %s\n", error->message);
        g_error_free (error);
        return NULL;
    }

    int n_pages = poppler_document_get_n_pages (doc);
    GString *html = g_string_new ("");

    for (int i = 0; i < n_pages; i++) {
        PopplerPage *page = poppler_document_get_page (doc, i);
        if (!page) continue;
        char *text = poppler_page_get_text (page);
        if (text) {
            page_text_to_html (html, text);
            g_free (text);
        }
        g_object_unref (page);
        /* page separator */
        if (i < n_pages - 1)
            g_string_append (html, "<p>\u00a0</p>\n");
    }

    g_object_unref (doc);
    return g_string_free (html, FALSE);
}

/*
 * Plain text → Taguette-compatible HTML.
 * Paragraphs split on blank lines.
 */
char *
importer_text_to_html (const char *text)
{
    if (!text) return g_strdup ("");
    GString *html = g_string_new ("");
    page_text_to_html (html, text);
    return g_string_free (html, FALSE);
}
