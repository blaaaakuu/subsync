#include "match/wordmatcher.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;


namespace
{

struct Options
{
	string backend = "all";
	size_t batch = 0;
	unsigned iterations = 30;
};


Options parseOptions(int argc, char **argv)
{
	Options options;
	for (int i = 1; i < argc; ++i)
	{
		const string arg = argv[i];
		auto readValue = [&](const char *name) -> string
		{
			if (i + 1 >= argc)
				throw invalid_argument(string(name) + " requires a value");
			return argv[++i];
		};

		if (arg == "--backend")
			options.backend = readValue("--backend");
		else if (arg == "--batch")
			options.batch = stoull(readValue("--batch"));
		else if (arg == "--iterations")
			options.iterations = stoul(readValue("--iterations"));
		else if (arg == "--help" || arg == "-h")
		{
			cout
				<< "Usage: subsync-matcher-benchmark [options]\n"
				<< "  --backend all|cpu|cuda|opencl\n"
				<< "  --batch COUNT       benchmark one candidate count\n"
				<< "  --iterations COUNT  timed repetitions (default: 30)\n";
			exit(0);
		}
		else
			throw invalid_argument("unknown option: " + arg);
	}

	if (!options.iterations)
		throw invalid_argument("--iterations must be greater than zero");
	return options;
}


vector<Word> makeWords(size_t count)
{
	static const vector<string> samples = {
		"Subtitle",
		"subtitle",
		"Subtitles",
		"captions",
		"Subsync",
		"sub",
	};

	vector<Word> words;
	words.reserve(count);
	for (size_t i = 0; i < count; ++i)
		words.emplace_back(samples[i % samples.size()], static_cast<float>(i));
	return words;
}


vector<const Word*> makePointers(const vector<Word> &words)
{
	vector<const Word*> pointers;
	pointers.reserve(words.size());
	for (const Word &word : words)
		pointers.push_back(&word);
	return pointers;
}


double percentile(vector<double> samples, double fraction)
{
	sort(samples.begin(), samples.end());
	const size_t index = min(
			samples.size() - 1,
			static_cast<size_t>(ceil(fraction * samples.size())) - 1);
	return samples[index];
}


void verifyUnicodeParity(WordMatcher &reference, WordMatcher &matcher)
{
	const vector<Word> words = {
		Word(u8"Żółw", 0.0f),
		Word(u8"żółw", 1.0f),
		Word(u8"ŻÓŁWIE", 2.0f),
		Word("turtle", 3.0f),
	};
	const vector<const Word*> candidates = makePointers(words);
	const vector<size_t> expected =
		reference.findMatches(u8"Żółw", candidates, 0.6f);
	const vector<size_t> actual =
		matcher.findMatches(u8"Żółw", candidates, 0.6f);
	if (actual != expected)
		throw runtime_error("Unicode result differs from the CPU reference");
}


void benchmark(
		const string &backend,
		const vector<size_t> &batchSizes,
		unsigned iterations)
{
	unique_ptr<WordMatcher> reference =
		createWordMatcher(MatchingBackend::cpu);
	unique_ptr<WordMatcher> matcher =
		createWordMatcher(parseMatchingBackend(backend));
	verifyUnicodeParity(*reference, *matcher);

	for (size_t batchSize : batchSizes)
	{
		const vector<Word> words = makeWords(batchSize);
		const vector<const Word*> candidates = makePointers(words);
		const vector<size_t> expected =
			reference->findMatches("Subtitle", candidates, 0.6f);
		const vector<size_t> actual =
			matcher->findMatches("Subtitle", candidates, 0.6f);
		if (actual != expected)
			throw runtime_error("result differs from the CPU reference");

		for (unsigned i = 0; i < 3; ++i)
			matcher->findMatches("Subtitle", candidates, 0.6f);

		size_t checksum = 0;
		vector<double> samples;
		samples.reserve(iterations);
		for (unsigned i = 0; i < iterations; ++i)
		{
			const auto start = chrono::steady_clock::now();
			const vector<size_t> matches =
				matcher->findMatches("Subtitle", candidates, 0.6f);
			const auto end = chrono::steady_clock::now();
			checksum += matches.size();
			samples.push_back(
					chrono::duration<double, milli>(end - start).count());
		}

		cout << left << setw(9) << backend
			<< right << setw(10) << batchSize
			<< setw(14) << fixed << setprecision(3)
			<< percentile(samples, 0.5)
			<< setw(14) << percentile(samples, 0.95)
			<< setw(14) << expected.size()
			<< setw(14) << checksum
			<< '\n';
	}
}

}


int main(int argc, char **argv)
{
	try
	{
		const Options options = parseOptions(argc, argv);
		const vector<size_t> batchSizes = options.batch
			? vector<size_t>{options.batch}
			: vector<size_t>{
				128, 512, 1024, 2048, 4096, 8192, 16384, 65536};
		const vector<string> backends = options.backend == "all"
			? availableMatchingBackends()
			: vector<string>{options.backend};

		cout << left << setw(9) << "backend"
			<< right << setw(10) << "candidates"
			<< setw(14) << "median ms"
			<< setw(14) << "p95 ms"
			<< setw(14) << "matches"
			<< setw(14) << "checksum"
			<< '\n';

		unsigned completed = 0;
		for (const string &backend : backends)
		{
			try
			{
				benchmark(backend, batchSizes, options.iterations);
				++completed;
			}
			catch (const exception &error)
			{
				cerr << backend << ": unavailable: " << error.what() << '\n';
				if (options.backend != "all")
					return 2;
			}
		}

		return completed ? 0 : 2;
	}
	catch (const exception &error)
	{
		cerr << "error: " << error.what() << '\n';
		return 1;
	}
}
