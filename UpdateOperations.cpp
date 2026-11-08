#include "UpdateOperations.h"

#include <iostream>
#include <cstring>
#include <vector>

#include "Doctor.h"
#include "Appointment.h"
#include "FileHandler.h"

using namespace std;

// ============================================================
// PRIVATE HELPERS (file-local, so they never clash with main.cpp)
// ============================================================
namespace
{
    string fieldToStr(const char* field, size_t size)
    {
        size_t length = 0;
        while (length < size && field[length] != '\0')
            length++;
        while (length > 0 && field[length - 1] == ' ')
            length--;
        return string(field, length);
    }

    string trimRight(string s)
    {
        while (!s.empty() && s.back() == ' ')
            s.pop_back();
        return s;
    }

    // Copies src into a fixed-size field and zero-pads the rest.
    // Caller guarantees src.size() <= size.
    void setField(char* dest, size_t size, const string& src)
    {
        memset(dest, 0, size);
        memcpy(dest, src.data(), src.size());
    }

    // Same rule used when the indexes are built (key < MAX_STR_SIZE).
    bool validKey(const string& key)
    {
        return !key.empty() && key.size() < MAX_STR_SIZE;
    }

    bool addToSecondary(SecondaryIndexFile& index,
                        LinkedListFile& list,
                        const string& secondaryKey,
                        const string& value)
    {
        int head = LinkedListFile::NULL_POINTER;
        index.find(secondaryKey.c_str(), head);

        int newHead = list.insert(head, value);
        if (newHead == LinkedListFile::NULL_POINTER)
            return false;

        index.insert(secondaryKey.c_str(), newHead);
        return true;
    }

    bool removeFromSecondary(SecondaryIndexFile& index,
                             LinkedListFile& list,
                             const string& secondaryKey,
                             const string& value)
    {
        int head;
        if (!index.find(secondaryKey.c_str(), head))
            return true;

        int newHead = list.remove(head, value);

        if (newHead == LinkedListFile::NULL_POINTER)
            index.remove(secondaryKey.c_str());
        else if (newHead != head)
            index.insert(secondaryKey.c_str(), newHead);

        return true;
    }

    bool validDoctorID(const string& id)
    {
        return validKey(id) &&
               id.size() <= sizeof(Doctor::doctorID) &&
               id[0] != '*';
    }
}


// ============================================================
// UPDATE DOCTOR NAME  (non-key, in place)
// ============================================================
bool updateDoctorName(IndexRefs& idx,
                      const string& doctorIDIn,
                      const string& newNameIn)
{
    string doctorID = trimRight(doctorIDIn);
    string newName  = trimRight(newNameIn);

    if (doctorID.empty() || doctorID.size() > sizeof(Doctor::doctorID))
    {
        cout << "updateDoctorName: invalid Doctor ID.\n";
        return false;
    }

    // Must fit the 30-byte field AND be a valid index key (< 30).
    if (!validKey(newName) || newName.size() > sizeof(Doctor::doctorName))
    {
        cout << "updateDoctorName: name must be 1 to "
             << (MAX_STR_SIZE - 1) << " characters.\n";
        return false;
    }

    // Search with the primary index (no full file scan).
    int offset;
    if (!idx.doctorIDIndex.find(doctorID.c_str(), offset))
    {
        cout << "updateDoctorName: Doctor not found: " << doctorID << '\n';
        return false;
    }

    Doctor doctor;
    if (!readDoctor(offset, doctor) || isDeletedDoctor(doctor))
    {
        cout << "updateDoctorName: record could not be read.\n";
        return false;
    }

    string oldName = fieldToStr(doctor.doctorName, sizeof(doctor.doctorName));

    if (oldName == newName)
        return true;    // nothing to change

    // In-place update: the record size does not change.
    setField(doctor.doctorName, sizeof(doctor.doctorName), newName);

    if (!writeDoctor(doctor, offset))
    {
        cout << "updateDoctorName: error writing the record.\n";
        return false;
    }

    // The name has a secondary index: move the Doctor ID from the old
    // name's list to the new name's list.
    removeFromSecondary(idx.doctorNameIndex, idx.secondaryValuesList,
                        oldName, doctorID);

    if (!addToSecondary(idx.doctorNameIndex, idx.secondaryValuesList,
                        newName, doctorID))
    {
        cout << "updateDoctorName: error updating the name index.\n";
        return false;
    }

    return true;
}


// ============================================================
// UPDATE APPOINTMENT DATE  (non-key, in place)
// ============================================================
bool updateAppointmentDate(IndexRefs& idx,
                           const string& appointmentIDIn,
                           const string& newDateIn)
{
    string appointmentID = trimRight(appointmentIDIn);
    string newDate       = trimRight(newDateIn);

    if (appointmentID.empty() ||
        appointmentID.size() > sizeof(Appointment::appointmentID))
    {
        cout << "updateAppointmentDate: invalid Appointment ID.\n";
        return false;
    }

    if (newDate.empty() ||
        newDate.size() > sizeof(Appointment::appointmentDate))
    {
        cout << "updateAppointmentDate: date must be 1 to "
             << sizeof(Appointment::appointmentDate) << " characters.\n";
        return false;
    }

    int offset;
    if (!idx.appointmentIDIndex.find(appointmentID.c_str(), offset))
    {
        cout << "updateAppointmentDate: Appointment not found: "
             << appointmentID << '\n';
        return false;
    }

    Appointment appointment;
    if (!readAppointment(offset, appointment) ||
        isDeletedAppointment(appointment))
    {
        cout << "updateAppointmentDate: record could not be read.\n";
        return false;
    }

    setField(appointment.appointmentDate,
             sizeof(appointment.appointmentDate), newDate);

    if (!writeAppointment(appointment, offset))
    {
        cout << "updateAppointmentDate: error writing the record.\n";
        return false;
    }

    return true;    // the date has no index, so nothing else to update
}


// ============================================================
// UPDATE DOCTOR ID  (primary key update)
// ============================================================
bool updateDoctorID(IndexRefs& idx,
                    const string& oldIDIn,
                    const string& newIDIn)
{
    string oldID = trimRight(oldIDIn);
    string newID = trimRight(newIDIn);

    if (oldID.empty() || oldID.size() > sizeof(Doctor::doctorID))
    {
        cout << "updateDoctorID: invalid old Doctor ID.\n";
        return false;
    }

    if (!validDoctorID(newID))
    {
        cout << "updateDoctorID: invalid new Doctor ID.\n";
        return false;
    }

    if (oldID == newID)
    {
        cout << "updateDoctorID: the new ID is the same as the old one.\n";
        return false;
    }

    // ---- The old doctor must exist ---------------------------
    int offset;
    if (!idx.doctorIDIndex.find(oldID.c_str(), offset))
    {
        cout << "updateDoctorID: Doctor not found: " << oldID << '\n';
        return false;
    }

    // ---- The new ID must be unused ---------------------------
    int dummy;
    if (idx.doctorIDIndex.find(newID.c_str(), dummy))
    {
        cout << "updateDoctorID: Doctor ID already exists: " << newID << '\n';
        return false;
    }

    Doctor doctor;
    if (!readDoctor(offset, doctor) || isDeletedDoctor(doctor))
    {
        cout << "updateDoctorID: record could not be read.\n";
        return false;
    }

    string doctorName = fieldToStr(doctor.doctorName, sizeof(doctor.doctorName));
    string specialty  = fieldToStr(doctor.specialty,  sizeof(doctor.specialty));

    // ---- Collect this doctor's appointments BEFORE changing anything
    vector<string> appointmentIDs;
    int appointmentsHead = LinkedListFile::NULL_POINTER;
    bool hasAppointments =
        idx.appointmentDoctorIDIndex.find(oldID.c_str(), appointmentsHead);

    if (hasAppointments)
        appointmentIDs = idx.secondaryValuesList.getAllElements(appointmentsHead);

    // ---- 1. Doctor record (in place, same size) --------------
    setField(doctor.doctorID, sizeof(doctor.doctorID), newID);

    if (!writeDoctor(doctor, offset))
    {
        cout << "updateDoctorID: error writing the Doctor record.\n";
        return false;
    }

    // ---- 2. Primary index: remove old key, insert new key ----
    // IndexFile::remove/insert use vector erase/insert, so the entries
    // are shifted and the index stays sorted ascending.
    idx.doctorIDIndex.remove(oldID.c_str());
    idx.doctorIDIndex.insert(newID.c_str(), offset);

    // ---- 3. Name and Specialty lists hold Doctor IDs ---------
    removeFromSecondary(idx.doctorNameIndex, idx.secondaryValuesList,
                        doctorName, oldID);
    addToSecondary(idx.doctorNameIndex, idx.secondaryValuesList,
                   doctorName, newID);

    removeFromSecondary(idx.doctorSpecialtyIndex, idx.secondaryValuesList,
                        specialty, oldID);
    addToSecondary(idx.doctorSpecialtyIndex, idx.secondaryValuesList,
                   specialty, newID);

    // ---- 4. Appointment-by-Doctor index: re-key it -----------
    // Its linked-list values are Appointment IDs (unchanged), so only the
    // key changes: same list head, new key.
    if (hasAppointments)
    {
        idx.appointmentDoctorIDIndex.remove(oldID.c_str());
        idx.appointmentDoctorIDIndex.insert(newID.c_str(), appointmentsHead);
    }

    // ---- 5. Foreign Key inside every appointment of the doctor
    bool ok = true;

    for (const string& appointmentID : appointmentIDs)
    {
        int appointmentOffset;
        Appointment appointment;

        if (!idx.appointmentIDIndex.find(appointmentID.c_str(), appointmentOffset) ||
            !readAppointment(appointmentOffset, appointment) ||
            isDeletedAppointment(appointment))
        {
            cout << "updateDoctorID: could not read appointment "
                 << appointmentID << '\n';
            ok = false;
            continue;
        }

        setField(appointment.doctorID, sizeof(appointment.doctorID), newID);

        if (!writeAppointment(appointment, appointmentOffset))
        {
            cout << "updateDoctorID: could not update appointment "
                 << appointmentID << '\n';
            ok = false;
        }
    }

    return ok;
}


// ============================================================
// PRINT OPERATIONS (search with the primary index)
// ============================================================
bool printDoctorInfo(IndexRefs& idx, const string& doctorIDIn)
{
    string doctorID = trimRight(doctorIDIn);

    int offset;
    if (doctorID.empty() ||
        !idx.doctorIDIndex.find(doctorID.c_str(), offset))
    {
        cout << "Doctor not found: " << doctorID << '\n';
        return false;
    }

    Doctor d;
    if (!readDoctor(offset, d) || isDeletedDoctor(d))
    {
        cout << "Doctor record could not be read.\n";
        return false;
    }

    cout << "Doctor ID: "   << fieldToStr(d.doctorID,   sizeof(d.doctorID))   << '\n'
         << "Doctor Name: " << fieldToStr(d.doctorName, sizeof(d.doctorName)) << '\n'
         << "Specialty: "   << fieldToStr(d.specialty,  sizeof(d.specialty))  << '\n'
         << "Address: "     << fieldToStr(d.address,    sizeof(d.address))    << '\n';

    return true;
}

bool printAppointmentInfo(IndexRefs& idx, const string& appointmentIDIn)
{
    string appointmentID = trimRight(appointmentIDIn);

    int offset;
    if (appointmentID.empty() ||
        !idx.appointmentIDIndex.find(appointmentID.c_str(), offset))
    {
        cout << "Appointment not found: " << appointmentID << '\n';
        return false;
    }

    Appointment a;
    if (!readAppointment(offset, a) || isDeletedAppointment(a))
    {
        cout << "Appointment record could not be read.\n";
        return false;
    }

    cout << "Appointment ID: "   << fieldToStr(a.appointmentID,   sizeof(a.appointmentID))   << '\n'
         << "Appointment Date: " << fieldToStr(a.appointmentDate, sizeof(a.appointmentDate)) << '\n'
         << "Doctor ID: "        << fieldToStr(a.doctorID,        sizeof(a.doctorID))        << '\n';

    return true;
}