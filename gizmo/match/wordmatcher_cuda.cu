#include "wordmatcher.h"
#include <cuda_runtime.h>
#include <sstream>
#include <stdexcept>

using namespace std;


namespace
{

void checkCuda(cudaError_t status, const char *operation)
{
	if (status != cudaSuccess)
	{
		ostringstream msg;
		msg << operation << " failed: " << cudaGetErrorString(status);
		throw runtime_error(msg.str());
	}
}


template <typename T>
class DeviceBuffer
{
	public:
		DeviceBuffer() :
			m_ptr(nullptr),
			m_capacity(0)
		{
		}

		~DeviceBuffer()
		{
			if (m_ptr)
				cudaFree(m_ptr);
		}

		DeviceBuffer(const DeviceBuffer&) = delete;
		DeviceBuffer &operator=(const DeviceBuffer&) = delete;

		bool reserve(size_t size)
		{
			if (size <= m_capacity)
				return false;

			size_t capacity = m_capacity ? m_capacity : 256;
			while (capacity < size)
				capacity *= 2;

			T *replacement = nullptr;
			checkCuda(cudaMalloc(reinterpret_cast<void**>(&replacement),
						capacity * sizeof(T)), "cudaMalloc");
			if (m_ptr)
				cudaFree(m_ptr);
			m_ptr = replacement;
			m_capacity = capacity;
			return true;
		}

		T *get() const { return m_ptr; }

	private:
		T *m_ptr;
		size_t m_capacity;
};


template <typename T>
void upload(
		DeviceBuffer<T> &destination,
		const vector<T> &source,
		const char *operation)
{
	destination.reserve(source.size());
	if (!source.empty())
		checkCuda(cudaMemcpy(destination.get(), source.data(),
					source.size() * sizeof(T), cudaMemcpyHostToDevice),
				operation);
}


template <typename T>
void uploadRange(
		DeviceBuffer<T> &destination,
		const vector<T> &source,
		size_t offset,
		const char *operation)
{
	if (offset < source.size())
		checkCuda(cudaMemcpy(destination.get() + offset,
					source.data() + offset,
					(source.size() - offset) * sizeof(T),
					cudaMemcpyHostToDevice),
				operation);
}


__global__ void matchWordsKernel(
		const uint32_t *query,
		const uint32_t *queryLower,
		uint32_t queryLength,
		const uint32_t *candidates,
		const uint32_t *candidatesLower,
		const uint32_t *candidateOffsets,
		const uint32_t *candidateLengths,
		float minSimilarity,
		uint8_t *matches,
		uint32_t count)
{
	const uint32_t candidateId = blockIdx.x * blockDim.x + threadIdx.x;
	if (candidateId >= count)
		return;

	const uint32_t offset = candidateOffsets[candidateId];
	const uint32_t candidateLength = candidateLengths[candidateId];
	float score = 0.0f;
	for (uint32_t i = 0; i < queryLength; ++i)
	{
		const uint32_t candidate = i < candidateLength
			? candidates[offset + i] : 0;
		const uint32_t candidateLower = i < candidateLength
			? candidatesLower[offset + i] : 0;
		if (query[i] == candidate)
			score += 2.0f;
		else if (queryLower[i] == candidateLower)
			score += 1.5f;
		else
			break;
	}

	const float denominator =
		static_cast<float>(queryLength + candidateLength);
	const float similarity = denominator > 0.0f ? score / denominator : 0.0f;
	matches[candidateId] = similarity >= minSimilarity;
}


class CudaWordMatcher final : public WordMatcher
{
	public:
		CudaWordMatcher()
		{
			int count = 0;
			checkCuda(cudaGetDeviceCount(&count), "cudaGetDeviceCount");
			if (count < 1)
				throw runtime_error("no CUDA device is available");
			checkCuda(cudaSetDevice(0), "cudaSetDevice");
		}

		vector<size_t> findMatches(
				const string &query,
				const vector<const Word*> &candidates,
				float minSimilarity) override
		{
			if (candidates.empty())
				return {};

			const EncodedWordBatch &batch =
				m_encoder.encode(query, candidates);
			upload(m_query, batch.query, "copy query to CUDA");
			upload(m_queryLower, batch.queryLower,
					"copy lowercase query to CUDA");
			const bool candidatesReallocated =
				m_candidates.reserve(batch.candidates.size());
			const bool candidatesLowerReallocated =
				m_candidatesLower.reserve(batch.candidatesLower.size());
			const size_t candidateUploadOffset =
				candidatesReallocated || candidatesLowerReallocated
				? 0 : m_uploadedCandidateCodepoints;
			uploadRange(m_candidates, batch.candidates,
					candidateUploadOffset, "copy candidates to CUDA");
			uploadRange(m_candidatesLower, batch.candidatesLower,
					candidateUploadOffset,
					"copy lowercase candidates to CUDA");
			m_uploadedCandidateCodepoints = batch.candidates.size();
			upload(m_offsets, batch.candidateOffsets,
					"copy candidate offsets to CUDA");
			upload(m_lengths, batch.candidateLengths,
					"copy candidate lengths to CUDA");
			m_matches.reserve(candidates.size());

			const uint32_t count = static_cast<uint32_t>(candidates.size());
			const uint32_t blockSize = 256;
			const uint32_t blockCount = (count + blockSize - 1) / blockSize;
			matchWordsKernel<<<blockCount, blockSize>>>(
					m_query.get(),
					m_queryLower.get(),
					static_cast<uint32_t>(batch.queryLength),
					m_candidates.get(),
					m_candidatesLower.get(),
					m_offsets.get(),
					m_lengths.get(),
					minSimilarity,
					m_matches.get(),
					count);
			checkCuda(cudaGetLastError(), "launch CUDA word matching kernel");

			m_matched.resize(candidates.size());
			checkCuda(cudaMemcpy(m_matched.data(), m_matches.get(),
						m_matched.size(),
						cudaMemcpyDeviceToHost), "copy CUDA matching results");

			vector<size_t> result;
			for (size_t i = 0; i < m_matched.size(); ++i)
				if (m_matched[i])
					result.push_back(i);
			return result;
		}

		const char *backendName() const override
		{
			return "cuda";
		}

	private:
		WordBatchEncoder m_encoder;
		DeviceBuffer<uint32_t> m_query;
		DeviceBuffer<uint32_t> m_queryLower;
		DeviceBuffer<uint32_t> m_candidates;
		DeviceBuffer<uint32_t> m_candidatesLower;
		DeviceBuffer<uint32_t> m_offsets;
		DeviceBuffer<uint32_t> m_lengths;
		DeviceBuffer<uint8_t> m_matches;
		vector<uint8_t> m_matched;
		size_t m_uploadedCandidateCodepoints = 0;
};

}


unique_ptr<WordMatcher> createCudaWordMatcher()
{
	return unique_ptr<WordMatcher>(new CudaWordMatcher());
}
