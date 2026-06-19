# Archivo de Tiempo (Historial UI)

## Cambios UI (GNOME HIG)
- Migrar lista etiquetas a GtkTreeView. Filtros arreglados.
- Añadir dropdown filtro documento en pestaña revisión.
- Cambiar botones texto por iconos en pestaña revisión.
- Aplicar diseño GNOME HIG.
- Menú principal: Botón "Export thematic table (HTML)".

## Editor y UX
- Restaurar editor texto. Añadir undo/redo.
- Desactivar Vim mode por defecto.

## CLI y Exportación
- Crear cuali-cli para análisis IA.
- Añadir comando `export-book` para libro códigos.
- Implementar exportación tabla temática HTML.
- Nueva query `db_get_theme_unique_frequencies` cuenta frecuencias únicas reales por tema.

## Notas
- UI cambios = pruebas. Archivo guarda historial para futuro reuso.
