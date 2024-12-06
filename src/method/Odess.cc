#include "../../include/odess.h"

Odess::Odess()
{
    // cout << " FP size is " << sizeof(int) << " Chunk_t is " << sizeof(Chunk_t) << " <super_feature_t, unordered_set<string>> is " << sizeof(super_feature_t);
    lz4ChunkBuffer = (uint8_t *)malloc(CONTAINER_MAX_SIZE * sizeof(uint8_t));
    mdCtx = EVP_MD_CTX_new();
    hashBuf = (uint8_t *)malloc(CHUNK_HASH_SIZE * sizeof(uint8_t));
    deltaMaxChunkBuffer = (uint8_t *)malloc(2 * CONTAINER_MAX_SIZE * sizeof(uint8_t));
    SFindex = new unordered_map<string, vector<int>>[FINESSE_SF_NUM];
}

Odess::~Odess()
{
    free(lz4ChunkBuffer);
    free(deltaMaxChunkBuffer);
    EVP_MD_CTX_free(mdCtx);
    free(hashBuf);
}

void Odess::ProcessTrace()
{
    string tmpChunkHash;
    string tmpChunkContent;
    while (true)
    {
        string hashStr;
        hashStr.assign(CHUNK_HASH_SIZE, 0);
        if (recieveQueue->done_ && recieveQueue->IsEmpty())
        {
            recieveQueue->done_ = false;
            ads_Version++;
            break;
        }
        Chunk_t tmpChunk;
        if (recieveQueue->Pop(tmpChunk))
        {
            // 计算特征
            tmpChunkContent.assign((char *)tmpChunk.chunkPtr, tmpChunk.chunkSize);
            tmpChunk.chunkID = uniquechunkNum++;
            logicalchunkSize += tmpChunk.chunkSize;
            // Odess 获取超级特征 & 获取时间
            startSF = std::chrono::high_resolution_clock::now();
            auto superfeature = table.feature_generator_.GenerateSuperFeatures(tmpChunkContent);
            endSF = std::chrono::high_resolution_clock::now();
            SFTime += (endSF - startSF);

            // 将超级特征的第一个值作为键插入到哈希表中
            long long firstFeature = superfeature[0];
            if (hashTable.find(firstFeature) == hashTable.end())
            {
                insertionOrder.push_back(firstFeature); // 记录插入顺序
            }
            hashTable[firstFeature].push_back(tmpChunk);
            localchunkSize += tmpChunk.chunkSize;
        }
    }
    recieveQueue->done_ = false;
}

void Odess::Migratory()
{
    // 输出到 .me 文件
    std::ofstream outputFile("output.me", std::ios::binary);
    if (outputFile.is_open())
    {
        for (const auto &key : insertionOrder)
        {
            const auto &chunks = hashTable[key];
            // std::cout << "Key: " << key << ", Vector size: " << chunks.size() << std::endl;
            for (const auto &chunk : chunks)
            {
                // std::cout << chunk.chunkID << std::endl;
                outputFile.write(reinterpret_cast<const char *>(chunk.chunkPtr), chunk.chunkSize);
            }
        }
        outputFile.close();
    }
    else
    {
        std::cerr << "Unable to open file";
    }

    return;
}

void Odess::MLC()
{
    MLCSize = CompressLargeFile("output.me", "output.me.lz4");
    return;
}