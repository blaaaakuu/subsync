#include "speechrec.h"
#include "text/utf8.h"
#include "general/exception.h"
#include <cstring>
#include <cstdint>

using namespace std;


SpeechRecognition::SpeechRecognition() :
	m_ps(NULL),
	m_config(NULL),
	m_endpointer(NULL),
	m_utteranceStarted(false),
	m_endpointFrameSize(0),
	m_sampleRate(0),
	m_framePeriod(0.0),
	m_deltaTime(-1.0),
	m_utteranceOffset(0.0),
	m_timeBase(0.0),
	m_minProb(1.0f),
	m_minLen(0)
{
	m_config = ps_config_init(NULL);
	if (m_config == NULL)
		throw EXCEPTION("can't init Sphinx configuration")
			.module("SpeechRecognition", "ps_config_init");
}

SpeechRecognition::~SpeechRecognition()
{
	if (m_endpointer)
		ps_endpointer_free(m_endpointer);
	if (m_ps)
		ps_free(m_ps);
	ps_config_free(m_config);
}

void SpeechRecognition::setParam(const string &key, const string &val)
{
	// PocketSphinx 5 configuration names no longer have the command-line '-'
	// prefix used by the legacy model manifests.
	const string name = !key.empty() && key[0] == '-' ? key.substr(1) : key;
	const ps_type_t type = ps_config_typeof(m_config, name.c_str());
	if (!type || !ps_config_set_str(m_config, name.c_str(), val.c_str()))
		throw EXCEPTION("parameter not supported")
			.module("SpeechRecognition", "setParameter")
			.add("parameter", key)
			.add("value", val);
}

void SpeechRecognition::addWordsListener(WordsListener listener)
{
	m_wordsNotifier.addListener(listener);
}

bool SpeechRecognition::removeWordsListener(WordsListener listener)
{
	return m_wordsNotifier.removeListener(listener);
}

void SpeechRecognition::setMinWordProb(float minProb)
{
	m_minProb = minProb;
}

void SpeechRecognition::setMinWordLen(unsigned minLen)
{
	m_minLen = minLen;
}

void SpeechRecognition::start(const AVStream *stream)
{
	const int32_t frate = static_cast<int32_t>(
			ps_config_int(m_config, "frate"));
	if (frate == 0)
		throw EXCEPTION("can't get frame rate value")
			.module("SpeechRecognition", "ps_config_int");
	m_framePeriod = 1.0 / static_cast<double>(frate);
	m_sampleRate = static_cast<int>(ps_config_int(m_config, "samprate"));
	if (m_sampleRate == 0)
		throw EXCEPTION("can't get sample rate value")
			.module("SpeechRecognition", "ps_config_int");

	if ((m_ps = ps_init(m_config)) == NULL)
		throw EXCEPTION("can't init Sphinx engine")
			.module("SpeechRecognition", "ps_init");

	resetEndpoint();

	m_utteranceStarted = false;
	m_pendingSamples.clear();
	m_deltaTime = -1.0;
	m_timeBase = av_q2d(stream->time_base);
}

void SpeechRecognition::stop()
{
	if (m_ps)
	{
		finishEndpoint();

		if (m_utteranceStarted)
			finishUtterance();

		ps_free(m_ps);
		m_ps = NULL;
	}
	if (m_endpointer)
	{
		ps_endpointer_free(m_endpointer);
		m_endpointer = NULL;
	}
	m_pendingSamples.clear();
}

void SpeechRecognition::feed(const AVFrame *frame)
{
	if (m_deltaTime < 0.0)
		m_deltaTime = m_timeBase * frame->pts;

	const int16 *data = (const int16*) frame->data[0];
	size_t size = frame->nb_samples;

	m_pendingSamples.insert(m_pendingSamples.end(), data, data + size);
	size_t consumed = 0;
	while (m_pendingSamples.size() - consumed >= m_endpointFrameSize)
	{
		processEndpointFrame(m_pendingSamples.data() + consumed);
		consumed += m_endpointFrameSize;
	}
	if (consumed)
		m_pendingSamples.erase(
				m_pendingSamples.begin(),
				m_pendingSamples.begin() + consumed);
}

void SpeechRecognition::processSpeech(const int16 *data, size_t size)
{
	const int no = ps_process_raw(m_ps, data, size, FALSE, FALSE);
	if (no < 0)
		throw EXCEPTION("speech recognition error")
			.module("SpeechRecognition", "ps_process_raw");
}

void SpeechRecognition::processEndpointFrame(const int16 *frame)
{
	const bool wasInSpeech = ps_endpointer_in_speech(m_endpointer);
	const int16 *speech = ps_endpointer_process(m_endpointer, frame);
	const bool inSpeech = ps_endpointer_in_speech(m_endpointer);

	if (speech)
	{
		if (!m_utteranceStarted)
		{
			if (ps_start_utt(m_ps))
				throw EXCEPTION("can't start utterance")
					.module("SpeechRecognition", "ps_start_utt");
			m_utteranceStarted = true;
			m_utteranceOffset =
				m_deltaTime + ps_endpointer_speech_start(m_endpointer);
		}
		processSpeech(speech, m_endpointFrameSize);
	}

	if (wasInSpeech && !inSpeech && m_utteranceStarted)
		finishUtterance();
}

void SpeechRecognition::finishUtterance()
{
	if (ps_end_utt(m_ps))
		throw EXCEPTION("can't end utterance")
			.module("SpeechRecognition", "ps_end_utt");

	if (m_utteranceStarted)
		parseUtterance();

	m_utteranceStarted = false;
}

void SpeechRecognition::finishEndpoint()
{
	if (!m_endpointer || m_pendingSamples.empty())
		return;

	const bool wasInSpeech = ps_endpointer_in_speech(m_endpointer);
	size_t speechSize = 0;
	const int16 *speech = ps_endpointer_end_stream(
			m_endpointer,
			m_pendingSamples.data(),
			m_pendingSamples.size(),
			&speechSize);
	if (speech && speechSize)
	{
		if (!m_utteranceStarted)
		{
			if (ps_start_utt(m_ps))
				throw EXCEPTION("can't start utterance")
					.module("SpeechRecognition", "ps_start_utt");
			m_utteranceStarted = true;
			m_utteranceOffset =
				m_deltaTime + ps_endpointer_speech_start(m_endpointer);
		}
		processSpeech(speech, speechSize);
	}
	const bool inSpeech = ps_endpointer_in_speech(m_endpointer);
	if (wasInSpeech && !inSpeech && m_utteranceStarted)
		finishUtterance();

	m_pendingSamples.clear();
}

void SpeechRecognition::resetEndpoint()
{
	if (m_endpointer)
		ps_endpointer_free(m_endpointer);

	m_endpointer = ps_endpointer_init(0, 0.0, PS_VAD_LOOSE,
			m_sampleRate, 0.0);
	if (!m_endpointer)
		throw EXCEPTION("can't initialize speech endpointer")
			.module("SpeechRecognition", "ps_endpointer_init");
	m_endpointFrameSize = ps_endpointer_frame_size(m_endpointer);
	m_pendingSamples.clear();
}

void SpeechRecognition::flush()
{
	finishEndpoint();
}

void SpeechRecognition::discontinuity()
{
	finishEndpoint();
	if (m_utteranceStarted)
		finishUtterance();

	m_deltaTime = -1.0;
	resetEndpoint();
}

void SpeechRecognition::parseUtterance()
{
	for (ps_seg_t *it=ps_seg_iter(m_ps); it!=NULL; it=ps_seg_next(it))
	{
		const char *text = ps_seg_word(it);
		if (text && text[0] != '<' && text[0] != '[')
		{
			string word = text;
			if ((word.size() > 3) && (word.back() == ')'))
			{
				size_t pos = word.size() - 2;
				while (word[pos] >= '0' && word[pos] <= '9' && pos > 0)
					pos--;

				if (pos <= word.size()-3 && word[pos] == '(')
					word.resize(pos);
			}

			int begin, end;
			ps_seg_frames(it, &begin, &end);
			const double time = ((double)begin+(double)end) * m_framePeriod / 2.0;

			const int pprob = ps_seg_prob(it, NULL, NULL, NULL);
			const float prob = logmath_exp(ps_get_logmath(m_ps), pprob);

			if (Utf8::size(word) >= m_minLen && prob >= m_minProb)
				m_wordsNotifier.notify(
						Word(word, time + m_utteranceOffset, 0.0f, prob));
		}
	}
}

