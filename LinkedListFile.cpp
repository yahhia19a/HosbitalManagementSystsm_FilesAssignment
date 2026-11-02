#include "LinkedListFile.h"

LinkedListFile::LinkedListFile(const std::string& filepath) {
    m_fstream.open(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (!m_fstream.is_open()) {
        // If the file doesn't exist yet, create it empty first
        std::ofstream creator(filepath, std::ios::binary);
        creator.close();
        m_fstream.open(filepath, std::ios::in | std::ios::out | std::ios::binary);
    }
}

LinkedListFile::~LinkedListFile() {
    if (m_fstream.is_open()) {
        m_fstream.close();
    }
}

int LinkedListFile::insert(int headPointer, const std::string& value) {
    m_fstream.seekp(0, std::ios::end);
    int offset = static_cast<int>(m_fstream.tellp());

    // Binary format: [int nextPointer][int stringLength][char[] stringData]
    m_fstream.write(reinterpret_cast<const char*>(&headPointer), sizeof(headPointer));

    int length = static_cast<int>(value.size());
    m_fstream.write(reinterpret_cast<const char*>(&length), sizeof(length));

    if (length > 0) {
        m_fstream.write(value.data(), length);
    }

    m_fstream.flush();
    return offset;
}

int LinkedListFile::remove(int headPointer, const std::string& value) {
    int currPointer = headPointer;
    int prevNextPointerOffset = -1; // Tracks the file offset of the previous node's nextPointer

    while (currPointer != NULL_POINTER) {
        m_fstream.seekg(currPointer, std::ios::beg);

        int nextPointer;
        m_fstream.read(reinterpret_cast<char*>(&nextPointer), sizeof(nextPointer));

        int length;
        m_fstream.read(reinterpret_cast<char*>(&length), sizeof(length));

        std::string nodeValue(length, '\0');
        if (length > 0) {
            m_fstream.read(nodeValue.data(), length);
        }

        if (nodeValue == value) {
            if (prevNextPointerOffset == -1) {
                // Removing the head node
                headPointer = nextPointer;
            } else {
                // Removing an intermediate or tail node: bypass it by updating predecessor's nextPointer
                m_fstream.seekp(prevNextPointerOffset, std::ios::beg);
                m_fstream.write(reinterpret_cast<const char*>(&nextPointer), sizeof(nextPointer));
                m_fstream.flush();
            }
            break;
        }

        prevNextPointerOffset = currPointer;
        currPointer = nextPointer;
    }

    return headPointer;
}

std::vector<std::string> LinkedListFile::getAllElements(int headPointer) const {
    std::vector<std::string> elements;
    int currPointer = headPointer;

    while (currPointer != NULL_POINTER) {
        m_fstream.seekg(currPointer, std::ios::beg);

        int nextPointer;
        m_fstream.read(reinterpret_cast<char*>(&nextPointer), sizeof(nextPointer));

        int length;
        m_fstream.read(reinterpret_cast<char*>(&length), sizeof(length));

        std::string value(length, '\0');
        if (length > 0) {
            m_fstream.read(&value[0], length);
        }

        elements.push_back(value);
        currPointer = nextPointer;
    }

    return elements;
}
