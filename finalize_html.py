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

def process_text(full_text, group_num, org_content):
    pattern = rf'\* Focus Group {group_num}(.*?)(?=\n\* Focus Group|\Z)'
    section_match = re.search(pattern, org_content, re.DOTALL)
    if not section_match:
        return full_text, []
    
    section = section_match.group(1)
    table_rows = re.findall(r'^\|.*\|.*\|.*\|.*\|$', section, re.MULTILINE)
    actual_rows = [r for r in table_rows if not re.match(r'^\|[-+]+\|$', r)][1:]

    # Ordenar por longitud descendente para evitar colisiones
    actual_rows.sort(key=lambda r: len(r.split('|')[2].strip()), reverse=True)

    missing = []
    found_count = 0
    
    # Marcamos el texto con placeholders para no pisar tags
    # Esto es una versión simplificada: buscamos y reemplazamos
    for row in actual_rows:
        cells = [c.strip() for c in row.split('|')][1:-1]
        if len(cells) < 2: continue
        cita = cells[1]
        tag_path = cells[2] if len(cells) > 2 else "Sin etiqueta"
        color = get_color(tag_path)
        
        words = cita.split()
        if not words: continue
        
        # Regex flexible: permite cualquier espacio/salto de línea entre palabras
        search_pattern = r'\s+'.join(re.escape(w) for w in words)
        
        match = re.search(search_pattern, full_text, re.IGNORECASE | re.MULTILINE)
        if match:
            # Envolver en mark
            tag_open = f'<mark style="background:{color}; border-radius:3px; padding:0 2px;" title="{tag_path}">'
            tag_close = '</mark>'
            
            # Solo si no estamos ya dentro de un <mark> (aproximación)
            # Reemplazamos usando el slice para ser precisos
            full_text = full_text[:match.start()] + tag_open + match.group() + tag_close + full_text[match.end():]
            found_count += 1
        else:
            # Reintento corto (primeras 5 palabras)
            if len(words) > 5:
                short_pattern = r'\s+'.join(re.escape(w) for w in words[:5])
                short_match = re.search(short_pattern, full_text, re.IGNORECASE | re.MULTILINE)
                if short_match:
                    tag_open = f'<mark style="background:{color}; border-radius:3px; padding:0 2px;" title="{tag_path} (Cita parcial)">'
                    tag_close = '</mark>'
                    full_text = full_text[:short_match.start()] + tag_open + short_match.group() + tag_close + full_text[short_match.end():]
                    found_count += 1
                    continue
            
            missing.append(f"{cells[0]} {cita[:60]}...")

    # Convertir \n a <br>
    html_body = full_text.replace('\n', '<br>\n')
    return html_body, missing

# Ejecución
with open(ORG_PATH, 'r', encoding='utf-8') as f:
    org_data = f.read()

for i in [1, 2, 3]:
    fname = f"fg{i}.txt"
    if os.path.exists(fname):
        with open(fname, 'r', encoding='utf-8') as f:
            raw_text = f.read()
        
        enriched, missing = process_text(raw_text, i, org_data)
        
        output_name = f"FG{i}_Analisis_Tematico.html"
        with open(output_name, 'w', encoding='utf-8') as f:
            f.write("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Análisis FG" + str(i) + "</title>")
            f.write("<style>body{font-family:'Segoe UI', sans-serif; line-height:1.7; color:#222; max-width:1000px; margin:40px auto; padding:40px; background:#f0f2f5;} ")
            f.write(".card{background:white; padding:50px; border-radius:12px; box-shadow:0 4px 20px rgba(0,0,0,0.08);} ")
            f.write("h1{color:#1a2b3c; border-bottom:3px solid #ffadad; display:inline-block; padding-bottom:5px;} ")
            f.write("mark{cursor:help; font-weight:500; border-bottom:1px solid rgba(0,0,0,0.1);}</style></head><body>")
            f.write("<div class='card'>")
            f.write(f"<h1>Focus Group {i}: Transcripción Etiquetada</h1><br><br>")
            f.write(enriched)
            f.write("</div></body></html>")
        
        print(f"\n--- RESULTADOS FOCUS GROUP {i} ---")
        print(f"Logrado: {len(missing)} citas no encontradas.")
        for m in missing:
            print(f"  [!] Faltó: {m}")
        print(f"Archivo generado: {output_name}")
