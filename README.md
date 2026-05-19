# Cuali

<p align="center">
  <img src="icons/hicolor/scalable/apps/org.cuali.CualiGTK.svg" alt="Cuali" width="128"/>
</p>

[🇪🇸 Read in Spanish](README.es.md)

Cuali is a native tool for qualitative data analysis, inspired by Taguette but built with C and GTK4/Libadwaita to offer a fast, lightweight, and perfectly integrated experience on the Linux desktop.

## Features

- **Project Management**: Create new projects or open existing .sqlite3 files compatible with the Taguette schema.
- **Recent History**: Quick access to your latest work from the welcome screen.
- **Document Import**: Support for importing plain text files (with PDF support via Poppler).
- **Live Editing**: Correct transcription errors directly in the application with the integrated editing mode.
- **Coding and Highlighting**: Select text and assign tags with a persistent highlighting system optimized for light and dark modes.
- **Interactive Tag Popover**: Click on any highlight to open a popover where you can:
  - View all currently assigned tags
  - Create new tags on the fly
  - Select or deselect tags from the highlight
- **Results Analysis**: A unified view of quotes grouped by tag, with frequency statistics sorted from highest to lowest.
- **Total Privacy**: Your data is yours. Everything is saved in a local SQLite database.

## Requirements

To compile and run Cuali, you need the following development libraries:

- GTK 4
- Libadwaita 1
- SQLite 3
- Poppler GLib

### On Debian/Ubuntu:
```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libsqlite3-dev libpoppler-glib-dev
```

### On Fedora:
```bash
sudo dnf install gtk4-devel libadwaita-devel sqlite-devel poppler-glib-devel
```

### On Arch:
```bash
sudo pacman -S gtk4 libadwaita sqlite poppler-glib
```

## Compilation

To compile the project, simply run:

```bash
make
```

This will generate the `cuali-gtk` executable binary.

To clean the compiled files:

```bash
make clean
```

## Running

To start the application:

```bash
./cuali-gtk
```

## Project Structure

```
cuali-gtk/
├── src/
│   ├── main.c           # Application entry point
│   ├── window.c         # GUI and main logic
│   ├── database.c       # SQLite operations
│   └── importer.c       # Document import
├── include/
│   ├── window.h
│   ├── database.h
│   └── importer.h
├── Makefile             # Build configuration
└── README.md
```

## How to Use

### 1. Create a New Project
- Click on "Create new project" on the welcome screen.
- Select where to save the database file (.sqlite3).
- Done! You can now start working.

### 2. Open an Existing Project
- Use "Open existing project" or select one from the recent list.
- The project will load automatically.

### 3. Import Documents
- In the "Documents" tab, click the "+" (Add) button.
- Select a plain text or PDF file.
- The document will be imported and ready to analyze.

### 4. Code Text
- Select the text you want to code.
- A popover will appear.
- Choose an existing tag or create a new one.
- The text will be highlighted and associated with the tag.

### 5. Interact with Highlights
- Click on any highlighted text to open the tag popover.
- From the popover you can:
  - See which tags are assigned.
  - Add new tags to the highlight.
  - Remove existing tags.
  - Create new tags directly.

### 6. Analyze Results
- Go to the "Results" tab.
- You will see all quotes grouped by tag.
- The statistics on the left show the frequency of each tag.

## License

This project is distributed under the **BSD 3-Clause License**, maintaining the same open-source and permission philosophy as Taguette. See the `LICENSE` file for more information.

## Development and Contributions

### TODO

- [ ] Fix positioning/tiling bug when opening projects
- [ ] Fix UI issues and layout stability
- [ ] Implement filtering system in the results view
- [ ] Improve performance when loading extremely large documents
- [ ] Add support for exporting in other formats (Excel, CSV)
- [ ] Multi-language support (i18n)
- [ ] Customizable themes

### Design Decisions

- **Why C + GTK4**: For a data analysis tool, the priorities are:
  - **Performance**: C is significantly faster than interpreted languages.
  - **Lightweight**: Low memory consumption and fast startup.
  - **Stability**: GTK4 is mature and reliable.
  - **Nativeness**: Perfect integration with the GNOME/Wayland desktop.
  
- **Why SQLite**: Embedded database, no need for external servers, perfect for desktop applications.

- **Why Libadwaita**: Modern and consistent design with the GNOME ecosystem, accessible and well-polished UI components.

## Support and Bug Reporting

If you find an error or have a suggestion for improvement, please open an [issue](https://github.com/diegoveraniego/cuali-gtk/issues).

---

**Project developed by Diego (2026).**

Inspired by [Taguette](https://www.taguette.org/) and built with ❤️ using C and GTK4.
