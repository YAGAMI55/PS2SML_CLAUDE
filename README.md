# PS2 Simple MultiLoader

Builds `LOADER.ELF` with the official PS2DEV container.

## Disc layout

```text
/
├── LOADER.ELF
├── PS2MLCONF/
│   ├── loader.cfg
│   ├── background.png   (or .jpg/.jpeg)
│   └── font.ttf
└── ... ELF files referenced by loader.cfg
```

Example:

```ini
[Settings]
VideoMode=AUTO
FontSize=24
ColorInactive=FFFFFF
ColorActive=FF0000
BackgroundImage=background.png
FontFile=font.ttf

[Items]
Game 1|First game|GAME1.ELF
Game 2|Second game|GAME2.ELF
```

Relative ELF paths are resolved as `cdrom0:\<path>`.
A path containing a device prefix such as `mc0:` or `mass:` is left unchanged.

The loader supports UTF-8 menu text through FreeType.
