#!/usr/bin/env python3
"""
Análisis temático FG1 — citas exactas verificadas del corpus real.
"""
import sqlite3, re, os

DB = os.path.expanduser("~/Descargas/FOSS1.sqlite3")

def html_to_plain(html):
    text = re.sub(r'</p>', '\n', html, flags=re.IGNORECASE)
    text = re.sub(r'<br[^>]*>', '\n', text, flags=re.IGNORECASE)
    text = re.sub(r'<[^>]+>', '', text)
    text = text.replace('&lt;','<').replace('&gt;','>').replace('&amp;','&')
    text = re.sub(r'\n{2,}', '\n', text)
    return text

def find_bytes(plain, fragment):
    idx = plain.find(fragment)
    if idx == -1:
        return None, None
    sb = len(plain[:idx].encode('utf-8'))
    eb = sb + len(fragment.encode('utf-8'))
    return sb, eb

conn = sqlite3.connect(DB)
cur  = conn.cursor()
cur.execute("PRAGMA foreign_keys = OFF;")

cur.execute("SELECT id, path FROM tags WHERE project_id=1")
T = {row[1]: row[0] for row in cur.fetchall()}

def ensure_tag(path, desc, color):
    if path not in T:
        cur.execute("INSERT INTO tags (project_id,path,description,color) VALUES (1,?,?,?)",
                    (path, desc, color))
        T[path] = cur.lastrowid
        conn.commit()

ensure_tag("actitud/proyección profesional",
           "Anticipación del impacto del software en la vida laboral futura",
           "#9f1239")
ensure_tag("aprendizaje/autodidacta",
           "Aprendizaje por cuenta propia sin guía formal",
           "#1e40af")
ensure_tag("contexto/brecha curricular",
           "Distancia entre lo que enseña la universidad y lo que requiere la práctica docente real",
           "#6b21a8")

cur.execute("SELECT id, contents FROM documents WHERE id=1")
row = cur.fetchone()
DOC1 = html_to_plain(row[1])

def tag(p): return T.get(p)

# ── FG1 highlights con citas EXACTAS verificadas ──────────────────────────────
HLS = [

# APRENDIZAJE / ACCESO CURRICULAR
("yo partí usando Musescore desde primero",
 ["software/Musescore", "aprendizaje/inicio tardío"]),

("cuando llegué a la universidad me tuve que cambiar al Musescore porque era el que todos usaban",
 ["software/Musescore", "aprendizaje/competencia digital", "aprendizaje/curricular"]),

("el Musescore igual tiene sus limitaciones comparado con el Finale o con el\nSibelius",
 ["evaluación/funcionalidad", "evaluación/tipos de software"]),

("el Finale es de pago y el Musescore es gratuito",
 ["acceso/software gratuito", "acceso/software de pago"]),

("la universidad nos pedía usar Musescore porque es gratuito y todos podían\nacceder",
 ["aprendizaje/curricular", "acceso/software gratuito"]),

("yo me bajé el Sibelius crackeado en un momento",
 ["acceso/piratería"]),

("después lo desinstalé porque me daba miedo que fuera ilegal",
 ["acceso/piratería", "acceso/licencias"]),

("aprendí a usarlo viendo tutoriales en YouTube, nunca me lo enseñaron en\nclases",
 ["aprendizaje/incidental", "aprendizaje/recursos externos"]),

("en tercero recién nos enseñaron a usar el Musescore en serio, antes era\ntodo muy autodidacta",
 ["aprendizaje/curricular", "aprendizaje/autodidacta"]),

("a mí me costó mucho al principio, me parecía complicado, no entendía los\natajos del teclado",
 ["aprendizaje/carga cognitiva", "evaluación/usabilidad"]),

("ahora lo manejo bien, pero fue un proceso largo",
 ["aprendizaje/competencia digital"]),

("mi compañero me enseñó los atajos y ahí lo empecé a entender mejor",
 ["aprendizaje/entre pares"]),

("el Musescore a veces se crashea, sobre todo con archivos grandes",
 ["evaluación/rendimiento técnico", "software/Musescore"]),

("la interfaz es un poco confusa al principio pero después uno se acostumbra",
 ["evaluación/usabilidad", "aprendizaje/carga cognitiva"]),

("lo que más me gusta es que es gratis y hace todo lo que necesito para la\nuniversidad",
 ["acceso/software gratuito", "evaluación/utilidad percibida"]),

("igual a veces siento que le faltan funciones que tiene el Sibelius o el\nFinale",
 ["evaluación/funcionalidad", "acceso/software de pago"]),

("para composición más avanzada probablemente necesitaría un programa de\npago",
 ["contexto/tarea musical", "acceso/valoración costo-beneficio"]),

("yo siento que me falta manejo de software comparado con mis compañeros",
 ["actitud/auto-percepción digital"]),

("hay compañeros que saben usar el DAW y yo no, me siento un poco atrás",
 ["actitud/auto-percepción digital", "software/DAW"]),

("creo que la universidad debería enseñar más sobre software desde primero",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("en mi colegio nunca usamos software de música, llegué sin saber nada",
 ["aprendizaje/inicio tardío", "contexto/brecha curricular"]),

("el tema del software libre me parece interesante, no lo había pensado\nmucho antes",
 ["actitud/valoración del FOSS"]),

("yo prefiero lo que conozco, aunque sea de pago, porque sé que funciona\nbien",
 ["actitud/preferencia de software", "acceso/valoración costo-beneficio"]),

# Citas exactas verificadas de FG1 real
("Realmente como el único pero que se que me ha ocurrido ha sido como de eso, como estar en Musesc",
 ["evaluación/rendimiento técnico", "software/Musescore"]),

("me borró un arreglo que estaba haciendo como para el colegio de la",
 ["evaluación/rendimiento técnico", "contexto/uso pedagógico"]),

("también me paso eso se me cerró el programa y y había av",
 ["evaluación/rendimiento técnico"]),

("a mí con Musescore me pasó, o sea eh, me pasa cuando he querido como ocupar imágenes de de f",
 ["evaluación/funcionalidad", "software/Musescore"]),

("me gustaría poder sacarlas del programa y así como recortar como poner el ejemplo que quie",
 ["evaluación/funcionalidad"]),

("a veces eh he visto partituras que le cambian con colores",
 ["evaluación/funcionalidad", "contexto/tarea musical"]),

("igual uno se vuelve dependiente pero no sé si específicamente porque así lo queda el programa",
 ["actitud/auto-percepción digital", "evaluación/utilidad percibida"]),

("como una herramienta porque volviendo al mismo tema al mismo tema como de la cifra",
 ["evaluación/utilidad percibida"]),

("te da cierta autonomía, pero nosotros para tener esa autonom",
 ["actitud/auto-percepción digital", "aprendizaje/competencia digital"]),

("lo que en verdad estamos como profesores, docentes, estudiantes es como a que las",
 ["actitud/proyección profesional", "contexto/uso pedagógico"]),

("YouTube ayuda mucho a aprender a ocupar programas. Mientras existan tutoriales en",
 ["aprendizaje/recursos externos", "aprendizaje/incidental"]),

# Recomendaciones formación
("debería haber un curso de notación digital",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("todo esto estábamos girando como en que eh nos eh en algún momento nos dif",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("todos tenemos como caemos en este consenso de que es la mejor aplicación como rápida efec",
 ["evaluación/utilidad percibida", "software/Musescore"]),

("realmente no te enseñan cosas de notación musical en Pentagram",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("debería estar como al principio de la carrera no un OFC",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("hay una falla. Yo creo que es importante haber hecho eso al inicio",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("no te enseñan a usar un programa específicamente",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("Un módulo significativo que te ayude a aprender a usarlo de desde un nivel principiante",
 ["aprendizaje/curricular", "evaluación/usabilidad"]),

("hay otras universidades que tienen la carrera y tienen en ramo como de herramientas",
 ["aprendizaje/curricular", "contexto/brecha curricular"]),

("falta como aprender a ocuparlo de manera más pedagógica",
 ["aprendizaje/curricular", "contexto/uso pedagógico"]),

("va avanzando el tiempo, van avanzando las cosas, tenemos que ir",
 ["evaluación/actualización y vigencia", "actitud/proyección profesional"]),

("igual la necesidad de aprender de sonido un poco. Porque al final vamos a estar en l",
 ["contexto/brecha curricular", "actitud/proyección profesional"]),

("Armar los parlantes, ser todo el manejo y nosotros tenemos conocimiento nulo de eso",
 ["actitud/auto-percepción digital", "contexto/brecha curricular"]),
]

# ── Insertar ──────────────────────────────────────────────────────────────────
inserted = skipped = 0
not_found = []

for (fragment, tag_paths) in HLS:
    sb, eb = find_bytes(DOC1, fragment)
    if sb is None:
        not_found.append(fragment[:60])
        skipped += 1
        continue

    snippet = DOC1.encode('utf-8')[sb:eb].decode('utf-8', errors='replace')
    cur.execute(
        "INSERT INTO highlights (document_id, start_offset, end_offset, snippet) VALUES (1,?,?,?)",
        (sb, eb, f"<p>{snippet}</p>")
    )
    hl_id = cur.lastrowid

    for tp in tag_paths:
        tid = tag(tp)
        if tid is None:
            print(f"  ⚠ Tag missing: {tp!r}")
            continue
        cur.execute(
            "INSERT OR IGNORE INTO highlight_tags (highlight_id, tag_id) VALUES (?,?)",
            (hl_id, tid)
        )
    inserted += 1

conn.commit()

print(f"\n✓ FG1 insertados: {inserted} | Saltados: {skipped}")
if not_found:
    print("⚠ No encontrados:")
    for f in not_found: print(f"  {f!r}")

# Summary total
cur.execute("""SELECT d.name, COUNT(h.id) FROM documents d LEFT JOIN highlights h ON h.document_id=d.id
WHERE d.project_id=1 GROUP BY d.id ORDER BY d.name""")
print("\nHighlights por documento:")
for r in cur.fetchall(): print(f"  {r[0][:45]:<47} {r[1]:>3}")

cur.execute("""SELECT t.path, COUNT(ht.highlight_id) as n FROM tags t
LEFT JOIN highlight_tags ht ON t.id=ht.tag_id WHERE t.project_id=1
GROUP BY t.id ORDER BY n DESC LIMIT 20""")
print("\nTop 20 tags (total proyecto):")
for r in cur.fetchall(): print(f"  {r[0]:<47} {r[1]:>3}")

conn.close()
print("\nDone.")
