import sqlite3

backup_db = "/home/diego/Descargas/2026-05-19_Tesina FOSS sostenibilidad.sqlite3.backup_user"
active_db = "/home/diego/Descargas/2026-05-19_Tesina FOSS sostenibilidad.sqlite3"

conn = sqlite3.connect(backup_db)
c = conn.cursor()
c.execute("SELECT * FROM highlights;")
rows = c.fetchall()
print("Backup Highlights rows:")
for r in rows[:5]:
    print(r)
print("Backup Highlights count:", len(rows))
conn.close()

conn2 = sqlite3.connect(active_db)
c2 = conn2.cursor()
c2.execute("SELECT * FROM highlights;")
rows2 = c2.fetchall()
print("Active Highlights count:", len(rows2))
conn2.close()
