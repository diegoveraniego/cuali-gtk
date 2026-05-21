import sqlite3

dbs = {
    "backup": "/home/diego/Descargas/2026-05-19_Tesina FOSS sostenibilidad.sqlite3.backup_user",
    "active": "/home/diego/Descargas/2026-05-19_Tesina FOSS sostenibilidad.sqlite3"
}

for name, path in dbs.items():
    print(f"\n=== Database: {name} ({path}) ===")
    conn = sqlite3.connect(path)
    c = conn.cursor()
    c.execute("SELECT name FROM sqlite_master WHERE type='table';")
    tables = [r[0] for r in c.fetchall()]
    for t in tables:
        try:
            c.execute(f"SELECT COUNT(*) FROM {t};")
            count = c.fetchone()[0]
            print(f"Table '{t}': {count} rows")
        except Exception as e:
            print(f"Table '{t}': Error: {e}")
    conn.close()
