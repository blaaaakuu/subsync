#define CL_TARGET_OPENCL_VERSION 120
#include "wordmatcher.h"
#include <CL/cl.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace std;


namespace
{

const char *MATCH_WORDS_KERNEL = R"CLC(
__kernel void match_words(
		__global const uint *query,
		__global const uint *query_lower,
		const uint query_length,
		__global const uint *candidates,
		__global const uint *candidates_lower,
		__global const uint *candidate_offsets,
		__global const uint *candidate_lengths,
		const float min_similarity,
		__global uchar *matches,
		const uint count)
{
	const uint candidate_id = get_global_id(0);
	if (candidate_id >= count)
		return;

	const uint offset = candidate_offsets[candidate_id];
	const uint candidate_length = candidate_lengths[candidate_id];
	float score = 0.0f;
	for (uint i = 0; i < query_length; ++i)
	{
		const uint candidate = i < candidate_length
			? candidates[offset + i] : 0;
		const uint candidate_lower = i < candidate_length
			? candidates_lower[offset + i] : 0;
		if (query[i] == candidate)
			score += 2.0f;
		else if (query_lower[i] == candidate_lower)
			score += 1.5f;
		else
			break;
	}

	const float denominator =
		(float)(query_length + candidate_length);
	const float similarity = denominator > 0.0f ? score / denominator : 0.0f;
	matches[candidate_id] = similarity >= min_similarity;
}
)CLC";


void checkCl(cl_int status, const char *operation)
{
	if (status != CL_SUCCESS)
	{
		ostringstream msg;
		msg << operation << " failed with OpenCL error " << status;
		throw runtime_error(msg.str());
	}
}


class ClBuffer
{
	public:
		ClBuffer() :
			m_buffer(nullptr),
			m_capacity(0)
		{
		}

		~ClBuffer()
		{
			release();
		}

		ClBuffer(const ClBuffer&) = delete;
		ClBuffer &operator=(const ClBuffer&) = delete;

		bool reserve(
				cl_context context,
				cl_mem_flags flags,
				size_t size)
		{
			size = max<size_t>(size, 1);
			if (size <= m_capacity)
				return false;

			size_t capacity = m_capacity ? m_capacity : 1024;
			while (capacity < size)
				capacity *= 2;

			cl_int status = CL_SUCCESS;
			cl_mem replacement = clCreateBuffer(
					context, flags, capacity, nullptr, &status);
			checkCl(status, "clCreateBuffer");
			release();
			m_buffer = replacement;
			m_capacity = capacity;
			return true;
		}

		void release()
		{
			if (m_buffer)
				clReleaseMemObject(m_buffer);
			m_buffer = nullptr;
			m_capacity = 0;
		}

		cl_mem get() const { return m_buffer; }

	private:
		cl_mem m_buffer;
		size_t m_capacity;
};


template <typename T>
void upload(
		cl_context context,
		cl_command_queue queue,
		ClBuffer &destination,
		const vector<T> &source,
		const char *operation)
{
	destination.reserve(
			context, CL_MEM_READ_ONLY, source.size() * sizeof(T));
	if (!source.empty())
		checkCl(clEnqueueWriteBuffer(queue, destination.get(), CL_FALSE,
					0, source.size() * sizeof(T), source.data(),
					0, nullptr, nullptr),
				operation);
}


template <typename T>
void uploadRange(
		cl_command_queue queue,
		ClBuffer &destination,
		const vector<T> &source,
		size_t offset,
		const char *operation)
{
	if (offset < source.size())
		checkCl(clEnqueueWriteBuffer(queue, destination.get(), CL_FALSE,
					offset * sizeof(T),
					(source.size() - offset) * sizeof(T),
					source.data() + offset, 0, nullptr, nullptr),
				operation);
}


class OpenClWordMatcher final : public WordMatcher
{
	public:
		OpenClWordMatcher() :
			m_context(nullptr),
			m_queue(nullptr),
			m_program(nullptr),
			m_kernel(nullptr)
		{
			try
			{
			cl_uint platformCount = 0;
			checkCl(clGetPlatformIDs(0, nullptr, &platformCount),
					"clGetPlatformIDs");
			if (!platformCount)
				throw runtime_error("no OpenCL platform is available");

			vector<cl_platform_id> platforms(platformCount);
			checkCl(clGetPlatformIDs(
						platformCount, platforms.data(), nullptr),
					"clGetPlatformIDs");

			cl_device_id device = nullptr;
			for (cl_platform_id platform : platforms)
			{
				cl_uint deviceCount = 0;
				cl_int status = clGetDeviceIDs(
						platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &deviceCount);
				if (status == CL_DEVICE_NOT_FOUND || !deviceCount)
					continue;
				checkCl(status, "clGetDeviceIDs");
				vector<cl_device_id> devices(deviceCount);
				checkCl(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU,
							deviceCount, devices.data(), nullptr),
						"clGetDeviceIDs");
				device = devices.front();
				break;
			}
			if (!device)
				throw runtime_error("no OpenCL GPU device is available");

			cl_int status = CL_SUCCESS;
			m_context = clCreateContext(nullptr, 1, &device, nullptr, nullptr,
					&status);
			checkCl(status, "clCreateContext");

			m_queue = clCreateCommandQueue(m_context, device, 0, &status);
			checkCl(status, "clCreateCommandQueue");

			const size_t sourceLength = char_traits<char>::length(
					MATCH_WORDS_KERNEL);
			m_program = clCreateProgramWithSource(m_context, 1,
					&MATCH_WORDS_KERNEL, &sourceLength, &status);
			checkCl(status, "clCreateProgramWithSource");

			status = clBuildProgram(m_program, 1, &device, "", nullptr, nullptr);
			if (status != CL_SUCCESS)
			{
				size_t logSize = 0;
				clGetProgramBuildInfo(m_program, device, CL_PROGRAM_BUILD_LOG,
						0, nullptr, &logSize);
				string log(logSize, '\0');
				if (logSize)
					clGetProgramBuildInfo(m_program, device,
							CL_PROGRAM_BUILD_LOG, log.size(), &log[0], nullptr);
				throw runtime_error("OpenCL kernel build failed: " + log);
			}

			m_kernel = clCreateKernel(m_program, "match_words", &status);
			checkCl(status, "clCreateKernel");
		}
		catch (...)
		{
			release();
			throw;
		}
	}

		~OpenClWordMatcher() override
		{
			release();
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
			upload(m_context, m_queue, m_query, batch.query,
					"write OpenCL query");
			upload(m_context, m_queue, m_queryLower, batch.queryLower,
					"write OpenCL lowercase query");
			const bool candidatesReallocated = m_candidates.reserve(
					m_context, CL_MEM_READ_ONLY,
					batch.candidates.size() * sizeof(uint32_t));
			const bool candidatesLowerReallocated =
				m_candidatesLower.reserve(
						m_context, CL_MEM_READ_ONLY,
						batch.candidatesLower.size() * sizeof(uint32_t));
			const size_t candidateUploadOffset =
				candidatesReallocated || candidatesLowerReallocated
					? 0 : m_uploadedCandidateCodepoints;
			uploadRange(m_queue, m_candidates, batch.candidates,
					candidateUploadOffset, "write OpenCL candidates");
			uploadRange(m_queue, m_candidatesLower, batch.candidatesLower,
					candidateUploadOffset,
					"write OpenCL lowercase candidates");
			m_uploadedCandidateCodepoints = batch.candidates.size();
			upload(m_context, m_queue, m_offsets, batch.candidateOffsets,
					"write OpenCL candidate offsets");
			upload(m_context, m_queue, m_lengths, batch.candidateLengths,
					"write OpenCL candidate lengths");
			m_matches.reserve(m_context, CL_MEM_WRITE_ONLY,
					candidates.size() * sizeof(cl_uchar));

			cl_mem queryMem = m_query.get();
			cl_mem queryLowerMem = m_queryLower.get();
			cl_mem candidatesMem = m_candidates.get();
			cl_mem candidatesLowerMem = m_candidatesLower.get();
			cl_mem offsetsMem = m_offsets.get();
			cl_mem lengthsMem = m_lengths.get();
			cl_mem matchesMem = m_matches.get();
			const cl_uint queryLength = static_cast<cl_uint>(batch.queryLength);
			const cl_uint count = static_cast<cl_uint>(candidates.size());

			checkCl(clSetKernelArg(m_kernel, 0, sizeof(cl_mem), &queryMem),
					"set OpenCL query argument");
			checkCl(clSetKernelArg(m_kernel, 1, sizeof(cl_mem), &queryLowerMem),
					"set OpenCL lowercase query argument");
			checkCl(clSetKernelArg(m_kernel, 2, sizeof(cl_uint), &queryLength),
					"set OpenCL query length argument");
			checkCl(clSetKernelArg(m_kernel, 3, sizeof(cl_mem), &candidatesMem),
					"set OpenCL candidates argument");
			checkCl(clSetKernelArg(m_kernel, 4, sizeof(cl_mem),
						&candidatesLowerMem),
					"set OpenCL lowercase candidates argument");
			checkCl(clSetKernelArg(m_kernel, 5, sizeof(cl_mem), &offsetsMem),
					"set OpenCL candidate offsets argument");
			checkCl(clSetKernelArg(m_kernel, 6, sizeof(cl_mem), &lengthsMem),
					"set OpenCL candidate lengths argument");
			checkCl(clSetKernelArg(m_kernel, 7, sizeof(float), &minSimilarity),
					"set OpenCL similarity argument");
			checkCl(clSetKernelArg(m_kernel, 8, sizeof(cl_mem), &matchesMem),
					"set OpenCL result argument");
			checkCl(clSetKernelArg(m_kernel, 9, sizeof(cl_uint), &count),
					"set OpenCL count argument");

			const size_t globalSize = candidates.size();
			checkCl(clEnqueueNDRangeKernel(m_queue, m_kernel, 1, nullptr,
						&globalSize, nullptr, 0, nullptr, nullptr),
					"clEnqueueNDRangeKernel");

			m_matched.resize(candidates.size());
			checkCl(clEnqueueReadBuffer(m_queue, m_matches.get(), CL_TRUE,
						0, m_matched.size() * sizeof(cl_uchar),
						m_matched.data(),
						0, nullptr, nullptr),
					"read OpenCL matching results");

			vector<size_t> result;
			for (size_t i = 0; i < m_matched.size(); ++i)
				if (m_matched[i])
					result.push_back(i);
			return result;
		}

		const char *backendName() const override
		{
			return "opencl";
		}

	private:
		void release()
		{
			m_query.release();
			m_queryLower.release();
			m_candidates.release();
			m_candidatesLower.release();
			m_offsets.release();
			m_lengths.release();
			m_matches.release();
			if (m_kernel) clReleaseKernel(m_kernel);
			if (m_program) clReleaseProgram(m_program);
			if (m_queue) clReleaseCommandQueue(m_queue);
			if (m_context) clReleaseContext(m_context);
			m_kernel = nullptr;
			m_program = nullptr;
			m_queue = nullptr;
			m_context = nullptr;
		}

		cl_context m_context;
		cl_command_queue m_queue;
		cl_program m_program;
		cl_kernel m_kernel;
		WordBatchEncoder m_encoder;
		ClBuffer m_query;
		ClBuffer m_queryLower;
		ClBuffer m_candidates;
		ClBuffer m_candidatesLower;
		ClBuffer m_offsets;
		ClBuffer m_lengths;
		ClBuffer m_matches;
		vector<cl_uchar> m_matched;
		size_t m_uploadedCandidateCodepoints = 0;
};

}


unique_ptr<WordMatcher> createOpenClWordMatcher()
{
	return unique_ptr<WordMatcher>(new OpenClWordMatcher());
}
