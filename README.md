# Cuali

Cuali es una herramienta nativa para el análisis cualitativo de datos, inspirada en Taguette pero construida con C y GTK4/Libadwaita para ofrecer una experiencia rápida, ligera y perfectamente integrada en el escritorio Linux.

## Características

- **Gestión de Proyectos**: Crea nuevos proyectos o abre archivos .sqlite3 existentes compatibles con el esquema de Taguette.
- **Historial de Recientes**: Acceso rápido a tus últimos trabajos desde la pantalla de bienvenida.
- **Importación de Documentos**: Soporte para importar archivos de texto plano (con soporte para PDF mediante Poppler).
- **Edición en Vivo**: Corrige errores de transcripción directamente en la aplicación con el modo de edición integrado.
- **Codificación y Resaltado**: Selecciona texto y asigna etiquetas (tags) con un sistema de resaltado persistente y optimizado para modos claro y oscuro.
- **Análisis de Resultados**: Vista unificada de citas agrupadas por etiqueta, con estadísticas de frecuencia ordenadas de mayor a menor.
- **Privacidad Total**: Tus datos son tuyos. Todo se guarda en una base de datos SQLite local.

## Requisitos

Para compilar y ejecutar Cuali, necesitas las siguientes librerías de desarrollo:

- GTK 4
- Libadwaita 1
- SQLite 3
- Poppler GLib

## Compilación

Para compilar el proyecto, simplemente ejecuta:

```bash
make
```

Esto generará el binario ejecutable `cuali-gtk`.

## Ejecución

Para iniciar la aplicación:

```bash
./cuali-gtk
```

## Licencia

Este proyecto se distribuye bajo la licencia **BSD 3-Clause**, manteniendo la misma filosofía de código abierto y permisos que Taguette. Consulta el archivo `LICENSE` para más información.

## TODO (Pendientes)

- [ ] Corregir error de posicionamiento/tiling de la ventana al abrir proyectos.
- [ ] Implementar sistema de filtrado en la vista de resultados.
- [ ] Mejorar el rendimiento al cargar documentos extremadamente largos.
- [ ] Añadir soporte para exportación en otros formatos (Excel, CSV).

---
Proyecto desarrollado por Diego (2026).
