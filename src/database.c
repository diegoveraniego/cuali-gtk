#include "database.h"
#include <stdio.h>
#include <glib.h>

static sqlite3 *db = NULL;

bool db_init(const char *path) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }

    int rc = sqlite3_open(path, &db);
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

bool db_tag_add(int project_id, const char *path, const char *description) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO tags (project_id, path, description) VALUES (?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    
    sqlite3_bind_int(stmt, 1, project_id);
    sqlite3_bind_text(stmt, 2, path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

sqlite3_stmt* db_tags_get_all(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, path FROM tags WHERE project_id = ? ORDER BY path ASC;";
    
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

sqlite3_stmt* db_highlights_get_for_document(int document_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT start_offset, end_offset FROM highlights WHERE document_id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, document_id);
    return stmt;
}

sqlite3_stmt* db_tags_get_for_highlight(int highlight_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT t.path FROM tags t "
        "JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE ht.highlight_id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    
    sqlite3_bind_int(stmt, 1, highlight_id);
    return stmt;
}

sqlite3_stmt* db_results_get_all(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT h.snippet, d.name, GROUP_CONCAT(t.path, ', ') as tags FROM highlights h "
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
        "SELECT t.path, COUNT(ht.highlight_id) as count FROM tags t "
        "LEFT JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE t.project_id = ? "
        "GROUP BY t.id "
        "ORDER BY count DESC, t.path ASC;";
    
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
