#include "../../include/bisearch.h"
#define LOCAL_MAX_ERROR 2
BiSearch::BiSearch(double ratio)
{
    lz4ChunkBuffer = (uint8_t *)malloc(CONTAINER_MAX_SIZE * sizeof(uint8_t));
    mdCtx = EVP_MD_CTX_new();
    hashBuf = (uint8_t *)malloc(CHUNK_HASH_SIZE * sizeof(uint8_t));
    deltaMaxChunkBuffer = (uint8_t *)malloc(2 * CONTAINER_MAX_SIZE * sizeof(uint8_t));
    // only biSearch has
    // β = ratio;
    plchunk.chunkId = -1;
    plchunk.chunkType = DUP;
    plchunk.compressionRatio = 0.0;
}

BiSearch::~BiSearch()
{
    free(lz4ChunkBuffer);
    free(deltaMaxChunkBuffer);
    free(hashBuf);
    EVP_MD_CTX_free(mdCtx);
}

bool BiSearch::estimateGain(uint64_t chunkSize, uint64_t deltaSize)
{
    // lz4 cost
    double avgLz4CompressionRatio = (double)lz4LogicalSize / (double)lz4UniqueSize;
    double CostSelf = chunkSize - (chunkSize / avgLz4CompressionRatio);

    double newDeltaNum = deltachunkNum / basechunkNum;
    // double deltaGain = DeltaReduct / deltachunkNum;
    double deltaGain = DCESum / deltachunkNum;

    double futureDeltaCost = deltaGain * newDeltaNum;
    if (avgLz4CompressionRatio + futureDeltaCost / β > (chunkSize / deltaSize))
        return false;
    else
        return true;
}

void BiSearch::ProcessTrace()
{
    string tmpChunkHash;
    string tmpChunkContent;
    string hashStr;
    hashStr.assign(CHUNK_HASH_SIZE, 0);
    while (true)
    {

        if (recieveQueue->done_ && recieveQueue->IsEmpty())
        {
            // outputMQ_->done_ = true;
            recieveQueue->done_ = false;
            Version++;
            ads_Version++;
            SFnum = basechunkNum * 3;
            break;
        }
        Chunk_t tmpChunk;
        if (recieveQueue->Pop(tmpChunk))
        {
            // compute FP
            SetTime(startDedup);
            GenerateHash(mdCtx, tmpChunk.chunkPtr, tmpChunk.chunkSize, hashBuf);
            hashStr.assign((char *)hashBuf, 32);
            int findRes = FP_Find(hashStr);
            SetTime(endDedup);
            DedupTime += (endDedup - startDedup);

            if (findRes == -1)
            // unique chunk
            {
                tmpChunk.chunkID = uniquechunkNum;
                tmpChunk.deltaFlag = NO_DELTA;
                SetTime(startDedup);
                FP_Insert(hashStr, tmpChunk.chunkID);
                SetTime(endDedup);
                DedupTime += (endDedup - startDedup);

                DedupGap++;
                tmpChunkContent.assign((char *)tmpChunk.chunkPtr, tmpChunk.chunkSize);
                tmpChunkHash.assign((char *)hashBuf, CHUNK_HASH_SIZE);

                // unique chunk & locality try & in locality windows
                if (plchunk.chunkId + DedupGap < tmpChunk.chunkID - 1 && Version > 0 && localFlag == true)
                {
                    SetTime(startLocalityMatch);
                    uint8_t *deltachunk;
                    uint64_t tmpdeltachunksize = 0;
                    Chunk_t tmpbaseChunkinfo;
                    Chunk_t tmpLocalChunkInfo = dataWrite_->Get_Chunk_MetaInfo(plchunk.chunkId + DedupGap);
                    SetTime(endLocalityMatch);
                    LocalityMatchTime += (endLocalityMatch - startLocalityMatch);
                    // the chunk who locality find is delta chunk, then we need to find its basechunk as tmpchunk's base chunk
                    if (tmpLocalChunkInfo.deltaFlag == FINESSE_DELTA || tmpLocalChunkInfo.deltaFlag == LOCAL_DELTA)
                    {
                        tmpbaseChunkinfo = dataWrite_->Get_Chunk_Info(tmpLocalChunkInfo.basechunkID);
                        tmpChunk.basechunkID = tmpLocalChunkInfo.basechunkID;
                    }
                    // the chunk who locality find is basechunk, then turn again
                    else
                    {
                        tmpbaseChunkinfo = dataWrite_->Get_Chunk_Info(plchunk.chunkId + DedupGap);
                        tmpChunk.basechunkID = plchunk.chunkId + DedupGap;
                    }

                    // Locality match and need to delta compress
                    SetTime(startLocalityDelta);
                    deltachunk = xd3_encode(tmpChunk.chunkPtr, tmpChunk.chunkSize, tmpbaseChunkinfo.chunkPtr, tmpbaseChunkinfo.chunkSize, &tmpdeltachunksize, deltaMaxChunkBuffer);
                    SetTime(endLocalityDelta);
                    deltaCompressionTime += (endLocalityDelta - startLocalityDelta);
                    LocalityDeltaTmp = (endLocalityDelta - startLocalityDelta); // maybe not accept, then it should be add to Feature-base MatchTime

                    if (tmpdeltachunksize > tmpChunk.chunkSize)
                    {
                        // cout << "bug in unique chunk & locality try & in locality windows" << endl;
                        // cout << "tmpdeltachunksize:" << tmpdeltachunksize << " tmpchunk size is " << tmpChunk.chunkSize << " tmpchunk id is " << tmpChunk.chunkID << endl;
                        bugCount++;
                        tmpChunk.saveSize = tmpChunk.chunkSize;
                        tmpChunk.deltaFlag = NO_DELTA;
                    }
                    else
                    {
                        // memcpy(tmpbaseChunkinfo.chunkPtr, deltachunk,tmpdeltachunksize);//迁移到后面那个if里写比较好
                        tmpChunk.deltaFlag = LOCAL_DELTA;
                    }

                    if (tmpbaseChunkinfo.loadFromDisk)
                    {
                        free(tmpbaseChunkinfo.chunkPtr);
                        tmpbaseChunkinfo.chunkPtr = nullptr;
                    }

                    float tmpratio = tmpChunk.chunkSize / tmpdeltachunksize;

                    // unique chunk & locality hit &locality can be accept
                    // tmpratio > LZ4_RATIO && tmpChunk.deltaFlag != NO_DELTA
                    // estimateGain(tmpChunk.chunkSize, tmpdeltachunksize) && tmpChunk.deltaFlag != NO_DELTA
                    if (estimateGain(tmpChunk.chunkSize, tmpdeltachunksize) && tmpChunk.deltaFlag != NO_DELTA) //&&  ((plchunk.chunkType == FI && (tmpratio  >= plchunk.compressionRatio - FiOffset)) || plchunk.chunkType == DUP) )
                    {
                        tmpChunk.deltaFlag = LOCAL_DELTA;
                        tmpChunk.saveSize = tmpdeltachunksize;
                        memcpy(tmpChunk.chunkPtr, deltachunk, tmpChunk.saveSize);
                        free(deltachunk);
                        localUniqueSize += tmpChunk.saveSize;
                        localLogicalSize += tmpChunk.chunkSize;
                        localchunkSize += tmpChunk.saveSize;
                        localPrechunkSize += tmpChunk.chunkSize;
                        localError = 0;
                        StatsDeltaLocality(tmpChunk);
                        // save delta
                        dataWrite_->Chunk_Insert(tmpChunk);
                    }
                    // unique chunk & locality can't be accept
                    else
                    {

                        // unique chunk & in locality windows & running odess
                        uint64_t basechunkID = -1;
                        SetTime(startFeatureMatch);
                        startSF = std::chrono::high_resolution_clock::now();
                        auto superfeature = table.feature_generator_.GenerateSuperFeatures(tmpChunkContent);
                        endSF = std::chrono::high_resolution_clock::now();
                        SFTime += (endSF - startSF);
                        basechunkID = table.SF_Find(superfeature);
                        SetTime(endFeatureMatch);
                        FeatureMatchTime += (endFeatureMatch - startFeatureMatch);

                        if (tmpChunk.basechunkID == basechunkID && tmpChunk.deltaFlag != NO_DELTA)
                        {
                            tmpChunk.deltaFlag = LOCAL_DELTA;
                            tmpChunk.saveSize = tmpdeltachunksize;
                            memcpy(tmpChunk.chunkPtr, deltachunk, tmpChunk.saveSize);
                            free(deltachunk);
                            localUniqueSize += tmpChunk.saveSize;
                            localLogicalSize += tmpChunk.chunkSize;
                            localchunkSize += tmpChunk.saveSize;
                            localPrechunkSize += tmpChunk.chunkSize;
                            localError = 0;
                            StatsDeltaLocality(tmpChunk);
                            // save delta
                            dataWrite_->Chunk_Insert(tmpChunk);
                        }
                        // unique chunk & in locality windows & odess considered this is a base chunk
                        else if (basechunkID == -1)
                        {
                            if (deltachunk != nullptr)
                            {
                                free(deltachunk);
                                deltachunk = nullptr;
                            }
                            int tmpChunkLz4CompressSize = 0;
                            SetTime(startLz4);
                            tmpChunkLz4CompressSize = LZ4_compress_fast((char *)tmpChunk.chunkPtr, (char *)lz4ChunkBuffer, tmpChunk.chunkSize, tmpChunk.chunkSize, 3);
                            SetTime(endLz4);
                            lz4CompressionTime += (endLz4 - startLz4);
                            // FeatureMatchTime += LocalityDeltaTmp;
                            // FeatureMatchTime1 += LocalityDeltaTmp;
                            if (tmpChunkLz4CompressSize > 0)
                            {
                                tmpChunk.deltaFlag = NO_DELTA;
                                tmpChunk.saveSize = tmpChunkLz4CompressSize;
                            }
                            else
                            {
                                // cout << "lz4 compress error" << endl;
                                tmpChunk.deltaFlag = NO_LZ4;
                                tmpChunk.saveSize = tmpChunk.chunkSize;
                            }

                            localError++;
                            tmpChunk.basechunkID = -1;
                            table.SF_Insert(superfeature, tmpChunk.chunkID);
                            basechunkNum++;
                            basechunkSize += tmpChunk.saveSize;
                            lz4LogicalSize += tmpChunk.chunkSize;
                            lz4UniqueSize += tmpChunk.saveSize;
                            LocalReduct += tmpChunk.chunkSize - tmpChunk.saveSize;

                            if (localError > LOCAL_MAX_ERROR)
                            {
                                localFlag = false;
                            }

                            // save base
                            if (tmpChunk.deltaFlag == NO_LZ4)
                                dataWrite_->Chunk_Insert(tmpChunk);
                            else
                                dataWrite_->Chunk_Insert(tmpChunk, lz4ChunkBuffer);
                        }
                        // unique chunk & in locality windows & odess hits
                        else
                        {
                            if (deltachunk != nullptr)
                            {
                                free(deltachunk);
                                deltachunk = nullptr;
                            }
                            Chunk_t basechunkinfo;
                            uint8_t *deltachunk;
                            tmpChunk.saveSize = 0;
                            basechunkinfo = dataWrite_->Get_Chunk_Info(basechunkID);
                            SetTime(startFeatureDelta);
                            deltachunk = xd3_encode(tmpChunk.chunkPtr, tmpChunk.chunkSize, basechunkinfo.chunkPtr, basechunkinfo.chunkSize, &tmpChunk.saveSize, deltaMaxChunkBuffer);
                            SetTime(endFeatureDelta);
                            FeatureDeltaTime += (endFeatureDelta - startFeatureDelta);
                            FeatureMatchTime += LocalityDeltaTmp;
                            FeatureMatchTime1 += LocalityDeltaTmp;
                            if (tmpChunk.saveSize == 0)
                            // odess hit but odess bug
                            {
                                cout << "delta error" << endl;
                                return;
                            }
                            else
                            {
                                // odess hit but odess bug
                                if (tmpChunk.saveSize > tmpChunk.chunkSize)
                                {
                                    // cout << "bug in odess hit but odess bug" << endl;
                                    bugCount++;
                                    int tmpChunkLz4CompressSize = 0;
                                    SetTime(startLz4);
                                    tmpChunkLz4CompressSize = LZ4_compress_fast((char *)tmpChunk.chunkPtr, (char *)lz4ChunkBuffer, tmpChunk.chunkSize, tmpChunk.chunkSize, 3);
                                    SetTime(endLz4);
                                    lz4CompressionTime += (endLz4 - startLz4);
                                    if (tmpChunkLz4CompressSize > 0)
                                    {
                                        tmpChunk.deltaFlag = NO_DELTA;
                                        tmpChunk.saveSize = tmpChunkLz4CompressSize;
                                    }
                                    else
                                    {
                                        // cout << "lz4 compress error" << endl;
                                        tmpChunk.deltaFlag = NO_LZ4;
                                        tmpChunk.saveSize = tmpChunk.chunkSize;
                                    }
                                    localError++;
                                    tmpChunk.basechunkID = -1;
                                    table.SF_Insert(superfeature, tmpChunk.chunkID);
                                    basechunkNum++;
                                    basechunkSize += tmpChunk.saveSize;
                                    lz4LogicalSize += tmpChunk.chunkSize;
                                    lz4UniqueSize += tmpChunk.saveSize;
                                    LocalReduct += tmpChunk.chunkSize - tmpChunk.saveSize;
                                    if (localError > LOCAL_MAX_ERROR)
                                    {
                                        localFlag = false;
                                    }
                                    // save base
                                    if (tmpChunk.deltaFlag == NO_LZ4)
                                        dataWrite_->Chunk_Insert(tmpChunk);
                                    else
                                        dataWrite_->Chunk_Insert(tmpChunk, lz4ChunkBuffer);
                                }
                                else
                                {
                                    // unique chunk & in locality windows & odess hits &odess delta normally
                                    memcpy(tmpChunk.chunkPtr, deltachunk, tmpChunk.saveSize);
                                    if (tmpChunk.HeaderFlag == 0)
                                        plchunk.chunkId = basechunkID;
                                    plchunk.chunkType = FI;
                                    plchunk.compressionRatio = (double)tmpChunk.chunkSize / (double)tmpChunk.saveSize;
                                    DedupGap = 0;
                                    tmpChunk.deltaFlag = FINESSE_DELTA;
                                    tmpChunk.basechunkID = basechunkID;
                                    localError = 0;
                                    finessehit++;
                                    finessechunkSize += tmpChunk.saveSize;
                                    finessePrechunkSize += tmpChunk.chunkSize;
                                    StatsDeltaFeature(tmpChunk);
                                    localFlag = true;
                                    // save delta
                                    dataWrite_->Chunk_Insert(tmpChunk);
                                }
                                free(deltachunk);
                                if (basechunkinfo.loadFromDisk)
                                    free(basechunkinfo.chunkPtr);
                            }
                            deltaCompressionTime += (endFeatureDelta - startFeatureDelta);
                        }
                        // free(tmpChunkSF);
                    }
                }
                // odess try & not in locality windows
                else
                {
                    uint64_t basechunkID = -1;
                    SetTime(startFeatureMatch);
                    startSF = std::chrono::high_resolution_clock::now();
                    auto superfeature = table.feature_generator_.GenerateSuperFeatures(tmpChunkContent);
                    endSF = std::chrono::high_resolution_clock::now();
                    SFTime += (endSF - startSF);
                    basechunkID = table.SF_Find(superfeature);
                    SetTime(endFeatureMatch);
                    FeatureMatchTime += (endFeatureMatch - startFeatureMatch);

                    computeSFtimes++;
                    if (basechunkID == -1)
                    // odess try & not in locality windows &odess considered this is a base chunk
                    {
                        int tmpChunkLz4CompressSize = 0;
                        SetTime(startLz4);
                        tmpChunkLz4CompressSize = LZ4_compress_fast((char *)tmpChunk.chunkPtr, (char *)lz4ChunkBuffer, tmpChunk.chunkSize, tmpChunk.chunkSize, 3);
                        SetTime(endLz4);
                        lz4CompressionTime += (endLz4 - startLz4);
                        if (tmpChunkLz4CompressSize > 0)
                        {
                            tmpChunk.deltaFlag = NO_DELTA;
                            tmpChunk.saveSize = tmpChunkLz4CompressSize;
                        }
                        else
                        {
                            // cout << "lz4 compress error" << endl;
                            tmpChunk.deltaFlag = NO_LZ4;
                            tmpChunk.saveSize = tmpChunk.chunkSize;
                        }
                        tmpChunk.basechunkID = -1;

                        table.SF_Insert(superfeature, tmpChunk.chunkID);
                        basechunkNum++;
                        basechunkSize += tmpChunk.saveSize;
                        lz4LogicalSize += tmpChunk.chunkSize;
                        lz4UniqueSize += tmpChunk.saveSize;
                        LocalReduct += tmpChunk.chunkSize - tmpChunk.saveSize;
                        // save base
                        if (tmpChunk.deltaFlag == NO_LZ4)
                            dataWrite_->Chunk_Insert(tmpChunk);
                        else
                            dataWrite_->Chunk_Insert(tmpChunk, lz4ChunkBuffer);
                    }
                    else
                    // odess try & not in locality windows &odess hits
                    {
                        Chunk_t basechunkinfo;
                        tmpChunk.saveSize = 0;
                        uint8_t *deltachunk;
                        basechunkinfo = dataWrite_->Get_Chunk_Info(basechunkID);
                        SetTime(startFeatureDelta);
                        deltachunk = xd3_encode(tmpChunk.chunkPtr, tmpChunk.chunkSize, basechunkinfo.chunkPtr, basechunkinfo.chunkSize, &tmpChunk.saveSize, deltaMaxChunkBuffer);
                        SetTime(endFeatureDelta);
                        FeatureDeltaTime += (endFeatureDelta - startFeatureDelta);
                        if (tmpChunk.saveSize == 0)
                        {
                            cout << "delta error" << endl;
                            return;
                        }
                        else
                        {
                            if (tmpChunk.saveSize > tmpChunk.chunkSize)
                            {
                                // cout << "bug in odess try & not in locality windows &odess hits" << endl;
                                bugCount++;
                                int tmpChunkLz4CompressSize = 0;
                                tmpChunkLz4CompressSize = LZ4_compress_fast((char *)tmpChunk.chunkPtr, (char *)lz4ChunkBuffer, tmpChunk.chunkSize, tmpChunk.chunkSize, 3);
                                if (tmpChunkLz4CompressSize > 0)
                                {
                                    tmpChunk.deltaFlag = NO_DELTA;
                                    tmpChunk.saveSize = tmpChunkLz4CompressSize;
                                }
                                else
                                {
                                    // cout << "lz4 compress error" << endl;
                                    tmpChunk.deltaFlag = NO_LZ4;
                                    tmpChunk.saveSize = tmpChunk.chunkSize;
                                }
                                tmpChunk.basechunkID = -1;

                                table.SF_Insert(superfeature, tmpChunk.chunkID);
                                basechunkNum++;
                                basechunkSize += tmpChunk.saveSize;
                                lz4LogicalSize += tmpChunk.chunkSize;
                                lz4UniqueSize += tmpChunk.saveSize;
                                LocalReduct += tmpChunk.chunkSize - tmpChunk.saveSize;
                                // save base
                                if (tmpChunk.deltaFlag == NO_LZ4)
                                    dataWrite_->Chunk_Insert(tmpChunk);
                                else
                                    dataWrite_->Chunk_Insert(tmpChunk, lz4ChunkBuffer);
                            }
                            else
                            {
                                if (tmpChunk.HeaderFlag == 0)
                                    plchunk.chunkId = basechunkID;
                                plchunk.chunkType = FI;
                                plchunk.compressionRatio = (double)tmpChunk.chunkSize / (double)tmpChunk.saveSize;

                                memcpy(tmpChunk.chunkPtr, deltachunk, tmpChunk.saveSize); // new

                                DedupGap = 0;
                                tmpChunk.deltaFlag = FINESSE_DELTA;
                                tmpChunk.basechunkID = basechunkID;
                                finessehit++;
                                StatsDeltaFeature(tmpChunk);
                                localFlag = true;
                                // save delta
                                dataWrite_->Chunk_Insert(tmpChunk);
                            }
                            free(deltachunk);
                            if (basechunkinfo.loadFromDisk)
                                free(basechunkinfo.chunkPtr);
                        }
                    }
                }
                // dataWrite_->Chunk_Insert(tmpChunk);
                uniquechunkSize += tmpChunk.saveSize;
                uniquechunkNum++;
            }
            else
            {
                free(tmpChunk.chunkPtr);
                auto tmpInfo = dataWrite_->Get_Chunk_MetaInfo(findRes);
                tmpChunk = tmpInfo;
                localFlag = true;
                if (tmpChunk.HeaderFlag == 0)
                    plchunk.chunkId = findRes;
                plchunk.chunkType = DUP;
                DedupGap = 0;
                // lz4LogicalSize += tmpChunk.chunkSize;
                DedupReduct += tmpChunk.chunkSize;
            }
            if (tmpChunk.HeaderFlag == 0)
                dataWrite_->Recipe_Insert(tmpChunk.chunkID);
            else
                dataWrite_->Recipe_Header_Insert(tmpChunk.chunkID);

            logicalchunkNum++;
            logicalchunkSize += tmpChunk.chunkSize;
        }
    }
    recieveQueue->done_ = false;
    return;
}

void BiSearch::Version_log(double time)
{
    cout << "Version: " << ads_Version << endl;
    cout << "-----------------CHUNK NUM-----------------------" << endl;
    cout << "logical chunk num: " << logicalchunkNum << endl;
    cout << "unique chunk num: " << uniquechunkNum << endl;
    cout << "base chunk num: " << basechunkNum << endl;
    cout << "delta chunk num: " << deltachunkNum << endl;
    cout << "-----------------CHUNK SIZE-----------------------" << endl;
    cout << "logicalchunkSize is " << logicalchunkSize << endl;
    cout << "uniquechunkSize is " << uniquechunkSize << endl;
    cout << "base chunk size: " << basechunkSize << endl;
    cout << "delta chunk size: " << deltachunkSize << endl;
    cout << "-----------------METRICS-------------------------" << endl;
    cout << "Overall Compression Ratio: " << (double)logicalchunkSize / (double)uniquechunkSize << endl;
    cout << "DCC: " << (double)deltachunkNum / (double)uniquechunkNum << endl;
    cout << "DCR: " << (double)deltachunkOriSize / (double)deltachunkSize << endl;
    cout << "DCE: " << DCESum / (double)deltachunkNum << endl;
    cout << "-----------------Time------------------------------" << endl;
    cout << "Version time: " << time << "s" << endl;
    cout << "Throughput: " << (double)(logicalchunkSize - preLogicalchunkiSize) / time / 1024 / 1024 << "MiB/s" << endl;
    cout << "Reduce data speed: " << (double)(logicalchunkSize - uniquechunkSize) / time / 1024 / 1024 << "MiB/s" << endl;
    cout << "SF generation time: " << SFTime.count() - preSFTime.count() << "s" << endl;
    cout << "SF generation throughput: " << (double)(logicalchunkSize - preLogicalchunkiSize) / (SFTime.count() - preSFTime.count()) / 1024 / 1024 << "MiB/s" << endl;
    cout << "Dedup Time: " << DedupTime.count() << "s" << endl;
    cout << "Locality Match Time: " << LocalityMatchTime.count() << "s" << endl;
    cout << "Locality Delta Time: " << LocalityDeltaTime.count() << "s" << endl;
    cout << "Feature Match Time: " << FeatureMatchTime.count() << "s" << endl;
    cout << "Feature Delta Time: " << FeatureDeltaTime.count() << "s" << endl;
    cout << "Lz4 Compression Time: " << lz4CompressionTime.count() << "s" << endl;
    cout << "Delta Compression Time: " << deltaCompressionTime.count() << "s" << endl;
    cout << "-----------------OVERHEAD--------------------------" << endl;
    cout << "Index Overhead: " << (double)(uniquechunkNum * 96 + basechunkNum * 48) / 1024 / 1024 << "MiB" << endl;
    cout << "Recipe Overhead: " << (double)logicalchunkNum * 8 / 1024 / 1024 << "MiB" << endl;
    cout << "SF number: " << SFnum << endl;
    cout << "-----------------REDUCT----------------------------" << endl;
    cout << "dedup reduct size : " << DedupReduct / 1024 / 1024 << "MiB" << endl;
    cout << "delta reduct size : " << DeltaReduct / 1024 / 1024 << "MiB" << endl;
    cout << "local reduct size : " << LocalReduct / 1024 / 1024 << "MiB" << endl;
    cout << "Feature reduct size: " << FeatureReduct / 1024 / 1024 << "MiB" << endl;
    cout << "Locality reduct size: " << LocalityReduct / 1024 / 1024 << "MiB" << endl;
    cout << "-----------------END-------------------------------" << endl;

    preLogicalchunkiSize = logicalchunkSize;
    preuniquechunkSize = uniquechunkSize;
    preSFTime = SFTime;
}
void BiSearch::Version_log(double time, double chunktime)
{
    cout << "Version: " << ads_Version << endl;
    cout << "-----------------CHUNK NUM-----------------------" << endl;
    cout << "logical chunk num: " << logicalchunkNum << endl;
    cout << "unique chunk num: " << uniquechunkNum << endl;
    cout << "base chunk num: " << basechunkNum << endl;
    cout << "delta chunk num: " << deltachunkNum << endl;
    cout << "-----------------CHUNK SIZE-----------------------" << endl;
    cout << "logicalchunkSize is " << logicalchunkSize << endl;
    cout << "uniquechunkSize is " << uniquechunkSize << endl;
    cout << "base chunk size: " << basechunkSize << endl;
    cout << "delta chunk size: " << deltachunkSize << endl;
    cout << "-----------------METRICS-------------------------" << endl;
    cout << "Overall Compression Ratio: " << (double)logicalchunkSize / (double)uniquechunkSize << endl;
    cout << "DCC: " << (double)deltachunkNum / (double)uniquechunkNum << endl;
    cout << "DCR: " << (double)deltachunkOriSize / (double)deltachunkSize << endl;
    cout << "DCE: " << DCESum / (double)deltachunkNum << endl;
    cout << "-----------------Time------------------------------" << endl;
    cout << "Version time: " << time << "s" << endl;
    cout << "Throughput: " << (double)(logicalchunkSize - preLogicalchunkiSize) / time / 1024 / 1024 << "MiB/s" << endl;
    cout << "Reduce data speed: " << (double)(logicalchunkSize - preLogicalchunkiSize - uniquechunkSize + preuniquechunkSize) / time / 1024 / 1024 << "MiB/s" << endl;
    cout << "SF generation time: " << SFTime.count() - preSFTime.count() << "s" << endl;
    cout << "SF generation throughput: " << (double)(logicalchunkSize - preLogicalchunkiSize) / (SFTime.count() - preSFTime.count()) / 1024 / 1024 << "MiB/s" << endl;
    cout << "Chunk Time: " << chunktime << "s" << endl;
    cout << "Dedup Time: " << DedupTime.count() << "s" << endl;
    cout << "Locality Match Time: " << LocalityMatchTime.count() << "s" << endl;
    cout << "Locality Delta Time: " << LocalityDeltaTime.count() << "s" << endl;
    cout << "Feature Match Time: " << FeatureMatchTime.count() << "s" << endl;
    cout << "Feature Match Time1: " << FeatureMatchTime1.count() << "s" << endl;
    cout << "Feature Delta Time: " << FeatureDeltaTime.count() << "s" << endl;
    cout << "Lz4 Compression Time: " << lz4CompressionTime.count() << "s" << endl;
    cout << "Delta Compression Time: " << deltaCompressionTime.count() << "s" << endl;
    cout << "-----------------OVERHEAD--------------------------" << endl;
    cout << "Index Overhead: " << (double)(uniquechunkNum * 96 + basechunkNum * 48) / 1024 / 1024 << "MiB" << endl;
    cout << "Recipe Overhead: " << (double)logicalchunkNum * 8 / 1024 / 1024 << "MiB" << endl;
    cout << "SF number: " << SFnum << endl;
    cout << "-----------------REDUCT----------------------------" << endl;
    cout << "dedup reduct size : " << DedupReduct / 1024 / 1024 << "MiB" << endl;
    cout << "delta reduct size : " << DeltaReduct / 1024 / 1024 << "MiB" << endl;
    cout << "local reduct size : " << LocalReduct / 1024 / 1024 << "MiB" << endl;
    cout << "Feature reduct size: " << FeatureReduct / 1024 / 1024 << "MiB" << endl;
    cout << "Locality reduct size: " << LocalityReduct / 1024 / 1024 << "MiB" << endl;
    cout << "-----------------END-------------------------------" << endl;

    preLogicalchunkiSize = logicalchunkSize;
    preuniquechunkSize = uniquechunkSize;
    preSFTime = SFTime;
}
void BiSearch::PrintChunkInfo(string inputDirpath, int chunkingMethod, int method, int fileNum, int64_t time, double ratio)
{
    ofstream out;
    string fileName = "./chunkInfoLog.txt";
    if (!tool::FileExist(fileName))
    {
        out.open(fileName, ios::out);
        out << "-----------------INSTRUCTION----------------------" << endl;
        out << "./BiSearch -i " << inputDirpath << " -c " << chunkingMethod << " -m " << method << " -n " << fileNum << " -r " << ratio << endl;
        out << "-----------------CHUNK NUM-----------------------" << endl;
        out << "logical chunk num: " << logicalchunkNum << endl;
        out << "unique chunk num: " << uniquechunkNum << endl;
        out << "base chunk num: " << basechunkNum << endl;
        out << "delta chunk num: " << deltachunkNum << endl;
        out << "finesse hit:" << finessehit << endl;
        out << "-----------------CHUNK SIZE-----------------------" << endl;
        out << "logical chunk size: " << logicalchunkSize << endl;
        out << "unique chunk size: " << uniquechunkSize << endl;
        out << "base chunk size: " << basechunkSize << endl;
        out << "delta chunk size: " << deltachunkSize << endl;
        out << "-----------------METRICS-------------------------" << endl;
        out << "Overall Compression Ratio: " << (double)logicalchunkSize / (double)uniquechunkSize << endl;
        out << "DCC: " << (double)deltachunkNum / (double)uniquechunkNum << endl;
        out << "DCR: " << (double)deltachunkOriSize / (double)deltachunkSize << endl;
        out << "DCE: " << DCESum / (double)deltachunkNum << endl;
        out << "-----------------Time------------------------------" << endl;
        out << "total time: " << time << "s" << endl;
        out << "Throughput: " << (double)logicalchunkSize / time / 1024 / 1024 << "MiB/s" << endl;
        out << "Reduce data speed: " << (double)(logicalchunkSize - uniquechunkSize) / time / 1024 / 1024 << "MiB/s" << endl;
        out << "SF generation time: " << SFTime.count() << "s" << endl;
        out << "SF generation throughput: " << (double)logicalchunkSize / SFTime.count() / 1024 / 1024 << "MiB/s" << endl;
        out << "Dedup Time: " << DedupTime.count() << "s" << endl;
        out << "Locality Match Time: " << LocalityMatchTime.count() << "s" << endl;
        out << "Locality Delta Time: " << LocalityDeltaTime.count() << "s" << endl;
        out << "Feature Match Time: " << FeatureMatchTime.count() << "s" << endl;
        out << "Feature Delta Time: " << FeatureDeltaTime.count() << "s" << endl;
        out << "Lz4 Compression Time: " << lz4CompressionTime.count() << "s" << endl;
        out << "Delta Compression Time: " << deltaCompressionTime.count() << "s" << endl;
        out << "-----------------OverHead--------------------------" << endl;
        out << "Index Overhead: " << (double)(uniquechunkNum * 96 + basechunkNum * 48) / 1024 / 1024 << "MiB" << endl;
        out << "Recipe Overhead: " << (double)logicalchunkNum * 8 / 1024 / 1024 << "MiB" << endl;
        out << "SF number: " << SFnum << endl;
        out << "-----------------Reduct----------------------------" << endl;
        out << "dedup reduct size : " << DedupReduct / 1024 / 1024 << "MiB" << endl;
        out << "delta reduct size : " << DeltaReduct / 1024 / 1024 << "MiB" << endl;
        out << "local reduct size : " << LocalReduct / 1024 / 1024 << "MiB" << endl;
        out << "Feature reduct size: " << FeatureReduct / 1024 / 1024 << "MiB" << endl;
        out << "Locality reduct size: " << LocalityReduct / 1024 / 1024 << "MiB" << endl;
        out << "-----------------END-------------------------------" << endl;
    }
    else
    {
        out.open(fileName, ios::app);
        out << "-----------------INSTRUCTION----------------------" << endl;
        out << "./BiSearch -i " << inputDirpath << " -c " << chunkingMethod << " -m " << method << " -n " << fileNum << " -r " << ratio << endl;
        out << "-----------------CHUNK NUM-----------------------" << endl;
        out << "logical chunk num: " << logicalchunkNum << endl;
        out << "unique chunk num: " << uniquechunkNum << endl;
        out << "base chunk num: " << basechunkNum << endl;
        out << "delta chunk num: " << deltachunkNum << endl;
        out << "finesse hit:" << finessehit << endl;
        out << "-----------------CHUNK SIZE-----------------------" << endl;
        out << "logical chunk size: " << logicalchunkSize << endl;
        out << "unique chunk size: " << uniquechunkSize << endl;
        out << "base chunk size: " << basechunkSize << endl;
        out << "delta chunk size: " << deltachunkSize << endl;
        out << "-----------------METRICS-------------------------" << endl;
        out << "Overall Compression Ratio: " << (double)logicalchunkSize / (double)uniquechunkSize << endl;
        out << "DCC: " << (double)deltachunkNum / (double)uniquechunkNum << endl;
        out << "DCR: " << (double)deltachunkOriSize / (double)deltachunkSize << endl;
        out << "DCE: " << DCESum / (double)deltachunkNum << endl;
        out << "-----------------Time------------------------------" << endl;
        out << "total time: " << time << "s" << endl;
        out << "Throughput: " << (double)logicalchunkSize / time / 1024 / 1024 << "MiB/s" << endl;
        out << "Reduce data speed: " << (double)(logicalchunkSize - uniquechunkSize) / time / 1024 / 1024 << "MiB/s" << endl;
        out << "SF generation time: " << SFTime.count() << "s" << endl;
        out << "SF generation throughput: " << (double)logicalchunkSize / SFTime.count() / 1024 / 1024 << "MiB/s" << endl;
        out << "Dedup Time: " << DedupTime.count() << "s" << endl;
        out << "Locality Match Time: " << LocalityMatchTime.count() << "s" << endl;
        out << "Locality Delta Time: " << LocalityDeltaTime.count() << "s" << endl;
        out << "Feature Match Time: " << FeatureMatchTime.count() << "s" << endl;
        out << "Feature Delta Time: " << FeatureDeltaTime.count() << "s" << endl;
        out << "Lz4 Compression Time: " << lz4CompressionTime.count() << "s" << endl;
        out << "Delta Compression Time: " << deltaCompressionTime.count() << "s" << endl;
        out << "-----------------OverHead--------------------------" << endl;
        out << "Index Overhead: " << (double)(uniquechunkNum * 96 + basechunkNum * 48) / 1024 / 1024 << "MiB" << endl;
        out << "Recipe Overhead: " << (double)logicalchunkNum * 8 / 1024 / 1024 << "MiB" << endl;
        out << "SF number: " << SFnum << endl;
        out << "-----------------Reduct----------------------------" << endl;
        out << "dedup reduct size : " << DedupReduct / 1024 / 1024 << "MiB" << endl;
        out << "delta reduct size : " << DeltaReduct / 1024 / 1024 << "MiB" << endl;
        out << "local reduct size : " << LocalReduct / 1024 / 1024 << "MiB" << endl;
        out << "Feature reduct size: " << FeatureReduct / 1024 / 1024 << "MiB" << endl;
        out << "Locality reduct size: " << LocalityReduct / 1024 / 1024 << "MiB" << endl;
        out << "-----------------END-------------------------------" << endl;
    }
    out.close();
    return;
}

void BiSearch::PrintChunkInfo(string inputDirpath, int chunkingMethod, int method, int fileNum, int64_t time, double ratio, double chunktime)
{
    ofstream out;
    string fileName = "./chunkInfoLog.txt";
    if (!tool::FileExist(fileName))
    {
        out.open(fileName, ios::out);
        out << "-----------------INSTRUCTION----------------------" << endl;
        out << "./BiSearch -i " << inputDirpath << " -c " << chunkingMethod << " -m " << method << " -n " << fileNum << " -r " << ratio << endl;
        out << "-----------------CHUNK NUM-----------------------" << endl;
        out << "logical chunk num: " << logicalchunkNum << endl;
        out << "unique chunk num: " << uniquechunkNum << endl;
        out << "base chunk num: " << basechunkNum << endl;
        out << "delta chunk num: " << deltachunkNum << endl;
        out << "finesse hit:" << finessehit << endl;
        out << "-----------------CHUNK SIZE-----------------------" << endl;
        out << "logical chunk size: " << logicalchunkSize << endl;
        out << "unique chunk size: " << uniquechunkSize << endl;
        out << "base chunk size: " << basechunkSize << endl;
        out << "delta chunk size: " << deltachunkSize << endl;
        out << "-----------------METRICS-------------------------" << endl;
        out << "Overall Compression Ratio: " << (double)logicalchunkSize / (double)uniquechunkSize << endl;
        out << "DCC: " << (double)deltachunkNum / (double)uniquechunkNum << endl;
        out << "DCR: " << (double)deltachunkOriSize / (double)deltachunkSize << endl;
        out << "DCE: " << DCESum / (double)deltachunkNum << endl;
        out << "-----------------Time------------------------------" << endl;
        out << "total time: " << time << "s" << endl;
        out << "Throughput: " << (double)logicalchunkSize / time / 1024 / 1024 << "MiB/s" << endl;
        out << "Reduce data speed: " << (double)(logicalchunkSize - uniquechunkSize) / time / 1024 / 1024 << "MiB/s" << endl;
        out << "SF generation time: " << SFTime.count() << "s" << endl;
        out << "SF generation throughput: " << (double)logicalchunkSize / SFTime.count() / 1024 / 1024 << "MiB/s" << endl;
        out << "Chunk Time: " << chunktime << "s" << endl;
        out << "Dedup Time: " << DedupTime.count() << "s" << endl;
        out << "Locality Match Time: " << LocalityMatchTime.count() << "s" << endl;
        out << "Locality Delta Time: " << LocalityDeltaTime.count() << "s" << endl;
        out << "Feature Match Time: " << FeatureMatchTime.count() << "s" << endl;
        out << "Feature Delta Time: " << FeatureDeltaTime.count() << "s" << endl;
        out << "Lz4 Compression Time: " << lz4CompressionTime.count() << "s" << endl;
        out << "Delta Compression Time: " << deltaCompressionTime.count() << "s" << endl;
        out << "-----------------OverHead--------------------------" << endl;
        out << "Index Overhead: " << (double)(uniquechunkNum * 96 + basechunkNum * 48) / 1024 / 1024 << "MiB" << endl;
        out << "Recipe Overhead: " << (double)logicalchunkNum * 8 / 1024 / 1024 << "MiB" << endl;
        out << "SF number: " << SFnum << endl;
        out << "-----------------Reduct----------------------------" << endl;
        out << "dedup reduct size : " << DedupReduct / 1024 / 1024 << "MiB" << endl;
        out << "delta reduct size : " << DeltaReduct / 1024 / 1024 << "MiB" << endl;
        out << "local reduct size : " << LocalReduct / 1024 / 1024 << "MiB" << endl;
        out << "Feature reduct size: " << FeatureReduct / 1024 / 1024 << "MiB" << endl;
        out << "Locality reduct size: " << LocalityReduct / 1024 / 1024 << "MiB" << endl;
        out << "-----------------END-------------------------------" << endl;
    }
    else
    {
        out.open(fileName, ios::app);
        out << "-----------------INSTRUCTION----------------------" << endl;
        out << "./BiSearch -i " << inputDirpath << " -c " << chunkingMethod << " -m " << method << " -n " << fileNum << " -r " << ratio << endl;
        out << "-----------------CHUNK NUM-----------------------" << endl;
        out << "logical chunk num: " << logicalchunkNum << endl;
        out << "unique chunk num: " << uniquechunkNum << endl;
        out << "base chunk num: " << basechunkNum << endl;
        out << "delta chunk num: " << deltachunkNum << endl;
        out << "finesse hit:" << finessehit << endl;
        out << "-----------------CHUNK SIZE-----------------------" << endl;
        out << "logical chunk size: " << logicalchunkSize << endl;
        out << "unique chunk size: " << uniquechunkSize << endl;
        out << "base chunk size: " << basechunkSize << endl;
        out << "delta chunk size: " << deltachunkSize << endl;
        out << "-----------------METRICS-------------------------" << endl;
        out << "Overall Compression Ratio: " << (double)logicalchunkSize / (double)uniquechunkSize << endl;
        out << "DCC: " << (double)deltachunkNum / (double)uniquechunkNum << endl;
        out << "DCR: " << (double)deltachunkOriSize / (double)deltachunkSize << endl;
        out << "DCE: " << DCESum / (double)deltachunkNum << endl;
        out << "-----------------Time------------------------------" << endl;
        out << "total time: " << time << "s" << endl;
        out << "Throughput: " << (double)logicalchunkSize / time / 1024 / 1024 << "MiB/s" << endl;
        out << "Reduce data speed: " << (double)(logicalchunkSize - uniquechunkSize) / time / 1024 / 1024 << "MiB/s" << endl;
        out << "SF generation time: " << SFTime.count() << "s" << endl;
        out << "SF generation throughput: " << (double)logicalchunkSize / SFTime.count() / 1024 / 1024 << "MiB/s" << endl;
        out << "Chunk Time: " << chunktime << "s" << endl;
        out << "Dedup Time: " << DedupTime.count() << "s" << endl;
        out << "Locality Match Time: " << LocalityMatchTime.count() << "s" << endl;
        out << "Locality Delta Time: " << LocalityDeltaTime.count() << "s" << endl;
        out << "Feature Match Time: " << FeatureMatchTime.count() << "s" << endl;
        out << "Feature Delta Time: " << FeatureDeltaTime.count() << "s" << endl;
        out << "Lz4 Compression Time: " << lz4CompressionTime.count() << "s" << endl;
        out << "Delta Compression Time: " << deltaCompressionTime.count() << "s" << endl;
        out << "-----------------OverHead--------------------------" << endl;
        out << "Index Overhead: " << (double)(uniquechunkNum * 96 + basechunkNum * 48) / 1024 / 1024 << "MiB" << endl;
        out << "Recipe Overhead: " << (double)logicalchunkNum * 8 / 1024 / 1024 << "MiB" << endl;
        out << "SF number: " << SFnum << endl;
        out << "-----------------Reduct----------------------------" << endl;
        out << "dedup reduct size : " << DedupReduct / 1024 / 1024 << "MiB" << endl;
        out << "delta reduct size : " << DeltaReduct / 1024 / 1024 << "MiB" << endl;
        out << "local reduct size : " << LocalReduct / 1024 / 1024 << "MiB" << endl;
        out << "Feature reduct size: " << FeatureReduct / 1024 / 1024 << "MiB" << endl;
        out << "Locality reduct size: " << LocalityReduct / 1024 / 1024 << "MiB" << endl;
        out << "-----------------END-------------------------------" << endl;
    }
    out.close();
    return;
}