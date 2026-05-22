#pragma once

// Fork Awesome icon glyph macros — a trimmed subset of the upstream
// IconsForkAwesome.h (juliettef/IconFontCppHeaders), covering only the icons
// the editor actually uses. Codepoints live in the Unicode Private Use Area
// and are merged into the UI fonts by Loom::FontManager::MergeIconFont.
//
// Font file: resources/fonts/fork_awesome/fork_awesome.ttf  (SIL OFL 1.1)

#define ICON_MIN_FK 0xf000
#define ICON_MAX_FK 0xf2e0

#define ICON_FK_MUSIC          "\xef\x80\x81"  // U+F001  Music folder
#define ICON_FK_STAR           "\xef\x80\x85"  // U+F005  favorites / bookmarks
#define ICON_FK_FILM           "\xef\x80\x88"  // U+F008  Videos folder
#define ICON_FK_HOME           "\xef\x80\x95"  // U+F015  Home folder
#define ICON_FK_DOWNLOAD       "\xef\x80\x99"  // U+F019  Downloads folder
#define ICON_FK_FONT           "\xef\x80\xb1"  // U+F031  .ttf / .otf
#define ICON_FK_PICTURE_O      "\xef\x80\xbe"  // U+F03E  .hdr environment
#define ICON_FK_FOLDER         "\xef\x81\xbb"  // U+F07B  directory
#define ICON_FK_FOLDER_OPEN    "\xef\x81\xbc"  // U+F07C  directory (open)
#define ICON_FK_FILE_TEXT_O    "\xef\x83\xb6"  // U+F0F6  .loom scene (YAML)
#define ICON_FK_DESKTOP        "\xef\x84\x88"  // U+F108  Desktop folder
#define ICON_FK_FILE           "\xef\x85\x9b"  // U+F15B  generic file
#define ICON_FK_CUBE           "\xef\x86\xb2"  // U+F1B2  mesh / prefab
#define ICON_FK_FILE_IMAGE_O   "\xef\x87\x85"  // U+F1C5  image textures
#define ICON_FK_FILE_AUDIO_O   "\xef\x87\x87"  // U+F1C7  audio clips
#define ICON_FK_FILE_CODE_O    "\xef\x87\x89"  // U+F1C9  .lua scripts
