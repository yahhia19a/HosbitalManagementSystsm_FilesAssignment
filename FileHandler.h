#pragma once
#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <string>
using namespace std;

struct Doctor {
    char doctorID[15];
    char doctorName[30];
    char specialty[20];
    char address[30];
};

struct Appointment {
    char appointmentID[15];
    char appointmentDate[30];
    char doctorID[15];
};

void initializeFiles();

int readDoctorHeader();
int readAppointmentHeader();

void writeDoctorHeader(int value);
void writeAppointmentHeader(int value);

bool writeDoctor(Doctor d, int offset);
bool readDoctor(int offset, Doctor& d);

bool writeAppointment(Appointment a, int offset);
bool readAppointment(int offset, Appointment& a);

int getDoctorFileEnd();
int getAppointmentFileEnd();

Doctor createDoctor(string id, string name,
    string specialty, string address);

Appointment createAppointment(string id, string date,
    string doctorID);

#endif