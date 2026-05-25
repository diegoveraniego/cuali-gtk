/*
 * cuali-cli — CLI wrapper for AI-assisted qualitative data analysis
 *
 * Designed to be called by AI agents (gemini-cli, opencode, etc.) as a tool.
 * All output is JSON. Offsets are never exposed. Works purely with IDs.
 *
 * Exit codes:
 *   0 = success
 *   1 = bad arguments / usage
 *   2 = DB not found or corrupt
 *   3 = resource not found (highlight, tag, doc)
 *   4 = validation error (bad tag format, etc.)
 *   5 = write error
 */

#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <sqlite3.h>

/* ── Helpers ── */

/* Escape a string for JSON output */
static char *
json_escape (const char *s)
{
    if (!s) return g_strdup ("null");
    GString *out = g_string_new ("\"");
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if      (c == '"')  g_string_append (out, "\\\"");
        else if (c == '\\') g_string_append (out, "\\\\");
        else if (c == '\n') g_string_append (out, "\\n");
        else if (c == '\r') g_string_append (out, "\\r");
        else if (c == '\t') g_string_append (out, "\\t");
        else if (c < 0x20)  g_string_append_printf (out, "\\u%04x", c);
        else                g_string_append_c (out, (char)c);
    }
    g_string_append_c (out, '"');
    return g_string_free (out, FALSE);
}

static void
print_error (int code, const char *msg)
{
    char *escaped = json_escape (msg);
    fprintf (stderr, "{\"error\":%s,\"code\":%d}\n", escaped, code);
    g_free (escaped);
}

static void
print_ok (const char *msg)
{
    char *escaped = json_escape (msg);
    printf ("{\"ok\":true,\"message\":%s}\n", escaped);
    g_free (escaped);
}

/*
 * Validate a tag path:
 * - must not be empty
 * - all characters must be lowercase (Unicode-aware via GLib)
 *   accented lowercase (á é í ó ú ñ ü etc.) are allowed
 * - must not start or end with '/'
 * - must not contain '//' (empty segments)
 * - each segment must be non-empty
 * Returns NULL on success, or a static error string.
 */
static const char *
validate_tag_path (const char *path)
{
    if (!path || *path == '\0')
        return "tag path is empty";
    if (path[0] == '/')
        return "tag path must not start with '/'";
    if (path[strlen(path) - 1] == '/')
        return "tag path must not end with '/'";
    if (strstr (path, "//"))
        return "tag path contains empty segment ('//')";

    /* Check each Unicode character is not uppercase */
    const char *p = path;
    while (*p) {
        gunichar c = g_utf8_get_char (p);
        if (g_unichar_isupper (c)) {
            return "tag path must be all lowercase (use tildes, not capitals)";
        }
        p = g_utf8_next_char (p);
    }
    return NULL; /* valid */
}

/* Get the project's first (and usually only) project_id */
static int
get_project_id (void)
{
    return db_project_get_first_id ();
}

/* ── Commands ── */

/* info DB — project summary */
static int
cmd_info (const char *db_path)
{
    if (!db_init (db_path)) {
        print_error (2, "Cannot open database");
        return 2;
    }

    int project_id = get_project_id ();
    if (project_id < 0) {
        print_error (3, "No project found in database");
        db_close ();
        return 3;
    }

    char *name = NULL, *desc = NULL;
    db_project_get_info (project_id, &name, &desc);

    /* Count docs, tags, highlights */
    int n_docs = 0, n_tags = 0, n_hl = 0;
    sqlite3_stmt *s;

    s = db_documents_get_all (project_id);
    if (s) { while (sqlite3_step (s) == SQLITE_ROW) n_docs++; sqlite3_finalize (s); }

    s = db_tags_get_all (project_id);
    if (s) { while (sqlite3_step (s) == SQLITE_ROW) n_tags++; sqlite3_finalize (s); }

    s = db_results_get_all (project_id);
    if (s) { while (sqlite3_step (s) == SQLITE_ROW) n_hl++; sqlite3_finalize (s); }

    char *jname = json_escape (name);
    char *jdesc = json_escape (desc);
    char *jpath = json_escape (db_path);

    printf ("{\n"
            "  \"project_id\": %d,\n"
            "  \"name\": %s,\n"
            "  \"description\": %s,\n"
            "  \"db_path\": %s,\n"
            "  \"stats\": {\n"
            "    \"documents\": %d,\n"
            "    \"tags\": %d,\n"
            "    \"highlights\": %d\n"
            "  },\n"
            "  \"tagging_rules\": {\n"
            "    \"format\": \"tema/subtema/sub-subtema/...\",\n"
            "    \"depth\": \"unlimited\",\n"
            "    \"lowercase\": true,\n"
            "    \"preserve_accents\": true,\n"
            "    \"separator\": \"/\"\n"
            "  }\n"
            "}\n",
            project_id, jname, jdesc, jpath,
            n_docs, n_tags, n_hl);

    g_free (jname); g_free (jdesc); g_free (jpath);
    g_free (name);  g_free (desc);
    db_close ();
    return 0;
}

/* tags DB — list all tags with frequency */
static int
cmd_tags (const char *db_path)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();
    if (project_id < 0) { print_error (3, "No project"); db_close (); return 3; }

    sqlite3_stmt *stmt = db_tags_get_stats (project_id);
    if (!stmt) { print_error (2, "Failed to query tags"); db_close (); return 2; }

    printf ("[\n");
    bool first = true;
    while (sqlite3_step (stmt) == SQLITE_ROW) {
        int    id    = sqlite3_column_int  (stmt, 0);
        const char *path  = (const char *)sqlite3_column_text (stmt, 1);
        const char *color = (const char *)sqlite3_column_text (stmt, 2);
        int    count = sqlite3_column_int  (stmt, 3);

        char *jpath  = json_escape (path);
        char *jcolor = json_escape (color);

        if (!first) printf (",\n");
        printf ("  {\"id\":%d,\"path\":%s,\"color\":%s,\"count\":%d}",
                id, jpath, jcolor, count);
        first = false;
        g_free (jpath); g_free (jcolor);
    }
    printf ("\n]\n");
    sqlite3_finalize (stmt);
    db_close ();
    return 0;
}

/* doc DB ID — full document text */
static int
cmd_doc (const char *db_path, int doc_id)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }

    char *contents = db_document_get_contents (doc_id);
    if (!contents) {
        print_error (3, "Document not found");
        db_close ();
        return 3;
    }

    /* Output as plain text (not JSON) — easier to pass to AI as context */
    printf ("%s\n", contents);
    g_free (contents);
    db_close ();
    return 0;
}

/*
 * highlights DB [--doc DOC_ID] [--untagged] [--no-memo]
 * List highlights with optional filters.
 */
static int
cmd_highlights (const char *db_path, int doc_id_filter,
                bool untagged_only, bool no_memo)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();
    if (project_id < 0) { print_error (3, "No project"); db_close (); return 3; }

    /*
     * We query highlights via results (which joins through tags).
     * For untagged highlights, we need a different query.
     * Use the db directly since database.h doesn't expose a filtered query.
     */
    sqlite3_stmt *stmt;
    const char *sql;

    if (untagged_only) {
        sql = doc_id_filter > 0
            ? "SELECT h.id, h.snippet, h.memo, d.id, d.name "
              "FROM highlights h JOIN documents d ON h.document_id=d.id "
              "WHERE d.project_id=? AND d.id=? "
              "AND h.id NOT IN (SELECT highlight_id FROM highlight_tags) "
              "ORDER BY d.name, h.start_offset;"
            : "SELECT h.id, h.snippet, h.memo, d.id, d.name "
              "FROM highlights h JOIN documents d ON h.document_id=d.id "
              "WHERE d.project_id=? "
              "AND h.id NOT IN (SELECT highlight_id FROM highlight_tags) "
              "ORDER BY d.name, h.start_offset;";
    } else {
        sql = doc_id_filter > 0
            ? "SELECT h.id, h.snippet, h.memo, d.id, d.name "
              "FROM highlights h JOIN documents d ON h.document_id=d.id "
              "WHERE d.project_id=? AND d.id=? "
              "ORDER BY d.name, h.start_offset;"
            : "SELECT h.id, h.snippet, h.memo, d.id, d.name "
              "FROM highlights h JOIN documents d ON h.document_id=d.id "
              "WHERE d.project_id=? "
              "ORDER BY d.name, h.start_offset;";
    }

    /* We need raw sqlite3 handle here — use internal query */
    extern sqlite3 *_cuali_db_handle (void); /* forward decl hack avoided: use g_file */
    /* Instead, open our own connection for the filtered query */
    db_close ();
    /* Re-open via sqlite3 directly for the filter query */
    sqlite3 *raw_db = NULL;
    if (sqlite3_open (db_path, &raw_db) != SQLITE_OK) {
        print_error (2, "Cannot open database");
        return 2;
    }

    if (sqlite3_prepare_v2 (raw_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        print_error (2, "Query error");
        sqlite3_close (raw_db);
        return 2;
    }
    sqlite3_bind_int (stmt, 1, project_id);
    if (doc_id_filter > 0)
        sqlite3_bind_int (stmt, 2, doc_id_filter);

    printf ("[\n");
    bool first = true;
    while (sqlite3_step (stmt) == SQLITE_ROW) {
        int   hl_id  = sqlite3_column_int  (stmt, 0);
        const char *snippet = (const char *)sqlite3_column_text (stmt, 1);
        const char *memo    = (const char *)sqlite3_column_text (stmt, 2);
        int   d_id   = sqlite3_column_int  (stmt, 3);
        const char *d_name  = (const char *)sqlite3_column_text (stmt, 4);

        /* Get tags for this highlight */
        sqlite3_stmt *tag_stmt;
        const char *tag_sql =
            "SELECT t.path FROM tags t "
            "JOIN highlight_tags ht ON t.id=ht.tag_id "
            "WHERE ht.highlight_id=? ORDER BY t.path;";
        GString *tags_arr = g_string_new ("[");
        bool tag_first = true;
        if (sqlite3_prepare_v2 (raw_db, tag_sql, -1, &tag_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int (tag_stmt, 1, hl_id);
            while (sqlite3_step (tag_stmt) == SQLITE_ROW) {
                const char *tp = (const char *)sqlite3_column_text (tag_stmt, 0);
                char *jtp = json_escape (tp);
                if (!tag_first) g_string_append (tags_arr, ",");
                g_string_append (tags_arr, jtp);
                g_free (jtp);
                tag_first = false;
            }
            sqlite3_finalize (tag_stmt);
        }
        g_string_append_c (tags_arr, ']');

        char *jsnip   = json_escape (snippet);
        char *jmemo   = no_memo ? g_strdup ("null") : json_escape (memo);
        char *jdname  = json_escape (d_name);

        if (!first) printf (",\n");
        printf ("  {\"id\":%d,\"document_id\":%d,\"document\":%s,"
                "\"snippet\":%s,\"memo\":%s,\"tags\":%s}",
                hl_id, d_id, jdname, jsnip, jmemo, tags_arr->str);
        first = false;

        g_free (jsnip); g_free (jmemo); g_free (jdname);
        g_string_free (tags_arr, TRUE);
    }
    printf ("\n]\n");

    sqlite3_finalize (stmt);
    sqlite3_close (raw_db);
    return 0;
}

/*
 * highlight DB ID [--context N]
 * Show one highlight with surrounding context (N chars on each side).
 */
static int
cmd_highlight (const char *db_path, int hl_id, int context_chars)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }

    char *memo = NULL;
    db_highlight_get_memo (hl_id, &memo);

    int doc_id = -1;
    char *doc_name = NULL;
    char *contents = db_document_get_contents_by_highlight (hl_id, &doc_id, &doc_name);

    if (!contents) {
        print_error (3, "Highlight not found");
        g_free (memo);
        db_close ();
        return 3;
    }

    int start = 0, end = 0;
    db_highlight_get_offsets (hl_id, &start, &end);

    /* Tags */
    sqlite3_stmt *tag_stmt = db_tags_get_for_highlight (hl_id);
    GString *tags_arr = g_string_new ("[");
    bool tag_first = true;
    if (tag_stmt) {
        while (sqlite3_step (tag_stmt) == SQLITE_ROW) {
            const char *tp = (const char *)sqlite3_column_text (tag_stmt, 1);
            char *jtp = json_escape (tp);
            if (!tag_first) g_string_append (tags_arr, ",");
            g_string_append (tags_arr, jtp);
            g_free (jtp);
            tag_first = false;
        }
        sqlite3_finalize (tag_stmt);
    }
    g_string_append_c (tags_arr, ']');

    /* Context extraction */
    int content_len = (int)strlen (contents);
    int ctx_start = (start - context_chars > 0) ? start - context_chars : 0;
    int ctx_end   = (end + context_chars < content_len) ? end + context_chars : content_len;

    char *before = g_strndup (contents + ctx_start, start - ctx_start);
    char *snippet = g_strndup (contents + start, end - start);
    char *after   = g_strndup (contents + end,   ctx_end - end);

    char *jsnip   = json_escape (snippet);
    char *jmemo   = json_escape (memo);
    char *jdoc    = json_escape (doc_name);
    char *jbefore = json_escape (before);
    char *jafter  = json_escape (after);

    printf ("{\n"
            "  \"id\": %d,\n"
            "  \"document_id\": %d,\n"
            "  \"document\": %s,\n"
            "  \"snippet\": %s,\n"
            "  \"memo\": %s,\n"
            "  \"tags\": %s,\n"
            "  \"context\": {\n"
            "    \"before\": %s,\n"
            "    \"after\": %s,\n"
            "    \"context_chars\": %d\n"
            "  }\n"
            "}\n",
            hl_id, doc_id, jdoc, jsnip, jmemo, tags_arr->str,
            jbefore, jafter, context_chars);

    g_free (jsnip); g_free (jmemo); g_free (jdoc);
    g_free (jbefore); g_free (jafter);
    g_free (before); g_free (snippet); g_free (after);
    g_free (memo); g_free (doc_name); g_free (contents);
    g_string_free (tags_arr, TRUE);
    db_close ();
    return 0;
}

/*
 * export-book DB
 * Generates a Markdown "Libro de temas" (Codebook) for the thesis.
 */
static int
cmd_export_book (const char *db_path)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();
    if (project_id < 0) { print_error (3, "No project"); db_close (); return 3; }

    char *name = NULL, *desc = NULL;
    db_project_get_info (project_id, &name, &desc);

    printf ("# Libro de Temas: %s\n\n", name ? name : "Proyecto sin nombre");
    if (desc && *desc) printf ("%s\n\n", desc);

    db_close ();
    sqlite3 *raw_db = NULL;
    if (sqlite3_open (db_path, &raw_db) != SQLITE_OK) {
        print_error (2, "Cannot open database");
        return 2;
    }

    const char *stats_sql = 
        "SELECT t.id, t.path, t.description, COUNT(ht.highlight_id) "
        "FROM tags t "
        "LEFT JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE t.project_id = ? "
        "GROUP BY t.id "
        "ORDER BY t.path ASC;";
    
    sqlite3_stmt *t_stmt;
    if (sqlite3_prepare_v2 (raw_db, stats_sql, -1, &t_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int (t_stmt, 1, project_id);
        while (sqlite3_step (t_stmt) == SQLITE_ROW) {
            int tag_id = sqlite3_column_int (t_stmt, 0);
            const char *path = (const char *)sqlite3_column_text (t_stmt, 1);
            const char *tdesc = (const char *)sqlite3_column_text (t_stmt, 2);
            int count = sqlite3_column_int (t_stmt, 3);

            printf ("## %s\n", path);
            if (tdesc && *tdesc) printf ("**Descripción:** %s\n", tdesc);
            printf ("**Frecuencia:** %d cita%s\n\n", count, count == 1 ? "" : "s");

            if (count > 0) {
                const char *ex_sql = 
                    "SELECT h.snippet FROM highlights h "
                    "JOIN highlight_tags ht ON h.id = ht.highlight_id "
                    "WHERE ht.tag_id = ? ORDER BY RANDOM() LIMIT 2;";
                sqlite3_stmt *ex_stmt;
                if (sqlite3_prepare_v2 (raw_db, ex_sql, -1, &ex_stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int (ex_stmt, 1, tag_id);
                    bool has_examples = false;
                    while (sqlite3_step (ex_stmt) == SQLITE_ROW) {
                        const char *snip = (const char *)sqlite3_column_text (ex_stmt, 0);
                        if (snip) {
                            if (!has_examples) {
                                printf ("**Ejemplos:**\n");
                                has_examples = true;
                            }
                            printf ("> \"%s\"\n\n", snip);
                        }
                    }
                    sqlite3_finalize (ex_stmt);
                }
            }
            printf ("---\n\n");
        }
        sqlite3_finalize (t_stmt);
    }
    sqlite3_close (raw_db);
    g_free (name); g_free (desc);
    return 0;
}

/*
 * export-for-ai DB HIGHLIGHT_ID
 * Full JSON package for AI consumption.
 */
static int
cmd_export_for_ai (const char *db_path, int hl_id)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }

    int project_id = get_project_id ();
    if (project_id < 0) { print_error (3, "No project"); db_close (); return 3; }

    char *proj_name = NULL, *proj_desc = NULL;
    db_project_get_info (project_id, &proj_name, &proj_desc);

    char *memo = NULL;
    db_highlight_get_memo (hl_id, &memo);

    int doc_id = -1;
    char *doc_name = NULL;
    char *contents = db_document_get_contents_by_highlight (hl_id, &doc_id, &doc_name);
    if (!contents) {
        print_error (3, "Highlight not found");
        g_free (memo); g_free (proj_name); g_free (proj_desc);
        db_close ();
        return 3;
    }

    int start = 0, end = 0;
    db_highlight_get_offsets (hl_id, &start, &end);

    int content_len = (int)strlen (contents);
    int ctx_chars = 300;
    int ctx_start = (start - ctx_chars > 0) ? start - ctx_chars : 0;
    int ctx_end   = (end + ctx_chars < content_len) ? end + ctx_chars : content_len;
    char *before  = g_strndup (contents + ctx_start, start - ctx_start);
    char *snippet = g_strndup (contents + start, end - start);
    char *after   = g_strndup (contents + end,   ctx_end - end);

    /* Current tags */
    sqlite3_stmt *tag_stmt = db_tags_get_for_highlight (hl_id);
    GString *cur_tags = g_string_new ("[");
    bool tag_first = true;
    if (tag_stmt) {
        while (sqlite3_step (tag_stmt) == SQLITE_ROW) {
            const char *tp = (const char *)sqlite3_column_text (tag_stmt, 1);
            char *jtp = json_escape (tp);
            if (!tag_first) g_string_append (cur_tags, ",");
            g_string_append (cur_tags, jtp);
            g_free (jtp);
            tag_first = false;
        }
        sqlite3_finalize (tag_stmt);
    }
    g_string_append_c (cur_tags, ']');

    /* All project tags with counts */
    sqlite3_stmt *all_tags = db_tags_get_stats (project_id);
    GString *tags_list = g_string_new ("[");
    bool atag_first = true;
    if (all_tags) {
        while (sqlite3_step (all_tags) == SQLITE_ROW) {
            const char *tp    = (const char *)sqlite3_column_text (all_tags, 1);
            int         count = sqlite3_column_int (all_tags, 3);
            char *jtp = json_escape (tp);
            if (!atag_first) g_string_append (tags_list, ",");
            g_string_append_printf (tags_list, "{\"path\":%s,\"count\":%d}", jtp, count);
            g_free (jtp);
            atag_first = false;
        }
        sqlite3_finalize (all_tags);
    }
    g_string_append_c (tags_list, ']');

    char *jpname  = json_escape (proj_name);
    char *jpdesc  = json_escape (proj_desc);
    char *jdpath  = json_escape (db_path);
    char *jdoc    = json_escape (doc_name);
    char *jsnip   = json_escape (snippet);
    char *jmemo   = json_escape (memo);
    char *jbefore = json_escape (before);
    char *jafter  = json_escape (after);

    printf ("{\n"
            "  \"schema_version\": 1,\n"
            "  \"project\": {\n"
            "    \"name\": %s,\n"
            "    \"description\": %s,\n"
            "    \"db_path\": %s\n"
            "  },\n"
            "  \"highlight\": {\n"
            "    \"id\": %d,\n"
            "    \"snippet\": %s,\n"
            "    \"memo\": %s,\n"
            "    \"document\": %s,\n"
            "    \"document_id\": %d,\n"
            "    \"current_tags\": %s\n"
            "  },\n"
            "  \"context\": {\n"
            "    \"before\": %s,\n"
            "    \"after\": %s,\n"
            "    \"full_document_command\": \"cuali-cli doc %s %d\"\n"
            "  },\n"
            "  \"existing_tags\": %s,\n"
            "  \"tagging_rules\": {\n"
            "    \"format\": \"tema/subtema/sub-subtema/...\",\n"
            "    \"depth\": \"unlimited\",\n"
            "    \"lowercase\": true,\n"
            "    \"preserve_accents\": true,\n"
            "    \"separator\": \"/\",\n"
            "    \"spaces_within_segment\": true,\n"
            "    \"good_examples\": [\"software/musescore\",\"sostenible/gratuito\",\"percepci\\u00f3n/accesibilidad\"],\n"
            "    \"bad_examples\": [\"Software/MuseScore\",\"sostenible/Gratuito\",\"percepcion/accesibilidad\"]\n"
            "  },\n"
            "  \"available_commands\": {\n"
            "    \"tag_existing\": \"cuali-cli tag-highlight %s %d <TAG_PATH>\",\n"
            "    \"create_and_tag\": \"cuali-cli create-tag %s <TAG_PATH> --color <HEX> && cuali-cli tag-highlight %s %d <TAG_PATH>\",\n"
            "    \"append_memo\": \"cuali-cli append-memo %s %d \\\"<NOTE>\\\" --ai <MODEL_NAME>\",\n"
            "    \"skip\": \"# do nothing, move to next highlight\"\n"
            "  }\n"
            "}\n",
            jpname, jpdesc, jdpath,
            hl_id, jsnip, jmemo, jdoc, doc_id, cur_tags->str,
            jbefore, jafter, db_path, doc_id,
            tags_list->str,
            db_path, hl_id,
            db_path, db_path, hl_id,
            db_path, hl_id);

    g_free (jpname); g_free (jpdesc); g_free (jdpath);
    g_free (jdoc); g_free (jsnip); g_free (jmemo);
    g_free (jbefore); g_free (jafter);
    g_free (before); g_free (snippet); g_free (after);
    g_free (memo); g_free (doc_name); g_free (contents);
    g_free (proj_name); g_free (proj_desc);
    g_string_free (cur_tags, TRUE);
    g_string_free (tags_list, TRUE);
    db_close ();
    return 0;
}

/*
 * next DB [--untagged]
 * Return the next highlight (optionally untagged only).
 */
static int
cmd_next (const char *db_path, bool untagged_only)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();
    if (project_id < 0) { print_error (3, "No project"); db_close (); return 3; }

    sqlite3 *raw_db = NULL;
    db_close ();
    if (sqlite3_open (db_path, &raw_db) != SQLITE_OK) {
        print_error (2, "Cannot open database");
        return 2;
    }

    const char *sql = untagged_only
        ? "SELECT h.id FROM highlights h "
          "JOIN documents d ON h.document_id=d.id "
          "WHERE d.project_id=? "
          "AND h.id NOT IN (SELECT highlight_id FROM highlight_tags) "
          "ORDER BY h.id LIMIT 1;"
        : "SELECT h.id FROM highlights h "
          "JOIN documents d ON h.document_id=d.id "
          "WHERE d.project_id=? ORDER BY h.id LIMIT 1;";

    sqlite3_stmt *stmt;
    int hl_id = -1;
    if (sqlite3_prepare_v2 (raw_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int (stmt, 1, project_id);
        if (sqlite3_step (stmt) == SQLITE_ROW)
            hl_id = sqlite3_column_int (stmt, 0);
        sqlite3_finalize (stmt);
    }
    sqlite3_close (raw_db);

    if (hl_id < 0) {
        printf ("{\"done\":true,\"message\":\"No more highlights\"}\n");
        return 0;
    }

    /* Delegate to export-for-ai for the full package */
    return cmd_export_for_ai (db_path, hl_id);
}

/*
 * tag-highlight DB HIGHLIGHT_ID TAG_PATH
 * Assign an existing tag to a highlight.
 */
static int
cmd_tag_highlight (const char *db_path, int hl_id, const char *tag_path,
                   bool interactive)
{
    const char *err = validate_tag_path (tag_path);
    if (err) { print_error (4, err); return 4; }

    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();

    /* Find tag by path */
    sqlite3_stmt *stmt = db_tags_get_all (project_id);
    int tag_id = -1;
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            int   tid = sqlite3_column_int  (stmt, 0);
            const char *tp  = (const char *)sqlite3_column_text (stmt, 1);
            if (tp && strcmp (tp, tag_path) == 0) {
                tag_id = tid;
                break;
            }
        }
        sqlite3_finalize (stmt);
    }

    if (tag_id < 0) {
        char *msg = g_strdup_printf ("Tag '%s' not found. Use create-tag first.", tag_path);
        print_error (3, msg);
        g_free (msg);
        db_close ();
        return 3;
    }

    if (interactive) {
        /* Show proposal and wait for 1/0 */
        char *snippet_buf = NULL;
        int doc_id; char *doc_name;
        char *contents = db_document_get_contents_by_highlight (hl_id, &doc_id, &doc_name);
        int start = 0, end = 0;
        db_highlight_get_offsets (hl_id, &start, &end);
        if (contents) {
            int len = MIN (60, end - start);
            snippet_buf = g_strndup (contents + start, len);
            g_free (contents); g_free (doc_name);
        }
        fprintf (stderr, "\nProposal:\n");
        fprintf (stderr, "  Tag:       %s\n", tag_path);
        fprintf (stderr, "  Highlight: #%d \"%s%s\"\n",
                 hl_id, snippet_buf ? snippet_buf : "", snippet_buf && strlen(snippet_buf) >= 60 ? "..." : "");
        fprintf (stderr, "Apply? [1=yes / 0=skip]: ");
        g_free (snippet_buf);
        char line[8] = {0};
        if (fgets (line, sizeof(line), stdin) && line[0] == '0') {
            printf ("{\"ok\":false,\"skipped\":true,\"highlight_id\":%d}\n", hl_id);
            db_close ();
            return 0;
        }
    }

    bool ok = db_highlight_link_tag (hl_id, tag_id);
    if (!ok) {
        print_error (5, "Failed to link tag (already linked?)");
        db_close ();
        return 5;
    }

    char *msg = g_strdup_printf ("Tagged highlight #%d with '%s'", hl_id, tag_path);
    printf ("{\"ok\":true,\"highlight_id\":%d,\"tag_path\":\"%s\"}\n", hl_id, tag_path);
    g_free (msg);
    db_close ();
    return 0;
}

/*
 * untag-highlight DB HIGHLIGHT_ID TAG_PATH
 */
static int
cmd_untag_highlight (const char *db_path, int hl_id, const char *tag_path)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();

    sqlite3_stmt *stmt = db_tags_get_all (project_id);
    int tag_id = -1;
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            int   tid = sqlite3_column_int  (stmt, 0);
            const char *tp  = (const char *)sqlite3_column_text (stmt, 1);
            if (tp && strcmp (tp, tag_path) == 0) { tag_id = tid; break; }
        }
        sqlite3_finalize (stmt);
    }
    if (tag_id < 0) {
        print_error (3, "Tag not found");
        db_close ();
        return 3;
    }

    bool ok = db_highlight_unlink_tag (hl_id, tag_id);
    if (!ok) { print_error (5, "Failed to unlink tag"); db_close (); return 5; }

    printf ("{\"ok\":true,\"highlight_id\":%d,\"tag_path\":\"%s\",\"action\":\"untagged\"}\n",
            hl_id, tag_path);
    db_close ();
    return 0;
}

/*
 * create-highlight DB DOC_ID START END SNIPPET [MEMO]
 */
static int
cmd_create_highlight (const char *db_path, int doc_id, int start, int end,
                      const char *snippet, const char *memo)
{
    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    
    int hl_id = db_highlight_add (doc_id, start, end, snippet);
    if (hl_id < 0) {
        print_error (5, "Failed to create highlight");
        db_close ();
        return 5;
    }
    
    if (memo && *memo != '\0') {
        db_highlight_set_memo (hl_id, memo);
    }
    
    char *jsnip = json_escape (snippet);
    char *jmemo = memo ? json_escape (memo) : g_strdup ("null");
    printf ("{\"ok\":true,\"highlight_id\":%d,\"document_id\":%d,\"start_offset\":%d,\"end_offset\":%d,\"snippet\":%s,\"memo\":%s}\n",
            hl_id, doc_id, start, end, jsnip, jmemo);
    g_free (jsnip); g_free (jmemo);
    db_close ();
    return 0;
}

/*
 * create-tag DB TAG_PATH [--color HEX] [--desc "text"]
 */
static int
cmd_create_tag (const char *db_path, const char *tag_path,
                const char *color, const char *desc)
{
    const char *err = validate_tag_path (tag_path);
    if (err) { print_error (4, err); return 4; }

    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();

    /* Check not already existing */
    sqlite3_stmt *stmt = db_tags_get_all (project_id);
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            const char *tp = (const char *)sqlite3_column_text (stmt, 1);
            if (tp && strcmp (tp, tag_path) == 0) {
                sqlite3_finalize (stmt);
                print_error (4, "Tag already exists");
                db_close ();
                return 4;
            }
        }
        sqlite3_finalize (stmt);
    }

    int tag_id = db_tag_add (project_id, tag_path,
                             desc ? desc : "",
                             color ? color : "#3584e4");
    if (tag_id < 0) {
        print_error (5, "Failed to create tag");
        db_close ();
        return 5;
    }

    char *jpath  = json_escape (tag_path);
    char *jcolor = json_escape (color ? color : "#3584e4");
    char *jdesc  = json_escape (desc ? desc : "");
    printf ("{\"ok\":true,\"tag_id\":%d,\"path\":%s,\"color\":%s,\"description\":%s}\n",
            tag_id, jpath, jcolor, jdesc);
    g_free (jpath); g_free (jcolor); g_free (jdesc);
    db_close ();
    return 0;
}

/*
 * append-memo DB HIGHLIGHT_ID "text" --ai NAME
 * Appends to the existing memo WITHOUT replacing it.
 * Format: existing memo + "\n[IA:NAME] text"
 */
static int
cmd_append_memo (const char *db_path, int hl_id, const char *text,
                 const char *ai_name)
{
    if (!text || *text == '\0') {
        print_error (1, "memo text cannot be empty");
        return 1;
    }
    if (!ai_name || *ai_name == '\0') {
        print_error (1, "--ai NAME is required for memo append");
        return 1;
    }

    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }

    char *existing = NULL;
    if (!db_highlight_get_memo (hl_id, &existing)) {
        print_error (3, "Highlight not found");
        db_close ();
        return 3;
    }

    char *ai_tag     = g_strdup_printf ("[IA:%s]", ai_name);
    char *new_entry  = g_strdup_printf ("%s %s", ai_tag, text);
    char *new_memo;

    if (existing && *existing != '\0') {
        new_memo = g_strdup_printf ("%s\n%s", existing, new_entry);
    } else {
        new_memo = g_strdup (new_entry);
    }

    bool ok = db_highlight_set_memo (hl_id, new_memo);
    if (!ok) {
        print_error (5, "Failed to save memo");
        g_free (existing); g_free (ai_tag); g_free (new_entry); g_free (new_memo);
        db_close ();
        return 5;
    }

    char *jmemo = json_escape (new_memo);
    printf ("{\"ok\":true,\"highlight_id\":%d,\"memo\":%s}\n", hl_id, jmemo);
    g_free (jmemo);
    g_free (existing); g_free (ai_tag); g_free (new_entry); g_free (new_memo);
    db_close ();
    return 0;
}

/*
 * append-tag-desc DB TAG_PATH "text" --ai NAME
 * Appends an analytical note to a tag's description.
 */
static int
cmd_append_tag_desc (const char *db_path, const char *tag_path, const char *text,
                     const char *ai_name)
{
    if (!text || *text == '\0') {
        print_error (1, "description text cannot be empty");
        return 1;
    }
    if (!ai_name || *ai_name == '\0') {
        print_error (1, "--ai NAME is required");
        return 1;
    }

    if (!db_init (db_path)) { print_error (2, "Cannot open database"); return 2; }
    int project_id = get_project_id ();

    sqlite3_stmt *stmt = db_tags_get_all (project_id);
    int tag_id = -1;
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            int   tid = sqlite3_column_int  (stmt, 0);
            const char *tp  = (const char *)sqlite3_column_text (stmt, 1);
            if (tp && strcmp (tp, tag_path) == 0) { tag_id = tid; break; }
        }
        sqlite3_finalize (stmt);
    }

    if (tag_id < 0) {
        print_error (3, "Tag not found");
        db_close ();
        return 3;
    }

    char *cur_path = NULL, *cur_desc = NULL, *cur_color = NULL;
    if (!db_tag_get_info (tag_id, &cur_path, &cur_desc, &cur_color)) {
        print_error (3, "Failed to get tag info");
        db_close ();
        return 3;
    }

    char *ai_tag     = g_strdup_printf ("[IA:%s]", ai_name);
    char *new_entry  = g_strdup_printf ("%s %s", ai_tag, text);
    char *new_desc;

    if (cur_desc && *cur_desc != '\0') {
        new_desc = g_strdup_printf ("%s\n%s", cur_desc, new_entry);
    } else {
        new_desc = g_strdup (new_entry);
    }

    bool ok = db_tag_update (tag_id, cur_path, new_desc);
    if (!ok) {
        print_error (5, "Failed to update tag description");
        g_free (cur_path); g_free (cur_desc); g_free (cur_color);
        g_free (ai_tag); g_free (new_entry); g_free (new_desc);
        db_close ();
        return 5;
    }

    char *jdesc = json_escape (new_desc);
    printf ("{\"ok\":true,\"tag_path\":\"%s\",\"description\":%s}\n", tag_path, jdesc);
    
    g_free (jdesc);
    g_free (cur_path); g_free (cur_desc); g_free (cur_color);
    g_free (ai_tag); g_free (new_entry); g_free (new_desc);
    db_close ();
    return 0;
}

/* ── Usage ── */

static void
print_usage (void)
{
    fprintf (stderr,
        "cuali-cli — AI-assisted qualitative analysis tool\n"
        "\n"
        "Usage:\n"
        "  cuali-cli info       DB\n"
        "  cuali-cli tags       DB\n"
        "  cuali-cli doc        DB DOC_ID\n"
        "  cuali-cli highlights DB [--doc DOC_ID] [--untagged] [--no-memo]\n"
        "  cuali-cli highlight  DB HIGHLIGHT_ID [--context N]\n"
        "  cuali-cli next       DB [--untagged]\n"
        "  cuali-cli export-for-ai DB HIGHLIGHT_ID\n"
        "  cuali-cli export-book  DB\n"
        "\n"
        "  cuali-cli tag-highlight   DB HIGHLIGHT_ID TAG_PATH [--interactive]\n"
        "  cuali-cli untag-highlight DB HIGHLIGHT_ID TAG_PATH\n"
        "  cuali-cli create-tag      DB TAG_PATH [--color #HEX] [--desc \"text\"]\n"
        "  cuali-cli create-highlight DB DOC_ID START END \"SNIPPET\" [\"MEMO\"]\n"
        "  cuali-cli append-memo     DB HIGHLIGHT_ID \"text\" --ai MODEL_NAME\n"
        "  cuali-cli append-tag-desc DB TAG_PATH \"text\" --ai MODEL_NAME\n"
        "\n"
        "Tag format: tema/subtema/sub-subtema/... (lowercase, accents preserved)\n"
        "\n"
        "Exit codes: 0=ok 1=usage 2=db-error 3=not-found 4=validation 5=write-error\n"
    );
}

/* ── Main ── */

int
main (int argc, char *argv[])
{
    if (argc < 3) {
        print_usage ();
        return 1;
    }

    const char *cmd    = argv[1];
    const char *db_path = argv[2];

    /* ── info ── */
    if (strcmp (cmd, "info") == 0)
        return cmd_info (db_path);

    /* ── tags ── */
    if (strcmp (cmd, "tags") == 0)
        return cmd_tags (db_path);

    /* ── export-book ── */
    if (strcmp (cmd, "export-book") == 0)
        return cmd_export_book (db_path);

    /* ── doc DB ID ── */
    if (strcmp (cmd, "doc") == 0) {
        if (argc < 4) { print_usage (); return 1; }
        return cmd_doc (db_path, atoi (argv[3]));
    }

    /* ── highlights DB [--doc N] [--untagged] [--no-memo] ── */
    if (strcmp (cmd, "highlights") == 0) {
        int  doc_id_filter = -1;
        bool untagged      = false;
        bool no_memo       = false;
        for (int i = 3; i < argc; i++) {
            if (strcmp (argv[i], "--untagged") == 0) untagged = true;
            else if (strcmp (argv[i], "--no-memo") == 0) no_memo = true;
            else if (strcmp (argv[i], "--doc") == 0 && i + 1 < argc)
                doc_id_filter = atoi (argv[++i]);
        }
        return cmd_highlights (db_path, doc_id_filter, untagged, no_memo);
    }

    /* ── highlight DB ID [--context N] ── */
    if (strcmp (cmd, "highlight") == 0) {
        if (argc < 4) { print_usage (); return 1; }
        int hl_id = atoi (argv[3]);
        int ctx   = 200;
        for (int i = 4; i < argc; i++) {
            if (strcmp (argv[i], "--context") == 0 && i + 1 < argc)
                ctx = atoi (argv[++i]);
        }
        return cmd_highlight (db_path, hl_id, ctx);
    }

    /* ── next DB [--untagged] ── */
    if (strcmp (cmd, "next") == 0) {
        bool untagged = false;
        for (int i = 3; i < argc; i++)
            if (strcmp (argv[i], "--untagged") == 0) untagged = true;
        return cmd_next (db_path, untagged);
    }

    /* ── export-for-ai DB ID ── */
    if (strcmp (cmd, "export-for-ai") == 0) {
        if (argc < 4) { print_usage (); return 1; }
        return cmd_export_for_ai (db_path, atoi (argv[3]));
    }

    /* ── tag-highlight DB ID PATH [--interactive] ── */
    if (strcmp (cmd, "tag-highlight") == 0) {
        if (argc < 5) { print_usage (); return 1; }
        int  hl_id = atoi (argv[3]);
        const char *tag_path = argv[4];
        bool interactive = false;
        for (int i = 5; i < argc; i++)
            if (strcmp (argv[i], "--interactive") == 0) interactive = true;
        return cmd_tag_highlight (db_path, hl_id, tag_path, interactive);
    }

    /* ── untag-highlight DB ID PATH ── */
    if (strcmp (cmd, "untag-highlight") == 0) {
        if (argc < 5) { print_usage (); return 1; }
        return cmd_untag_highlight (db_path, atoi (argv[3]), argv[4]);
    }

    /* ── create-tag DB PATH [--color HEX] [--desc text] ── */
    if (strcmp (cmd, "create-tag") == 0) {
        if (argc < 4) { print_usage (); return 1; }
        const char *tag_path = argv[3];
        const char *color = NULL, *desc = NULL;
        for (int i = 4; i < argc; i++) {
            if (strcmp (argv[i], "--color") == 0 && i + 1 < argc) color = argv[++i];
            else if (strcmp (argv[i], "--desc") == 0 && i + 1 < argc) desc = argv[++i];
        }
        return cmd_create_tag (db_path, tag_path, color, desc);
    }

    /* ── create-highlight DB DOC_ID START END SNIPPET [MEMO] ── */
    if (strcmp (cmd, "create-highlight") == 0) {
        if (argc < 7) { print_usage (); return 1; }
        int doc_id = atoi (argv[3]);
        int start  = atoi (argv[4]);
        int end    = atoi (argv[5]);
        const char *snippet = argv[6];
        const char *memo = (argc >= 8) ? argv[7] : NULL;
        return cmd_create_highlight (db_path, doc_id, start, end, snippet, memo);
    }

    /* ── append-memo DB ID "text" --ai NAME ── */
    if (strcmp (cmd, "append-memo") == 0) {
        if (argc < 5) { print_usage (); return 1; }
        int  hl_id  = atoi (argv[3]);
        const char *text = argv[4];
        const char *ai_name = NULL;
        for (int i = 5; i < argc; i++)
            if (strcmp (argv[i], "--ai") == 0 && i + 1 < argc)
                ai_name = argv[++i];
        return cmd_append_memo (db_path, hl_id, text, ai_name);
    }

    /* ── append-tag-desc DB PATH "text" --ai NAME ── */
    if (strcmp (cmd, "append-tag-desc") == 0) {
        if (argc < 5) { print_usage (); return 1; }
        const char *tag_path = argv[3];
        const char *text = argv[4];
        const char *ai_name = NULL;
        for (int i = 5; i < argc; i++)
            if (strcmp (argv[i], "--ai") == 0 && i + 1 < argc)
                ai_name = argv[++i];
        return cmd_append_tag_desc (db_path, tag_path, text, ai_name);
    }

    fprintf (stderr, "Unknown command: %s\n", cmd);
    print_usage ();
    return 1;
}
