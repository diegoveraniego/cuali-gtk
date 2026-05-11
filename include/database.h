#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <stdbool.h>

bool db_init(const char *path);
void db_close(void);

// Project functions
bool db_project_add(const char *name, const char *description);
int db_project_get_first_id(void);

// Document functions
int db_document_add(int project_id, const char *name, const char *contents);
sqlite3_stmt* db_documents_get_all(int project_id);
bool db_document_delete(int document_id);
bool db_document_update_contents(int document_id, const char *contents);

// Tag functions
bool db_tag_add(int project_id, const char *path, const char *description);
sqlite3_stmt* db_tags_get_all(int project_id);
bool db_tag_delete(int tag_id);

// Highlight functions
int db_highlight_add(int document_id, int start, int end, const char *snippet);
bool db_highlight_link_tag(int highlight_id, int tag_id);
sqlite3_stmt* db_highlights_get_for_document(int document_id);
sqlite3_stmt* db_tags_get_for_highlight(int highlight_id);
sqlite3_stmt* db_results_get_all(int project_id);
sqlite3_stmt* db_tags_get_stats(int project_id);
int db_tag_get_count(int tag_id);
bool db_project_get_info(int project_id, char **name, char **description);
bool db_project_update_info(int project_id, const char *name, const char *description);

#endif
