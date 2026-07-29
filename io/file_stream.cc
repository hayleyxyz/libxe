//
// Created by github.com/hayleyxyz on 29/10/2018.
//

#include "file_stream.h"


namespace xe {
namespace io {

FileStream::FileStream() {

}

FileStream::~FileStream() {
    if(isOpen()) close();
}

void FileStream::open(const char *path, std::ios_base::openmode mode) {
    stream.open(path, mode);
}

void FileStream::close() {
    if(isOpen()) stream.close();
}

bool FileStream::isOpen() {
    return stream.is_open();
}

size_t FileStream::length() {
    size_t saved = position();

    seek(0, std::ios::end);
    size_t end = position();

    seek(saved, std::ios::beg);

    return static_cast<size_t>(end);
}

size_t FileStream::position() {
    return stream.tellg();
}

size_t FileStream::seek(size_t pos, std::ios_base::seekdir dir) {

    stream.seekg(pos, dir);
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