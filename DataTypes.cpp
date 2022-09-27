#include "DataTypes.h"

struct vector makeVectorFromPoints(struct point start, struct point end) {
	return { start.x - end.x, start.y - end.y };
}

double dotProduct(struct vector v1, struct vector v2) {
	return v1.x * v2.x + v1.y * v2.y;
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

	// Keeps track of the distance and rightness of the current best point
	int bestIndex = -1;
	double maxVal = -FLT_MAX, rightmostVal = -FLT_MAX;

	for (int i = 0; i < pointList->size(); i++) {
		struct vector p1ToPoint = makeVectorFromPoints(p1, pointList->at(i));
		// Gets the distance and 'rightness' of the current point
		double d = dotProduct(p1ToPoint, vPerp);
		double r = dotProduct(p1ToPoint, v);

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