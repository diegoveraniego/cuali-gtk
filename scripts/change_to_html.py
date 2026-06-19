import sys

# 1. Update exporter.h
with open('include/exporter.h', 'r') as f:
    content = f.read()

content = content.replace("export_thematic_table_csv", "export_thematic_table_html")

with open('include/exporter.h', 'w') as f:
    f.write(content)

# 2. Update exporter.c
with open('src/exporter.c', 'r') as f:
    content = f.read()

# Replace function name
content = content.replace("export_thematic_table_csv", "export_thematic_table_html")

# Find the loop where CSV is printed and replace with HTML
old_print_loop = """    fprintf (f, "Tema Principal,Códigos Emergentes,Frecuencia (Citas),Descripción Analítica\\n");"""
new_print_loop = """    fprintf(f, "<!DOCTYPE html><html><head><meta charset=\\"UTF-8\\"></head><body>\\n");
    fprintf(f, "<table border=\\"1\\" style=\\"border-collapse: collapse; width: 100%%; font-family: sans-serif;\\">\\n");
    fprintf(f, "<thead><tr><th>Tema Principal</th><th>Códigos Emergentes</th><th>Frecuencia</th><th>Descripción Analítica</th></tr></thead>\\n");
    fprintf(f, "<tbody>\\n");"""
if old_print_loop in content:
    content = content.replace(old_print_loop, new_print_loop)

old_row_print = """    for (guint i = 0; i < ordered_themes->len; i++) {
        ThematicRow *row = g_ptr_array_index(ordered_themes, i);
        char *e_theme = csv_escape(row->theme_name);
        char *e_codes = csv_escape(row->codes->str);
        char *e_desc = csv_escape(row->description);
        fprintf(f, "%s,%s,%d,%s\\n", e_theme, e_codes, row->frequency, e_desc);
        g_free(e_theme); g_free(e_codes); g_free(e_desc);
    }"""
new_row_print = """    for (guint i = 0; i < ordered_themes->len; i++) {
        ThematicRow *row = g_ptr_array_index(ordered_themes, i);
        fprintf(f, "<tr><td style=\\"padding: 8px;\\">%s</td><td style=\\"padding: 8px;\\">%s</td><td style=\\"padding: 8px; text-align: center;\\">%d</td><td style=\\"padding: 8px;\\">%s</td></tr>\\n", 
                row->theme_name ? row->theme_name : "", 
                row->codes ? row->codes->str : "", 
                row->frequency, 
                row->description ? row->description : "");
    }
    fprintf(f, "</tbody></table></body></html>\\n");"""
if old_row_print in content:
    content = content.replace(old_row_print, new_row_print)

with open('src/exporter.c', 'w') as f:
    f.write(content)

# 3. Update window.c
with open('src/window.c', 'r') as f:
    content = f.read()

content = content.replace("export_thematic_table_csv", "export_thematic_table_html")
content = content.replace("thematic_table.csv", "thematic_table.html")
content = content.replace("Export thematic table (CSV)", "Export thematic table (HTML)")

with open('src/window.c', 'w') as f:
    f.write(content)

print("Changed to HTML")
