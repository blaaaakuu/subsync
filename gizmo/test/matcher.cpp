#include "catch.hpp"
#include "match/wordmatcher.h"
#include "synchro/synchronizer.h"
#include <memory>
#include <vector>

using namespace std;


TEST_CASE("word matcher CPU backend preserves legacy similarity semantics")
{
	Word exact("Subtitle", 0.0f);
	Word caseFolded("subtitle", 1.0f);
	Word prefix("Subtitles", 2.0f);
	Word mismatch("captions", 3.0f);
	vector<const Word*> candidates = {
		&exact,
		&caseFolded,
		&prefix,
		&mismatch,
	};

	unique_ptr<WordMatcher> matcher =
		createWordMatcher(MatchingBackend::cpu);

	REQUIRE( string(matcher->backendName()) == "cpu" );
	REQUIRE(
			matcher->findMatches("Subtitle", candidates, 0.98f)
			== vector<size_t>({0}) );
	REQUIRE(
			matcher->findMatches("Subtitle", candidates, 0.95f)
			== vector<size_t>({0, 1}) );
	REQUIRE(
			matcher->findMatches("Subtitle", candidates, 0.90f)
			== vector<size_t>({0, 1, 2}) );
}


TEST_CASE("word matcher encodes Unicode batches for accelerators")
{
	Word first("żółw", 0.0f);
	Word second("ŻÓŁWIE", 1.0f);
	vector<const Word*> candidates = { &first, &second };

	const EncodedWordBatch batch = encodeWordBatch("Żółw", candidates);

	REQUIRE( batch.queryLength == 4 );
	REQUIRE( batch.candidateLengths == vector<uint32_t>({4, 6}) );
	REQUIRE( batch.candidateOffsets == vector<uint32_t>({0, 4}) );
	REQUIRE( batch.query.size() == batch.queryLength );
	REQUIRE( batch.candidates.size() == 10 );
}


TEST_CASE("word batch encoder reuses cached candidate encodings")
{
	Word first("first", 0.0f);
	Word second("second", 1.0f);
	vector<const Word*> candidates = { &first, &second };
	WordBatchEncoder encoder;

	const EncodedWordBatch &initial = encoder.encode("query", candidates);
	const size_t initialPoolSize = initial.candidates.size();
	REQUIRE( initial.candidateOffsets == vector<uint32_t>({0, 5}) );
	REQUIRE( initial.candidateLengths == vector<uint32_t>({5, 6}) );

	const EncodedWordBatch &reused = encoder.encode("other", { &second });
	REQUIRE( reused.candidates.size() == initialPoolSize );
	REQUIRE( reused.candidateOffsets == vector<uint32_t>({5}) );

	second.text = "changed";
	const EncodedWordBatch &changed = encoder.encode("other", { &second });
	REQUIRE( changed.candidates.size() == initialPoolSize + 7 );
	REQUIRE(
			changed.candidateOffsets
			== vector<uint32_t>({static_cast<uint32_t>(initialPoolSize)}) );
	REQUIRE( changed.candidateLengths == vector<uint32_t>({7}) );
}


TEST_CASE("synchronizer only batches candidates inside its time window")
{
	Synchronizer synchronizer(
			10.0f, 0.99, 2.0f, 1, 0.6f, "cpu", 1024);

	synchronizer.addRefWord(Word("matching", 100.0f));
	synchronizer.addSubWord(Word("matching", 0.0f));
	REQUIRE( synchronizer.getAllPoints().empty() );

	synchronizer.addSubWord(Word("matching", 95.0f));
	REQUIRE( synchronizer.getAllPoints().size() == 1 );
}
