import sqlite3
import datetime
import re
import os
import json

# Configuración de rutas
DB_PATH = "/home/diego/Descargas/Rescate_Tesis.sqlite3"
ORG_PATH = "/home/diego/org/roam/20260519012447-analisis_tematico_de_mi_tesis.org"

TAG_COLORS = {
    "Formación": "#ffadad", "Uso": "#ffd6a5", "Percepciones": "#fdffb6",
    "Funcionalidad": "#caffbf", "Ecosistema": "#9bf6ff", "Tecnología": "#a0c4ff",
    "Acceso": "#bdb2ff", "Usabilidad": "#ffc6ff", "Sostenibilidad": "#fffffc",
    "Seguridad": "#ff595e", "Pedagogía": "#1982c4", "Contexto": "#8ac926",
    "Política": "#6a4c93", "Proyección": "#ffca3a"
}

def get_tag_color(tag_path):
    prefix = tag_path.split('/')[0]
    return TAG_COLORS.get(prefix, "#cccccc")

def insert_row(cursor, table, data):
    cursor.execute(f"PRAGMA table_info({table})")
    columns_info = cursor.fetchall()
    for col in columns_info:
        name, notnull, dflt_value, pk = col[1], col[3], col[4], col[5]
        if pk == 1 and name not in data: continue
        if notnull == 1 and dflt_value is None and name not in data:
            if name == 'created': data[name] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            elif name == 'text_direction': data[name] = 'ltr'
            elif name == 'filename': data[name] = 'archivo.txt'
            elif name == 'description': data[name] = ""
            elif col[2].upper() in ('TEXT', 'VARCHAR', 'CLOB'): data[name] = ""
            elif col[2].upper() in ('INTEGER', 'REAL', 'NUMERIC'): data[name] = 0
            else: data[name] = ""
    keys = list(data.keys())
    sql = f"INSERT INTO {table} ({', '.join(keys)}) VALUES ({', '.join(['?']*len(keys))})"
    cursor.execute(sql, [data[k] for k in keys])
    return cursor.lastrowid

def find_citation_offsets(full_text, cita):
    words = cita.split()
    if not words: return None, None, None
    pattern = r'\s+'.join(re.escape(w) for w in words)
    match = re.search(pattern, full_text, re.IGNORECASE | re.MULTILINE)
    if match:
        start_char = match.start()
        end_char = match.end()
        return start_char, end_char, full_text[start_char:end_char]
    if len(words) > 5:
        short_pattern = r'\s+'.join(re.escape(w) for w in words[:5])
        match = re.search(short_pattern, full_text, re.IGNORECASE | re.MULTILINE)
        if match:
            start_char = match.start()
            end_char = match.end()
            return start_char, end_char, full_text[start_char:end_char]
    return None, None, None

def migrate():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    project_id = 1
    user_login = 'admin'
    now_str = datetime.datetime.now().isoformat()

    print(f"Limpiando datos del Proyecto {project_id}...")
    cursor.execute("DELETE FROM highlight_tags WHERE highlight_id IN (SELECT id FROM highlights WHERE document_id IN (SELECT id FROM documents WHERE project_id = ?))", (project_id,))
    cursor.execute("DELETE FROM highlights WHERE document_id IN (SELECT id FROM documents WHERE project_id = ?)", (project_id,))
    cursor.execute("DELETE FROM tags WHERE project_id = ?", (project_id,))
    # Limpiar comandos relacionados con tags y highlights para evitar duplicidad visual
    cursor.execute("DELETE FROM commands WHERE project_id = ? AND payload LIKE '%\"type\": \"highlight_%'", (project_id,))
    cursor.execute("DELETE FROM commands WHERE project_id = ? AND payload LIKE '%\"type\": \"tag_add\"%'", (project_id,))
    conn.commit()

    with open(ORG_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    groups = re.split(r'^\* (Focus Group \d+)', content, flags=re.MULTILINE)
    for i in range(1, len(groups), 2):
        group_name, group_content = groups[i], groups[i+1]
        group_num = re.search(r'\d+', group_name).group()
        doc_id = int(group_num)
        
        cursor.execute("SELECT contents FROM documents WHERE id = ? AND project_id = ?", (doc_id, project_id))
        doc_row = cursor.fetchone()
        if not doc_row:
            print(f"Error: No se encontró el documento {doc_id}.")
            continue
            
        full_text = doc_row[0]
        print(f"Procesando {group_name}...")
        
        table_rows = re.findall(r'^\|.*\|.*\|.*\|.*\|$', group_content, re.MULTILINE)
        actual_rows = [r for r in table_rows if not re.match(r'^\|[-+]+\|$', r)][1:]

        # Agrupar por offsets para manejar múltiples etiquetas por frase
        highlights_map = {}

        for row in actual_rows:
            cells = [c.strip() for c in row.split('|')][1:-1]
            if len(cells) < 2: continue
            cita, tag_path, memo = cells[1], cells[2] if len(cells) > 2 else "Sin Etiqueta", cells[3] if len(cells) > 3 else ""
            
            start, end, snippet = find_citation_offsets(full_text, cita)
            if start is None: continue

            key = (start, end)
            if key not in highlights_map:
                highlights_map[key] = {'snippet': snippet, 'tags': set(), 'memo': memo}
            highlights_map[key]['tags'].add(tag_path)
            if memo and not highlights_map[key]['memo']:
                highlights_map[key]['memo'] = memo

        for (start, end), data in highlights_map.items():
            # 1. Insertar Highlight
            hl_id = insert_row(cursor, "highlights", {
                "document_id": doc_id,
                "start_offset": start,
                "end_offset": end,
                "snippet": data['snippet'],
                "memo": data['memo']
            })

            # Comando para el highlight
            insert_row(cursor, "commands", {
                "date": now_str, "user_login": user_login, "project_id": project_id, "document_id": doc_id,
                "payload": json.dumps({"id": hl_id, "memo": data['memo'], "snippet": data['snippet'], "start_offset": start, "end_offset": end, "type": "highlight_add"})
            })

            for tag_path in data['tags']:
                # 2. Buscar o crear Tag
                cursor.execute("SELECT id FROM tags WHERE project_id = ? AND path = ?", (project_id, tag_path))
                tag_row = cursor.fetchone()
                if tag_row:
                    tag_id = tag_row[0]
                else:
                    tag_id = insert_row(cursor, "tags", {
                        "project_id": project_id, "path": tag_path, "description": "", "color": get_tag_color(tag_path)
                    })
                    # Comando para el tag
                    insert_row(cursor, "commands", {
                        "date": now_str, "user_login": user_login, "project_id": project_id,
                        "payload": json.dumps({"description": "", "tag_id": tag_id, "tag_path": tag_path, "type": "tag_add"})
                    })
                
                # 3. Vincular Tag
                cursor.execute("INSERT INTO highlight_tags (highlight_id, tag_id) VALUES (?, ?)", (hl_id, tag_id))
                # Comando para vincular
                insert_row(cursor, "commands", {
                    "date": now_str, "user_login": user_login, "project_id": project_id, "document_id": doc_id,
                    "payload": json.dumps({"highlight_id": hl_id, "tag_id": tag_id, "type": "highlight_tag_add"})
                })

        print(f"  -> {len(highlights_map)} frases procesadas.")

    conn.commit()
    conn.close()
    print("Migración completada. Reinicia Cuali para ver los cambios.")

if __name__ == "__main__":
    migrate()
