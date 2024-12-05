#include "../../include/datawrite.h"

dataWrite::dataWrite()
{
    // MQ = new MessageQueue<Container_t>(32);
    MultiHeaderBuffer = (uint8_t *)malloc(16 * 512);
    curContainer.size = 0;
    curContainer.containerID = 0;
    curContainer.chunkNum = 0;
    containerCache = new ReadCache();
    // restore
    normalSize = CalNormalSize(minChunkSize, avgChunkSize, maxChunkSize);
    bits = (uint32_t)round(log2(static_cast<double>(avgChunkSize)));
    maskS = GenerateFastCDCMask(bits + 1);
    maskL = GenerateFastCDCMask(bits - 1);
    lz4SafeChunkBuffer = (uint8_t *)malloc(CONTAINER_MAX_SIZE * sizeof(uint8_t));
}
dataWrite::~dataWrite()
{
    free(MultiHeaderBuffer);
    delete containerCache;
    free(lz4SafeChunkBuffer);
}
void dataWrite::PrintBinaryArray(const uint8_t *buffer, size_t buffer_size)
{
    for (size_t i = 0; i < buffer_size; i++)
    {
        fprintf(stdout, "%02x", buffer[i]);
    }
    fprintf(stdout, "\n");
    return;
}
void dataWrite::writing()
{
    while (true)
    {
        if (recieveQueue->done_ && recieveQueue->IsEmpty())
        {
            cout << "writing end" << endl;
            recieveQueue->done_ = false;
            break;
        }
        Chunk_t chunk;
        if (recieveQueue->Pop(chunk))
        {
            // cout << "writing if start" << endl;
            int tmpSize = 0;
            if (chunk.deltaFlag == NO_DELTA)
                tmpSize = chunk.chunkSize;
            else
                tmpSize = chunk.saveSize;
            // cout << "flag is " << static_cast<int>(chunk.deltaFlag) << endl;
            chunkNum++;
            containerSize += chunk.saveSize;
            curContainer.chunkNum++;

            if (curContainer.size + tmpSize > CONTAINER_MAX_SIZE)
            {
                // TODO put into MQ
                // cout << " curContainer.chunkNum is" << curContainer.chunkNum << " curContainer.containerID is " << curContainer.containerID << endl;
                startTime = std::chrono::high_resolution_clock::now();
                // cout << "push container " << containerNum << " into MQ" << endl;
                // cout << "cur container size is " << curContainer.size << endl;
                // MQ->Push(curContainer);
                string fileName = "./Containers/" + to_string(curContainer.containerID);
                ofstream outfile(fileName);
                if (outfile.is_open())
                {
                    // cout << "write id is " << tmpContainer.containerID << " size is " << tmpContainer.size << endl;
                    outfile.write(reinterpret_cast<const char *>(&curContainer.size), sizeof(curContainer.size));

                    outfile.write(reinterpret_cast<const char *>(curContainer.data), curContainer.size);
                    // outfile.write(reinterpret_cast<const char *>(&tmpContainer.size), sizeof(tmpContainer.size));
                    // outfile << tmpContainer.data;
                    outfile.close();
                    // cout << "write done" << endl;
                }
                else
                {
                    cout << "open file failed" << endl;
                }

                // sleep(1);
                containerNum++;
                containerSize = 0;
                curOffset = 0;
                curContainer.size = 0;
                curContainer.containerID = containerNum;
                curContainer.chunkNum = 0;
                endTime = std::chrono::high_resolution_clock::now();
                writeIOTime += (endTime - startTime);
            }
            // TODO: put chunk into container
            chunk.containerID = containerNum;
            chunk.offset = curOffset;
            // cout << " curContainer.size is " << curContainer.size << " tmpSize is " << tmpSize << " offset is " << curOffset << endl;
            curContainer.size += tmpSize;
            // cout<< "tmp size is " << tmpSize << " curoffset is " << curOffset<<endl;
            memcpy(curContainer.data + curOffset, chunk.chunkPtr, tmpSize);
            curOffset += tmpSize;
            // cout << "free chunk " << endl;
            free(chunk.chunkPtr);
            // cout << "free chunk done" << endl;
            chunk.chunkPtr = nullptr;
            // chunkprint(chunk); //debug
            // std::lock_guard<std::mutex> lock(mtx);
            // chunklist.push_back(chunk);
            // cout << "dataWrite entry id is  " << chunklist[chunk.chunkID].chunkID << endl;
            // cout << "writing start if end" << endl;
        }
    }
}
bool dataWrite::Chunk_Insert(Chunk_t chunk)
{
    int tmpSize = 0;
    if (chunk.deltaFlag == NO_DELTA)
        tmpSize = chunk.chunkSize;
    else
        tmpSize = chunk.saveSize;
    // cout << "flag is " << static_cast<int>(chunk.deltaFlag) << endl;
    chunkNum++;
    containerSize += chunk.saveSize;
    curContainer.chunkNum++;
    if (curContainer.size + tmpSize > CONTAINER_MAX_SIZE)
    {
        // TODO put into MQ
        // cout << " curContainer.chunkNum is" << curContainer.chunkNum << " curContainer.containerId is " << curContainer.containerID << endl;
        startTime = std::chrono::high_resolution_clock::now();
        // cout << "push container " << containerNum << " into MQ" << endl;
        // cout << "cur container size is " << curContainer.size << endl;
        // MQ->Push(curContainer);
        string fileName = "./Containers/" + to_string(curContainer.containerID);
        ofstream outfile(fileName, std::ios::binary);
        if (outfile.is_open())
        {
            // cout << "write id is " << tmpContainer.containerId << " size is " << tmpContainer.size << endl;
            outfile.write(reinterpret_cast<const char *>(&curContainer.size), sizeof(curContainer.size));

            outfile.write(reinterpret_cast<const char *>(curContainer.data), curContainer.size);
            // outfile.write(reinterpret_cast<const char *>(&tmpContainer.size), sizeof(tmpContainer.size));
            // outfile << tmpContainer.data;
            outfile.close();
            // cout << "write done" << endl;
        }
        else
        {
            cout << "open file failed" << endl;
        }

        // sleep(1);
        containerNum++;
        containerSize = 0;
        curOffset = 0;
        curContainer.size = 0;
        curContainer.containerID = containerNum;
        curContainer.chunkNum = 0;
        endTime = std::chrono::high_resolution_clock::now();
        writeIOTime += (endTime - startTime);
    }
    // TODO: put chunk into container
    chunk.containerID = containerNum;
    chunk.offset = curOffset;
    // cout << " curContainer.size is " << curContainer.size << " tmpSize is " << tmpSize << " offset is " << curOffset << endl;
    curContainer.size += tmpSize;
    // cout<< "tmp size is " << tmpSize << " curoffset is " << curOffset<<endl;
    memcpy(curContainer.data + curOffset, chunk.chunkPtr, tmpSize);
    curOffset += tmpSize;
    // cout << "free chunk " << endl;
    free(chunk.chunkPtr);
    // cout << "free chunk done" << endl;
    chunk.chunkPtr = nullptr;
    chunklist.push_back(chunk);
    // cout << "chunkset entry id is  " << chunklist[chunk.chunkid].chunkid << endl;
    return true;
}
// lz4 compress insert
bool dataWrite::Chunk_Insert(Chunk_t chunk, uint8_t *lz4Buffer)
{
    int tmpSize = 0;
    tmpSize = chunk.saveSize;
    // cout << "flag is " << static_cast<int>(chunk.deltaFlag) << endl;
    chunkNum++;
    containerSize += chunk.saveSize;
    curContainer.chunkNum++;

    if (curContainer.size + tmpSize > CONTAINER_MAX_SIZE)
    {
        // TODO put into MQ
        // cout << " curContainer.chunkNum is" << curContainer.chunkNum << " curContainer.containerId is " << curContainer.containerID << endl;
        startTime = std::chrono::high_resolution_clock::now();
        // cout << "push container " << containerNum << " into MQ" << endl;
        // cout << "cur container size is " << curContainer.size << endl;
        // MQ->Push(curContainer);
        string fileName = "./Containers/" + to_string(curContainer.containerID);
        ofstream outfile(fileName, std::ios::binary);
        if (outfile.is_open())
        {
            // cout << "write id is " << tmpContainer.containerId << " size is " << tmpContainer.size << endl;
            outfile.write(reinterpret_cast<const char *>(&curContainer.size), sizeof(curContainer.size));

            outfile.write(reinterpret_cast<const char *>(curContainer.data), curContainer.size);
            // outfile.write(reinterpret_cast<const char *>(&tmpContainer.size), sizeof(tmpContainer.size));
            // outfile << tmpContainer.data;
            outfile.close();
            // cout << "write done" << endl;
        }
        else
        {
            cout << "open file failed" << endl;
        }

        // sleep(1);
        containerNum++;
        containerSize = 0;
        curOffset = 0;
        curContainer.size = 0;
        curContainer.containerID = containerNum;
        curContainer.chunkNum = 0;
        endTime = std::chrono::high_resolution_clock::now();
        writeIOTime += (endTime - startTime);
    }
    // TODO: put chunk into container
    chunk.containerID = containerNum;
    chunk.offset = curOffset;
    // cout << " curContainer.size is " << curContainer.size << " tmpSize is " << tmpSize << " offset is " << curOffset << endl;
    curContainer.size += tmpSize;
    // cout<< "tmp size is " << tmpSize << " curoffset is " << curOffset<<endl;
    // if (chunk.chunkID == 1149)
    // {
    //     cout << "1149 is here and is base" << endl;
    //     tool::PrintBinaryArray(lz4Buffer, chunk.saveSize);
    // }

    memcpy(curContainer.data + curOffset, lz4Buffer, tmpSize);

    // **compare diff**
    // uint8_t *lz4SafeChunkBuffer = (uint8_t *)malloc(CONTAINER_MAX_SIZE * sizeof(uint8_t));
    // int decompressedSize = LZ4_decompress_safe((char *)curContainer.data + curOffset, (char *)lz4SafeChunkBuffer, tmpSize, CONTAINER_MAX_SIZE);
    // if (decompressedSize != chunk.chunkSize)
    //     cout << "decompress error" << endl;
    // // cout << "cmp is " << std::memcmp(chunk.chunkPtr, lz4SafeChunkBuffer, chunk.chunkSize) << endl;
    // free(lz4SafeChunkBuffer);

    curOffset += tmpSize;
    // cout << "free chunk " << endl;
    free(chunk.chunkPtr);
    // cout << "free chunk done" << endl;
    chunk.chunkPtr = nullptr;
    chunklist.push_back(chunk);
    // cout << "chunkset entry id is  " << chunklist[chunk.chunkid].chunkid << endl;
    return true;
}

void dataWrite::restoreHeaderFile(string fileName)
{
    string name;
    size_t pos = fileName.find_last_of('/');
    if (pos != std::string::npos)
    {
        name = fileName.substr(pos + 1);
    }
    else
    {
        name = fileName;
    }
    string writePath = "./restoreFile/" + name;
    // cout << chunkSet_.size() << endl;
    cout << "write path is " << writePath << endl;
    ofstream outFile(writePath);

    // but why i haven't to use C++
    auto tmpHeaderRecipe = RecipeMap_header[fileName];
    Recipe_Header_t *HeaderP = tmpHeaderRecipe.data();
    Recipe_Header_t *HeaderEnd = tmpHeaderRecipe.data() + tmpHeaderRecipe.size();

    auto tmpRecipe = RecipeMap[fileName];
    Recipe_t *DataP = tmpRecipe.data();
    Recipe_t *DataEnd = tmpRecipe.data() + tmpRecipe.size();

    // HeaderP arrive the end &DataP arrive the end& HeaderBuffer is empty;
    while (HeaderP != HeaderEnd || DataP != DataEnd || MultiHeaderChunkSize != MultiHeaderOffset)
    {
        // Supplement MultiHeaderBuffer
        if (MultiHeaderChunkSize == MultiHeaderOffset && HeaderP != HeaderEnd)
        {
            Chunk_t tmpChunkInfo = Get_Chunk_Info(*HeaderP++);
            // reset MultiHeaderSize and offset
            MultiHeaderChunkSize = tmpChunkInfo.chunkSize;
            MultiHeaderOffset = 0;
            if (tmpChunkInfo.deltaFlag == NO_DELTA || tmpChunkInfo.deltaFlag == NO_LZ4)
            {
                memcpy(MultiHeaderBuffer, tmpChunkInfo.chunkPtr, tmpChunkInfo.chunkSize);
            }
            else
            {
                auto baseChunkInfo = Get_Chunk_Info(tmpChunkInfo.basechunkID);
                uint64_t recSize = 0;
                auto chunk_ptr = xd3_decode(tmpChunkInfo.chunkPtr, tmpChunkInfo.saveSize, baseChunkInfo.chunkPtr, baseChunkInfo.chunkSize, &recSize);
                memcpy(MultiHeaderBuffer, chunk_ptr, tmpChunkInfo.chunkSize);

                if (baseChunkInfo.loadFromDisk)
                    free(baseChunkInfo.chunkPtr);
                if (chunk_ptr != nullptr)
                {
                    free(chunk_ptr);
                    chunk_ptr = nullptr;
                }
            }
            if (tmpChunkInfo.loadFromDisk)
                free(tmpChunkInfo.chunkPtr);
        }

        Next_Chunk_Type = FILE_HEADER;
        uint64_t cp = CutPointTarFast(MultiHeaderBuffer + MultiHeaderOffset, MultiHeaderChunkSize - MultiHeaderOffset);
        // change Next_Chunk_Type and Next_Chunk_Size

        outFile.write((char *)MultiHeaderBuffer + MultiHeaderOffset, HeaderSize);
        MultiHeaderOffset += HeaderSize;
        if (Next_Chunk_Size == 0)
            continue;
        if (Next_Chunk_Type == FILE_CHUNK && DataP != DataEnd)
        {
            Chunk_t tmpChunkInfo = Get_Chunk_Info(*DataP++);
            if (tmpChunkInfo.chunkSize - Next_Chunk_Size >= 512)
            {
                cout << " chunkSize is " << tmpChunkInfo.chunkSize << " Next_Chunk_Size is " << Next_Chunk_Size << endl;
            }
            if (tmpChunkInfo.deltaFlag == NO_DELTA || tmpChunkInfo.deltaFlag == NO_LZ4)
            {
                outFile.write((char *)tmpChunkInfo.chunkPtr, tmpChunkInfo.chunkSize);
            }
            else
            {
                auto baseChunkInfo = Get_Chunk_Info(tmpChunkInfo.basechunkID);
                uint64_t recSize = 0;
                auto chunk_ptr = xd3_decode(tmpChunkInfo.chunkPtr, tmpChunkInfo.saveSize, baseChunkInfo.chunkPtr, baseChunkInfo.chunkSize, &recSize);
                outFile.write((char *)chunk_ptr, tmpChunkInfo.chunkSize);

                if (baseChunkInfo.loadFromDisk)
                    free(baseChunkInfo.chunkPtr);
                if (chunk_ptr != nullptr)
                {
                    free(chunk_ptr);
                    chunk_ptr = nullptr;
                }
            }
            if (tmpChunkInfo.loadFromDisk)
                free(tmpChunkInfo.chunkPtr);
        }

        if (Next_Chunk_Type == BIG_CHUNK && DataP != DataEnd)
        {
            uint64_t BigChunkSize = Big_Chunk_Size;
            while (BigChunkSize)
            {
                Chunk_t tmpChunkInfo = Get_Chunk_Info(*DataP++);
                if (tmpChunkInfo.deltaFlag == NO_DELTA || tmpChunkInfo.deltaFlag == NO_LZ4)
                {
                    outFile.write((char *)tmpChunkInfo.chunkPtr, tmpChunkInfo.chunkSize);
                }
                else
                {
                    auto baseChunkInfo = Get_Chunk_Info(tmpChunkInfo.basechunkID);
                    uint64_t recSize = 0;
                    auto chunk_ptr = xd3_decode(tmpChunkInfo.chunkPtr, tmpChunkInfo.saveSize, baseChunkInfo.chunkPtr, baseChunkInfo.chunkSize, &recSize);
                    outFile.write((char *)chunk_ptr, tmpChunkInfo.chunkSize);

                    if (baseChunkInfo.loadFromDisk)
                        free(baseChunkInfo.chunkPtr);
                    if (chunk_ptr != nullptr)
                    {
                        free(chunk_ptr);
                        chunk_ptr = nullptr;
                    }
                }
                BigChunkSize -= tmpChunkInfo.chunkSize;
                if (tmpChunkInfo.loadFromDisk)
                    free(tmpChunkInfo.chunkPtr);
            }
        }
    }
    Next_Chunk_Type = FILE_HEADER;
    outFile.close();
    return;
}

void dataWrite::restoreFile(string fileName)
{
    string name;
    size_t pos = fileName.find_last_of('/');
    if (pos != std::string::npos)
    {
        name = fileName.substr(pos + 1);
    }
    else
    {
        name = fileName;
    }
    string writePath = "./restoreFile/" + name;
    // cout << chunkSet_.size() << endl;
    cout << "write path is " << writePath << endl;
    ofstream outFile(writePath, std::ios_base::binary);

    auto tmpRecipe = RecipeMap[fileName];
    for (auto recipe : tmpRecipe)
    {
        Chunk_t tmpChunkInfo = Get_Chunk_Info(recipe);
        if (tmpChunkInfo.deltaFlag == NO_DELTA || tmpChunkInfo.deltaFlag == NO_LZ4)
        {
            outFile.write((char *)tmpChunkInfo.chunkPtr, tmpChunkInfo.chunkSize);
        }
        else
        {
            // auto tmpLocalChunkInfo = xd3_recursive_restore(tmpChunkInfo);
            auto baseChunkInfo = Get_Chunk_Info(tmpChunkInfo.basechunkID);
            uint64_t recSize = 0;
            auto chunk_ptr = xd3_decode(tmpChunkInfo.chunkPtr, tmpChunkInfo.saveSize, baseChunkInfo.chunkPtr, baseChunkInfo.chunkSize, &recSize);
            // cout << "rec size is " << recSize << endl;
            //  memcpy(tmpChunkInfo.chunkptr, chunk_ptr, recSize);
            outFile.write((char *)chunk_ptr, tmpChunkInfo.chunkSize);

            if (baseChunkInfo.loadFromDisk)
                free(baseChunkInfo.chunkPtr);
            if (chunk_ptr != nullptr)
            {
                free(chunk_ptr);
                chunk_ptr = nullptr;
            }
        }
        if (tmpChunkInfo.loadFromDisk)
            free(tmpChunkInfo.chunkPtr);
    }

    outFile.close();
    return;
}

int dataWrite::Get_Chunk_Num()
{
    return chunkNum;
}

int dataWrite::Get_Container_Num(Chunk_t chunk)
{
    if (containerSize + chunk.saveSize > CONTAINER_MAX_SIZE)
    {
        return containerNum + 1;
    }
    else
    {
        return containerNum;
    }
}

Chunk_t dataWrite::Get_Chunk_Info(int id)
{
    // TODO: cache read container
    // cout << "chunk list size is " << chunklist.size() << endl;
    int tmpSize = 0;
    if (chunklist[id].deltaFlag == NO_DELTA)
        tmpSize = chunklist[id].chunkSize;
    else
        tmpSize = chunklist[id].saveSize;

    string tmpContainerIDcontainerID = to_string(chunklist[id].containerID);
    bool cacheHitResult = containerCache->ExistsInCache(tmpContainerIDcontainerID);
    // TODO: if cache hit read from cache

    if (cacheHitResult)
    {

        chunklist[id].loadFromDisk = false;
        startTime = std::chrono::high_resolution_clock::now();
        string tmpContainer;
        tmpContainer.assign(CONTAINER_MAX_SIZE, 0);
        uint8_t *tmpContainerData = containerCache->ReadFromCache(tmpContainerIDcontainerID);
        // memcpy((uint8_t *)tmpContainer.c_str(), tmpContainerData, CONTAINER_MAX_SIZE);
        // memcpy(chunklist[id].chunkPtr, tmpContainer.c_str() + chunklist[id].offset, tmpSize);
        // chunklist[id].chunkPtr = tmpContainerData + chunklist[id].offset;

        if (chunklist[id].deltaFlag != NO_DELTA)
            chunklist[id].chunkPtr = tmpContainerData + chunklist[id].offset;
        else
        {
            // base chunk & lz4 compress
            int decompressedSize = LZ4_decompress_safe((char *)(tmpContainerData + chunklist[id].offset), (char *)lz4SafeChunkBuffer, chunklist[id].saveSize, CONTAINER_MAX_SIZE);
            chunklist[id].chunkPtr = (uint8_t *)malloc(chunklist[id].chunkSize);
            memcpy(chunklist[id].chunkPtr, lz4SafeChunkBuffer, tmpSize);
            chunklist[id].loadFromDisk = true;
        }
        // cout << "read from cache and size is " << chunklist[id].chunkSize << endl;
        //  memcpy(chunklist[id].chunkPtr, tmpContainerData + chunklist[id].offset, tmpSize);
        cacheHitTimes++;
        endTime = std::chrono::high_resolution_clock::now();
        readCacheTime += (endTime - startTime);
    }
    // TODO: if cache miss, read from file
    else if (chunklist[id].containerID != containerNum)
    {
        if (chunklist[id].deltaFlag != NO_DELTA)
            chunklist[id].chunkPtr = (uint8_t *)malloc(tmpSize);
        else
            chunklist[id].chunkPtr = (uint8_t *)malloc(chunklist[id].chunkSize);
        startTime = std::chrono::high_resolution_clock::now();
        string fileName = "./Containers/" + tmpContainerIDcontainerID;
        // cout << fileName << endl;
        ifstream infile(fileName, ios::binary);
        if (infile.is_open())
        {
            uint64_t size;
            infile.read((char *)&size, sizeof(uint64_t));
            //     Allocate memory for the container
            string container;
            container.assign(size, 0);

            // Read the entire container
            infile.read((char *)container.c_str(), size);
            // Copy the required data to chunkPtr
            // cout << "offset is " << chunklist[id].offset << "saveSize is " << chunklist[id].saveSize << endl;
            if (chunklist[id].deltaFlag != NO_DELTA)
                memcpy(chunklist[id].chunkPtr, (uint8_t *)(container.c_str() + chunklist[id].offset), tmpSize);
            else
            {
                // base chunk & lz4 compress
                int decompressedSize = LZ4_decompress_safe((char *)(container.c_str() + chunklist[id].offset), (char *)lz4SafeChunkBuffer, chunklist[id].saveSize, CONTAINER_MAX_SIZE);
                memcpy(chunklist[id].chunkPtr, lz4SafeChunkBuffer, tmpSize);
            }

            // Add the container to the cache
            startTime2 = std::chrono::high_resolution_clock::now();
            containerCache->InsertToCache(tmpContainerIDcontainerID, (uint8_t *)container.c_str(), size);
            endTime2 = std::chrono::high_resolution_clock::now();
            UpdateCacheTime += (endTime2 - startTime2);
            infile.close();
            // sleep(0.5);
        }
        endTime = std::chrono::high_resolution_clock::now();
        readIOTime += (endTime - startTime);
        loadContainerTimes++;
        chunklist[id].loadFromDisk = true;
        // cout << "read from disk and size is " << chunklist[id].chunkSize << endl;
    }
    else
    {
        chunklist[id].loadFromDisk = false;
        if (chunklist[id].containerID == containerNum)
        {
            // free(chunklist[id].chunkPtr);
            //  memcpy(chunklist[id].chunkPtr, curContainer.data + chunklist[id].offset, tmpSize);
            // chunklist[id].chunkPtr = curContainer.data + chunklist[id].offset;

            if (chunklist[id].deltaFlag != NO_DELTA)
                chunklist[id].chunkPtr = curContainer.data + chunklist[id].offset;
            else
            {
                // base chunk & lz4 compress
                int decompressedSize = LZ4_decompress_safe((char *)(curContainer.data + chunklist[id].offset), (char *)lz4SafeChunkBuffer, chunklist[id].saveSize, CONTAINER_MAX_SIZE);
                chunklist[id].chunkPtr = (uint8_t *)malloc(chunklist[id].chunkSize);
                memcpy(chunklist[id].chunkPtr, lz4SafeChunkBuffer, tmpSize);
                chunklist[id].loadFromDisk = true;
            }
        }
        else
        {
            cout << "open file failed" << endl;
        }
    }

    return chunklist[id];
}

// bool dataWrite::Recipe_Insert(Chunk_t &info)
// {
//     recipelist.push_back(info);
//     return true;
// }

bool dataWrite::Recipe_Insert(uint64_t chunkID)
{
    RecipeMap[filename].push_back(chunkID);
    return true;
}

bool dataWrite::Recipe_Header_Insert(uint64_t chunkID)
{
    RecipeMap_header[filename].push_back(chunkID);
    return true;
}

void dataWrite::Save_to_File_Chunking(string methodname)
{
    ofstream outfile;
    // uint64_t traceid = 0;
    string filename = "./" + methodname + "_recipe.txt";
    if (!tool::FileExist(filename))
    {
        outfile.open(filename, ios::out);
        outfile << "Traceid,"
                << "ChunkID,"
                << "BasechunkID,"
                << "ChunkSize,"
                << "SaveSize,"
                << "DeltaFlag,"
                //<< "tmpFinesseSize"
                //<< "tmpLocalSize"
                << endl;
    }
    else
    {
        outfile.open(filename, ios::out | ios::binary);
    }

    if (!outfile.is_open())
    {
        cout << "open file failed" << endl;
        return;
    }
    for (int i = 0; i < recipelist.size(); i++)
    {
        uint64_t traceid = i;
        auto tmpChunkrecipe = recipelist[i];
        // Chunk_t tmpChunkrecipe = this->Get_Chunk_Info(chunkID);
        int basechunkID = tmpChunkrecipe.basechunkID;
        uint64_t chunkSize = tmpChunkrecipe.chunkSize;
        uint64_t saveSize = tmpChunkrecipe.saveSize;
        uint64_t chunkID = tmpChunkrecipe.chunkID;

        int DeltaFlag = tmpChunkrecipe.deltaFlag;
        string ChunkFlag;
        // BUG flag

        // cutpoint
        stringstream ss;
        ss << hex << tmpChunkrecipe.chunkSize;
        string cutPoint = ss.str();

        if (DeltaFlag == NO_DELTA)
        {
            ChunkFlag = "Base";
        }
        else if (DeltaFlag == FINESSE_TO_BASE)
        {
            ChunkFlag = "Finesse_To_Base";
        }
        else if (DeltaFlag == FINESSE_DELTA)
        {
            ChunkFlag = "Finesse";
        }
        else if (DeltaFlag == LOCAL_DELTA)
        {
            // cout << "Local" << endl;
            ChunkFlag = "Local";
        }

        outfile << traceid << "," << chunkID << "," << basechunkID << "," << chunkSize << "," << saveSize
                << "," << ChunkFlag << "," << cutPoint << endl;
    }
    outfile.close();
    return;
}
void dataWrite::Save_to_File(string methodname)
{
    ofstream outfile;
    // uint64_t traceid = 0;
    string filename = "./" + methodname + "_recipe.txt";
    if (!tool::FileExist(filename))
    {
        outfile.open(filename, ios::out);
        outfile << "Traceid,"
                << "ChunkID,"
                << "BasechunkID,"
                << "ChunkSize,"
                << "SaveSize,"
                << "DeltaFlag,"
                << "ChunkFlag"
                //<< "tmpFinesseSize"
                //<< "tmpLocalSize"
                << endl;
    }
    else
    {
        outfile.open(filename, ios::out | ios::binary);
    }

    if (!outfile.is_open())
    {
        cout << "open file failed" << endl;
        return;
    }
    for (int i = 0; i < recipelist.size(); i++)
    {
        uint64_t traceid = i;
        auto tmpChunkrecipe = recipelist[i];
        // Chunk_t tmpChunkrecipe = this->Get_Chunk_Info(chunkID);
        int basechunkID = tmpChunkrecipe.basechunkID;
        uint64_t chunkSize = tmpChunkrecipe.chunkSize;
        uint64_t saveSize = tmpChunkrecipe.saveSize;

        uint64_t chunkID = tmpChunkrecipe.chunkID;

        int DeltaFlag = tmpChunkrecipe.deltaFlag;
        string ChunkFlag;
        // BUG flag

        // cutpoint
        stringstream ss;
        ss << hex << tmpChunkrecipe.chunkSize;
        string cutPoint = ss.str();

        if (DeltaFlag == NO_DELTA)
        {
            ChunkFlag = "Base";
        }
        else if (DeltaFlag == FINESSE_TO_BASE)
        {
            ChunkFlag = "Finesse_To_Base";
        }
        else if (DeltaFlag == FINESSE_DELTA)
        {
            ChunkFlag = "Finesse";
        }
        else if (DeltaFlag == LOCAL_DELTA)
        {
            // cout << "Local" << endl;
            ChunkFlag = "Local";
        }

        outfile << traceid << "," << chunkID << "," << basechunkID << "," << chunkSize << "," << saveSize
                << "," << ChunkFlag << endl;
    }
    outfile.close();
    return;
}

void dataWrite::writeContainers()
{
    Container_t tmpContainer;
    bool jobDoneFlag = false;

    while (true)
    {
        if (MQ->done_ && MQ->IsEmpty())
        {
            jobDoneFlag = true;
        }
        // consume a message
        if (MQ->Pop(tmpContainer))
        {
            //  write a container
            // cout << "write a container " << tmpContainer.containerID << endl;
            string fileName = "./Containers/" + to_string(tmpContainer.containerID);
            ofstream outfile(fileName);
            if (outfile.is_open())
            {
                // cout << "write id is " << tmpContainer.containerID << " size is " << tmpContainer.size << endl;
                outfile.write(reinterpret_cast<const char *>(&tmpContainer.size), sizeof(tmpContainer.size));

                outfile.write(reinterpret_cast<const char *>(tmpContainer.data), tmpContainer.size);
                // outfile.write(reinterpret_cast<const char *>(&tmpContainer.size), sizeof(tmpContainer.size));
                // outfile << tmpContainer.data;
                outfile.close();
                // cout << "write done" << endl;
            }
            else
            {
                cout << "open file failed" << endl;
            }
        }

        if (jobDoneFlag)
        {
            break;
        }
    }
    // cout << "write done" << endl;

    return;
}

void dataWrite::ProcessLastContainer()
{
    // if (curContainer.size != 0)
    // {
    //     MQ->Push(curContainer);
    // }
    // MQ->done_ = true;
    if (curContainer.size != 0)
    {
        string fileName = "./Containers/" + to_string(curContainer.containerID);
        ofstream outfile(fileName);
        if (outfile.is_open())
        {
            outfile.write(reinterpret_cast<const char *>(&curContainer.size), sizeof(curContainer.size));
            outfile.write(reinterpret_cast<const char *>(curContainer.data), curContainer.size);

            outfile.close();
        }
        else
        {
            cout << "open file failed" << endl;
        }
    }

    return;
}

bool dataWrite::isLz4(int id)
{
    if (chunklist[id].deltaFlag == NO_DELTA)
    {
        return true;
    }
    else
    {
        return false;
    }
}

Chunk_t dataWrite::Get_Chunk_MetaInfo(int id)
{
    // std::lock_guard<std::mutex> lock(mtx);
    if (chunklist.size() < id)
    {
        cout << "errorrrr!" << endl;
        cout << "size is " << chunklist.size() << " id is " << id << endl;
    }
    auto ret = chunklist[id];
    return ret;
}

void dataWrite::PrintMetrics()
{
    cout << "load container times: " << loadContainerTimes << endl;
    cout << "cache hit times: " << cacheHitTimes << endl;

    auto readIOTimeMin = std::chrono::duration_cast<std::chrono::seconds>(readIOTime);
    cout << "Read IO time: " << readIOTimeMin.count() << " seconds" << endl;
    auto writeIOTimeMin = std::chrono::duration_cast<std::chrono::seconds>(writeIOTime);
    cout << "Write IO time: " << writeIOTimeMin.count() << " seconds" << endl;
    auto UpdateCacheTimeMin = std::chrono::duration_cast<std::chrono::seconds>(UpdateCacheTime);
    cout << "Update Cache time: " << UpdateCacheTimeMin.count() << "seconds" << endl;
    auto readCacheTimeMin = std::chrono::duration_cast<std::chrono::seconds>(readCacheTime);
    cout << "Read Cache time: " << readCacheTimeMin.count() << "seconds" << endl;
    return;
}

// bool dataWrite::isDuplicate(int id)
// {
//     // cout << 777 <<endl;
//     if (chunklist[id].dedupChunks.size() != 0)
//     {
//         // cout << "is duplicate" << endl;
//         return true;
//     }
//     else
//     {
//         // cout << "not duplicate" << endl;
//         return false;
//     }
// }
uint8_t *dataWrite::xd3_decode(const uint8_t *in, size_t in_size, const uint8_t *ref, size_t ref_size, size_t *res_size) // 更改函数
{
    const auto max_buffer_size = CONTAINER_MAX_SIZE * 2;
    uint8_t *buffer;
    buffer = (uint8_t *)malloc(max_buffer_size);
    size_t sz;
    // cout << sz << endl;
    // cout << "max_buffer_size:" << max_buffer_size << endl;
    // auto ret = xd3_decode_memory(in, in_size, ref, ref_size, buffer, &sz, max_buffer_size, 0);
    auto ret = xd3_decode_memory(in, in_size, ref, ref_size, buffer, &sz, max_buffer_size, 0);
    if (ret != 0)
    {
        cout << "decode error" << endl;
        cout << "ret code is " << ret << endl;
        const char *errMsg = xd3_strerror(ret);
        if (errMsg != nullptr)
        {
            printf("%s\n", errMsg);
        }
        else
        {
            printf("Unknown error\n");
        }
    }
    uint8_t *res;
    res = (uint8_t *)malloc(sz);
    *res_size = sz;

    // printf("xdxd3的mem前\n");

    // cout << sz << endl;

    memcpy(res, buffer, sz);
    // printf("xdxd3的mem后\n");
    free(buffer);
    // printf("buffer后\n");
    return res;
}
uint32_t dataWrite::CutPointTarFast(const uint8_t *src, const uint32_t len)
{
    switch (Next_Chunk_Type)
    {
    case FILE_HEADER:
    {
        uint8_t data[12];
        std::memcpy(data, src + 124, 12);

        Next_Chunk_Size = 0;
        for (int i = 0; i < 11; i++)
        {
            Next_Chunk_Size = Next_Chunk_Size * 8 + data[i] - 48;
        }
        if (*(src + 156) == REGTYPE)
        {
            if (Next_Chunk_Size <= CONTAINER_MAX_SIZE)
                Next_Chunk_Type = FILE_CHUNK;
            else
            {
                Next_Chunk_Type = BIG_CHUNK;
                Big_Chunk_Size = (Next_Chunk_Size + 511) / 512 * 512;
                Big_Chunk_Offset = 0;
                // Big_Chunk_Allowance = Next_Chunk_Size / CONTAINER_MAX_SIZE;
                // Big_Chunk_Last_Size = Next_Chunk_Size % CONTAINER_MAX_SIZE;
                // if (Big_Chunk_Last_Size == 0)
                // {
                //     Big_Chunk_Allowance--;
                //     Big_Chunk_Last_Size = CONTAINER_MAX_SIZE;
                // }
            }
        }
        if (*(src + 156) == AREGTYPE)
        {
            Next_Chunk_Size = 0;
            for (int i = 0; i < 11; i++)
            {
                Next_Chunk_Size = Next_Chunk_Size * 8 + data[i];
            }
            if (Next_Chunk_Size <= CONTAINER_MAX_SIZE)
                Next_Chunk_Type = FILE_CHUNK;
            else
            {
                Next_Chunk_Type = BIG_CHUNK;
                Big_Chunk_Size = (Next_Chunk_Size + 511) / 512 * 512;
                Big_Chunk_Offset = 0;
                // Big_Chunk_Allowance = Next_Chunk_Size / CONTAINER_MAX_SIZE;
                // Big_Chunk_Last_Size = Next_Chunk_Size % CONTAINER_MAX_SIZE;
                // if (Big_Chunk_Last_Size == 0)
                // {
                //     Big_Chunk_Allowance--;
                //     Big_Chunk_Last_Size = CONTAINER_MAX_SIZE;
                // }
            }
        }
        if (*(src + 156) == 'x' || *(src + 156) == GNUTYPE_LONGNAME)
            Next_Chunk_Type = FILE_CHUNK;
        /*use to debug*/
        // cout<<"Next_Chunk_Flag: " <<int(*(src + 156));
        // cout<<"Next_Chunk_Type: " <<Next_Chunk_Type;
        // cout<<"Next_Chunk_Size: "<<Next_Chunk_Size<<endl;
        // if(int(*(src + 156)) == 32||int(*(src + 156)) == 0){
        //     for(int i=0;i<512;i++)
        //     cout<<src[i]<<" ";
        //     cout<<endl;
        // }

        if (len >= 512)
            return 512;
        else
        {
            // printf("emmmm");
            return len;
        }
        break;
    }
    case FILE_CHUNK:
    {
        uint32_t roundedUp = (Next_Chunk_Size + 511) / 512 * 512;
        Next_Chunk_Type = FILE_HEADER;
        Next_Chunk_Size = 512;
        if (roundedUp < len)
            return roundedUp;
        else
            return len;
        break;
    }
    case BIG_CHUNK:
    {
        if (Big_Chunk_Size - Big_Chunk_Offset > maxChunkSize)
        {
            // Big_Chunk_Allowance--;
            // cout << " BigChunkSize is " << Big_Chunk_Size << " BigChunkOffset is" << Big_Chunk_Offset << endl;
            uint32_t cp = CutPointFastCDC(src,
                                          Big_Chunk_Size - Big_Chunk_Offset);
            Big_Chunk_Offset += cp;
            // cout << "offset is " << Big_Chunk_Offset << " cp is " << cp << endl;
            return cp;
            // return CONTAINER_MAX_SIZE;
        }
        else
        {
            Next_Chunk_Type = FILE_HEADER;
            return Big_Chunk_Size - Big_Chunk_Offset;
        }
        break;
    }
    }
}

uint32_t dataWrite::CutPointFastCDC(const uint8_t *src, const uint32_t len)
{
    uint32_t n;
    uint32_t fp = 0;
    uint32_t i;
    i = min(len, static_cast<uint32_t>(minChunkSize));
    n = min(normalSize, len);
    for (; i < n; i++)
    {
        fp = (fp >> 1) + GEAR[src[i]];
        if (!(fp & maskS))
        {
            return (i + 1);
        }
    }

    n = min(static_cast<uint32_t>(maxChunkSize), len);
    for (; i < n; i++)
    {
        fp = (fp >> 1) + GEAR[src[i]];
        if (!(fp & maskL))
        {
            return (i + 1);
        }
    }
    return i;
};
uint32_t dataWrite::CalNormalSize(const uint32_t min, const uint32_t av, const uint32_t max)
{
    uint32_t off = min + DivCeil(min, 2);
    if (off > av)
    {
        off = av;
    }
    uint32_t diff = av - off;
    if (diff > max)
    {
        return max;
    }
    return diff;
}
inline uint32_t dataWrite::DivCeil(uint32_t a, uint32_t b)
{
    uint32_t tmp = a / b;
    if (a % b == 0)
    {
        return tmp;
    }
    else
    {
        return (tmp + 1);
    }
}
uint32_t dataWrite::GenerateFastCDCMask(uint32_t bits)
{
    uint32_t tmp;
    tmp = (1 << CompareLimit(bits, 1, 31)) - 1;
    return tmp;
}
inline uint32_t dataWrite::CompareLimit(uint32_t input, uint32_t lower, uint32_t upper)
{
    if (input <= lower)
    {
        return lower;
    }
    else if (input >= upper)
    {
        return upper;
    }
    else
    {
        return input;
    }
}

void dataWrite::SetFilename(string name)
{
    filename.assign(name);
    return;
}
void dataWrite::chunkprint(const Chunk_t chunk)
{
    cout << " chunkID: " << chunk.chunkID << endl;
    cout << " chunkSize: " << chunk.chunkSize << endl;
    cout << " saveSize: " << chunk.saveSize << endl;
    cout << " deltaFlag: " << chunk.deltaFlag << endl;
    // cout << " chunkPtr: " << chunk.chunkPtr << endl;
    cout << " loadFromDisk: " << chunk.loadFromDisk << endl;
    cout << " Offset: " << chunk.offset << endl;
    cout << " containerID: " << chunk.containerID << endl;
    cout << endl;
}

void dataWrite::Save_to_File_unique(string methodname)
{
    ofstream outfile;
    // uint64_t traceid = 0;
    string filename = "./" + methodname + "_chunkIndex.txt";
    if (!tool::FileExist(filename))
    {
        outfile.open(filename, ios::out);
        outfile << "ChunkID,"
                << "BasechunkID,"
                << "ChunkSize,"
                << "SaveSize,"
                << "DeltaFlag,"
                << "loadFromDisk, "
                << "HeaderFlag, "
                << "offset, "
                << "containerID, "
                << endl;
    }
    else
    {
        outfile.open(filename, ios::out | ios::binary);
    }

    if (!outfile.is_open())
    {
        cout << "open file failed" << endl;
        return;
    }
    for (int i = 0; i < chunklist.size(); i++)
    {
        Chunk_t tmpChunk = this->Get_Chunk_MetaInfo(i);
        outfile << tmpChunk.chunkID << "," << tmpChunk.basechunkID << "," << tmpChunk.chunkSize << "," << tmpChunk.saveSize
                << "," << (uint64_t)tmpChunk.deltaFlag << "," << tmpChunk.loadFromDisk << "," << tmpChunk.HeaderFlag << ","
                << tmpChunk.offset << "," << tmpChunk.containerID << endl;
    }
    outfile.close();
    return;
}
