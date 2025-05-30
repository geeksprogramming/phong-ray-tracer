#include "triangle.hpp"

#include <math.h>
#include <fstream>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace std;

extern bool debugOn;
#define EPSILON 0.00001

Triangle::Triangle(const glm::vec3& v1,
                   const glm::vec3& v2,
                   const glm::vec3& v3,
                   const Material& color) : Object(color) {
    this->v1 = v1;
    this->v2 = v2;
    this->v3 = v3;

    setNormal();
}

void Triangle::setNormal()
{
    N = glm::cross((v2 - v1), (v3 - v1)); 
    N = glm::normalize(N);
}

bool Triangle::intersects(const glm::vec3& start,
                          const glm::vec3& direction, Hit& hit) {

    glm::vec3 A = v1;
    glm::vec3 B = v2;
    glm::vec3 C = v3;
    glm::vec3 S = start;
    glm::vec3 V = direction;

    float  D = glm::determinant(glm::mat3(V, A-B, A-C));
    float Dt = glm::determinant(glm::mat3(A-S, A-B, A-C));
    float Du = glm::determinant(glm::mat3(V, A-S, A-C));
    float Dv = glm::determinant(glm::mat3(V, A-B, A-S));

    float t = Dt/D;
    float u = Du/D;
    float v = Dv/D;

    // Check if t, u, v, satisfy the constraints
    if (t >= EPSILON && EPSILON <= u && u <= 1 && EPSILON <= v && v <= 1 && EPSILON <= u + v && u + v <= 1) {
        glm::vec3 P = S + t * V;
        hit.set(P, N, this, t);
        return true;
    }

    return false;
}
