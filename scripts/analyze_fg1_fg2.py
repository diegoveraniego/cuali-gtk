#!/usr/bin/env python3
"""
Análisis temático definitivo FG1 y FG2 — citas exactas verificadas del corpus.
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

# Nuevos tags necesarios (no presentes en FG3)
NEW_TAGS = [
    ("actitud/proyección profesional",
     "Anticipación del impacto del software en la vida laboral futura",
     "#9f1239"),
    ("aprendizaje/autodidacta",
     "Aprendizaje por cuenta propia, sin guía formal. Trial-and-error",
     "#1e40af"),
    ("contexto/brecha curricular",
     "Distancia entre lo que enseña la universidad y lo que requiere la práctica docente real",
     "#6b21a8"),
]
for path, desc, color in NEW_TAGS:
    if path not in T:
        cur.execute("INSERT INTO tags (project_id,path,description,color) VALUES (1,?,?,?)",
                    (path, desc, color))
        T[path] = cur.lastrowid
conn.commit()

cur.execute("SELECT id, contents FROM documents WHERE id IN (1,2)")
DOCS = {row[0]: html_to_plain(row[1]) for row in cur.fetchall()}

def tag(p): return T.get(p)

# ─── HIGHLIGHTS ───────────────────────────────────────────────────────────────
# (doc_id, fragmento_exacto_en_texto_plano, [tags])
HLS = [

# ════════════════════════════════════════════════
# FOCUS GROUP 1
# ════════════════════════════════════════════════

# Sostenibilidad / atributos del software
(1, "debe ser accesible económicamente, porque clara",
    ["acceso/economía", "actitud/valoración del FOSS"]),

(1, "quizás nos querían enseñar Finale y claramente tenía la licencia",
    ["acceso/licencias", "acceso/software de pago", "aprendizaje/curricular"]),

(1, "debe haber un acceso",
    ["acceso/software gratuito", "actitud/valoración del FOSS"]),

(1, "la comodidad y también en la flexibilidad y const",
    ["evaluación/usabilidad", "evaluación/actualización y vigencia"]),

(1, "una característica importante igual es que sea intui",
    ["evaluación/usabilidad"]),

(1, "nosotros que somos profesores de música, nos estamos enfocando en lo",
    ["contexto/uso pedagógico", "actitud/proyección profesional"]),

# Proyección laboral
(1, "nunca he ocupado un software de pago porque en r",
    ["acceso/software de pago", "actitud/auto-percepción digital"]),

(1, "¿de qué me va a servir todo ese avance si después no lo puedo aplicar?",
    ["acceso/economía", "actitud/proyección profesional"]),

(1, "qué tanto vamos a depender también del software",
    ["actitud/valoración del FOSS", "actitud/proyección profesional"]),

(1, "están como cubiertas por estos software que son normalmente de uso gratuito o que son libres",
    ["acceso/software gratuito", "actitud/valoración del FOSS"]),

(1, "si nos vamos a un nivel más profesional a a trabajar como de manera más profesional, quizás sí necesit",
    ["contexto/uso profesional", "acceso/valoración costo-beneficio"]),

# Rendimiento / bugs Musescore
(1, "Con Musescore me ha pasado muchas veces",
    ["evaluación/rendimiento técnico", "software/Musescore"]),

(1, "he estado trabajando eh hago un arreglo, toda la cosa",
    ["contexto/tarea musical", "evaluación/rendimiento técnico"]),

(1, "cerrar eh guardar de nuevo como el archivo, cerrarlo, volver a abrir el programa para que recié",
    ["evaluación/rendimiento técnico", "software/Musescore"]),

(1, "imprimía la hoja que ya tenía música y después hacía los arreglos a mano co",
    ["contexto/tarea musical", "aprendizaje/autodidacta"]),

(1, "también me pasó los mismos problemas, pero ahora en cuanto a a guardar",
    ["evaluación/rendimiento técnico"]),

(1, "creo que son reiterativos los problemas con Music Score",
    ["evaluación/rendimiento técnico", "software/Musescore"]),

(1, "uno después lo ocupa como con miedo, guardando cada 5 min",
    ["evaluación/rendimiento técnico", "actitud/auto-percepción digital"]),

(1, "cuando uno patea mucho las actualizaciones, Musescore se te cierra solo",
    ["evaluación/actualización y vigencia", "evaluación/rendimiento técnico", "software/Musescore"]),

# Autonomía / identidad / brecha curricular
(1, "tuvimos que aprender cosas siempre por cuen",
    ["aprendizaje/autodidacta", "aprendizaje/incidental"]),

(1, "se centró en cosas muy eh más centrado en las ticks en vez de producción",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

(1, "debería haber un ramo que te enseñe a conectar la mesa de sonido",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

(1, "la universidad no tiene en cuenta eso",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

(1, "el principal problema es que no se lleva la realidad del docen",
    ["contexto/brecha curricular", "contexto/uso pedagógico"]),

(1, "falta como comunicación, ya sea con los dos con nuestros docentes",
    ["contexto/brecha curricular"]),

(1, "La formación en TICS",
    ["aprendizaje/competencia digital", "aprendizaje/curricular"]),

(1, "las universidades, sabiendo que sobre todo los profes que ya han trabajado en colegi",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

(1, "no te inhabilita como profe, Exacto. pero sí baja como la calidad",
    ["actitud/auto-percepción digital", "contexto/uso pedagógico"]),

# Autonomía con software
(1, "estaba acostumbrado a trabajar con Finale, Finale, Finale, Finale y cuando tuve que pasar a M",
    ["evaluación/tipos de software", "aprendizaje/competencia digital"]),

(1, "no te amarra, pero sí que se a veces te facilita en el trabajo",
    ["evaluación/utilidad percibida", "actitud/valoración del FOSS"]),

(1, "dentro como del mundo musical algunos músicos como que",
    ["evaluación/tipos de software", "actitud/preferencia de software"]),

(1, "Son más ideas a la hora de planificar, a la hora de enseñarle a tu estudiante",
    ["contexto/uso pedagógico", "evaluación/utilidad percibida"]),

(1, "el factor principal como para responder a esa pregunta es el tiempo",
    ["evaluación/eficiencia", "contexto/uso pedagógico"]),

(1, "si estás con poco tiempo, que es como la tónica de un profesor en realidad",
    ["evaluación/eficiencia", "contexto/uso pedagógico"]),

(1, "dependiendo de de lo que para uno lo va a usar, si en realidad el editor de partitura es pa",
    ["evaluación/tipos de software", "contexto/uso pedagógico"]),

(1, "No hay plata. No hay plata. Entonces hay que adaptarse a lo que es",
    ["acceso/economía", "acceso/software gratuito"]),

(1, "al fin y al cabo no te limita, eh quizás sí puede ser que te facilite la v",
    ["evaluación/utilidad percibida", "actitud/valoración del FOSS"]),

# Recomendaciones formación TIC
(1, "un ramo específico de TICS",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

(1, "ellos tenían un ramo de TICS de uso de si mal no recuerdo",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

(1, "vieron todos esos programas como Audacity, Reaper",
    ["evaluación/tipos de software", "aprendizaje/curricular"]),

(1, "tenía tenía un enfoque después igual ligado a lo que era la al uso responsable de las tec",
    ["aprendizaje/curricular", "actitud/valoración del FOSS"]),

(1, "igual es es como una una desvent",
    ["actitud/auto-percepción digital", "contexto/brecha curricular"]),

(1, "que lata igual que pasara lo mismo con nuestros estudiantes",
    ["actitud/proyección profesional", "contexto/uso pedagógico"]),

(1, "se transforma en un aprendizaje significativo",
    ["aprendizaje/curricular", "contexto/uso pedagógico"]),

(1, "las universidades, sabiendo que sobre todo los profes que ya han trabajado en colegi",
    ["aprendizaje/curricular", "contexto/brecha curricular"]),

# ════════════════════════════════════════════════
# FOCUS GROUP 2 — leer contenido primero
# ════════════════════════════════════════════════
]

# ─── Agregar FG2 dinámicamente desde texto leído ─────────────────────────────
fg2 = DOCS.get(2, "")

# Fragmentos FG2 con contexto leído
FG2_HLS = [
    ("Partimos. Ya comenzó la grabación",
     ["actitud/auto-percepción digital"]),  # contexto inicial

    ("no usaba tanto como\nninguna ningún software como para hacer música",
     ["contexto/uso personal", "aprendizaje/inicio tardío"]),

    ("empecé como a necesitarlo como en segundo, de segundo en adelante",
     ["aprendizaje/curricular", "aprendizaje/inicio tardío"]),

    ("nos pidieron como componer algunas cosas, entonces ya ahí\nempecé a usar como programas como Musescore",
     ["aprendizaje/curricular", "software/Musescore", "contexto/tarea musical"]),

    ("mi mención tampoco ameritó o no lo\nincluyeron como la utilización de software",
     ["aprendizaje/curricular", "contexto/brecha curricular"]),

    ("No solíamos usar eso en casi el único ramo que tuve que usarlo fue en\nOficio",
     ["aprendizaje/curricular", "contexto/brecha curricular"]),

    ("tampoco tengo mucho manejo de esos programas",
     ["actitud/auto-percepción digital", "aprendizaje/competencia digital"]),

    ("no tengo PC en un momento descargué una aplicación\npara el celular",
     ["evaluación/tipos de software", "acceso/decisión de adquisición"]),

    ("hubo que aprender básicamente a usar music\ncon la fuerza, porque antes de eso no habíamos tenido acercamiento",
     ["aprendizaje/inicio tardío", "aprendizaje/incidental", "software/Musescore"]),

    ("aprendiendo como con los comandos se hizo todo mucho más\nfácil",
     ["aprendizaje/competencia digital", "evaluación/eficiencia"]),

    ("Requiere una clase yo creo",
     ["aprendizaje/curricular", "contexto/brecha curricular"]),

    ("le pedí ayuda a [Estudiante 15]\ncomo para que me ayudara porque él sí sabía todos los comandos",
     ["aprendizaje/entre pares", "aprendizaje/competencia digital"]),

    ("era más",
     ["aprendizaje/carga cognitiva"]),

    ("antes de eso creo que lo más importante toda era Teams",
     ["contexto/plataformas digitales"]),

    ("ahí por ahí\nse compartían todo, se compartían las partituras, se compartían la materia",
     ["contexto/plataformas digitales", "aprendizaje/entre pares"]),

    ("yo parto. Yo creo que una de las principales como cosas que debería tener es que estos programas",
     ["actitud/valoración del FOSS", "evaluación/actualización y vigencia"]),

    ("tiene que ir como actualizándose y buscando como eh actualizarse",
     ["evaluación/actualización y vigencia"]),

    ("debe estar constantemente actualizándose ese programa",
     ["evaluación/actualización y vigencia"]),

    ("debe ser accesible económicamente, porque clara",
     ["acceso/economía", "actitud/valoración del FOSS"]),

    ("quizás nos querían enseñar Finale y claramente tenía la licencia",
     ["acceso/licencias", "acceso/software de pago", "contexto/brecha curricular"]),

    ("Yo he escuchado una un comentario o historia, me acuerdo, que que por ser de paga este este software",
     ["acceso/software de pago", "acceso/economía"]),

    ("sí o sí par",
     ["evaluación/utilidad percibida"]),

    ("también tiene",
     ["evaluación/funcionalidad"]),

    ("estos de los FOSS que se pueden editar como según los requeri",
     ["actitud/valoración del FOSS", "evaluación/funcionalidad"]),

    ("quizás tenemos, no sé, ya quizás Yo, por ejemplo, ocupo el fondo negro",
     ["evaluación/usabilidad"]),

    ("creo que estamos todos de acuerdo en que tiene que haber un acce",
     ["acceso/software gratuito", "acceso/economía"]),

    ("la comodidad y también en la flexibilidad y const",
     ["evaluación/usabilidad", "evaluación/actualización y vigencia"]),

    ("una característica importante igual es que sea intui",
     ["evaluación/usabilidad"]),

    ("nosotros que somos profesores de música, nos estamos enfocando en lo",
     ["contexto/uso pedagógico", "actitud/proyección profesional"]),

    ("¿qué diferencia prestan para su futuro laboral el",
     ["actitud/proyección profesional", "acceso/valoración costo-beneficio"]),

    ("en mi caso, por ejemplo, O sea, siento yo que no hay mucha diferencia",
     ["evaluación/tipos de software", "acceso/valoración costo-beneficio"]),

    ("nunca he ocupado un software de pago porque en r",
     ["acceso/software de pago", "actitud/auto-percepción digital"]),

    ("¿de qué me va a servir todo ese avance si después no lo puedo aplicar?",
     ["acceso/economía", "actitud/proyección profesional"]),

    ("qué tanto vamos a depender también del software",
     ["actitud/valoración del FOSS", "actitud/proyección profesional"]),

    ("muchas veces estos programas que son como de libre uso e",
     ["acceso/software gratuito", "evaluación/utilidad percibida"]),

    ("están como cubiertas por estos software que son normalmente de uso gratuito o que son libres",
     ["acceso/software gratuito", "actitud/valoración del FOSS"]),

    ("si nos vamos a un nivel más profesional a a trabajar como de manera más profesional, quizás sí necesit",
     ["contexto/uso profesional", "acceso/valoración costo-beneficio"]),

    ("Con Musescore me ha pasado muchas veces",
     ["evaluación/rendimiento técnico", "software/Musescore"]),

    ("he estado trabajando eh hago un arreglo, toda la cosa",
     ["contexto/tarea musical", "evaluación/rendimiento técnico"]),

    ("cerrar eh guardar de nuevo como el archivo, cerrarlo, volver a abrir el programa para que recié",
     ["evaluación/rendimiento técnico", "software/Musescore"]),

    ("en el área de composición uno de alguna manera q",
     ["contexto/tarea musical", "evaluación/rendimiento técnico"]),

    ("creo que son reiterativos los problemas con Music Score",
     ["evaluación/rendimiento técnico", "software/Musescore"]),

    ("uno después lo ocupa como con miedo, guardando cada 5 min",
     ["evaluación/rendimiento técnico", "actitud/auto-percepción digital"]),

    ("cuando uno patea mucho las actualizaciones, Musescore se te cierra solo",
     ["evaluación/actualización y vigencia", "evaluación/rendimiento técnico"]),

    ("estaba acostumbrado a trabajar con Finale, Finale, Finale, Finale y cuando tuve que pasar a M",
     ["evaluación/tipos de software", "aprendizaje/competencia digital"]),

    ("no te amarra, pero sí que se a veces te facilita en el trabajo",
     ["evaluación/utilidad percibida", "actitud/valoración del FOSS"]),

    ("el factor principal como para responder a esa pregunta es el tiempo",
     ["evaluación/eficiencia", "contexto/uso pedagógico"]),

    ("si estás con poco tiempo, que es como la tónica de un profesor en realidad",
     ["evaluación/eficiencia", "contexto/uso pedagógico"]),

    ("No hay plata. No hay plata",
     ["acceso/economía", "acceso/software gratuito"]),

    ("tuvimos que aprender cosas siempre por cuen",
     ["aprendizaje/autodidacta", "aprendizaje/incidental"]),

    ("el principal problema es que no se lleva la realidad del docen",
     ["contexto/brecha curricular", "contexto/uso pedagógico"]),

    ("falta como comunicación, ya sea con los dos con nuestros docentes",
     ["contexto/brecha curricular"]),

    ("La formación en TICS",
     ["aprendizaje/competencia digital", "aprendizaje/curricular"]),

    ("no te inhabilita como profe, Exacto. pero sí baja como la calidad",
     ["actitud/auto-percepción digital", "contexto/uso pedagógico"]),

    ("un ramo específico de TICS",
     ["aprendizaje/curricular", "contexto/brecha curricular"]),

    ("ellos tenían un ramo de TICS de uso de si mal no recuerdo",
     ["aprendizaje/curricular", "evaluación/tipos de software"]),

    ("vieron todos esos programas como Audacity, Reaper",
     ["evaluación/tipos de software", "aprendizaje/curricular"]),

    ("igual es es como una una desvent",
     ["actitud/auto-percepción digital", "contexto/brecha curricular"]),

    ("que lata igual que pasara lo mismo con nuestros estudiantes",
     ["actitud/proyección profesional", "contexto/uso pedagógico"]),

    ("se transforma en un aprendizaje significativo",
     ["aprendizaje/curricular", "contexto/uso pedagógico"]),

    ("las universidades, sabiendo que sobre todo los profes que ya han trabajado en colegi",
     ["aprendizaje/curricular", "contexto/brecha curricular"]),
]

for (frag, tags) in FG2_HLS:
    HLS.append((2, frag, tags))

# ─── Insertar ────────────────────────────────────────────────────────────────

inserted = skipped = 0
not_found = []

for (doc_id, fragment, tag_paths) in HLS:
    plain = DOCS.get(doc_id, "")
    sb, eb = find_bytes(plain, fragment)
    if sb is None:
        not_found.append((doc_id, fragment[:60]))
        skipped += 1
        continue

    snippet_text = plain.encode('utf-8')[sb:eb].decode('utf-8', errors='replace')
    cur.execute(
        "INSERT INTO highlights (document_id, start_offset, end_offset, snippet) VALUES (?,?,?,?)",
        (doc_id, sb, eb, f"<p>{snippet_text}</p>")
    )
    hl_id = cur.lastrowid

    for tp in tag_paths:
        tid = tag(tp)
        if tid is None:
            print(f"  ⚠ Tag not found: {tp!r}")
            continue
        cur.execute(
            "INSERT OR IGNORE INTO highlight_tags (highlight_id, tag_id) VALUES (?,?)",
            (hl_id, tid)
        )
    inserted += 1

conn.commit()

print(f"\n✓ Insertados: {inserted} | Saltados: {skipped}")
if not_found:
    print("⚠ No encontrados:")
    for d, f in not_found:
        print(f"  [doc {d}] {f!r}")

# Summary
cur.execute("""
SELECT d.name, COUNT(h.id) as n
FROM documents d
LEFT JOIN highlights h ON h.document_id=d.id
WHERE d.project_id=1
GROUP BY d.id ORDER BY d.name
""")
print("\nHighlights por documento:")
for r in cur.fetchall():
    print(f"  {r[0][:45]:<47} {r[1]:>3}")

cur.execute("""
SELECT t.path, COUNT(ht.highlight_id) as n
FROM tags t LEFT JOIN highlight_tags ht ON t.id=ht.tag_id
WHERE t.project_id=1
GROUP BY t.id ORDER BY n DESC LIMIT 20
""")
print("\nTop 20 tags:")
for r in cur.fetchall():
    print(f"  {r[0]:<47} {r[1]:>3}")

conn.close()
print("\nDone.")
