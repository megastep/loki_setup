# Theming the GTK 3 installer

Put a file named `setup.gtk3.css` in your media’s `setup.data/` directory,
alongside `setup.xml` and `setup.gtk3.ui`. GTK 3 loads it automatically at
startup. The stylesheet is optional. With no stylesheet, the installer uses
the system GTK theme. If the file fails to load, the installer prints a
diagnostic to standard error and continues with the system theme.

## Write the stylesheet

Use GTK 3 CSS and prefix your selectors with `.loki-setup` to target the
installer, license, and README windows. Rules are loaded at GTK application
priority, and the system theme continues to style everything the stylesheet
leaves unspecified. The example below gives the windows a warm background
and styles the **Begin Install** button.

```css
.loki-setup {
  background-color: #f4f1e8;
  color: #1b3831;
}

.loki-setup .suggested-action {
  background-image: none;
  background-color: #12675b;
  border-color: #12675b;
  color: white;
  text-shadow: none;
}

.loki-setup .suggested-action:hover {
  background-color: #0c5449;
  border-color: #0c5449;
}
```

Use `setup.gtk3.ui` for changes to widget placement or structure. If you
select a custom GtkBuilder file with `SETUP_GTK3_UI_FILE`, the stylesheet
still comes from `setup.data/setup.gtk3.css` on the media.

## Stage and preview

Place your stylesheet in `image/setup.data/` before staging external media
with `make install-image`. That target copies the stylesheet when it stages
GTK 3 and the destination has no stylesheet of its own. You can also add or
edit the file directly in the destination's `setup.data/` directory.

Run `./setup.sh` from the staged media on a system with GTK 3 and a graphical
display. Preview the options and any license or README windows your package
provides. GTK 2, legacy GTK 1, and console launches use their own presentation.
