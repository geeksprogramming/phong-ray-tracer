#include "sphere.hpp"
#include <math.h>
#include <glm/vec3.hpp>
#include <glm/gtx/string_cast.hpp>

#include <fstream>
#include <iostream>
using namespace std;

extern bool debugOn;
#define EPSILON 0.00001

Sphere::Sphere(const glm::vec3& center, float radius,
               const Material& color) : Object(color) {
    this->center = center;
    this->radius = radius;
}

bool Sphere::intersects(const glm::vec3& start,
                        const glm::vec3& direction, Hit& hit) {

    glm::vec3 V = direction;
    glm::vec3 S = start;
    glm::vec3 C = center;
    float R = radius;

    float qa = glm::dot(V, V);
    float qb = glm::dot(2.0f * V, (S - C));
    float qc = glm::dot((S - C), (S - C)) - (R * R);
    float d = (qb * qb) - (4.0f * qa * qc);
    
    float t;

    if (d <= EPSILON) {
        return false;
    }
    else {
        float t1 = (-qb - glm::sqrt(d)) / (2 * qa);
        float t2 = (-qb + glm::sqrt(d)) / (2 * qa);
        if (t1 >= EPSILON) {
            t = t1;
        }
        else if (t2 > EPSILON) {
            t = t2;
        }
        else {
            return false;
        }
    }

    glm::vec3 P = S + t * V;
    glm::vec3 N = glm::normalize(P - C);
    hit.set(P, N, this, t);
    return true;
}

