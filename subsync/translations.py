import os, builtins
from subsync import config

import logging
logger = logging.getLogger(__name__)


initialized = False


languageNames = {
    'de': 'German (Deutsch)',
    'en': 'English',
    'es': 'Spanish (Español)',
    'fr': 'French (Français)',
    'ja': 'Japanese (日本語)',
    'pl': 'Polish (Polski)',
    'ru': 'Russian (Русский)',
    'zh-cn': 'Chinese, Simplified (简体中文)',
}


def normalizeLanguage(language):
    if not language:
        return language

    language = language.lower().replace('_', '-')
    if language in ('zh', 'zh-cn', 'zh-hans'):
        return 'zh-cn'
    return language.split('-', 1)[0]


def init():
    import gettext
    gettext.install('messages', localedir=config.localedir)
    global initialized
    initialized = True

def setLanguage(language):
    import gettext, locale, importlib
    try:
        lang = normalizeLanguage(language)
        if lang is None:
            localeName = locale.getdefaultlocale()[0]
            lang = normalizeLanguage(localeName)

        logger.info('changing translation language to %s', lang)

        if not lang or lang == 'en':
            gettext.install('messages', localedir=config.localedir)

        else:
            tr = gettext.translation('messages',
                    localedir=config.localedir,
                    languages=[lang])
            tr.install()

        global initialized
        initialized = True

        # workaround for languages being loaded before language is set
        import subsync.data.languages
        importlib.reload(subsync.data.languages)
        import subsync.data.descriptions
        importlib.reload(subsync.data.descriptions)

    except Exception as e:
        if language is None:
            logger.debug('translation language setup failed, %r', e, exc_info=False)
        else:
            logger.warning('translation language setup failed, %r', e, exc_info=False)

def listLanguages():
    try:
        langs = os.listdir(config.localedir)
    except:
        langs = []

    if 'en' not in langs:
        langs.append('en')

    return langs


def languageName(language):
    language = normalizeLanguage(language)
    return languageNames.get(language, language)


def _(msg):
    if initialized:
        gettext = builtins.__dict__.get('_', None)
        if gettext is not None:
            return gettext(msg)
    return msg
