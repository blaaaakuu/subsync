# Assets, speech models, and dictionaries

SubSync assets extend the application without rebuilding it. The asset manager
understands three categories:

- speech-recognition models;
- translation dictionaries;
- application updater metadata.

The remote catalog is published as
[`assets.json`](https://github.com/sc0ty/subsync/releases/download/assets/assets.json).
The modernized fork continues to consume this established catalog for language
assets.

## Portable asset storage

The Windows portable launcher stores configuration, the catalog, downloads,
and installed assets inside the extracted application directory:

```text
subsync-portable/
  assets.json
  assets/
    speech/
    dict/
```

The package must therefore be extracted to a writable location. Moving the
whole directory preserves its settings and downloaded language data.

The `0.18.0.dev0` portable build includes the PocketSphinx 5.1.1 English model
and its descriptor, allowing English audio synchronization without a network
download.

## Speech-recognition models

Speech models convert reference audio into timed words. An asset contains an
acoustic model, language model, pronunciation dictionary, and a JSON
description named `[lang].speech`.

The selected reference language determines which model SubSync requests.
Models must be compatible with the PocketSphinx 5.x runtime used by the current
build. For model structure and training concepts, see the
[CMUSphinx documentation](https://cmusphinx.github.io/wiki/).

## Translation dictionaries

A dictionary is used when the subtitle and reference languages differ. Its
file name identifies the sorted language pair as `[lang1]-[lang2].dict`.

The format is UTF-8 text with one phrase per line. Equivalent translations are
separated by `|`, and lines beginning with `#` are comments:

```text
# Example dictionary
phrase1|translation1
phrase2|translation2a|translation2b
```

Dictionary generation helpers remain under `assets/dictmk`.

## Offline use

The command-line `--offline` option prevents attempts to retrieve missing
assets. Offline subtitle-to-subtitle matching does not need a speech model.
Audio matching needs an already-installed model for the reference language.
