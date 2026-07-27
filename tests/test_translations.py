import gettext
import re
from pathlib import Path

from subsync import config, translations
from tools.compile_translations import read_po


REQUESTED_LANGUAGES = {'de', 'en', 'es', 'fr', 'ja', 'ru', 'zh-cn'}
CATALOG_LANGUAGES = REQUESTED_LANGUAGES - {'en'}
FORMAT_FIELD = re.compile(
    r'\{[^{}\n]*\}|%\([^)]+\)[#0+ \-\d.]*[A-Za-z]|%[#0+ \-\d.]*[A-Za-z]'
)


def catalog_path(language, extension):
    return (
        Path(config.localedir)
        / language
        / 'LC_MESSAGES'
        / f'messages.{extension}'
    )


def test_requested_languages_are_available():
    assert REQUESTED_LANGUAGES <= set(translations.listLanguages())


def test_language_names_are_bilingual_except_english():
    assert translations.languageName('en') == 'English'
    assert translations.languageName('fr') == 'French (Français)'
    assert translations.languageName('de') == 'German (Deutsch)'
    assert translations.languageName('es') == 'Spanish (Español)'
    assert translations.languageName('ja') == 'Japanese (日本語)'
    assert translations.languageName('ru') == 'Russian (Русский)'
    assert translations.languageName('zh-cn') == 'Chinese, Simplified (简体中文)'


def test_chinese_locale_aliases_use_zh_cn_catalog():
    assert translations.normalizeLanguage('zh_CN') == 'zh-cn'
    assert translations.normalizeLanguage('zh-Hans') == 'zh-cn'
    assert translations.normalizeLanguage('zh-cn') == 'zh-cn'


def test_catalogs_can_be_selected_at_runtime():
    expected_settings = {
        'de': 'Einstellungen',
        'es': 'Configuración',
        'fr': 'Paramètres',
        'ja': '設定',
        'ru': 'Настройки',
        'zh_CN': '设置',
    }
    translations.init()
    try:
        for language, expected in expected_settings.items():
            translations.setLanguage(language)
            assert translations._('Settings') == expected
    finally:
        translations.setLanguage('en')


def test_catalogs_are_complete_and_compiled():
    source_ids = None
    expected_settings = {
        'de': 'Einstellungen',
        'es': 'Configuración',
        'fr': 'Paramètres',
        'ja': '設定',
        'ru': 'Настройки',
        'zh-cn': '设置',
    }

    for language in CATALOG_LANGUAGES:
        messages = read_po(catalog_path(language, 'po'))
        ids = set(messages) - {''}
        assert len(ids) >= 397
        if source_ids is None:
            source_ids = ids
        else:
            assert ids == source_ids

        with catalog_path(language, 'mo').open('rb') as stream:
            catalog = gettext.GNUTranslations(stream)
        assert catalog.gettext('Settings') == expected_settings[language]
        for msgid, msgstr in messages.items():
            assert catalog.gettext(msgid) == msgstr


def test_catalogs_preserve_format_fields_and_newlines():
    for language in CATALOG_LANGUAGES:
        for msgid, msgstr in read_po(catalog_path(language, 'po')).items():
            if not msgid:
                continue
            assert FORMAT_FIELD.findall(msgstr) == FORMAT_FIELD.findall(msgid)
            assert msgstr.count('\n') == msgid.count('\n')
            assert 'SUBSYNC_PLACEHOLDER' not in msgstr
            assert 'SUBSYNC_SEPARATOR' not in msgstr
