#include "student.hpp"

#include <iostream>

Student::Student(const std::string& name, const std::string& id, double score)
    : name_(name), id_(id), score_(score) {
}

void Student::printInfo() const {
    std::cout << "姓名: " << name_ << '\n'
              << "学号: " << id_ << '\n'
              << "成绩: " << score_ << '\n';
}

void Student::setScore(double newScore) {
    score_ = newScore;
}
