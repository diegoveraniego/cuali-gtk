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
    const char *sql = "SELECT id, name, contents FROM documents WHERE project_id = ? ORDER BY name ASC;";
    
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
        "SELECT h.snippet, d.name, GROUP_CONCAT(t.path || '|||' || COALESCE(t.color, '#77767b'), '@@@') as tags FROM highlights h "
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
        "SELECT t.id, t.path, t.color, COUNT(ht.highlight_id) as highlight_count "
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
