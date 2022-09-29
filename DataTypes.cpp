#include "DataTypes.h"
#include <math.h>

struct vector makeVectorFromPoints(struct point start, struct point end) {
	return { end.x - start.x, end.y - start.y };
}

double dotProduct(struct vector v1, struct vector v2) {
	return v1.x * v2.x + v1.y * v2.y;
}

struct vector normalize(struct vector v) {
	double mag = sqrt(pow(v.x, 2) + pow(v.y, 2));

	return { v.x / mag, v.y / mag };
}

/* Returns the index of the point in pointList which is farthest from the edge between p1 and p2,
* on the left side of the edge.
* If two points are equidistant from the edge, chooses the one which is perpendicular to the further point along the edge
* size: the number of elements in pointList
* Returns -1 if pointList is empty
* Based on the implementation on page 68 of Real-Time Collision Detection
*/
int getPointFarthestFromEdge(struct point p1, struct point p2, std::vector<struct point> *pointList){
	// The vector from p1 to p2
	struct vector v = makeVectorFromPoints(p1, p2);
	// The vector perpendicular to v
	struct vector vPerp = { v.y * -1, v.x };

	if (v.y == 0)
		vPerp.x = 0;

	// Keeps track of the distance and rightness of the current best point
	int bestIndex = -1;
	double maxVal = -FLT_MAX, rightmostVal = -FLT_MAX;

	for (int i = 0; i < pointList->size(); i++) {
		struct vector vectorPointToP1 = makeVectorFromPoints(pointList->at(i), p1);
		// Gets the distance and 'rightness' of the current point

		struct vector normalizedPointToP1 = normalize(vectorPointToP1);
		struct vector normalizedV = normalize(v);
		struct vector normalizedPerp = normalize(vPerp);

		double d = dotProduct(normalizedPointToP1, normalizedPerp);
		double r = dotProduct(normalizedPointToP1, normalizedV);

		if (d > maxVal || (d == maxVal && r > rightmostVal)) {
			bestIndex = i;
			maxVal = d;
			rightmostVal = r;
		}
	}

	return bestIndex;
}

void printPoints(FILE *f, std::vector<struct point> *v, const char *firstLine) {
	if (firstLine)
		fprintf(f, "%s\n", firstLine);

	for (int i = 0; i < v->size(); i++) {
		fprintf(f, "%lf, %lf\n", v->at(i).x, v->at(i).y);
	}

	fprintf(f, "\n");
}