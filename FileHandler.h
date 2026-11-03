#pragma once

#include <string>
#include "Doctor.h"
#include "Appointment.h"

void initializeFiles();

bool readDoctor(int offset, Doctor& d);
int readDoctorHeader();
void writeDoctorHeader(int value);
bool writeDoctor(const Doctor& d, int offset);
int getDoctorFileEnd(); /// ???

int readAppointmentHeader();
bool readAppointment(int offset, Appointment& a);
void writeAppointmentHeader(int value);
bool writeAppointment(const Appointment& a, int offset);
int getAppointmentFileEnd(); // ???

//-------------------------------
int allocateDoctorOffset();
int allocateAppointmentOffset();

// Logically deletes the record at 'offset' (writes '*' marker) and
// pushes it onto the AVAIL LIST. Returns false on error.
bool freeDoctorOffset(int offset);
bool freeAppointmentOffset(int offset);

// True if the record is a logically deleted one.
bool isDeletedDoctor(const Doctor& d);
bool isDeletedAppointment(const Appointment& a);