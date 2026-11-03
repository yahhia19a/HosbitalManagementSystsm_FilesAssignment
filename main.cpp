#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <cstdint>
#include <cstring>

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


// Removes trailing spaces (the same normalization fieldToString applies
// when records are read back, so index keys always match).
string trimTrailingSpaces(string s)
{
    while (!s.empty() && s.back() == ' ')
        s.pop_back();

    return s;
}


// Copies a string into a fixed-size field, zero-padding the rest.
// The caller must make sure src.size() <= size.
void copyToField(char* dest, size_t size, const string& src)
{
    memset(dest, 0, size);
    memcpy(dest, src.data(), src.size());
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
    // ADD DOCTOR
    //
    // 1. Validate the input.
    // 2. Reject a duplicate Doctor ID (primary index).
    // 3. Get a slot: AVAIL LIST head (LIFO) or end of file.
    // 4. Write the 95-byte record.
    // 5. Update the primary index and the Name / Specialty
    //    secondary indexes.
    // ========================================================

    bool addDoctor(
        const string& doctorIDIn,
        const string& doctorNameIn,
        const string& specialtyIn,
        const string& addressIn)
    {
        string doctorID = trimTrailingSpaces(doctorIDIn);
        string doctorName = trimTrailingSpaces(doctorNameIn);
        string specialty = trimTrailingSpaces(specialtyIn);
        string address = trimTrailingSpaces(addressIn);


        // ---- Validation --------------------------------------

        if (doctorID.empty() ||
            doctorID.size() > sizeof(Doctor::doctorID) ||
            doctorID[0] == '*')
        {
            cout << "addDoctor: invalid Doctor ID.\n";
            return false;
        }

        if (!isValidIndexKey(doctorName) ||
            doctorName.size() > sizeof(Doctor::doctorName))
        {
            cout << "addDoctor: invalid Doctor name.\n";
            return false;
        }

        if (!isValidIndexKey(specialty) ||
            specialty.size() > sizeof(Doctor::specialty))
        {
            cout << "addDoctor: invalid specialty.\n";
            return false;
        }

        if (address.size() > sizeof(Doctor::address))
        {
            cout << "addDoctor: address is too long.\n";
            return false;
        }


        // ---- Duplicate check (primary index) -----------------

        int existingOffset;

        if (doctorIDIndex.find(doctorID.c_str(), existingOffset))
        {
            cout << "addDoctor: Doctor ID already exists: "
                 << doctorID << '\n';
            return false;
        }


        // ---- Get a slot (AVAIL LIST or end of file) ----------

        int offset = allocateDoctorOffset();

        if (offset < 0)
        {
            cout << "addDoctor: could not allocate a record slot.\n";
            return false;
        }


        // ---- Build and write the fixed-size record -----------

        Doctor doctor;

        copyToField(doctor.doctorID, sizeof(doctor.doctorID), doctorID);
        copyToField(doctor.doctorName, sizeof(doctor.doctorName), doctorName);
        copyToField(doctor.specialty, sizeof(doctor.specialty), specialty);
        copyToField(doctor.address, sizeof(doctor.address), address);

        if (!writeDoctor(doctor, offset))
        {
            // Put the slot back so it is not lost.
            freeDoctorOffset(offset);

            cout << "addDoctor: error writing the record.\n";
            return false;
        }


        // ---- Update the indexes ------------------------------

        doctorIDIndex.insert(doctorID.c_str(), offset);

        if (!addToSecondary(doctorNameIndex, doctorName, doctorID) ||
            !addToSecondary(doctorSpecialtyIndex, specialty, doctorID))
        {
            cout << "addDoctor: error updating secondary indexes.\n";
            return false;
        }

        return true;
    }


    // ========================================================
    // ADD APPOINTMENT
    //
    // 1. Validate the input.
    // 2. Reject a duplicate Appointment ID.
    // 3. The Doctor ID must exist (Doctor primary index).
    // 4. Reuse a deleted slot or append; write 60-byte record.
    // 5. Update the primary index and the Doctor ID secondary
    //    index.
    // ========================================================

    bool addAppointment(
        const string& appointmentIDIn,
        const string& appointmentDateIn,
        const string& doctorIDIn)
    {
        string appointmentID = trimTrailingSpaces(appointmentIDIn);
        string appointmentDate = trimTrailingSpaces(appointmentDateIn);
        string doctorID = trimTrailingSpaces(doctorIDIn);


        // ---- Validation --------------------------------------

        if (appointmentID.empty() ||
            appointmentID.size() > sizeof(Appointment::appointmentID) ||
            appointmentID[0] == '*')
        {
            cout << "addAppointment: invalid Appointment ID.\n";
            return false;
        }

        if (appointmentDate.empty() ||
            appointmentDate.size() > sizeof(Appointment::appointmentDate))
        {
            cout << "addAppointment: invalid Appointment date.\n";
            return false;
        }

        if (doctorID.empty() ||
            doctorID.size() > sizeof(Appointment::doctorID))
        {
            cout << "addAppointment: invalid Doctor ID.\n";
            return false;
        }


        // ---- Duplicate Appointment ID ------------------------

        int existingOffset;

        if (appointmentIDIndex.find(appointmentID.c_str(), existingOffset))
        {
            cout << "addAppointment: Appointment ID already exists: "
                 << appointmentID << '\n';
            return false;
        }


        // ---- Doctor must exist -------------------------------

        int doctorOffset;

        if (!doctorIDIndex.find(doctorID.c_str(), doctorOffset))
        {
            cout << "addAppointment: Doctor ID does not exist: "
                 << doctorID << '\n';
            return false;
        }


        // ---- Get a slot (AVAIL LIST or end of file) ----------

        int offset = allocateAppointmentOffset();

        if (offset < 0)
        {
            cout << "addAppointment: could not allocate a record slot.\n";
            return false;
        }


        // ---- Build and write the fixed-size record -----------

        Appointment appointment;

        copyToField(appointment.appointmentID,
                    sizeof(appointment.appointmentID),
                    appointmentID);

        copyToField(appointment.appointmentDate,
                    sizeof(appointment.appointmentDate),
                    appointmentDate);

        copyToField(appointment.doctorID,
                    sizeof(appointment.doctorID),
                    doctorID);

        if (!writeAppointment(appointment, offset))
        {
            freeAppointmentOffset(offset);

            cout << "addAppointment: error writing the record.\n";
            return false;
        }


        // ---- Update the indexes ------------------------------

        appointmentIDIndex.insert(appointmentID.c_str(), offset);

        if (!addToSecondary(appointmentDoctorIDIndex, doctorID, appointmentID))
        {
            cout << "addAppointment: error updating secondary index.\n";
            return false;
        }

        return true;
    }


    // ========================================================
    // DELETE DOCTOR
    //
    // Policy: a doctor who still has appointments can NOT be
    // deleted (delete the appointments first).
    //
    // 1. Find the record with the primary index.
    // 2. Refuse if the doctor has appointments.
    // 3. Logical deletion ('*') + push on the AVAIL LIST and
    //    update the header.
    // 4. Remove the doctor from the primary index and from the
    //    Name / Specialty secondary indexes.
    // ========================================================

    bool deleteDoctor(const string& doctorIDIn)
    {
        string doctorID = trimTrailingSpaces(doctorIDIn);

        if (doctorID.empty() ||
            doctorID.size() > sizeof(Doctor::doctorID))
        {
            cout << "deleteDoctor: invalid Doctor ID.\n";
            return false;
        }


        // ---- Search with the primary index -------------------

        int offset;

        if (!doctorIDIndex.find(doctorID.c_str(), offset))
        {
            cout << "deleteDoctor: Doctor not found: "
                 << doctorID << '\n';
            return false;
        }


        // ---- Read the record (needed for the secondary keys) -

        Doctor doctor;

        if (!readDoctor(offset, doctor) || isDeletedDoctor(doctor))
        {
            cout << "deleteDoctor: record could not be read.\n";
            return false;
        }

        string doctorName = fieldToString(
            doctor.doctorName, sizeof(doctor.doctorName));

        string specialty = fieldToString(
            doctor.specialty, sizeof(doctor.specialty));


        // ---- Deletion policy: doctor must have no appointments

        int appointmentsHead;

        if (appointmentDoctorIDIndex.find(doctorID.c_str(), appointmentsHead) &&
            appointmentsHead != LinkedListFile::NULL_POINTER)
        {
            cout << "deleteDoctor: Doctor " << doctorID
                 << " still has appointments. Delete them first.\n";
            return false;
        }


        // ---- Logical deletion + AVAIL LIST + header ----------

        if (!freeDoctorOffset(offset))
        {
            cout << "deleteDoctor: error updating the data file.\n";
            return false;
        }


        // ---- Update the indexes ------------------------------

        doctorIDIndex.remove(doctorID.c_str());

        removeFromSecondary(doctorNameIndex, doctorName, doctorID);
        removeFromSecondary(doctorSpecialtyIndex, specialty, doctorID);

        return true;
    }


    // ========================================================
    // DELETE APPOINTMENT
    //
    // 1. Find the record with the primary index.
    // 2. Logical deletion ('*') + push on the AVAIL LIST and
    //    update the header.
    // 3. Remove it from the primary index and from the
    //    Doctor ID secondary index.
    // ========================================================

    bool deleteAppointment(const string& appointmentIDIn)
    {
        string appointmentID = trimTrailingSpaces(appointmentIDIn);

        if (appointmentID.empty() ||
            appointmentID.size() > sizeof(Appointment::appointmentID))
        {
            cout << "deleteAppointment: invalid Appointment ID.\n";
            return false;
        }


        // ---- Search with the primary index -------------------

        int offset;

        if (!appointmentIDIndex.find(appointmentID.c_str(), offset))
        {
            cout << "deleteAppointment: Appointment not found: "
                 << appointmentID << '\n';
            return false;
        }


        // ---- Read the record (needed for the secondary key) --

        Appointment appointment;

        if (!readAppointment(offset, appointment) ||
            isDeletedAppointment(appointment))
        {
            cout << "deleteAppointment: record could not be read.\n";
            return false;
        }

        string doctorID = fieldToString(
            appointment.doctorID, sizeof(appointment.doctorID));


        // ---- Logical deletion + AVAIL LIST + header ----------

        if (!freeAppointmentOffset(offset))
        {
            cout << "deleteAppointment: error updating the data file.\n";
            return false;
        }


        // ---- Update the indexes ------------------------------

        appointmentIDIndex.remove(appointmentID.c_str());

        removeFromSecondary(appointmentDoctorIDIndex, doctorID, appointmentID);

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
    // ADD ONE VALUE TO A SECONDARY INDEX
    //
    // secondaryKey -> linked list of values (in the shared
    // LinkedListFile). Creates the key if it does not exist,
    // otherwise prepends to the existing list and updates the
    // head pointer stored in the index.
    // ========================================================

    bool addToSecondary(
        SecondaryIndexFile& index,
        const string& secondaryKey,
        const string& value)
    {
        int headPointer = LinkedListFile::NULL_POINTER;

        // If the key is not found, headPointer stays NULL_POINTER
        // and a new logical list is started.
        index.find(secondaryKey.c_str(), headPointer);

        int newHead = secondaryValuesList.insert(headPointer, value);

        if (newHead == LinkedListFile::NULL_POINTER)
            return false;

        // insert() adds a new key or updates the head of an existing one.
        index.insert(secondaryKey.c_str(), newHead);

        return true;
    }


    // ========================================================
    // REMOVE ONE VALUE FROM A SECONDARY INDEX
    //
    // Unlinks the value from the key's linked list. If the list
    // becomes empty, the secondary key is removed from the index.
    // ========================================================

    bool removeFromSecondary(
        SecondaryIndexFile& index,
        const string& secondaryKey,
        const string& value)
    {
        int headPointer;

        if (!index.find(secondaryKey.c_str(), headPointer))
            return true;    // nothing to remove


        int newHead = secondaryValuesList.remove(headPointer, value);

        if (newHead == LinkedListFile::NULL_POINTER)
        {
            // The list is empty now.
            index.remove(secondaryKey.c_str());
        }
        else if (newHead != headPointer)
        {
            // The head node was removed.
            index.insert(secondaryKey.c_str(), newHead);
        }

        return true;
    }


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


            // Logically deleted records (on the AVAIL LIST) are not indexed.

            if (isDeletedDoctor(doctor))
            {
                offset += sizeof(Doctor);
                continue;
            }


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


            // Logically deleted records (on the AVAIL LIST) are not indexed.

            if (isDeletedAppointment(appointment))
            {
                offset += sizeof(Appointment);
                continue;
            }


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
// SIMPLE TEST HELPER
// ============================================================

static int g_passed = 0;
static int g_failed = 0;

void check(bool condition, const string& label)
{
    if (condition)
    {
        g_passed++;
        cout << "[PASS] " << label << '\n';
    }
    else
    {
        g_failed++;
        cout << "[FAIL] " << label << '\n';
    }
}


// ============================================================
// MAIN
// ============================================================

// When true, the data files are deleted at program start so the
// test sequence below always runs on a clean database.
// Set it to false to keep data between runs.
static const bool RESET_DATA_FOR_DEMO = true;


int main()
{
    // --------------------------------------------
    // 1. Initialize data files.
    // --------------------------------------------

    if (RESET_DATA_FOR_DEMO)
    {
        remove("Doctors.dat");
        remove("Appointments.dat");
    }

    initializeFiles();

    cout << "===== Healthcare Management System =====\n";


    // --------------------------------------------
    // 2. Remove old index files.
    //
    // This is necessary because we are rebuilding
    // all indexes from the data files.
    // --------------------------------------------

    resetIndexFiles();


    {
        // ----------------------------------------
        // 3. Construct the index manager.
        //
        // It opens:
        //   5 index files
        //   1 shared linked-list file
        // ----------------------------------------

        HealthcareIndexManager indexManager;


        // ----------------------------------------
        // 4. Build all five indexes.
        // ----------------------------------------

        if (!indexManager.buildAllIndexes())
        {
            cout << "Failed to build indexes!\n";
            return 1;
        }


        // ----------------------------------------
        // 5. Add the sample Doctor and Appointment
        //    (through addDoctor / addAppointment).
        // ----------------------------------------

        if (!indexManager.addDoctor("D1", "Ali", "Cardio", "Cairo"))
        {
            cout << "Error writing Doctor!\n";
            return 1;
        }

        if (!indexManager.addAppointment("A1", "30-10-2026", "D1"))
        {
            cout << "Error writing Appointment!\n";
            return 1;
        }


        // ====================================================
        // TEST 1: FIND DOCTOR BY DOCTOR ID
        // ====================================================

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


        // ====================================================
        // TEST 2: FIND APPOINTMENT BY APPOINTMENT ID
        // ====================================================

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


        // ====================================================
        // TEST 3: FIND APPOINTMENTS BY DOCTOR ID
        // ====================================================

        cout << "\n===== Appointments By Doctor ID =====\n";

        vector<Appointment> appointments =
            indexManager.getAppointmentsByDoctor("D1");

        for (const Appointment& appointment : appointments)
        {
            printAppointment(appointment);
            cout << "------------------------\n";
        }


        // ====================================================
        // TEST 4: FIND DOCTORS BY NAME
        // ====================================================

        cout << "\n===== Doctors By Name =====\n";

        vector<Doctor> doctorsByName =
            indexManager.getDoctorsByName("Ali");

        for (const Doctor& doctor : doctorsByName)
        {
            printDoctor(doctor);
            cout << "------------------------\n";
        }


        // ====================================================
        // TEST 5: FIND DOCTORS BY SPECIALTY
        // ====================================================

        cout << "\n===== Doctors By Specialty =====\n";

        vector<Doctor> doctorsBySpecialty =
            indexManager.getDoctorsBySpecialty("Cardio");

        for (const Doctor& doctor : doctorsBySpecialty)
        {
            printDoctor(doctor);
            cout << "------------------------\n";
        }


        // ====================================================
        // TEST 6: ADD / DELETE / AVAIL LIST
        // ====================================================

        cout << "\n===== Add / Delete / AVAIL LIST Tests =====\n";

        // ---- Duplicate IDs and invalid Doctor ID -----------

        check(!indexManager.addDoctor("D1", "Zed", "Neuro", "Giza"),
              "Duplicate Doctor ID is rejected");

        check(!indexManager.addAppointment("A1", "01-01-2027", "D1"),
              "Duplicate Appointment ID is rejected");

        check(!indexManager.addAppointment("A99", "01-01-2027", "D99"),
              "Appointment with a non-existing Doctor ID is rejected");

        // ---- More records ----------------------------------

        check(indexManager.addDoctor("D2", "Sara", "Neuro", "Giza"),
              "Add Doctor D2");

        check(indexManager.addDoctor("D3", "Omar", "Cardio", "Alexandria"),
              "Add Doctor D3");

        check(indexManager.addAppointment("A2", "31-10-2026", "D1"),
              "Add Appointment A2 (Doctor D1)");

        check(indexManager.addAppointment("A3", "01-11-2026", "D2"),
              "Add Appointment A3 (Doctor D2)");

        // ---- Deletion policy -------------------------------

        check(!indexManager.deleteDoctor("D1"),
              "Cannot delete Doctor D1 while it has appointments");

        // ---- Delete appointments (multiple deletions) ------

        check(indexManager.deleteAppointment("A1"),
              "Delete Appointment A1");

        check(indexManager.deleteAppointment("A2"),
              "Delete Appointment A2");

        // ---- Delete doctors --------------------------------

        check(indexManager.deleteDoctor("D1"),
              "Delete Doctor D1 (no appointments left)");

        check(indexManager.deleteDoctor("D3"),
              "Delete Doctor D3");
        // ---- Reuse deleted doctor slots (LIFO) -------------

        check(indexManager.addDoctor("D4", "Mona", "Cardio", "Cairo"),
              "Add Doctor D4 (reuses D3 slot)");

        check(indexManager.addDoctor("D5", "Ali", "Derma", "Tanta"),
              "Add Doctor D5 (reuses D1 slot)");

        check(indexManager.addDoctor("D6", "Sara", "Neuro", "Luxor"),
              "Add Doctor D6 (appended)");

        // ---- Reuse deleted appointment slots (LIFO) --------

        check(indexManager.addAppointment("A4", "02-11-2026", "D2"),
              "Add Appointment A4 (reuses A2 slot)");

        check(indexManager.addAppointment("A5", "03-11-2026", "D4"),
              "Add Appointment A5 (reuses A1 slot)");

        // ---- Index contents after the changes --------------

        check(!indexManager.deleteDoctor("D4"),
              "Cannot delete D4 while it has appointment A5");
    }
    // The manager is destroyed here: all index files are saved to disk.


    // ========================================================
    // TEST 7: REBUILD ALL INDEXES FROM THE DATA FILES
    //
    // Deleted records must be skipped by the rebuild.
    // ========================================================

   cout << "\n===== Rebuild Indexes From Data Files =====\n";

resetIndexFiles();

{
    HealthcareIndexManager rebuilt;

    if (!rebuilt.buildAllIndexes())
    {
        cout << "Failed to rebuild indexes!\n";
        return 1;
    }

    Doctor d;

    check(rebuilt.findDoctor("D2", d),
          "Rebuild: D2 found");
}

    // ========================================================
    // FILE INFORMATION
    // ========================================================

    cout << "\n===== File Information =====\n";

    cout << "Doctors.dat size: "
         << getDoctorFileEnd() << " bytes\n";

    cout << "Appointments.dat size: "
         << getAppointmentFileEnd() << " bytes\n";


    cout << "\nTests passed: " << g_passed
         << ", failed: " << g_failed << '\n';

    cout << "\nProgram finished "
         << (g_failed == 0 ? "successfully!" : "WITH FAILURES!") << '\n';

    return g_failed == 0 ? 0 : 1;
}