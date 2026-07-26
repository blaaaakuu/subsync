#ifndef __SPHINX_H__
#define __SPHINX_H__

#include "avout.h"
#include "text/words.h"
#include <pocketsphinx.h>
#include <string>
#include <vector>


class SpeechRecognition : public AVOutput
{
	public:
		SpeechRecognition();
		virtual ~SpeechRecognition();

		SpeechRecognition(const SpeechRecognition&) = delete;
		SpeechRecognition(SpeechRecognition&&) = delete;
		SpeechRecognition& operator= (const SpeechRecognition&) = delete;
		SpeechRecognition& operator= (SpeechRecognition&&) = delete;

		virtual void start(const AVStream *stream);
		virtual void stop();

		void setParam(const std::string &key, const std::string &val);

		void addWordsListener(WordsListener listener);
		bool removeWordsListener(WordsListener listener);

		void setMinWordProb(float minProb);
		void setMinWordLen(unsigned minLen);

		virtual void feed(const AVFrame *frame);
		virtual void flush();
		virtual void discontinuity();

	private:
		void processEndpointFrame(const int16 *frame);
		void processSpeech(const int16 *data, size_t size);
		void finishUtterance();
		void finishEndpoint();
		void resetEndpoint();
		void parseUtterance();

	private:
		ps_decoder_t *m_ps;
		ps_config_t *m_config;
		ps_endpointer_t *m_endpointer;

		bool m_utteranceStarted;
		size_t m_endpointFrameSize;
		int m_sampleRate;
		std::vector<int16> m_pendingSamples;

		double m_framePeriod;
		double m_deltaTime;
		double m_utteranceOffset;
		double m_timeBase;

		WordsNotifier m_wordsNotifier;
		float m_minProb;
		unsigned m_minLen;
};

#endif
