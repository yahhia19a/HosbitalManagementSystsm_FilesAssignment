#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <cstdint>

#include "Doctor.h"
#include "Appointment.h"
#include "FileHandler.h"
#include "IndexFile.h"
#include "LinkedListFile.h"

using namespace std;


// ============================================================
// UTILITY FUNCTIONS
// ============================================================

// Safely converts a fixed-size character array into a string.
string fieldToString(const char* field, size_t size)
{
    size_t length = 0;

    while (length < size && field[length] != '\0')
        length++;

    // Remove trailing spaces.
    while (length > 0 && field[length - 1] == ' ')
        length--;

    return string(field, length);
}


// Prints a fixed-size character array.
void printField(const char field[], int size)
{
    cout << fieldToString(field, size);
}


// Checks whether a string can be stored in an index field.
bool isValidIndexKey(const string& key)
{
    return !key.empty() && key.length() < MAX_STR_SIZE;
}


// ============================================================
// RESET INDEX FILES
// ============================================================

// Delete old indexes before rebuilding them.
// There is ONLY ONE linked-list file.
void resetIndexFiles()
{
    remove("DoctorID.idx");
    remove("AppointmentID.idx");

    remove("AppointmentDoctorID.idx");
    remove("DoctorName.idx");
    remove("DoctorSpecialty.idx");

    remove("SecondaryValues.ll");
}


// ============================================================
// HEALTHCARE INDEX MANAGER
// ============================================================

class HealthcareIndexManager
{
private:

    // --------------------------------------------------------
    // PRIMARY INDEXES
    // --------------------------------------------------------

    PrimaryIndexFile doctorIDIndex;
    PrimaryIndexFile appointmentIDIndex;


    // --------------------------------------------------------
    // SECONDARY INDEXES
    // --------------------------------------------------------

    SecondaryIndexFile appointmentDoctorIDIndex;
    SecondaryIndexFile doctorNameIndex;
    SecondaryIndexFile doctorSpecialtyIndex;


    // --------------------------------------------------------
    // ONE SHARED LINKED-LIST FILE
    // --------------------------------------------------------

    LinkedListFile secondaryValuesList;


    // --------------------------------------------------------
    // TEMPORARY GROUPS USED WHEN BUILDING SECONDARY INDEXES
    // --------------------------------------------------------

    // Doctor Name -> List of Doctor IDs
    map<string, vector<string>> doctorNameGroups;

    // Specialty -> List of Doctor IDs
    map<string, vector<string>> doctorSpecialtyGroups;

    // Doctor ID -> List of Appointment IDs
    map<string, vector<string>> appointmentDoctorGroups;


public:

    // ========================================================
    // CONSTRUCTOR
    // ========================================================

    HealthcareIndexManager()

        : doctorIDIndex("DoctorID.idx"),

          appointmentIDIndex("AppointmentID.idx"),

          appointmentDoctorIDIndex("AppointmentDoctorID.idx"),

          doctorNameIndex("DoctorName.idx"),

          doctorSpecialtyIndex("DoctorSpecialty.idx"),

          secondaryValuesList("SecondaryValues.ll")

    {
    }


    // ========================================================
    // BUILD ALL FIVE INDEXES
    // ========================================================

    bool buildAllIndexes()
    {
        cout << "\nBuilding indexes...\n";


        // 1. Build Doctor primary index.
        // 2. Collect Doctor Name groups.
        // 3. Collect Doctor Specialty groups.

        if (!buildDoctorIndexes())
        {
            cout << "Error building Doctor indexes!\n";
            return false;
        }


        // 4. Build Appointment primary index.
        // 5. Collect Appointment Doctor ID groups.

        if (!buildAppointmentIndexes())
        {
            cout << "Error building Appointment indexes!\n";
            return false;
        }


        // Build the three secondary indexes.
        // All three use the SAME LinkedListFile object.

        if (!buildSecondaryIndexes())
        {
            cout << "Error building Secondary indexes!\n";
            return false;
        }


        cout << "All five indexes built successfully!\n";

        return true;
    }


    // ========================================================
    // FIND DOCTOR USING PRIMARY INDEX
    // ========================================================

    bool findDoctor(
        const string& doctorID,
        Doctor& result)
    {
        int byteOffset;

        if (!doctorIDIndex.find(
                doctorID.c_str(),
                byteOffset))
        {
            return false;
        }

        return readDoctor(byteOffset, result);
    }


    // ========================================================
    // FIND APPOINTMENT USING PRIMARY INDEX
    // ========================================================

    bool findAppointment(
        const string& appointmentID,
        Appointment& result)
    {
        int byteOffset;

        if (!appointmentIDIndex.find(
                appointmentID.c_str(),
                byteOffset))
        {
            return false;
        }

        return readAppointment(byteOffset, result);
    }


    // ========================================================
    // FIND APPOINTMENTS BY DOCTOR ID
    // ========================================================

    vector<Appointment> getAppointmentsByDoctor(
        const string& doctorID)
    {
        vector<Appointment> result;

        int headPointer;

        // Find the linked-list head in the secondary index.

        if (!appointmentDoctorIDIndex.find(
                doctorID.c_str(),
                headPointer))
        {
            return result;
        }


        // Retrieve appointment IDs from the SHARED linked-list file.

        vector<string> appointmentIDs =
            secondaryValuesList.getAllElements(headPointer);


        // Use the Appointment primary index to retrieve records.

        for (const string& appointmentID : appointmentIDs)
        {
            Appointment appointment;

            if (findAppointment(appointmentID, appointment))
            {
                result.push_back(appointment);
            }
        }

        return result;
    }


    // ========================================================
    // FIND DOCTORS BY NAME
    // ========================================================

    vector<Doctor> getDoctorsByName(
        const string& name)
    {
        vector<Doctor> result;

        int headPointer;

        if (!doctorNameIndex.find(
                name.c_str(),
                headPointer))
        {
            return result;
        }


        // Retrieve Doctor IDs from the SHARED linked-list file.

        vector<string> doctorIDs =
            secondaryValuesList.getAllElements(headPointer);


        // Resolve Doctor IDs using the primary index.

        for (const string& doctorID : doctorIDs)
        {
            Doctor doctor;

            if (findDoctor(doctorID, doctor))
            {
                result.push_back(doctor);
            }
        }

        return result;
    }


    // ========================================================
    // FIND DOCTORS BY SPECIALTY
    // ========================================================

    vector<Doctor> getDoctorsBySpecialty(
        const string& specialty)
    {
        vector<Doctor> result;

        int headPointer;

        if (!doctorSpecialtyIndex.find(
                specialty.c_str(),
                headPointer))
        {
            return result;
        }


        // Retrieve Doctor IDs from the SHARED linked-list file.

        vector<string> doctorIDs =
            secondaryValuesList.getAllElements(headPointer);


        // Resolve Doctor IDs using the primary index.

        for (const string& doctorID : doctorIDs)
        {
            Doctor doctor;

            if (findDoctor(doctorID, doctor))
            {
                result.push_back(doctor);
            }
        }

        return result;
    }


private:

    // ========================================================
    // BUILD DOCTOR PRIMARY INDEX AND COLLECT SECONDARY GROUPS
    // ========================================================

    bool buildDoctorIndexes()
    {
        int fileEnd = getDoctorFileEnd();

        const int headerSize = sizeof(int);

        if (fileEnd < headerSize)
            return false;

        // Make sure the data file contains complete records.

        if ((fileEnd - headerSize) % sizeof(Doctor) != 0)
            return false;


        int offset = headerSize;

        while (offset < fileEnd)
        {
            Doctor doctor;

            if (!readDoctor(offset, doctor))
                return false;


            // Extract fields safely.

            string doctorID = fieldToString(
                doctor.doctorID,
                sizeof(doctor.doctorID)
            );

            string doctorName = fieldToString(
                doctor.doctorName,
                sizeof(doctor.doctorName)
            );

            string specialty = fieldToString(
                doctor.specialty,
                sizeof(doctor.specialty)
            );


            // Validate index keys.

            if (!isValidIndexKey(doctorID) ||
                !isValidIndexKey(doctorName) ||
                !isValidIndexKey(specialty))
            {
                cout << "Invalid Doctor index key at offset "
                     << offset << '\n';

                return false;
            }


            // ------------------------------------------------
            // PRIMARY INDEX:
            // Doctor ID -> Doctor byte offset
            // ------------------------------------------------

            if (!doctorIDIndex.insert(
                    doctorID.c_str(),
                    offset))
            {
                cout << "Error inserting Doctor ID: "
                     << doctorID << '\n';

                return false;
            }


            // ------------------------------------------------
            // SECONDARY GROUP:
            // Doctor Name -> Doctor IDs
            // ------------------------------------------------

            doctorNameGroups[doctorName].push_back(doctorID);


            // ------------------------------------------------
            // SECONDARY GROUP:
            // Specialty -> Doctor IDs
            // ------------------------------------------------

            doctorSpecialtyGroups[specialty].push_back(doctorID);


            offset += sizeof(Doctor);
        }

        return true;
    }


    // ========================================================
    // BUILD APPOINTMENT PRIMARY INDEX AND COLLECT GROUPS
    // ========================================================

    bool buildAppointmentIndexes()
    {
        int fileEnd = getAppointmentFileEnd();

        const int headerSize = sizeof(int);

        if (fileEnd < headerSize)
            return false;

        if ((fileEnd - headerSize) % sizeof(Appointment) != 0)
            return false;


        int offset = headerSize;

        while (offset < fileEnd)
        {
            Appointment appointment;

            if (!readAppointment(offset, appointment))
                return false;


            // Extract fields safely.

            string appointmentID = fieldToString(
                appointment.appointmentID,
                sizeof(appointment.appointmentID)
            );

            string doctorID = fieldToString(
                appointment.doctorID,
                sizeof(appointment.doctorID)
            );


            if (!isValidIndexKey(appointmentID) ||
                !isValidIndexKey(doctorID))
            {
                cout << "Invalid Appointment index key at offset "
                     << offset << '\n';

                return false;
            }


            // ------------------------------------------------
            // PRIMARY INDEX:
            // Appointment ID -> Appointment byte offset
            // ------------------------------------------------

            if (!appointmentIDIndex.insert(
                    appointmentID.c_str(),
                    offset))
            {
                cout << "Error inserting Appointment ID: "
                     << appointmentID << '\n';

                return false;
            }


            // ------------------------------------------------
            // SECONDARY GROUP:
            // Doctor ID -> Appointment IDs
            // ------------------------------------------------

            appointmentDoctorGroups[doctorID].push_back(
                appointmentID
            );


            offset += sizeof(Appointment);
        }

        return true;
    }


    // ========================================================
    // BUILD ONE SECONDARY INDEX AND ITS LOGICAL LINKED LISTS
    // ========================================================

    bool buildOneSecondaryIndex(
        SecondaryIndexFile& index,
        const map<string, vector<string>>& groups)
    {
        for (const auto& group : groups)
        {
            const string& secondaryKey = group.first;

            const vector<string>& values = group.second;


            if (!isValidIndexKey(secondaryKey) || values.empty())
                return false;


            // Start a new logical linked list.

            int32_t headPointer = LinkedListFile::NULL_POINTER;


            // ------------------------------------------------
            // Insert values in reverse order.
            //
            // LinkedListFile::insert() prepends nodes.
            //
            // All nodes are written into the SAME physical
            // SecondaryValues.ll file.
            // ------------------------------------------------

            for (auto it = values.rbegin();
                 it != values.rend();
                 ++it)
            {
                int32_t newHead = secondaryValuesList.insert(
                    headPointer,
                    *it
                );


                if (newHead == LinkedListFile::NULL_POINTER)
                {
                    cout << "Error inserting linked-list value: "
                         << *it << '\n';

                    return false;
                }


                headPointer = newHead;
            }


            // ------------------------------------------------
            // Insert secondary key -> linked-list head pointer.
            //
            // Each secondary key is inserted exactly once.
            // No update() method is required.
            // ------------------------------------------------

            if (!index.insert(
                    secondaryKey.c_str(),
                    headPointer))
            {
                cout << "Error inserting secondary key: "
                     << secondaryKey << '\n';

                return false;
            }
        }

        return true;
    }


    // ========================================================
    // BUILD ALL THREE SECONDARY INDEXES
    // ========================================================

    bool buildSecondaryIndexes()
    {
        // --------------------------------------------
        // 1. Doctor Name -> Doctor IDs
        // --------------------------------------------

        if (!buildOneSecondaryIndex(
                doctorNameIndex,
                doctorNameGroups))
        {
            return false;
        }


        // --------------------------------------------
        // 2. Doctor Specialty -> Doctor IDs
        // --------------------------------------------

        if (!buildOneSecondaryIndex(
                doctorSpecialtyIndex,
                doctorSpecialtyGroups))
        {
            return false;
        }


        // --------------------------------------------
        // 3. Doctor ID -> Appointment IDs
        // --------------------------------------------

        if (!buildOneSecondaryIndex(
                appointmentDoctorIDIndex,
                appointmentDoctorGroups))
        {
            return false;
        }


        return true;
    }
};


// ============================================================
// PRINT FUNCTIONS
// ============================================================

void printDoctor(const Doctor& doctor)
{
    cout << "Doctor ID: ";
    printField(doctor.doctorID, sizeof(doctor.doctorID));

    cout << "\nDoctor Name: ";
    printField(doctor.doctorName, sizeof(doctor.doctorName));

    cout << "\nSpecialty: ";
    printField(doctor.specialty, sizeof(doctor.specialty));

    cout << "\nAddress: ";
    printField(doctor.address, sizeof(doctor.address));

    cout << '\n';
}


void printAppointment(const Appointment& appointment)
{
    cout << "Appointment ID: ";
    printField(
        appointment.appointmentID,
        sizeof(appointment.appointmentID)
    );

    cout << "\nAppointment Date: ";
    printField(
        appointment.appointmentDate,
        sizeof(appointment.appointmentDate)
    );

    cout << "\nDoctor ID: ";
    printField(
        appointment.doctorID,
        sizeof(appointment.doctorID)
    );

    cout << '\n';
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // --------------------------------------------
    // 1. Initialize data files.
    // --------------------------------------------

    initializeFiles();

    cout << "===== Healthcare Management System =====\n";


    // --------------------------------------------
    // 2. Write sample Doctor record.
    // --------------------------------------------

    Doctor d = {
        "D1",
        "Ali",
        "Cardio",
        "Cairo"
    };


    if (!writeDoctor(d, sizeof(int)))
    {
        cout << "Error writing Doctor!\n";
        return 1;
    }


    // --------------------------------------------
    // 3. Write sample Appointment record.
    // --------------------------------------------

    Appointment a = {
        "A1",
        "30-10-2026",
        "D1"
    };


    if (!writeAppointment(a, sizeof(int)))
    {
        cout << "Error writing Appointment!\n";
        return 1;
    }


    // --------------------------------------------
    // 4. Remove old index files.
    //
    // This is necessary because we are rebuilding
    // all indexes from the data files.
    // --------------------------------------------

    resetIndexFiles();


    // --------------------------------------------
    // 5. Construct the index manager.
    //
    // It opens:
    //   5 index files
    //   1 shared linked-list file
    // --------------------------------------------

    HealthcareIndexManager indexManager;


    // --------------------------------------------
    // 6. Build all five indexes.
    // --------------------------------------------

    if (!indexManager.buildAllIndexes())
    {
        cout << "Failed to build indexes!\n";
        return 1;
    }


    // ========================================================
    // TEST 1: FIND DOCTOR BY DOCTOR ID
    // ========================================================

    cout << "\n===== Find Doctor By ID =====\n";

    Doctor doctorResult;

    if (indexManager.findDoctor("D1", doctorResult))
    {
        printDoctor(doctorResult);
    }
    else
    {
        cout << "Doctor not found!\n";
    }


    // ========================================================
    // TEST 2: FIND APPOINTMENT BY APPOINTMENT ID
    // ========================================================

    cout << "\n===== Find Appointment By ID =====\n";

    Appointment appointmentResult;

    if (indexManager.findAppointment("A1", appointmentResult))
    {
        printAppointment(appointmentResult);
    }
    else
    {
        cout << "Appointment not found!\n";
    }


    // ========================================================
    // TEST 3: FIND APPOINTMENTS BY DOCTOR ID
    // ========================================================

    cout << "\n===== Appointments By Doctor ID =====\n";

    vector<Appointment> appointments =
        indexManager.getAppointmentsByDoctor("D1");

    for (const Appointment& appointment : appointments)
    {
        printAppointment(appointment);
        cout << "------------------------\n";
    }


    // ========================================================
    // TEST 4: FIND DOCTORS BY NAME
    // ========================================================

    cout << "\n===== Doctors By Name =====\n";

    vector<Doctor> doctorsByName =
        indexManager.getDoctorsByName("Ali");

    for (const Doctor& doctor : doctorsByName)
    {
        printDoctor(doctor);
        cout << "------------------------\n";
    }


    // ========================================================
    // TEST 5: FIND DOCTORS BY SPECIALTY
    // ========================================================

    cout << "\n===== Doctors By Specialty =====\n";

    vector<Doctor> doctorsBySpecialty =
        indexManager.getDoctorsBySpecialty("Cardio");

    for (const Doctor& doctor : doctorsBySpecialty)
    {
        printDoctor(doctor);
        cout << "------------------------\n";
    }


    // ========================================================
    // FILE INFORMATION
    // ========================================================

    cout << "\n===== File Information =====\n";

    cout << "Doctors.dat size: "
         << getDoctorFileEnd() << " bytes\n";

    cout << "Appointments.dat size: "
         << getAppointmentFileEnd() << " bytes\n";


    cout << "\nProgram finished successfully!\n";

    return 0;
}
