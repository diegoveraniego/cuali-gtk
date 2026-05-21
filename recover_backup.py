import sqlite3

backup_db = "/home/diego/Descargas/2026-05-19_Tesina FOSS sostenibilidad.sqlite3.backup_user"
active_db = "/home/diego/Descargas/2026-05-19_Tesina FOSS sostenibilidad.sqlite3"

print("Starting recovery from backup...")
conn = sqlite3.connect(active_db)
cursor = conn.cursor()

# Attach the backup database
cursor.execute(f"ATTACH DATABASE '{backup_db}' AS backup;")

# 1. Show highlights count in both
cursor.execute("SELECT COUNT(*) FROM backup.highlights;")
backup_hl_count = cursor.fetchone()[0]
print(f"Highlights in backup database: {backup_hl_count}")

cursor.execute("SELECT COUNT(*) FROM highlights;")
active_hl_count = cursor.fetchone()[0]
print(f"Highlights in active database (before): {active_hl_count}")

# 2. Sync tags (insert any tags from backup that are missing in active)
cursor.execute("""
INSERT OR IGNORE INTO tags (id, project_id, path, description, color)
SELECT id, project_id, path, description, color FROM backup.tags;
""")
print("Synchronized tags from backup.")

# 3. Clear existing highlights in active DB
cursor.execute("DELETE FROM highlight_tags;")
cursor.execute("DELETE FROM highlights;")
print("Cleared old highlights in active database.")

# 4. Copy highlights from backup
cursor.execute("""
INSERT INTO highlights (id, document_id, start_offset, end_offset, snippet, memo)
SELECT id, document_id, start_offset, end_offset, snippet, memo FROM backup.highlights;
""")
print("Recovered highlights.")

# 5. Copy highlight_tags from backup
cursor.execute("""
INSERT INTO highlight_tags (highlight_id, tag_id)
SELECT highlight_id, tag_id FROM backup.highlight_tags;
""")
print("Recovered highlight mappings.")

# Verify after copy
cursor.execute("SELECT COUNT(*) FROM highlights;")
active_hl_count_after = cursor.fetchone()[0]
print(f"Highlights in active database (after): {active_hl_count_after}")

conn.commit()
conn.close()
print("Recovery completed successfully!")
