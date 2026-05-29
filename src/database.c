#include "database.h"
#include <stdio.h>
#include <glib.h>

static sqlite3 *db = NULL;
static char *current_db_path = NULL;

bool db_init(const char *path) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }

    int rc = sqlite3_open(path, &db);
    if (rc == SQLITE_OK) {
        g_free(current_db_path);
        current_db_path = g_strdup(path);
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return false;
    }

    const char *sql = 
        "CREATE TABLE IF NOT EXISTS projects ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  description TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS documents ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  project_id INTEGER NOT NULL,"
        "  name TEXT NOT NULL,"
        "  contents TEXT,"
        "  FOREIGN KEY(project_id) REFERENCES projects(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS tags ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  project_id INTEGER NOT NULL,"
        "  path TEXT NOT NULL,"
        "  description TEXT,"
        "  FOREIGN KEY(project_id) REFERENCES projects(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS highlights ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  document_id INTEGER NOT NULL,"
        "  start_offset INTEGER NOT NULL,"
        "  end_offset INTEGER NOT NULL,"
        "  snippet TEXT,"
        "  FOREIGN KEY(document_id) REFERENCES documents(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS highlight_tags ("
        "  highlight_id INTEGER NOT NULL,"
        "  tag_id INTEGER NOT NULL,"
        "  PRIMARY KEY(highlight_id, tag_id),"
        "  FOREIGN KEY(highlight_id) REFERENCES highlights(id),"
        "  FOREIGN KEY(tag_id) REFERENCES tags(id)"
        ");";

    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    const char *idx_sql =
        "CREATE INDEX IF NOT EXISTS idx_documents_project    ON documents(project_id);"
        "CREATE INDEX IF NOT EXISTS idx_highlights_document  ON highlights(document_id);"
        "CREATE INDEX IF NOT EXISTS idx_highlight_tags_hl    ON highlight_tags(highlight_id);"
        "CREATE INDEX IF NOT EXISTS idx_highlight_tags_tag   ON highlight_tags(tag_id);"
        "CREATE INDEX IF NOT EXISTS idx_tags_project         ON tags(project_id);";

    rc = sqlite3_exec(db, idx_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error creating indexes: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }

    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA cache_size=-32000;", NULL, NULL, NULL); /* 32 MB */
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);

    /* Migración: añadir color a tags si no existe */
    sqlite3_stmt *check_col;
    bool has_color = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM pragma_table_info('tags') WHERE name='color';", -1, &check_col, NULL) == SQLITE_OK) {
        has_color = (sqlite3_step(check_col) == SQLITE_ROW);
        sqlite3_finalize(check_col);
    }
    if (!has_color) {
        sqlite3_exec(db, "ALTER TABLE tags ADD COLUMN color TEXT;", NULL, NULL, NULL);
    }

    /* Migration: add memo to highlights if missing */
    sqlite3_stmt *check_memo;
    bool has_memo = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM pragma_table_info('highlights') WHERE name='memo';", -1, &check_memo, NULL) == SQLITE_OK) {
        has_memo = (sqlite3_step(check_memo) == SQLITE_ROW);
        sqlite3_finalize(check_memo);
    }
    if (!has_memo) {
        sqlite3_exec(db, "ALTER TABLE highlights ADD COLUMN memo TEXT;", NULL, NULL, NULL);
    }

    return true;
}

void db_close(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

bool db_project_add(const char *name, const char *description) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO projects (name, description) VALUES (?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, description, -1, SQLITE_STATIC);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

int db_project_get_first_id(void) {
    sqlite3_stmt *stmt;
    int id = -1;
    if (sqlite3_prepare_v2(db, "SELECT id FROM projects LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return id;
}

int db_document_add(int project_id, const char *name, const char *contents) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO documents (project_id, name, contents) VALUES (?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, project_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, contents, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int id = (int)sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        return id;
    }
    
    sqlite3_finalize(stmt);
    return -1;
}

bool db_document_update_contents(int document_id, const char *contents) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE documents SET contents = ? WHERE id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, contents, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, document_id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

int db_tag_add(int project_id, const char *path, const char *description, const char *color) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO tags (project_id, path, description, color) VALUES (?, ?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, project_id);
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, color, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int id = (int)sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        return id;
    }
    
    sqlite3_finalize(stmt);
    return -1;
}

bool db_tag_get_info(int tag_id, char **path, char **description, char **color) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT path, description, color FROM tags WHERE id = ?;";
    bool found = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, tag_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (path)        *path        = g_strdup((const char *)sqlite3_column_text(stmt, 0));
            if (description) *description = g_strdup((const char *)sqlite3_column_text(stmt, 1));
            if (color)       *color       = g_strdup((const char *)sqlite3_column_text(stmt, 2));
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

bool db_tag_update(int tag_id, const char *path, const char *description) {
    // 1. Fetch original path and project_id to check for changes and cascade
    char *orig_path = NULL;
    int project_id = -1;
    sqlite3_stmt *select_stmt;
    const char *select_sql = "SELECT path, project_id FROM tags WHERE id = ?;";
    if (sqlite3_prepare_v2(db, select_sql, -1, &select_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(select_stmt, 1, tag_id);
        if (sqlite3_step(select_stmt) == SQLITE_ROW) {
            orig_path = g_strdup((const char *)sqlite3_column_text(select_stmt, 0));
            project_id = sqlite3_column_int(select_stmt, 1);
        }
        sqlite3_finalize(select_stmt);
    }

    if (orig_path && strcmp(orig_path, path) != 0) {
        // 2. Cascade rename all descendant tags (e.g. parent/child -> newparent/child)
        sqlite3_stmt *cascade_stmt;
        const char *cascade_sql = "UPDATE tags SET path = ? || SUBSTR(path, ?) WHERE project_id = ? AND path LIKE ?;";
        if (sqlite3_prepare_v2(db, cascade_sql, -1, &cascade_stmt, NULL) == SQLITE_OK) {
            char *like_pattern = g_strdup_printf("%s/%%", orig_path);
            int substr_start = (int)strlen(orig_path) + 1; // 1-based index in SQLite SUBSTR
            
            sqlite3_bind_text(cascade_stmt, 1, path, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(cascade_stmt, 2, substr_start);
            sqlite3_bind_int(cascade_stmt, 3, project_id);
            sqlite3_bind_text(cascade_stmt, 4, like_pattern, -1, SQLITE_TRANSIENT);
            
            sqlite3_step(cascade_stmt);
            sqlite3_finalize(cascade_stmt);
            g_free(like_pattern);
        }
    }
    g_free(orig_path);

    // 3. Update the tag itself
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE tags SET path = ?, description = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, tag_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}


bool db_tag_update_color(int tag_id, const char *color) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE tags SET color = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, color, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, tag_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

sqlite3_stmt* db_tags_get_all(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, path, color FROM tags WHERE project_id = ? ORDER BY path ASC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

int db_highlight_add(int document_id, int start, int end, const char *snippet) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO highlights (document_id, start_offset, end_offset, snippet) VALUES (?, ?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, document_id);
    sqlite3_bind_int(stmt, 2, start);
    sqlite3_bind_int(stmt, 3, end);
    sqlite3_bind_text(stmt, 4, snippet, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int id = (int)sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        return id;
    }
    
    sqlite3_finalize(stmt);
    return -1;
}

bool db_highlight_set_memo(int highlight_id, const char *memo) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE highlights SET memo = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, memo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, highlight_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool db_highlight_get_memo(int highlight_id, char **memo) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT memo FROM highlights WHERE id = ?;";
    bool found = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, highlight_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *m = (const char *)sqlite3_column_text(stmt, 0);
            if (memo) *memo = m ? g_strdup(m) : g_strdup("");
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

bool db_highlight_link_tag(int highlight_id, int tag_id) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO highlight_tags (highlight_id, tag_id) VALUES (?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt, 1, highlight_id);
    sqlite3_bind_int(stmt, 2, tag_id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool db_highlight_unlink_tag(int highlight_id, int tag_id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM highlight_tags WHERE highlight_id = ? AND tag_id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt, 1, highlight_id);
    sqlite3_bind_int(stmt, 2, tag_id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool db_highlight_get_offsets(int highlight_id, int *start, int *end) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT start_offset, end_offset FROM highlights WHERE id = ?;";
    bool found = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, highlight_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (start) *start = sqlite3_column_int(stmt, 0);
            if (end)   *end   = sqlite3_column_int(stmt, 1);
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

bool db_highlights_shift_offsets(int document_id, int from_offset, int delta) {
    if (!db) return false;
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE highlights "
                      "SET start_offset = start_offset + ?, "
                      "    end_offset = end_offset + ? "
                      "WHERE document_id = ? AND start_offset >= ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement for shifting offsets: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int(stmt, 1, delta);
    sqlite3_bind_int(stmt, 2, delta);
    sqlite3_bind_int(stmt, 3, document_id);
    sqlite3_bind_int(stmt, 4, from_offset);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        fprintf(stderr, "Failed to execute statement for shifting offsets: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    // Also update highlights that contain the edit
    const char *sql_contain = "UPDATE highlights "
                              "SET end_offset = end_offset + ? "
                              "WHERE document_id = ? AND start_offset < ? AND end_offset >= ?;";
    
    if (sqlite3_prepare_v2(db, sql_contain, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement for shifting contained offsets: %s\n", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int(stmt, 1, delta);
    sqlite3_bind_int(stmt, 2, document_id);
    sqlite3_bind_int(stmt, 3, from_offset);
    sqlite3_bind_int(stmt, 4, from_offset);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
         fprintf(stderr, "Failed to execute statement for shifting contained offsets: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    return success;
}

bool db_highlight_delete(int highlight_id) {
    if (!db) return false;
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    sqlite3_stmt *stmt;
    const char *sql1 = "DELETE FROM highlight_tags WHERE highlight_id = ?;";
    if (sqlite3_prepare_v2(db, sql1, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, highlight_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char *sql2 = "DELETE FROM highlights WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, highlight_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    sqlite3_exec(db, ok ? "COMMIT;" : "ROLLBACK;", NULL, NULL, NULL);
    return ok;
}

sqlite3_stmt* db_documents_get_all(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name FROM documents WHERE project_id = ? ORDER BY name ASC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

bool db_is_open(void) {
    return db != NULL;
}

bool db_document_delete(int document_id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM documents WHERE id = ?;";
    
    // First delete associated highlights
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    
    // Delete highlight_tags for highlights of this document
    const char *sql_ht = "DELETE FROM highlight_tags WHERE highlight_id IN (SELECT id FROM highlights WHERE document_id = ?);";
    if (sqlite3_prepare_v2(db, sql_ht, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, document_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Delete highlights
    const char *sql_h = "DELETE FROM highlights WHERE document_id = ?;";
    if (sqlite3_prepare_v2(db, sql_h, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, document_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, document_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return success;
}

bool db_tag_delete(int tag_id) {
    sqlite3_stmt *stmt;
    
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    
    // Delete links
    const char *sql_ht = "DELETE FROM highlight_tags WHERE tag_id = ?;";
    if (sqlite3_prepare_v2(db, sql_ht, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, tag_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    const char *sql = "DELETE FROM tags WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, tag_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return success;
}

bool db_project_clear_tags(int project_id) {
    if (!db) return false;
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    sqlite3_stmt *stmt;
    
    const char *sql_ht = "DELETE FROM highlight_tags WHERE tag_id IN (SELECT id FROM tags WHERE project_id = ?);";
    if (sqlite3_prepare_v2(db, sql_ht, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, project_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    const char *sql_h = "DELETE FROM highlights WHERE document_id IN (SELECT id FROM documents WHERE project_id = ?);";
    if (sqlite3_prepare_v2(db, sql_h, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, project_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char *sql_t = "DELETE FROM tags WHERE project_id = ?;";
    if (sqlite3_prepare_v2(db, sql_t, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, project_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

bool db_project_clear_data(int project_id) {
    if (!db) return false;
    db_project_clear_tags(project_id);
    
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    sqlite3_stmt *stmt;
    
    const char *sql_d = "DELETE FROM documents WHERE project_id = ?;";
    if (sqlite3_prepare_v2(db, sql_d, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, project_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

sqlite3_stmt* db_highlights_get_for_document(int document_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT start_offset, end_offset, id FROM highlights WHERE document_id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, document_id);
    return stmt;
}

sqlite3_stmt* db_tags_get_for_highlight(int highlight_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT t.id, t.path, t.color "
        "FROM tags t "
        "JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE ht.highlight_id = ? "
        "ORDER BY t.path ASC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, highlight_id);
    return stmt;
}

sqlite3_stmt* db_results_get_all(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT h.snippet, d.name, GROUP_CONCAT(t.path || '|||' || COALESCE(t.color, '#77767b'), '@@@') as tags, h.id, h.memo FROM highlights h "
        "JOIN documents d ON h.document_id = d.id "
        "JOIN highlight_tags ht ON h.id = ht.highlight_id "
        "JOIN tags t ON ht.tag_id = t.id "
        "WHERE d.project_id = ? "
        "GROUP BY h.id "
        "ORDER BY d.name ASC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

sqlite3_stmt* db_tags_get_stats(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT t.id, t.path, t.color, COUNT(ht.highlight_id) as highlight_count, t.description "
        "FROM tags t "
        "LEFT JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE t.project_id = ? "
        "GROUP BY t.id "
        "ORDER BY t.path ASC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

int db_tag_get_count(int tag_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM highlight_tags WHERE tag_id = ?;";
    int count = 0;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, tag_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

bool db_project_get_info(int project_id, char **name, char **description) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name, description FROM projects WHERE id = ?;";
    bool found = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, project_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (name) *name = g_strdup((const char *)sqlite3_column_text(stmt, 0));
            if (description) *description = g_strdup((const char *)sqlite3_column_text(stmt, 1));
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

bool db_project_update_info(int project_id, const char *name, const char *description) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE projects SET name = ?, description = ? WHERE id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, project_id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

const char *db_get_path(void) {
    return current_db_path;
}

sqlite3_stmt* db_tags_get_cooccurrence(int project_id) {
    if (!db) return NULL;
    const char *sql = 
        "SELECT a.tag_id, b.tag_id, COUNT(*) as weight "
        "FROM highlight_tags a "
        "JOIN highlight_tags b ON a.highlight_id = b.highlight_id AND a.tag_id < b.tag_id "
        "JOIN tags ta ON a.tag_id = ta.id "
        "JOIN tags tb ON b.tag_id = tb.id "
        "WHERE ta.project_id = ? "
        "GROUP BY a.tag_id, b.tag_id;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare cooccurrence statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

bool db_highlight_update_bounds(int highlight_id, int start, int end, const char *snippet) {
    if (!db) return false;
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE highlights SET start_offset = ?, end_offset = ?, snippet = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare highlight bounds update: %s\n", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int(stmt, 1, start);
    sqlite3_bind_int(stmt, 2, end);
    sqlite3_bind_text(stmt, 3, snippet, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, highlight_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

char* db_document_get_contents_by_highlight(int highlight_id, int *document_id, char **doc_name) {
    if (!db) return NULL;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT d.id, d.name, d.contents FROM documents d JOIN highlights h ON h.document_id = d.id WHERE h.id = ?;";
    char *contents = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, highlight_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (document_id) *document_id = sqlite3_column_int(stmt, 0);
            if (doc_name) *doc_name = g_strdup((const char*)sqlite3_column_text(stmt, 1));
            contents = g_strdup((const char*)sqlite3_column_text(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }
    return contents;
}

char* db_highlight_get_first_tag_color(int highlight_id) {
    if (!db) return g_strdup("#3584e4");
    sqlite3_stmt *stmt;
    const char *sql = "SELECT t.color FROM tags t JOIN highlight_tags ht ON t.id = ht.tag_id WHERE ht.highlight_id = ? LIMIT 1;";
    char *color = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, highlight_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            color = g_strdup((const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    if (!color) color = g_strdup("#3584e4");
    return color;
}

char* db_document_get_contents(int document_id) {
    if (!db) return NULL;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT contents FROM documents WHERE id = ?;";
    char *contents = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, document_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            contents = g_strdup((const char*)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
    }
    return contents;
}

sqlite3_stmt* db_highlight_colors_for_document(int document_id) {
    if (!db) return NULL;
    const char *sql =
        "SELECT h.id, t.color "
        "FROM highlights h "
        "JOIN highlight_tags ht ON h.id = ht.highlight_id "
        "JOIN tags t ON ht.tag_id = t.id "
        "WHERE h.document_id = ? "
        "GROUP BY h.id;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, document_id);
    return stmt;
}


sqlite3_stmt* db_get_theme_unique_frequencies(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT substr(t.path, 1, instr(t.path, '/') - 1) as tema, COUNT(DISTINCT ht.highlight_id) as freq "
        "FROM tags t "
        "JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE t.project_id = ? AND instr(t.path, '/') > 0 "
        "GROUP BY tema;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

sqlite3_stmt* db_tags_get_matrix(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT h.document_id, ht.tag_id, COUNT(DISTINCT h.id) as freq "
        "FROM highlights h "
        "JOIN highlight_tags ht ON h.id = ht.highlight_id "
        "JOIN documents d ON h.document_id = d.id "
        "WHERE d.project_id = ? "
        "GROUP BY h.document_id, ht.tag_id;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}

sqlite3_stmt* db_documents_get_all_contents(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT contents FROM documents WHERE project_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}
