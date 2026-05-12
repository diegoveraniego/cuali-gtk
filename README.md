# Cuali

Cuali es una herramienta nativa para el análisis cualitativo de datos, inspirada en Taguette pero construida con C y GTK4/Libadwaita para ofrecer una experiencia rápida, ligera y perfectamente integrada en el escritorio Linux.

## Características

- **Gestión de Proyectos**: Crea nuevos proyectos o abre archivos .sqlite3 existentes compatibles con el esquema de Taguette.
- **Historial de Recientes**: Acceso rápido a tus últimos trabajos desde la pantalla de bienvenida.
- **Importación de Documentos**: Soporte para importar archivos de texto plano (con soporte para PDF mediante Poppler).
- **Edición en Vivo**: Corrige errores de transcripción directamente en la aplicación con el modo de edición integrado.
- **Codificación y Resaltado**: Selecciona texto y asigna etiquetas (tags) con un sistema de resaltado persistente y optimizado para modos claro y oscuro.
- **Menú Interactivo de Etiquetas**: Haz click en cualquier subrayado para abrir un menú flotante centrado en pantalla donde puedes:
  - Ver todas las etiquetas asignadas actualmente
  - Crear nuevas etiquetas sobre la marcha
  - Seleccionar o deseleccionar etiquetas del highlight
  - El menú es completamente movible para no obstruir tu vista
- **Análisis de Resultados**: Vista unificada de citas agrupadas por etiqueta, con estadísticas de frecuencia ordenadas de mayor a menor.
- **Privacidad Total**: Tus datos son tuyos. Todo se guarda en una base de datos SQLite local.

## Requisitos

Para compilar y ejecutar Cuali, necesitas las siguientes librerías de desarrollo:

- GTK 4
- Libadwaita 1
- SQLite 3
- Poppler GLib

### En Debian/Ubuntu:
```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libsqlite3-dev libpoppler-glib-dev
```

### En Fedora:
```bash
sudo dnf install gtk4-devel libadwaita-devel sqlite-devel poppler-glib-devel
```

### En Arch:
```bash
sudo pacman -S gtk4 libadwaita sqlite poppler-glib
```

## Compilación

Para compilar el proyecto, simplemente ejecuta:

```bash
make
```

Esto generará el binario ejecutable `cuali-gtk`.

Para limpiar los archivos compilados:

```bash
make clean
```

## Ejecución

Para iniciar la aplicación:

```bash
./cuali-gtk
```

## Estructura del Proyecto

```
cuali-gtk/
├── src/
│   ├── main.c           # Punto de entrada de la aplicación
│   ├── window.c         # Interfaz gráfica y lógica principal
│   ├── database.c       # Operaciones con SQLite
│   └── importer.c       # Importación de documentos
├── include/
│   ├── window.h
│   ├── database.h
│   └── importer.h
├── Makefile             # Configuración de compilación
└── README.md
```

## Cómo Usar

### 1. Crear un Nuevo Proyecto
- Haz click en "Crear Nuevo Proyecto" en la pantalla de bienvenida
- Selecciona dónde guardar el archivo de base de datos (.sqlite3)
- ¡Listo! Ya puedes empezar a trabajar

### 2. Abrir un Proyecto Existente
- Usa "Abrir Proyecto Existente" o selecciona uno de la lista de recientes
- El proyecto se cargará automáticamente

### 3. Importar Documentos
- En la pestaña "Documentos", haz click en el botón "+" (Agregar)
- Selecciona un archivo de texto plano
- El documento se importará y estará listo para analizar

### 4. Codificar Texto
- Selecciona el texto que deseas marcar
- Presiona el botón de subrayado en la barra de herramientas
- Elige una etiqueta existente o crea una nueva
- El texto se resaltará y se asociará con la etiqueta

### 5. Interactuar con Highlights
- **Haz click en cualquier texto subrayado** para abrir el menú de etiquetas
- Desde el menú puedes:
  - Ver qué etiquetas están asignadas
  - Agregar nuevas etiquetas al highlight
  - Quitar etiquetas existentes
  - Crear etiquetas nuevas directamente
- El menú es movible: simplemente arrastra su título para repositionarlo

### 6. Analizar Resultados
- Ve a la pestaña "Resultados"
- Verás todas las citas agrupadas por etiqueta
- Las estadísticas en la izquierda muestran la frecuencia de cada etiqueta

## Licencia

Este proyecto se distribuye bajo la licencia **BSD 3-Clause**, manteniendo la misma filosofía de código abierto y permisos que Taguette. Consulta el archivo `LICENSE` para más información.

## Desarrollo y Contribuciones

### TODO (Pendientes)

- [ ] **Menú flotante de etiquetas** (EN DESARROLLO): Interfaz interactiva para manejar etiquetas directamente desde los highlights
- [ ] Corregir error de posicionamiento/tiling de la ventana al abrir proyectos
- [ ] Fix UI issues and layout stability
- [ ] Implementar sistema de filtrado en la vista de resultados
- [ ] Mejorar el rendimiento al cargar documentos extremadamente largos
- [ ] Añadir soporte para exportación en otros formatos (Excel, CSV)
- [ ] Soporte para múltiples idiomas (i18n)
- [ ] Temas personalizables

### Decisiones de Diseño

- **Por qué C + GTK4**: Para una herramienta de análisis de datos, se prioriza:
  - **Rendimiento**: C es significativamente más rápido que lenguajes interpretados
  - **Ligereza**: Bajo consumo de memoria y arranque rápido
  - **Estabilidad**: GTK4 es maduro y confiable
  - **Natividad**: Integración perfecta con el escritorio GNOME/Wayland
  
- **Por qué SQLite**: Base de datos embebida, sin necesidad de servidores externos, perfecta para aplicaciones de escritorio

- **Por qué Libadwaita**: Diseño moderno y consistente con el ecosistema GNOME, componentes UI accesibles y bien pulidas

## Soporte y Reporte de Bugs

Si encuentras un error o tienes una sugerencia de mejora, por favor abre un [issue](https://github.com/diegoveraniego/cuali-gtk/issues).

---

**Proyecto desarrollado por Diego (2026).**

Inspirado en [Taguette](https://www.taguette.org/) y construido con ❤️ usando C y GTK4.
