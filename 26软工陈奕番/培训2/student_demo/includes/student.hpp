#ifndef STUDENT_HPP
#define STUDENT_HPP

#include <string>

class Student {
public:
    Student(const std::string& name, const std::string& id, double score);

    void printInfo() const;
    void setScore(double newScore);

private:
    std::string name_;
    std::string id_;
    double score_;
};

#endif
