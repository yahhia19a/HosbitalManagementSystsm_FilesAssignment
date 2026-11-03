#include "FileHandler.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <vector>
using namespace std;

static void initializeFile(string filename) {

    fstream file(filename, ios::in | ios::out | ios::binary);

    if (!file.is_open()) {
        ofstream create(filename, ios::binary);
        create.close();
        file.clear();
        file.open(filename, ios::in | ios::out | ios::binary);
    }

    file.seekg(0, ios::end);

    if (file.tellg() == 0) { // File is empty
        int header = -1;

        file.seekp(0, ios::beg);
        file.write((char*)&header, sizeof(int));
    }

    file.close();
}

void initializeFiles() {
    initializeFile("Doctors.dat");
    initializeFile("Appointments.dat");
}
static_assert(sizeof(Doctor) == 95, "Doctor size must be 95 bytes");
static_assert(sizeof(Appointment) == 60, "Appointment size must be 60 bytes");

int readDoctorHeader() {

    ifstream file("Doctors.dat", ios::binary);

    int header = -1;

    if (!file) {
        return -1;
    }

    file.read((char*)&header, sizeof(int));

    if (!file) {
        file.close();
        return -1;
    }

    file.close();

    return header;
}

int readAppointmentHeader() {

    ifstream file("Appointments.dat", ios::binary);

    int header = -1;

    if (!file) {
        return -1;
    }

    file.read((char*)&header, sizeof(int));

    if (!file) {
        file.close();
        return -1;
    }

    file.close();

    return header;
}

void writeDoctorHeader(int value) {

    fstream file("Doctors.dat", ios::in | ios::out | ios::binary);

    file.seekp(0, ios::beg);

    file.write((char*)&value, sizeof(int));

    file.close();
}

void writeAppointmentHeader(int value) {

    fstream file("Appointments.dat", ios::in | ios::out | ios::binary);
    file.seekp(0, ios::beg);
    file.write((char*)&value, sizeof(int));
    file.close();
}

bool writeDoctor(const Doctor& d, int offset) {

    fstream file("Doctors.dat", ios::in | ios::out | ios::binary);

    if (!file)
        return false;

    file.seekp(offset, ios::beg);

    file.write((char*)&d, sizeof(Doctor));

    file.close();

    return true;
}

bool readDoctor(int offset, Doctor& d) {

    ifstream file("Doctors.dat", ios::binary);

    if (!file)
        return false;

    file.seekg(offset, ios::beg);

    file.read((char*)&d, sizeof(Doctor));

    bool success = file.good();

    file.close();

    return success;
}

bool writeAppointment(const Appointment& a, int offset) {

    fstream file("Appointments.dat", ios::in | ios::out | ios::binary);

    if (!file)
        return false;

    file.seekp(offset, ios::beg);

    file.write((char*)&a, sizeof(Appointment));

    file.close();

    return true;
}

bool readAppointment(int offset, Appointment& a) {

    ifstream file("Appointments.dat", ios::binary);

    if (!file)
        return false;

    file.seekg(offset, ios::beg);

    file.read((char*)&a, sizeof(Appointment));

    bool success = file.good();

    file.close();

    return success;
}
int getDoctorFileEnd() {

    ifstream file("Doctors.dat", ios::binary | ios::ate);

    int offset = -1;
    if (file) {
        offset = (int)file.tellg();
    }

    file.close();

    return offset;
}

int getAppointmentFileEnd() {

    ifstream file("Appointments.dat", ios::binary | ios::ate);

    int offset = -1;
    if (file) {
        offset = (int)file.tellg();
    }

    file.close();

    return offset;
}
//---------------------------------

static const char DELETED_MARKER = '*';
static const int AVAIL_NULL = -1;
static const int FILE_HEADER_SIZE = (int)sizeof(int);

// Reads raw bytes of one record.
static bool readRawRecord(const char* filename, int offset, char* buffer, size_t size) {

    ifstream file(filename, ios::binary);

    if (!file)
        return false;

    file.seekg(offset, ios::beg);
    file.read(buffer, (streamsize)size);

    return !file.fail();
}

// Writes raw bytes of one record.
static bool writeRawRecord(const char* filename, int offset, const char* buffer, size_t size) {

    fstream file(filename, ios::in | ios::out | ios::binary);

    if (!file)
        return false;

    file.seekp(offset, ios::beg);
    file.write(buffer, (streamsize)size);
    file.flush();

    return !file.fail();
}

// Pops the head of the AVAIL LIST, or returns the end of file if empty.
// Reuses the existing header/end-of-file functions of each data file.
static int allocateSlot(const char* filename,
                        int recordSize,
                        int (*readHeader)(),
                        void (*writeHeader)(int),
                        int (*getFileEnd)()) {

    int head = readHeader();

    // AVAIL LIST empty -> append at the end of the file.
    if (head == AVAIL_NULL) {

        int end = getFileEnd();

        if (end < FILE_HEADER_SIZE)
            return -1;

        if ((end - FILE_HEADER_SIZE) % recordSize != 0)
            return -1;

        return end;
    }

    // AVAIL LIST not empty -> reuse the most recently deleted record (LIFO).
    if (head < FILE_HEADER_SIZE)
        return -1;

    vector<char> record(recordSize);

    if (!readRawRecord(filename, head, record.data(), record.size()))
        return -1;

    // Sanity check: a record on the AVAIL LIST must be marked deleted.
    if (record[0] != DELETED_MARKER)
        return -1;

    int nextPointer;
    memcpy(&nextPointer, record.data() + 1, sizeof(int));

    // The next deleted record becomes the new head.
    writeHeader(nextPointer);

    return head;
}

// Marks the record deleted and pushes it onto the AVAIL LIST.
static bool freeSlot(const char* filename,
                     int recordSize,
                     int offset,
                     int (*readHeader)(),
                     void (*writeHeader)(int)) {

    if (offset < FILE_HEADER_SIZE)
        return false;

    int oldHead = readHeader();

    vector<char> record(recordSize, 0);

    record[0] = DELETED_MARKER;
    memcpy(record.data() + 1, &oldHead, sizeof(int));

    if (!writeRawRecord(filename, offset, record.data(), record.size()))
        return false;

    writeHeader(offset);

    return true;
}

int allocateDoctorOffset() {
    return allocateSlot("Doctors.dat", (int)sizeof(Doctor),
                        readDoctorHeader, writeDoctorHeader, getDoctorFileEnd);
}

bool freeDoctorOffset(int offset) {
    return freeSlot("Doctors.dat", (int)sizeof(Doctor), offset,
                    readDoctorHeader, writeDoctorHeader);
}

int allocateAppointmentOffset() {
    return allocateSlot("Appointments.dat", (int)sizeof(Appointment),
                        readAppointmentHeader, writeAppointmentHeader, getAppointmentFileEnd);
}

bool freeAppointmentOffset(int offset) {
    return freeSlot("Appointments.dat", (int)sizeof(Appointment), offset,
                    readAppointmentHeader, writeAppointmentHeader);
}

bool isDeletedDoctor(const Doctor& d) {
    return d.doctorID[0] == DELETED_MARKER;
}

bool isDeletedAppointment(const Appointment& a) {
    return a.appointmentID[0] == DELETED_MARKER;
}
