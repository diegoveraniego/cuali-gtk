#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <stdbool.h>

bool db_init(const char *path);
void db_close(void);
const char *db_get_path(void);

bool db_project_add(const char *name, const char *description);
int db_project_get_first_id(void);
bool db_project_get_info(int project_id, char **name, char **description);
bool db_project_update_info(int project_id, const char *name, const char *description);

int db_document_add(int project_id, const char *name, const char *contents);
bool db_document_update_contents(int document_id, const char *contents);
bool db_document_delete(int document_id);
sqlite3_stmt* db_documents_get_all(int project_id);

bool db_tag_add(int project_id, const char *path, const char *description, const char *color);
bool db_tag_update(int tag_id, const char *path, const char *description);
bool db_tag_update_color(int tag_id, const char *color);
bool db_tag_get_info(int tag_id, char **path, char **description, char **color);
bool db_tag_delete(int tag_id);
sqlite3_stmt* db_tags_get_all(int project_id);
sqlite3_stmt* db_tags_get_for_highlight(int highlight_id);
sqlite3_stmt* db_tags_get_stats(int project_id);
int db_tag_get_count(int tag_id);

int db_highlight_add(int document_id, int start, int end, const char *snippet);
bool db_highlight_set_memo(int highlight_id, const char *memo);
bool db_highlight_get_memo(int highlight_id, char **memo);
bool db_highlight_link_tag(int highlight_id, int tag_id);
bool db_highlight_unlink_tag(int highlight_id, int tag_id);
bool db_highlight_delete(int highlight_id);
bool db_highlights_shift_offsets(int document_id, int from_offset, int delta);
bool db_highlight_get_offsets(int highlight_id, int *start, int *end);
sqlite3_stmt* db_highlights_get_for_document(int document_id);

sqlite3_stmt* db_results_get_all(int project_id);

bool db_is_open(void);

#endif
