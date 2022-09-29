#include "ConvexHull.h"

ConvexHull::ConvexHull(std::vector<struct point> points) {
	this->pointList = points;
}

bool ConvexHull::contains(std::vector<struct point>* hull, struct point p) {
	for (int i = 0; i < hull->size(); i++) {
		struct point temp = (*hull)[i];
		if (temp.x == p.x && temp.y == p.y)
			return true;
	}

	return false;

}

std::vector<struct point> *ConvexHull::getHull() {
	hull = new std::vector<struct point>;

	/* The topmost, rightmost, bottommost, and leftmost points in the list, in that order */
	struct point extremePoints[] = { pointList[0], pointList[0], pointList[0], pointList[0] };

	for (int i = 0; i < pointList.size(); i++) {
		if (pointList[i].y < extremePoints[0].y)
			extremePoints[0] = pointList[i];
		if (pointList[i].x > extremePoints[1].x)
			extremePoints[1] = pointList[i];
		if (pointList[i].y > extremePoints[2].y)
			extremePoints[2] = pointList[i];
		if (pointList[i].x < extremePoints[3].x)
			extremePoints[3] = pointList[i];
	}

	for (int i = 0; i < 4; i++)
		hull->push_back(extremePoints[i]);

	// printPoints(stdout, hull, "Extreme points");

	for (int i = 1; i < hull->size() + 1; i++) {
		if (i != hull->size()) {
			int farthestPoint = getPointFarthestFromEdge((*hull)[i - 1], (*hull)[i], &pointList);
			if (farthestPoint != -1 && !contains(hull, pointList[farthestPoint])) {
				hull->insert(hull->begin() + i, pointList[farthestPoint]);
				if (i > 1)
					i = i - 2;
				else
					i--;
			}
		}
		else {
			int farthestPoint = getPointFarthestFromEdge((*hull)[hull->size() - 1], (*hull)[0], &pointList);
			if (farthestPoint != -1 && !contains(hull, pointList[farthestPoint])) {
				hull->insert(hull->begin() + i, pointList[farthestPoint]);
				if (i > 1)
					i = i - 2;
				else
					i--;
			}
		}
	}

	return hull;
}


/* Returns true if the point p is inside this convex hull */
bool ConvexHull::containsPoint(struct point p) {
	getHull();
	
	for (int i = 0; i < hull->size(); i++) {
		/* A vector representing the edge between points i and i+1 */
		struct vector v = makeVectorFromPoints(hull->at(i), hull->at((i + 1) % hull->size()));
		/* The vector perpendicular to the edge, pointing into the hull */
		struct vector vPerp = { v.y, -v.x };
		/* The vector between the i'th point on the hull to p */
		struct vector hullToP = makeVectorFromPoints(hull->at(i), p);

		/* vPerp points into the convex hull, so if the dotproduct is < 0 the
		 * point is on the outside side of that edge, and is outside the hull */
		if (dotProduct(vPerp, hullToP) < 0)
			return false;
	}
	/* If the dot product is >= 0 for every edge, p is inside the convex hull */
	return true;
}