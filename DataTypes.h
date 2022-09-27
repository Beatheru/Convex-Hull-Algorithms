#pragma once

#include <vector>
#include <stdlib.h>

struct vector {
	double x;
	double y;
};

struct vector makeVectorFromPoints(struct point p1, struct point p2);
double dotProduct(struct vector v1, struct vector v2);


struct point {
	double x;
	double y;
};

int getPointFarthestFromEdge(struct point p1, struct point p2, std::vector<struct point> *pointList);
void printPoints(FILE *f, std::vector<struct point> *v, const char *firstLine);