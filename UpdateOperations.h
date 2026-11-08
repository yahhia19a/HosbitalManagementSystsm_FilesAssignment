#pragma once

#include <string>
#include "IndexFile.h"
#include "LinkedListFile.h"

// ------------------------------------------------------------
// References to the indexes owned by HealthcareIndexManager.
// The manager creates one of these (see refs() in main.cpp) and
// passes it to every function below.
// ------------------------------------------------------------
struct IndexRefs
{
    PrimaryIndexFile&   doctorIDIndex;
    PrimaryIndexFile&   appointmentIDIndex;
    SecondaryIndexFile& appointmentDoctorIDIndex;
    SecondaryIndexFile& doctorNameIndex;
    SecondaryIndexFile& doctorSpecialtyIndex;
    LinkedListFile&     secondaryValuesList;
};

// Non-key update (in place). Also fixes the Doctor Name secondary index.
bool updateDoctorName(IndexRefs& idx,
                      const std::string& doctorID,
                      const std::string& newName);

// Key update: Doctor record, Doctor primary index (with shifting),
// Name/Specialty secondary lists, Appointment-by-Doctor index and the
// Foreign Key inside every appointment of this doctor.
bool updateDoctorID(IndexRefs& idx,
                    const std::string& oldID,
                    const std::string& newID);

// Non-key update (in place). No index is built on the date.
bool updateAppointmentDate(IndexRefs& idx,
                           const std::string& appointmentID,
                           const std::string& newDate);

// Search with the primary index, then print. Return false if not found.
bool printDoctorInfo(IndexRefs& idx, const std::string& doctorID);
bool printAppointmentInfo(IndexRefs& idx, const std::string& appointmentID);