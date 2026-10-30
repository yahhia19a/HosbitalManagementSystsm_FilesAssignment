#include <iostream>
#include "FileHandler.h"
using namespace std;

void printField(char field[], int size) {

    int length = size;

    while (length > 0 && field[length - 1] == ' ') {
        length--;
    }

    cout.write(field, length);
}

int main() {

    initializeFiles();

    cout << "===== Healthcare Management System =====\n";

    Doctor d = createDoctor(
        "D1",
        "Ali",
        "Cardio",
        "Cairo"
    );

    writeDoctor(d, 4);

    Doctor result;

    if (readDoctor(4, result)) {

        cout << "\n===== Doctor Information =====\n";

        cout << "Doctor ID: ";
        printField(result.doctorID, 15);

        cout << "\nDoctor Name: ";
        printField(result.doctorName, 30);

        cout << "\nSpecialty: ";
        printField(result.specialty, 20);

        cout << "\nAddress: ";
        printField(result.address, 30);

        cout << endl;
    }
    else {
        cout << "Error reading Doctor!\n";
    }
    Appointment a = createAppointment(
        "A1",
        "30-10-2026",
        "D1"
    );

    writeAppointment(a, 4);

    Appointment result2;

    if (readAppointment(4, result2)) {

        cout << "\n===== Appointment Information =====\n";

        cout << "Appointment ID: ";
        printField(result2.appointmentID, 15);

        cout << "\nAppointment Date: ";
        printField(result2.appointmentDate, 30);

        cout << "\nDoctor ID: ";
        printField(result2.doctorID, 15);

        cout << endl;
    }
    else {
        cout << "Error reading Appointment!\n";
    }

    cout << "\n===== File Information =====\n";

    cout << "Doctors.dat size: "
        << getDoctorFileEnd() << " bytes\n";

    cout << "Appointments.dat size: "
        << getAppointmentFileEnd() << " bytes\n";

    cout << "\nProgram finished successfully!\n";

    return 0;
}