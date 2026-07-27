# Translating the SubSync GUI

SubSync uses GNU gettext catalogs for user-interface translations. Each
language lives under:

```text
subsync/locale/[LANG]/LC_MESSAGES/
  messages.po
  messages.mo
```

`messages.po` is the editable source catalog. `messages.mo` is the compiled
catalog loaded at runtime.

## Add or update a translation

1. Copy the closest existing `messages.po` into a directory named with the
   language's two-letter code.
2. Edit the catalog with a gettext-aware tool such as
   [Poedit](https://poedit.net).
3. Compile or export `messages.mo` beside the source catalog.
4. Start SubSync, select the language in Settings, and restart the application
   to verify the result.
5. Check dialogs at typical Windows display scaling so translated labels are
   not clipped.

Language metadata is defined in
[`subsync/data/languages.py`](../subsync/data/languages.py). Add a missing
two-letter/three-letter mapping there before using the language in assets or
the GUI.

When contributing a translation, include both the reviewed `.po` file and its
compiled `.mo` output in the pull request.
