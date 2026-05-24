# Cuali — Contexto para IAs

Documento de referencia técnica para IAs que trabajen con datos de Cuali.
Cubre: esquema SQLite, sistema de offsets, convenciones de etiquetado, y comparación con Taguette.

---

## 1. Esquema SQLite de Cuali

Cuali usa SQLite como única fuente de verdad. Un proyecto = un archivo `.sqlite3`.

### Tablas principales

```sql
-- Proyecto (solo uno por archivo)
CREATE TABLE projects (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  name        TEXT NOT NULL,
  description TEXT
);

-- Documentos (los textos a analizar, ej: transcripciones de focus groups)
CREATE TABLE documents (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  project_id  INTEGER NOT NULL REFERENCES projects(id),
  name        TEXT NOT NULL,
  contents    TEXT        -- El texto almacenado como HTML simplificado
);

-- Etiquetas / Códigos de análisis
CREATE TABLE tags (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  project_id  INTEGER NOT NULL REFERENCES projects(id),
  path        TEXT NOT NULL,   -- Formato: "tema/subtema" (ver sección 4)
  description TEXT,
  color       TEXT             -- Color hex, ej: "#3584e4"
);

-- Fragmentos resaltados (citas codificadas)
CREATE TABLE highlights (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  document_id   INTEGER NOT NULL REFERENCES documents(id),
  start_offset  INTEGER NOT NULL,  -- Byte offset en el texto plano (UTF-8)
  end_offset    INTEGER NOT NULL,  -- Byte offset en el texto plano (UTF-8)
  snippet       TEXT,              -- Copia del texto resaltado (desnormalizada)
  memo          TEXT               -- Nota analítica del investigador sobre este fragmento
);

-- Relación muchos-a-muchos: highlight puede tener múltiples tags
CREATE TABLE highlight_tags (
  highlight_id  INTEGER NOT NULL REFERENCES highlights(id) ON DELETE CASCADE,
  tag_id        INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
  PRIMARY KEY (highlight_id, tag_id)
);
```

### Consultas útiles para IAs

```sql
-- Ver todos los highlights de un proyecto con sus memos y etiquetas
SELECT
  h.id,
  h.snippet,
  h.memo,
  h.start_offset,
  h.end_offset,
  GROUP_CONCAT(t.path, ' | ') AS etiquetas,
  d.name AS documento
FROM highlights h
JOIN documents d ON h.document_id = d.id
LEFT JOIN highlight_tags ht ON h.id = ht.highlight_id
LEFT JOIN tags t ON ht.tag_id = t.id
WHERE d.project_id = 1
GROUP BY h.id
ORDER BY d.name, h.start_offset;

-- Ver frecuencia de etiquetas (ordenado por más usadas)
SELECT t.path, COUNT(ht.highlight_id) AS frecuencia
FROM tags t
LEFT JOIN highlight_tags ht ON t.id = ht.tag_id
WHERE t.project_id = 1
GROUP BY t.id
ORDER BY frecuencia DESC;

-- Co-ocurrencia de etiquetas (qué etiquetas aparecen juntas)
SELECT ta.path AS tag_a, tb.path AS tag_b, COUNT(*) AS veces_juntas
FROM highlight_tags a
JOIN highlight_tags b ON a.highlight_id = b.highlight_id AND a.tag_id < b.tag_id
JOIN tags ta ON a.tag_id = ta.id
JOIN tags tb ON b.tag_id = tb.id
WHERE ta.project_id = 1
GROUP BY a.tag_id, b.tag_id
ORDER BY veces_juntas DESC;

-- Memos de un highlight específico (para flujo de sugerencia de etiquetas)
SELECT h.snippet, h.memo,
       GROUP_CONCAT(t.path, ' | ') AS etiquetas_actuales
FROM highlights h
LEFT JOIN highlight_tags ht ON h.id = ht.highlight_id
LEFT JOIN tags t ON ht.tag_id = t.id
WHERE h.id = ?
GROUP BY h.id;
```

---

## 2. Sistema de Offsets (Desplazamiento de Resaltados)

### Cómo funcionan los offsets

Los `start_offset` y `end_offset` son **offsets de bytes en UTF-8** sobre el texto plano del documento.

- El texto se almacena en `documents.contents` como HTML simplificado (puede contener `<p>`, `<b>`, etc.)
- Al cargar en la UI, Cuali convierte el HTML a texto plano mediante `map_html()`, que también construye un `offset_map` para traducir posiciones de texto plano ↔ HTML
- Los offsets en la BD corresponden al **texto plano** (post-conversión), NO al HTML crudo
- Son offsets de **bytes** (no de caracteres Unicode), lo que importa para texto con tildes/eñes

### Algoritmo de desplazamiento al editar

Cuando el usuario edita el texto (inserta o borra caracteres), todos los highlights posteriores al punto de edición deben actualizarse:

```
Función: db_highlights_shift_offsets(document_id, from_offset, delta)

Caso INSERT (delta > 0):
  - Todos los highlights con start_offset >= from_offset:
      start_offset += delta
      end_offset   += delta
  - Highlights que CONTIENEN el punto de edición (start < from_offset <= end):
      solo end_offset += delta

Caso DELETE (delta < 0):
  - Igual: todos los que empiezan después del punto borrado se desplazan hacia atrás
  - Los que contenían el rango borrado encogen su end_offset
```

Esto garantiza que al corregir errores de transcripción, los resaltados existentes no se "descuadran".

### ⚠️ Advertencia crítica para IAs que inserten highlights via SQL

Si insertas highlights directamente en la BD (sin pasar por la UI):
1. El `start_offset` y `end_offset` deben ser **byte offsets del texto plano** (no del HTML)
2. Para calcularlos: toma `documents.contents`, conviértelo a texto plano, cuenta bytes desde el inicio
3. Para texto ASCII puro: offset = posición de carácter. Para UTF-8: una tilde ocupa 2 bytes, una Ñ ocupa 2 bytes

```python
# Ejemplo Python para calcular offsets correctamente
texto_plano = "El software es útil"
fragmento = "útil"
start_bytes = texto_plano.encode('utf-8').index(fragmento.encode('utf-8'))
end_bytes = start_bytes + len(fragmento.encode('utf-8'))
```

---

## 3. Convenciones de Etiquetado

### Formato obligatorio del campo `path`

```
tema/subtema
tema/subtema/sub-subtema
```

**Reglas estrictas:**
- **Minúsculas siempre**, incluyendo la primera letra
- **Tildes obligatorias**: `percepción` ✓, `percepcion` ✗
- **Eñes obligatorias**: `enseñanza` ✓, `ensenanza` ✗  
- Separador de jerarquía: barra `/`
- Sin espacios alrededor de `/`
- Espacios dentro del nombre sí son válidos: `uso en clase` ✓
- Sin mayúsculas aunque sea nombre propio en el contexto: `software/musescore` ✓

### Ejemplos del proyecto actual (tesis FOSS/educación musical)

```
software/musescore
software/musescore/opción sana
percepción/concepción de tipos de software
percepción/ciberseguridad
sostenible/gratuito
sostenible/mantenimiento y actualizaciones
sostenible/seguro
más utilizado/musescore
uso/supervivencia académica
función/reproducir lo escrito
```

### Flujo recomendado para IA: memo → etiqueta

Cuando el investigador pasa un memo para que la IA sugiera etiquetas:

1. Leer lista de etiquetas existentes del proyecto (campo `path` de tabla `tags`)
2. Analizar el memo
3. Si encaja con etiqueta existente → devolver esa etiqueta exacta (sin modificar capitalización ni tildes)
4. Si no encaja → proponer etiqueta nueva siguiendo el formato `tema/subtema`
5. Siempre devolver en formato JSON para integración fácil:

```json
{
  "highlight_id": 42,
  "etiquetas_sugeridas": [
    {"accion": "usar_existente", "path": "software/musescore"},
    {"accion": "crear_nueva", "path": "barrera/costo licencia", "descripcion": "Menciona el costo como obstáculo"}
  ]
}
```

---

## 4. Comparación con Taguette (compatibilidad e incompatibilidades)

Cuali es compatible con el esquema SQLite de Taguette (puede abrir archivos `.sqlite3` de Taguette). Sin embargo, hay diferencias importantes:

### Tablas adicionales en Taguette (que Cuali ignora)

| Tabla Taguette | Descripción | Estado en Cuali |
|---|---|---|
| `users` | Sistema de usuarios/login | Ignorada — Cuali es single-user |
| `project_members` | Permisos por proyecto | Ignorada |
| `commands` | Log de operaciones (para colaboración) | Ignorada |
| `alembic_version` | Control de migraciones | Ignorada |

### Diferencias de esquema en tablas compartidas

| Campo | Taguette | Cuali | Impacto |
|---|---|---|---|
| `documents.description` | `TEXT NOT NULL` (requerido) | No existe | Al migrar Cuali→Taguette, agregar campo vacío |
| `documents.filename` | `VARCHAR(200) NOT NULL` | No existe | Al migrar, usar el `name` como filename |
| `documents.text_direction` | `VARCHAR(13) NOT NULL` | No existe | Al migrar, usar `'left-to-right'` por defecto |
| `documents.created` | `DATETIME NOT NULL` | No existe | Al migrar, usar timestamp actual |
| `tags.color` | Existe (agregado vía migración en Cuali) | Existe | Compatible ✓ |
| `highlights.memo` | Existe | Existe | Compatible ✓ |
| `highlights.snippet` | `TEXT NOT NULL` | `TEXT` (nullable) | Compatible ✓ |
| `projects.description` | `TEXT NOT NULL` | `TEXT` (nullable) | Compatible ✓ |

### Script de migración Cuali → Taguette

Para migrar un `.sqlite3` de Cuali a formato Taguette completo:

```sql
-- Paso 1: Agregar tablas faltantes
CREATE TABLE IF NOT EXISTS users (
  login VARCHAR(30) NOT NULL PRIMARY KEY,
  created DATETIME NOT NULL DEFAULT (datetime('now')),
  hashed_password VARCHAR(192),
  disabled BOOLEAN NOT NULL DEFAULT 0,
  password_set_date DATETIME,
  language VARCHAR(10),
  email VARCHAR(256),
  email_sent DATETIME
);

CREATE TABLE IF NOT EXISTS project_members (
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  user_login VARCHAR(30) NOT NULL REFERENCES users(login) ON DELETE CASCADE ON UPDATE CASCADE,
  privileges VARCHAR(11) NOT NULL DEFAULT 'admin',
  PRIMARY KEY (project_id, user_login)
);

CREATE TABLE IF NOT EXISTS commands (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  date DATETIME NOT NULL DEFAULT (datetime('now')),
  user_login VARCHAR(30) NOT NULL REFERENCES users(login) ON UPDATE CASCADE,
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  document_id INTEGER,
  payload TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS alembic_version (
  version_num VARCHAR(32) NOT NULL PRIMARY KEY
);

-- Paso 2: Crear usuario por defecto
INSERT OR IGNORE INTO users (login, created, disabled) VALUES ('admin', datetime('now'), 0);

-- Paso 3: Asignar usuario a todos los proyectos
INSERT OR IGNORE INTO project_members (project_id, user_login, privileges)
SELECT id, 'admin', 'admin' FROM projects;

-- Paso 4: Agregar columnas faltantes en documents
-- (Taguette requiere estas columnas con NOT NULL)
-- NOTA: SQLite no soporta ADD COLUMN NOT NULL sin DEFAULT en tablas con datos
-- Se necesita recrear la tabla:

-- 4a. Renombrar tabla original
ALTER TABLE documents RENAME TO documents_old;

-- 4b. Crear tabla con esquema Taguette completo
CREATE TABLE documents (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name VARCHAR(200) NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  filename VARCHAR(200) NOT NULL DEFAULT '',
  created DATETIME NOT NULL DEFAULT (datetime('now')),
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  text_direction VARCHAR(13) NOT NULL DEFAULT 'left-to-right',
  contents TEXT NOT NULL DEFAULT ''
);

-- 4c. Copiar datos
INSERT INTO documents (id, name, description, filename, created, project_id, text_direction, contents)
SELECT id, name, '', name, datetime('now'), project_id, 'left-to-right', COALESCE(contents, '')
FROM documents_old;

-- 4d. Limpiar
DROP TABLE documents_old;

-- Paso 5: Agregar versión de alembic (la última versión conocida de Taguette)
INSERT OR IGNORE INTO alembic_version (version_num) VALUES ('b7f5d9b8e2a1');
```

### ⚠️ Conflictos conocidos al abrir Cuali en Taguette

1. **Columnas `description`, `filename`, `text_direction` en documents**: Si abres un `.sqlite3` de Cuali directamente en Taguette, fallará porque Taguette espera estas columnas con `NOT NULL`. Usar el script de migración arriba.
2. **Sin usuario**: Taguette requiere al menos un usuario en `users` y en `project_members`. Cuali no los tiene. El script de migración crea un usuario `admin` por defecto.
3. **highlights.snippet**: Taguette puede tener `NOT NULL` constraint. Los snippets de Cuali deberían estar presentes, pero verificar antes de migrar.

### Abrir Taguette en Cuali (sin problemas)

Cuali detecta y maneja las tablas extra de Taguette automáticamente al abrir (las ignora). Las migraciones de Cuali (`color` en tags, `memo` en highlights) se aplican con `ALTER TABLE ADD COLUMN IF NOT EXISTS`, por lo que no rompen datos de Taguette.

---

## 5. Leer la DB desde línea de comandos (referencia rápida)

```bash
# Ver resumen del proyecto
sqlite3 proyecto.sqlite3 "SELECT * FROM projects;"

# Listar documentos
sqlite3 proyecto.sqlite3 "SELECT id, name FROM documents;"

# Ver highlights con memos (para flujo IA de sugerencia de etiquetas)
sqlite3 -column -header proyecto.sqlite3 "
SELECT h.id, substr(h.snippet,1,60) as fragmento, h.memo,
       GROUP_CONCAT(t.path,' | ') as etiquetas
FROM highlights h
LEFT JOIN highlight_tags ht ON h.id=ht.highlight_id
LEFT JOIN tags t ON ht.tag_id=t.id
GROUP BY h.id;"

# Exportar highlights+memos sin etiqueta (candidatos para codificar)
sqlite3 proyecto.sqlite3 "
SELECT h.id, h.snippet, h.memo
FROM highlights h
WHERE h.id NOT IN (SELECT DISTINCT highlight_id FROM highlight_tags);"

# Verificar offsets de un documento
sqlite3 proyecto.sqlite3 "
SELECT h.id, h.start_offset, h.end_offset, length(h.snippet) as snippet_len
FROM highlights h
JOIN documents d ON h.document_id=d.id
WHERE d.name LIKE '%fg1%'
ORDER BY h.start_offset;"
```
