#!/usr/bin/env python3
"""
Refinamiento de etiquetas para tesis.
Opera sobre una copia de la BD, no toca el original.
"""

import sqlite3
import sys

DB = "/home/diego/Documentos/Rescate_Tesis_REFINADO.sqlite3"

con = sqlite3.connect(DB)
cur = con.cursor()

def rename(old, new):
    """Renombra una etiqueta y todas sus hijas en cascada. Ignora si ya existe el destino."""
    # Primero hijas (LIKE 'old/%')
    cur.execute("""
        UPDATE OR IGNORE tags SET path = ? || SUBSTR(path, ?)
        WHERE path LIKE ? || '/%'
    """, (new, len(old) + 1, old))
    # Luego la propia etiqueta
    cur.execute("UPDATE OR IGNORE tags SET path = ? WHERE path = ?", (new, old))
    affected = cur.rowcount
    print(f"  {'OK' if affected else '--'} {old!r:55s} → {new!r}")

def merge(old, target):
    """
    Fusiona: mueve todos los highlight_tags del tag 'old' al tag 'target'.
    Borra el tag 'old' al final.
    """
    cur.execute("SELECT id FROM tags WHERE path = ?", (old,))
    row = cur.fetchone()
    if not row:
        print(f"  ?? no existe para fusionar: {old!r}")
        return
    old_id = row[0]

    cur.execute("SELECT id FROM tags WHERE path = ?", (target,))
    row2 = cur.fetchone()
    if not row2:
        print(f"  ?? destino no existe: {target!r}")
        return
    new_id = row2[0]

    # Reasigna highlight_tags (ignora duplicados)
    cur.execute("""
        INSERT OR IGNORE INTO highlight_tags (highlight_id, tag_id)
        SELECT highlight_id, ? FROM highlight_tags WHERE tag_id = ?
    """, (new_id, old_id))
    cur.execute("DELETE FROM highlight_tags WHERE tag_id = ?", (old_id,))
    cur.execute("DELETE FROM tags WHERE id = ?", (old_id,))
    print(f"  FUSIÓN {old!r} → {target!r}")

print("=" * 65)
print("TEMA 1 — pragmatismo")
print("=" * 65)

rename("percepción/pragmatismo",                   "pragmatismo/elección pragmática")
rename("percepción/utilidad",                      "pragmatismo/utilidad")
rename("percepción/suficiencia técnica",           "pragmatismo/suficiencia técnica")
rename("percepción/software como medio y no como fin", "pragmatismo/software como medio y no fin")
rename("percepción/comparación con competencia",   "pragmatismo/comparación con competencia")
rename("percepción/autonomía",                     "pragmatismo/autonomía")
rename("percepción/inercia al cambiar de software","pragmatismo/inercia al cambiar")
rename("percepción/concepción de tipos de software","pragmatismo/indiferencia libre-propietario")
rename("percepción/misconcepción de tipos de software","pragmatismo/indiferencia libre-propietario/misconcepciones")
rename("percepción/dependencia",                   "pragmatismo/dependencia de herramienta")
rename("percepción/modelo de negocio",             "pragmatismo/modelo de negocio")
rename("percepción/prejuicio de calidad",          "pragmatismo/prejuicio de calidad")
rename("percepción/adaptación al sistema",         "pragmatismo/adaptación al sistema")
rename("percepción/nativo digital",                "pragmatismo/nativo digital")
rename("percepción/power user",                    "pragmatismo/power user")
rename("percepción/porpósito del software",        "pragmatismo/propósito del software")
rename("percepción/tecnología compensatoria",      "pragmatismo/tecnología compensatoria")
rename("percepción/accesibilidad",                 "pragmatismo/acceso/accesibilidad")
rename("acceso/barrera económica",                 "pragmatismo/acceso/barrera económica")
rename("acceso/barrera técnica",                   "pragmatismo/acceso/barrera técnica")
rename("acceso/piratería",                         "pragmatismo/acceso/piratería")
rename("acceso/software pagado/dificultad de conseguir", "pragmatismo/acceso/barrera económica/software pagado difícil")
rename("acceso/ayudante",                          "pragmatismo/acceso/ayudante")
rename("gestión/alternativas gratuitas",           "pragmatismo/alternativas gratuitas")
rename("gestión/evaluación de herramientas",       "pragmatismo/evaluación de herramientas")
rename("gestión/financiamiento",                   "pragmatismo/financiamiento institucional")
rename("gestión/decisión curricular",              "pragmatismo/decisión curricular")

print()
print("=" * 65)
print("TEMA 2 — formacion")
print("=" * 65)

rename("aprendizaje/limitaciones del currículo",   "formacion/limitaciones del currículo")
rename("aprendizaje/carga cognitiva",              "formacion/carga cognitiva")
rename("aprendizaje/curva de aprendizaje",         "formacion/curva de aprendizaje")
rename("aprendizaje/estrategia/autodidacta",       "formacion/estrategia/autodidacta")
rename("aprendizaje/estrategia/recursos externos", "formacion/estrategia/recursos externos")
rename("aprendizaje/estrategia/transmisión oral",  "formacion/estrategia/transmisión oral")
rename("aprendizaje/apoyo entre pares",            "formacion/apoyo entre pares")
rename("aprendizaje/apoyo/recursos externos",      "formacion/apoyo/recursos externos")
rename("aprendizaje/alfabetización digital",       "formacion/alfabetización digital")
rename("aprendizaje/déficit formativo",            "formacion/déficit formativo")
rename("aprendizaje/limitaciones técnicas",        "formacion/limitaciones técnicas")
rename("aprendizaje/adaptación al sistema",        "formacion/adaptación al sistema")
rename("acceso/recomendación de docente",          "formacion/recomendación de docente")
rename("acceso/recomendación de pares",            "formacion/recomendación de pares")
rename("percepción/brecha universidad-escuela",    "formacion/brecha universidad-escuela")
rename("percepción/déficit formativo",             "formacion/déficit formativo/percepción")
rename("percepción/limitaciones técnicas",         "formacion/limitaciones técnicas/percepción")
rename("uso/supervivencia académica",              "formacion/supervivencia académica")
rename("uso/herramienta de estudio",               "formacion/herramienta de estudio")
rename("uso/exigencia/académica",                  "formacion/exigencia académica")
rename("necesidad/indagar sobre el software a utilizar", "formacion/necesidad de orientación sobre software")

print()
print("=" * 65)
print("TEMA 3 — uso pedagogico")
print("=" * 65)

rename("uso/enseñanza en aula",                    "pedagogico/enseñanza en aula")
rename("uso/creación de material didáctico",       "pedagogico/creación de material didáctico")
rename("uso/práctica profesional",                 "pedagogico/práctica profesional")
rename("uso/solución mixta",                       "pedagogico/solución mixta")
rename("uso/exigencia/profesional",                "pedagogico/exigencia profesional")
rename("percepción/potencial pedagógico",          "pedagogico/potencial pedagógico")
rename("percepción/realidad escolar",              "pedagogico/realidad escolar")
rename("percepción/sobrecarga de rol",             "pedagogico/sobrecarga de rol docente")
rename("percepción/frustración de la música de papel", "pedagogico/frustración con partitura impresa")
rename("percepción/autoridad docente",             "pedagogico/autoridad docente")
rename("proyección/futuro laboral",                "pedagogico/proyección laboral")
rename("necesidad/fortalecer tecnologías musicales aplicadas en el aula", "pedagogico/necesidad de formación TIC musical")
rename("ética/uso responsable",                    "pedagogico/ética/uso responsable")
rename("teoría/tpack",                             "pedagogico/marco teórico/tpack")
rename("teoría/dua",                               "pedagogico/marco teórico/dua")
rename("uso/notación musical",                     "pedagogico/notación musical")
rename("uso/arreglos",                             "pedagogico/arreglos")
rename("uso/componer",                             "pedagogico/composición")
rename("uso/producción musical",                   "pedagogico/producción musical")
rename("uso/sonido en vivo",                       "pedagogico/sonido en vivo")
rename("uso/transcripción",                        "pedagogico/transcripción")
rename("uso/dispositivos móviles",                 "pedagogico/dispositivos móviles")
rename("uso/mutliplataforma",                      "pedagogico/multiplataforma")
rename("tipo/daw",                                 "pedagogico/tipo de herramienta/daw")
rename("función/banco de sonidos",                 "pedagogico/función/banco de sonidos")
rename("función/nube",                             "pedagogico/función/nube")
rename("función/reproducir lo escrito",            "pedagogico/función/reproducir lo escrito")

print()
print("=" * 65)
print("TEMA 4 — sostenibilidad")
print("=" * 65)

# Fusionar los dos crashes duplicados
rename("sostenibilidad/fallos/crashes",            "sostenible/estabilidad/crashes")
# El resto de sostenible/* ya tiene buen nombre, solo normalizamos
rename("sostenible/estabilidad",                   "sostenibilidad/estabilidad")
rename("sostenible/estabilidad/crashes",           "sostenibilidad/estabilidad/crashes")
rename("sostenible/mantenimiento y actualizaciones","sostenibilidad/mantenimiento y actualizaciones")
rename("sostenible/mantenimiento y actualizaciones/nuevas funciones","sostenibilidad/mantenimiento y actualizaciones/nuevas funciones")
rename("sostenible/gratuito",                      "sostenibilidad/gratuito")
rename("sostenible/pocos requisitos de hardware",  "sostenibilidad/pocos requisitos de hardware")
rename("sostenible/intuitividad",                  "sostenibilidad/intuitividad")
rename("sostenible/multiplataforma",               "sostenibilidad/multiplataforma")
rename("sostenible/portabilidad",                  "sostenibilidad/portabilidad")
rename("sostenible/compatibilidad retroactiva",    "sostenibilidad/compatibilidad retroactiva")
rename("sostenible/escalabilidad",                 "sostenibilidad/escalabilidad")
rename("sostenible/fallos/crashes",                "sostenibilidad/estabilidad/crashes")
rename("sostenible/facilidad de aprendizaje",      "sostenibilidad/facilidad de aprendizaje")
rename("sostenible/interfaz atractiva",            "sostenibilidad/interfaz atractiva")
rename("sostenible/nube",                          "sostenibilidad/nube")
rename("sostenible/obsolescencia programada",      "sostenibilidad/obsolescencia programada")
rename("sostenible/retroalimentación del usuario", "sostenibilidad/retroalimentación del usuario")
rename("sostenible/seguro",                        "sostenibilidad/seguro")
rename("sostenible/sistema operativo",             "sostenibilidad/sistema operativo")
rename("sostenible/software ligero",               "sostenibilidad/software ligero")
rename("percepción/intuitividad",                  "sostenibilidad/intuitividad/percepción")
rename("percepción/intuitividad/interfaz intuitiva","sostenibilidad/intuitividad/interfaz intuitiva")
rename("percepción/ciberseguridad",                "sostenibilidad/ciberseguridad")
rename("percepción/diseño visual",                 "sostenibilidad/diseño visual")
rename("percepción/validación profesional",        "sostenibilidad/validación profesional")

print()
print("=" * 65)
print("MENCIONES y SOFTWARE (se dejan sin tema, solo limpieza)")
print("=" * 65)

rename("mención/inteligencia artificial",          "menciones/inteligencia artificial")
rename("mención/composición y creación",           "menciones/composición y creación")
rename("mención/dirección coral",                  "menciones/dirección coral")
rename("mención/dirección de conjuntos instrumentales","menciones/dirección de conjuntos instrumentales")
rename("mención/youtube",                          "menciones/youtube")
rename("más utilizado/musescore",                  "menciones/más utilizado/musescore")
# Eliminar tags de sistema
cur.execute("DELETE FROM tags WHERE path IN ('sistema/revisar','importante')")
print("  Eliminadas etiquetas de sistema: sistema/revisar, importante")

con.commit()
print()
print("✓ LISTO. BD guardada en:", DB)
print()

# Mostrar resumen
print("=" * 65)
print("RESUMEN FINAL DE TEMAS")
print("=" * 65)
cur.execute("""
    SELECT SUBSTR(path, 1, INSTR(path||'/', '/') - 1) as tema,
           COUNT(*) as etiquetas,
           SUM(COALESCE((SELECT COUNT(*) FROM highlight_tags ht WHERE ht.tag_id = t.id),0)) as citas
    FROM tags t
    GROUP BY tema
    ORDER BY tema
""")
for row in cur.fetchall():
    print(f"  {row[0]:35s}  {row[1]:3d} etiquetas  {row[2]:4d} citas")

con.close()
