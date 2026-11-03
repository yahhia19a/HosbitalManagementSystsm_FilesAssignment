#include "IndexFile.h"
#include <cstring>
#include <stdexcept>

const char* PrimaryIndexField::key() const { return primaryKey; }

void PrimaryIndexField::setKey(const std::string& key)
{
    if (key.size() > (MAX_STR_SIZE - 1))
        throw std::length_error("key length exceeds permitted limit");
    strncpy(primaryKey, key.data(), MAX_STR_SIZE - 1);
    primaryKey[MAX_STR_SIZE - 1] = '\0';
}

const char* PrimaryIndexField::value() const { return primaryKey; }
int PrimaryIndexField::value() { return byteOffset; }
void PrimaryIndexField::setValue(int value) { byteOffset = value; }

const char* SecondaryIndexField::key() const { return secondaryKey; }

void SecondaryIndexField::setKey(const std::string& key)
{
    if (key.size() > (MAX_STR_SIZE - 1))
        throw std::length_error("key length exceeds permitted limit");
    strncpy(secondaryKey, key.data(), MAX_STR_SIZE - 1);
    secondaryKey[MAX_STR_SIZE - 1] = '\0';
}

const char* SecondaryIndexField::value() const { return secondaryKey; }
int SecondaryIndexField::value() { return headPointer; }
void SecondaryIndexField::setValue(int value) { headPointer = value; }

template<typename Field>
IndexFile<Field>::IndexFile(const std::string& filename)
    : m_filename(filename)
    , m_index()
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return;

    Field nextField { };
    while (in.read(reinterpret_cast<char*>(&nextField), sizeof(nextField))) {
        m_index.emplace_back(std::move(nextField));
    }
}

template<typename Field>
IndexFile<Field>::~IndexFile()
{
    std::ofstream out(m_filename, std::ios::binary | std::ios::trunc);
    if (!out)
        return;

    for (auto it = m_index.cbegin(); it != m_index.cend(); it++) {
        out.write(reinterpret_cast<const char*>(it.operator->()), sizeof(*it.operator->()));
    }
}

template<typename Field>
bool IndexFile<Field>::find(const char* key, int& outValue)
{
    auto it = binary_search(key);

    if (it == m_index.end() || strcmp(it->key(), key) != 0) {
        return false;
    }

    outValue = it->value();
    return true;
}

template<typename Field>
bool IndexFile<Field>::insert(const char* key, int value)
{
    auto it = binary_search(key);
    if (it != m_index.end() && strcmp(it->key(), key) == 0) {
        it->setValue(value);
        return false;
    }

    Field field { };
    field.setKey(key);
    field.setValue(value);

    m_index.insert(it, field);
    return true;
}

// Removes the entry with the given key from the (in-memory, sorted) index.
// Returns false if the key does not exist.
template<typename Field>
bool IndexFile<Field>::remove(const char* key)
{
    auto it = binary_search(key);

    if (it == m_index.end() || strcmp(it->key(), key) != 0) {
        return false;
    }

    m_index.erase(it);
    return true;
}

template<typename Field>
typename std::vector<Field>::iterator IndexFile<Field>::binary_search(const char* key)
{
    auto first = m_index.begin();
    auto last = m_index.end();

    while (first != last) {
        auto middle = first + std::distance(first, last) / 2;

        if (std::strcmp(middle->key(), key) < 0)
            first = middle + 1;
        else
            last = middle;
    }

    return first;
}

template class IndexFile<PrimaryIndexField>;
template class IndexFile<SecondaryIndexField>;