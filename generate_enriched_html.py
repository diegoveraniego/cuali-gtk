import re
import os

# Configuración de archivos
ORG_PATH = "/home/diego/org/roam/20260519012447-analisis_tematico_de_mi_tesis.org"
TAG_COLORS = {
    "Formación": "#ffadad", "Uso": "#ffd6a5", "Percepciones": "#fdffb6",
    "Funcionalidad": "#caffbf", "Ecosistema": "#9bf6ff", "Tecnología": "#a0c4ff",
    "Acceso": "#bdb2ff", "Usabilidad": "#ffc6ff", "Sostenibilidad": "#fffffc",
    "Seguridad": "#ff595e", "Pedagogía": "#1982c4", "Contexto": "#8ac926",
    "Política": "#6a4c93", "Proyección": "#ffca3a"
}

def get_color(tag_path):
    prefix = tag_path.split('/')[0]
    return TAG_COLORS.get(prefix, "#cccccc")

def apply_html_tags(transcript_path, group_num):
    if not os.path.exists(transcript_path):
        return None
    
    with open(transcript_path, 'r', encoding='utf-8') as f:
        full_text = f.read()
    
    with open(ORG_PATH, 'r', encoding='utf-8') as f:
        org_content = f.read()

    # Extraer la sección del Org para este Focus Group
    pattern = rf'\* Focus Group {group_num}(.*?)(?=\n\* Focus Group|\Z)'
    section_match = re.search(pattern, org_content, re.DOTALL)
    if not section_match:
        return full_text
    
    section = section_match.group(1)
    table_rows = re.findall(r'^\|.*\|.*\|.*\|.*\|$', section, re.MULTILINE)
    actual_rows = [r for r in table_rows if not re.match(r'^\|[-+]+\|$', r)][1:]

    # Invertir el texto para no romper los índices al insertar tags (o usar reemplazo inteligente)
    # Vamos a usar reemplazo por fragmentos únicos para evitar el problema de los índices
    
    for row in actual_rows:
        cells = [c.strip() for c in row.split('|')][1:-1]
        if len(cells) < 3: continue
        cita = cells[1]
        tag_path = cells[2]
        color = get_color(tag_path)
        
        # Búsqueda flexible de la cita
        words = cita.split()
        if not words: continue
        search_pattern = r'\s+'.join(re.escape(w) for w in words)
        
        # Envolver en <mark>
        # Nota: Esto puede solaparse si no tenemos cuidado. 
        # Para esta versión, usaremos una sustitución simple.
        replacement = f'<mark style="background:{color}; border-radius:3px; padding:0 2px;" title="{tag_path}">{cita}</mark>'
        
        # Solo reemplazamos la primera ocurrencia que no esté ya dentro de un <mark>
        # (Aproximación simple para evitar recursión infinita)
        if cita in full_text and f'">{cita}</mark>' not in full_text:
            full_text = re.sub(search_pattern, replacement, full_text, count=1, flags=re.IGNORECASE)

    # Convertir saltos de línea a <br> o <p> para que Cuali lo renderice bonito
    html_ready = full_text.replace('\n', '<br>\n')
    return html_ready

for i in [1, 2, 3]:
    print(f"Generando HTML enriquecido para FG{i}...")
    html = apply_html_tags(f"fg{i}.txt", i)
    if html:
        with open(f"FG{i}_Enriquecido.html", "w", encoding='utf-8') as f:
            f.write(f"<html><body style='font-family:sans-serif; line-height:1.6; padding:20px;'>\n")
            f.write(f"<h1>Focus Group {i} - Análisis Temático</h1>\n")
            f.write(html)
            f.write("\n</body></html>")
    print(f"Archivo FG{i}_Enriquecido.html creado.")
