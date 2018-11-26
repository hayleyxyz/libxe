//
// Created by yuikonnu on 29/10/2018.
//


#include "sfcx_image.h"
#include "logical_stream.h"

#include <iostream>

namespace xe {
namespace sfcx {

SfcxImage::SfcxImage(xe::io::FileStream &stream) : stream(stream) {
    read();
}

bool SfcxImage::detectImageGeometry() {
    auto totalBlockCount = physicalLength() / blockTotalSize;

    switch(totalBlockCount) {
        case 1024: // 16mb
            reservedBlockCount = 32;
            return true;
        case 4096: // 64mb
            reservedBlockCount = 128;
            return true;
        default:
            valid = false;
            return false;
    }
}

void SfcxImage::read() {
    if(!detectImageGeometry()) {
        return;
    }

    // First byte must be 0xFF
    if(stream.readInt<uint8_t>() != 0xFF) {
        return;
    }

    readLogicalMap();
    readBootLoaders();

    valid = true;
}

void SfcxImage::readLogicalMap() {
    auto totalLength = physicalLength();
    auto pagesCount = (totalLength - (reservedBlockCount * blockTotalSize)) / pageTotalSize;
    auto reservePagesCount = (totalLength - (pagesCount * pageTotalSize)) / pageTotalSize;

    std::map<int, int> reserveMap;

    // Seek to reserve area
    stream.seek(pagesCount * pageTotalSize, xe::io::FileStream::beg);

    for(int i = 0; i < reservePagesCount; i++) {
        int physicalPage = pagesCount + i;

        // Seek past the data
        stream.seek(pageDataSize, xe::io::FileStream::cur);

        // Parse the page meta
        Page page(i);
        page.read(stream);

        if(page.isEmpty || page.isBad) {
            continue;
        }

        int logicalPage = (page.blockNumber * pagesPerBlock) + (page.pageNumber % 32);

        reserveMap.insert(std::make_pair(logicalPage, physicalPage));
    }

    // Reset to start
    stream.seek(0, xe::io::FileStream::beg);

    int logicalPage = 0;

    for(int i = 0; i < pagesCount; i++) {
        int physicalPage = i;

        off_t pos = stream.position();

        // Seek past the data
        stream.seek(pageDataSize, xe::io::FileStream::cur);

        // Parse the page meta
        Page page(i);
        page.read(stream);

        if(page.isBad || page.isEmpty) {
            auto reloc = reserveMap.find(physicalPage);
            if(reloc == reserveMap.end()) {
                continue;
            }

            physicalPage = reloc->second;
        }

        logicalMap.insert(std::make_pair(logicalPage, physicalPage));

        logicalPage++;
    }
}

int32_t SfcxImage::logicalToPhysicalAddress(int32_t logical) {
    int wantedLogicalPage = (logical / pageDataSize);
    int offsetInPage = (logical % pageDataSize);

    auto entry = logicalMap.find(wantedLogicalPage);
    if(entry == logicalMap.end()) {
        return -1;
    }

    return (entry->second * pageTotalSize) + offsetInPage;
}

int32_t SfcxImage::physicalToLogicalAddress(int32_t physical) {
    for(auto &e : logicalMap) {
        auto pageStart = e.second * pageTotalSize;

        if(physical >= pageStart && physical < (pageStart + pageDataSize)) {
            return (e.first * pageDataSize) + (physical % pageTotalSize);
        }
    }

    return -1;
}

void SfcxImage::dumpBlockTable() {
    std::cout << "Logical page,Physical page,Logical address,Physical address" << std::endl;

    for(auto &w : logicalMap) {
        std::cout << std::hex << "0x" << w.first << ",0x" << w.second << ",0x" << (w.first * pageDataSize) << ",0x" << (w.second * pageTotalSize) << std::endl;
    }
}

size_t SfcxImage::logicalLength() {
    int topPage = -1;

    for(auto &w : logicalMap) {
        if(w.first > topPage) {
            topPage = w.first;
        }
    }

    return topPage * pageDataSize;
}

size_t SfcxImage::physicalLength() {
    return stream.length();
}

void SfcxImage::readBootLoaders() {
    LogicalStream logicalStream(*this);
    logicalStream.seek(0x8, xe::io::Stream::beg);

    auto bloffset = logicalStream.readIntBE<uint32_t>();
    logicalStream.seek(bloffset, xe::io::Stream::beg);

    for(int i = 0; i < 4; ++i) {
        auto bl = new xe::bootloaders::Bootloader();
        bl->read(logicalStream);
        bootloaders.push_back(bl);
    }
}


}
}