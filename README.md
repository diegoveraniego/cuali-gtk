# Cuali

<p align="center">
  <img src="icons/hicolor/scalable/apps/org.cuali.CualiGTK.svg" alt="Cuali Logo" width="128"/>
</p>

<p align="center">
  <a href="https://opensource.org/licenses/BSD-3-Clause"><img src="https://img.shields.io/badge/License-BSD_3--Clause-blue.svg" alt="License"></a>
  <img src="https://img.shields.io/badge/Platform-Linux-lightgrey.svg" alt="Platform">
  <img src="https://img.shields.io/badge/GTK-4.0-green.svg" alt="GTK4">
</p>

<p align="center">
  <em>A fast, native qualitative data analysis tool for the Linux desktop.</em><br>
  <a href="README.es.md">Read in Spanish</a>
</p>

---

Cuali is a native tool for qualitative data analysis, inspired by [Taguette](https://www.taguette.org/). Built with C and GTK4/Libadwaita, it is designed to offer a fast, lightweight, and seamlessly integrated experience on Linux and Wayland environments.

## Snapshot

![Cuali Results View](assets/results_dark.png)
*Cuali's results view in dark mode, displaying tag frequencies and their associated highlights.*

## Features

- **Project Management**: Create new projects or open existing `.sqlite3` files compatible with the Taguette schema.
- **Recent History**: Quick access to your latest work right from the welcome screen.
- **Document Import**: Full support for importing plain text files, with PDF support powered by Poppler.
- **Live Editing**: Correct transcription errors directly within the application using the integrated editing mode.
- **Advanced Coding & Highlighting**: Select text and assign tags with a persistent highlighting system perfectly optimized for both light and dark modes.
- **Interactive Tag Popover**: Click on any highlight to open a dynamic popover to:
  - View currently assigned tags.
  - Create and assign new tags on the fly.
  - Toggle tags on or off for specific highlights.
- **Results Analysis**: A unified, split-view interface of quotes grouped by tag, featuring frequency statistics sorted automatically.
- **Total Privacy**: Your data belongs to you. Everything is stored locally in an SQLite database with no cloud dependencies.

## Gallery

| Documents View (Dark) | Documents View (Light) |
| :---: | :---: |
| ![Documents Dark](assets/docs_dark.png) | ![Documents Light](assets/docs_light.png) |

| Results View (Light) |
| :---: |
| ![Results Light](assets/results_light.png) |

## Requirements

To compile and run Cuali, you need the following development libraries installed on your system:

- **GTK 4**
- **Libadwaita 1**
- **SQLite 3**
- **Poppler GLib**

### Installation by Distribution

**Debian / Ubuntu:**
```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libsqlite3-dev libpoppler-glib-dev
```

**Fedora:**
```bash
sudo dnf install gtk4-devel libadwaita-devel sqlite-devel poppler-glib-devel
```

**Arch Linux:**
```bash
sudo pacman -S gtk4 libadwaita sqlite poppler-glib
```

**Windows / macOS (Testing):**
Experimental builds are now available for testing. You can download the latest installers from the [Releases](https://github.com/diegoveraniego/cuali-gtk/releases) page.

## Compilation & Usage

Building Cuali is straightforward. Clone the repository and run:

```bash
make
```

This will generate the `cuali-gtk` executable binary. To start the application:

```bash
./cuali-gtk
```

(Optional) To clean the compiled files, run `make clean`.

## How to Use

1. **Create a New Project**: Click on "Create new project" on the welcome screen and choose a location to save your `.sqlite3` database.
2. **Import Documents**: Navigate to the "Documents" tab, click the "+" (Add) button, and select your `.txt` or `.pdf` files.
3. **Code Text**: Highlight the text you want to code. A popover will appear allowing you to select an existing tag or create a new one.
4. **Interact with Highlights**: Click any highlighted snippet to see assigned tags, add new ones, or remove existing ones.
5. **Analyze Results**: Go to the "Results" tab to view all quotes grouped by tags, alongside a frequency breakdown.

## Architecture & Design Decisions

- **Why C + GTK4?** For data analysis, performance is key. C provides a low memory footprint and rapid execution. GTK4 ensures maturity, reliability, and native integration with the GNOME ecosystem.
- **Why SQLite?** An embedded, serverless database is the perfect fit for a privacy-respecting desktop application.
- **Why Libadwaita?** It provides modern, accessible, and highly polished UI components that feel right at home on modern Linux desktops.

## Project Structure

```plaintext
cuali-gtk/
├── src/
│   ├── main.c           # Application entry point
│   ├── window.c         # GUI and main logic
│   ├── database.c       # SQLite operations
│   └── importer.c       # Document import processing
├── include/
│   ├── window.h
│   ├── database.h
│   └── importer.h
├── Makefile             # Build configuration
└── README.md
```

## Roadmap

- [ ] Fix positioning/tiling bug when opening projects
- [ ] Fix UI issues and improve layout stability
- [ ] Implement filtering system in the results view
- [ ] Improve performance when loading extremely large documents
- [ ] Add export support (Excel, CSV)
- [ ] Multi-language support (i18n)
- [ ] Customizable themes

## Contributing & Support

Contributions are welcome. If you find a bug, have a feature request, or want to contribute code, please feel free to open an issue or submit a Pull Request.

---

Developed by Diego (2026).

Built with ❤️.

Inspired by Taguette. Distributed under the BSD 3-Clause License.
