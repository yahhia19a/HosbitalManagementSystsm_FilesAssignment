#pragma once

#include <string>
#include <vector>
#include <fstream>

template<typename Field>
class IndexFile {
public:
    explicit IndexFile(const std::string& filename);
    ~IndexFile();

    IndexFile(const IndexFile&) = delete;
    IndexFile& operator=(const IndexFile&) = delete;

    bool find(const char* primaryKey, int& outByteOffset);
    bool insert(const char* primaryKey, int byteOffset);

protected:
    typename std::vector<Field>::iterator binary_search(const char* primaryKey);

private:
    std::string m_filename;
    std::vector<Field> m_index;
};

#define MAX_STR_SIZE 30

struct PrimaryIndexField {
    char primaryKey[MAX_STR_SIZE];
    int byteOffset;

    const char* key() const;
    void setKey(const std::string& key);

    const char* value() const;
    int value();
    void setValue(int value);
};

struct SecondaryIndexField {
    char secondaryKey[MAX_STR_SIZE];
    int headPointer;

    const char* key() const;
    void setKey(const std::string& key);

    const char* value() const;
    int value();
    void setValue(int value);
};

using PrimaryIndexFile = IndexFile<PrimaryIndexField>;
using SecondaryIndexFile = IndexFile<SecondaryIndexField>;
