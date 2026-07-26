#include "wordmatcher.h"
#include "text/dictionary.h"
#include "text/utf8.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

using namespace std;


namespace
{

class CpuWordMatcher final : public WordMatcher
{
	public:
		vector<size_t> findMatches(
				const string &query,
				const vector<const Word*> &candidates,
				float minSimilarity) override
		{
			vector<size_t> matches;
			for (size_t i = 0; i < candidates.size(); ++i)
			{
				if (compareWords(query, candidates[i]->text) >= minSimilarity)
					matches.push_back(i);
			}
			return matches;
		}

		const char *backendName() const override
		{
			return "cpu";
		}
};


class DispatchingWordMatcher final : public WordMatcher
{
	public:
		DispatchingWordMatcher(
				unique_ptr<WordMatcher> accelerator,
				size_t gpuMinBatch) :
			m_cpu(new CpuWordMatcher()),
			m_accelerator(move(accelerator)),
			m_gpuMinBatch(gpuMinBatch)
		{
		}

		vector<size_t> findMatches(
				const string &query,
				const vector<const Word*> &candidates,
				float minSimilarity) override
		{
			if (m_accelerator && candidates.size() >= m_gpuMinBatch)
			{
				try
				{
					return m_accelerator->findMatches(
							query, candidates, minSimilarity);
				}
				catch (...)
				{
					// Automatic mode must remain usable when a driver disappears,
					// a device is reset, or a workload exceeds device limits.
					m_accelerator.reset();
				}
			}
			return m_cpu->findMatches(query, candidates, minSimilarity);
		}

		const char *backendName() const override
		{
			return m_accelerator ? m_accelerator->backendName() : "cpu";
		}

	private:
		unique_ptr<WordMatcher> m_cpu;
		unique_ptr<WordMatcher> m_accelerator;
		size_t m_gpuMinBatch;
};


void appendEncoded(
		const string &word,
		vector<uint32_t> &original,
		vector<uint32_t> &lower)
{
	for (Utf8::iterator it(word); *it; ++it)
	{
		original.push_back(*it);
		lower.push_back(it.toLower());
	}
}

}


struct WordBatchEncoder::Impl
{
	struct EncodedWord
	{
		string text;
		uint32_t offset = 0;
		uint32_t length = 0;
	};

	EncodedWordBatch batch;
	unordered_map<const Word*, EncodedWord> words;
};


WordBatchEncoder::WordBatchEncoder() :
	m_impl(new Impl())
{
}


WordBatchEncoder::~WordBatchEncoder() = default;


const EncodedWordBatch &WordBatchEncoder::encode(
		const string &query,
		const vector<const Word*> &candidates)
{
	EncodedWordBatch &batch = m_impl->batch;
	batch.query.clear();
	batch.queryLower.clear();
	appendEncoded(query, batch.query, batch.queryLower);
	batch.queryLength = batch.query.size();
	if (batch.queryLength > numeric_limits<uint32_t>::max())
		throw length_error("query is too long for a GPU word batch");
	if (candidates.size() > numeric_limits<uint32_t>::max())
		throw length_error("too many candidates for a GPU word batch");

	batch.candidateOffsets.clear();
	batch.candidateLengths.clear();
	batch.candidateOffsets.reserve(candidates.size());
	batch.candidateLengths.reserve(candidates.size());

	for (const Word *candidate : candidates)
	{
		if (!candidate)
			throw invalid_argument("word batch contains a null candidate");

		Impl::EncodedWord &entry = m_impl->words[candidate];
		if (entry.text != candidate->text)
		{
			entry.text = candidate->text;
			const size_t offset = batch.candidates.size();
			if (offset > numeric_limits<uint32_t>::max())
				throw length_error(
						"candidate cache is too large for a GPU matcher");
			appendEncoded(
					entry.text, batch.candidates, batch.candidatesLower);
			const size_t length = batch.candidates.size() - offset;
			if (length > numeric_limits<uint32_t>::max()
					|| length > numeric_limits<uint32_t>::max() - offset)
				throw length_error(
						"candidate cache is too large for a GPU matcher");
			entry.offset = static_cast<uint32_t>(offset);
			entry.length = static_cast<uint32_t>(length);
		}

		batch.candidateOffsets.push_back(entry.offset);
		batch.candidateLengths.push_back(entry.length);
	}
	return batch;
}


MatchingBackend parseMatchingBackend(const string &name)
{
	if (name.empty() || name == "auto")
		return MatchingBackend::automatic;
	if (name == "cpu")
		return MatchingBackend::cpu;
	if (name == "cuda")
		return MatchingBackend::cuda;
	if (name == "opencl")
		return MatchingBackend::opencl;
	throw invalid_argument("unknown matching backend: " + name);
}


const char *matchingBackendName(MatchingBackend backend)
{
	switch (backend)
	{
		case MatchingBackend::automatic: return "auto";
		case MatchingBackend::cpu:       return "cpu";
		case MatchingBackend::cuda:      return "cuda";
		case MatchingBackend::opencl:    return "opencl";
	}
	return "unknown";
}


vector<string> availableMatchingBackends()
{
	vector<string> backends = { "cpu" };
#if defined(SUBSYNC_ENABLE_CUDA)
	backends.push_back("cuda");
#endif
#if defined(SUBSYNC_ENABLE_OPENCL)
	backends.push_back("opencl");
#endif
	return backends;
}


unique_ptr<WordMatcher> createWordMatcher(
		MatchingBackend backend,
		size_t gpuMinBatch)
{
	if (backend == MatchingBackend::cpu)
		return unique_ptr<WordMatcher>(new CpuWordMatcher());

	if (backend == MatchingBackend::cuda)
	{
#if defined(SUBSYNC_ENABLE_CUDA)
		return createCudaWordMatcher();
#else
		throw runtime_error("CUDA matching backend was not compiled");
#endif
	}

	if (backend == MatchingBackend::opencl)
	{
#if defined(SUBSYNC_ENABLE_OPENCL)
		return createOpenClWordMatcher();
#else
		throw runtime_error("OpenCL matching backend was not compiled");
#endif
	}

	unique_ptr<WordMatcher> accelerator;
#if defined(SUBSYNC_ENABLE_CUDA)
	try
	{
		accelerator = createCudaWordMatcher();
	}
	catch (...)
	{
	}
#endif
#if defined(SUBSYNC_ENABLE_OPENCL)
	if (!accelerator)
	{
		try
		{
			accelerator = createOpenClWordMatcher();
		}
		catch (...)
		{
		}
	}
#endif
	return unique_ptr<WordMatcher>(
			new DispatchingWordMatcher(move(accelerator), gpuMinBatch));
}


EncodedWordBatch encodeWordBatch(
		const string &query,
		const vector<const Word*> &candidates)
{
	WordBatchEncoder encoder;
	return encoder.encode(query, candidates);
}
