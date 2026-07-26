#ifndef __WORDMATCHER_H__
#define __WORDMATCHER_H__

#include "text/words.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>


enum class MatchingBackend
{
	automatic,
	cpu,
	cuda,
	opencl,
};


struct EncodedWordBatch
{
	std::vector<uint32_t> query;
	std::vector<uint32_t> queryLower;
	std::vector<uint32_t> candidates;
	std::vector<uint32_t> candidatesLower;
	std::vector<uint32_t> candidateOffsets;
	std::vector<uint32_t> candidateLengths;
	size_t queryLength;
};


class WordBatchEncoder
{
	public:
		WordBatchEncoder();
		~WordBatchEncoder();

		WordBatchEncoder(const WordBatchEncoder&) = delete;
		WordBatchEncoder &operator=(const WordBatchEncoder&) = delete;

		const EncodedWordBatch &encode(
				const std::string &query,
				const std::vector<const Word*> &candidates);

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
};


class WordMatcher
{
	public:
		virtual ~WordMatcher() = default;

		virtual std::vector<size_t> findMatches(
				const std::string &query,
				const std::vector<const Word*> &candidates,
				float minSimilarity) = 0;

		virtual const char *backendName() const = 0;
};


MatchingBackend parseMatchingBackend(const std::string &name);
const char *matchingBackendName(MatchingBackend backend);
std::vector<std::string> availableMatchingBackends();

std::unique_ptr<WordMatcher> createWordMatcher(
		MatchingBackend backend = MatchingBackend::automatic,
		size_t gpuMinBatch = 8192);

EncodedWordBatch encodeWordBatch(
		const std::string &query,
		const std::vector<const Word*> &candidates);

#if defined(SUBSYNC_ENABLE_CUDA)
std::unique_ptr<WordMatcher> createCudaWordMatcher();
#endif

#if defined(SUBSYNC_ENABLE_OPENCL)
std::unique_ptr<WordMatcher> createOpenClWordMatcher();
#endif

#endif
