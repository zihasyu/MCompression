#ifndef BiSearch_H
#define BiSearch_H

#include "absmethod.h"
#include "odess_similarity_detection.h"
#include "zlib.h"
#include <filesystem>
#include <fstream>
using namespace std;

class BiSearch : public AbsMethod
{
private:
    const string myName_ = "BiSearch";
    PLChunk plchunk;
    uint64_t logicalId = 0;
    int DedupGap = 0;
    int Version = 0;
    int localError = 0;
    bool localFlag = true;
    double LZ4_RATIO = 4;
    double FiOffset = 0;
    uint64_t lz4LogicalSize = 0;
    uint64_t lz4UniqueSize = 0;
    uint64_t localLogicalSize = 0;
    uint64_t localUniqueSize = 0;
    FeatureIndexTable table;
    uint64_t sameContainerTimes = 0;
    uint64_t lastContainerId = 0;

    uint64_t costSum = 0;
    vector<char> chunkingbuffer;
    uint64_t bugCount = 0;
    z_stream defstream;
    long errorCount = 0;
    long sameCount = 0;
    double β = 10;
    int LOCAL_MAX_ERROR = 2;

public:
    std::chrono::time_point<std::chrono::high_resolution_clock>
        startTime,
        endTime;
    BiSearch(double ratio);
    ~BiSearch();
    void ProcessTrace();
    bool estimateGain(uint64_t chunkSize, uint64_t deltaSize);
    long deflateCompress(uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);
    void Version_log(double time);
    void Version_log(double time, double chunktime);
    void PrintChunkInfo(string inputDirpath, int chunkingMethod, int method, int fileNum, int64_t time, double ratio);
    void PrintChunkInfo(string inputDirpath, int chunkingMethod, int method, int fileNum, int64_t time, double ratio, double chunktime);
};
#endif