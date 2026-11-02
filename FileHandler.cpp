#include "FileHandler.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
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
