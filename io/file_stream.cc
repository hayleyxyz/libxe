//
// Created by yuikonnu on 29/10/2018.
//

#include "file_stream.h"


namespace xe {
namespace io {

FileStream::FileStream() {

}

FileStream::~FileStream() {
    if(isOpen()) close();
}

void FileStream::open(const char *path, xe::io::FileStream::openmode mode) {
    std::ios_base::openmode stdmode = mode;

    stream.open(path, stdmode);
}

void FileStream::close() {
    if(isOpen()) stream.close();
}

bool FileStream::isOpen() {
    return stream.is_open();
}

size_t FileStream::length() {
    off_t saved = position();

    seek(0, end);
    off_t end = position();

    seek(saved, beg);

    return static_cast<size_t>(end);
}

off_t FileStream::position() {
    return stream.tellg();
}

off_t FileStream::seek(off_t pos, Stream::seekdir dir) {
    std::ios_base::seekdir stddir;

    switch(dir) {
        case beg:
            stddir = std::ios_base::seekdir::beg;
            break;
        case cur:
            stddir = std::ios_base::seekdir::cur;
            break;
        case end:
            stddir = std::ios_base::seekdir::end;
            break;
        default:
            return -1;
    }

    stream.seekg(pos, stddir);
    return stream.tellg();
}

size_t FileStream::read(uint8_t *buf, size_t length) {
    stream.read(reinterpret_cast<char *>(buf), length);
    return static_cast<size_t>(stream.gcount());
}

size_t FileStream::write(uint8_t *buf, size_t length) {
    auto before = stream.tellp();
    stream.write(reinterpret_cast<const char *>(buf), length);
    return static_cast<size_t>(stream.tellp() - before);
}

};
};