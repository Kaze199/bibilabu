#include "student.hpp"

int main() {
    Student student("CJ", "2026", 85.5);

    student.printInfo();
    student.setScore(92.0);
    student.printInfo();

    return 0;
}
