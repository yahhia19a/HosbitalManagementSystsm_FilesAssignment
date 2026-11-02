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
