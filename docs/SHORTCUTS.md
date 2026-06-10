# Keyboard shortcuts

Press **F1** in the app (or Help → Keyboard Shortcuts) for this list.

## Main window

| Shortcut | Action |
|---|---|
| `Ctrl+1` / `Ctrl+2` | Switch to File / Live mode |
| `F5` | Start — mode-aware: file export or live capture |
| `Shift+F5` | Stop the running live capture |
| `Ctrl+B` | Browse for a capture file |
| `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | Open / Save / Save-As project |
| `Ctrl+I` | Import ICD (.docx) |
| `Ctrl+T` | Toggle light / dark theme |
| `F1` | Shortcuts help |

## Field definition dialog (Configure Fields)

| Shortcut | Action |
|---|---|
| `Insert` | Add a new field row |
| `Ctrl+E` | Edit the selected field |
| `Ctrl+Delete` | Remove the selected field |
| Arrow keys / `Tab` | Move between rows and cells (native) |
| `Enter` on a row | Confirm dialog (native) |

Implementation notes: main-window shortcuts are `QShortcut`s created in the
`MainWindow` constructor; the dialog ones use
`Qt::WidgetWithChildrenShortcut` context so they fire while the table has
focus. Tooltips on the Add/Edit/Remove buttons advertise the keys.
