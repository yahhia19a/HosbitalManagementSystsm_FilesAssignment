#pragma once

#include <string>
#include <vector>
#include <fstream>

class LinkedListFile {
public:
    static constexpr int NULL_POINTER = -1;

    explicit LinkedListFile(const std::string& filepath);
    ~LinkedListFile();

    LinkedListFile(const LinkedListFile&) = delete;
    LinkedListFile& operator=(const LinkedListFile&) = delete;

    int insert(int headPointer, const std::string& value);
    int remove(int headPointer, const std::string& value);
    std::vector<std::string> getAllElements(int headPointer) const;

private:
    mutable std::fstream m_fstream;
};
