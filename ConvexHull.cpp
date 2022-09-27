#include "ConvexHull.h"

ConvexHull::ConvexHull(std::vector<struct point> points) {
	this->pointList = points;
}

std::vector<struct point> *ConvexHull::getHull() {
	//delete hull;
	hull = new std::vector<struct point>;

	/* The topmost, rightmost, bottommost, and leftmost points in the list, in that order */
	struct point extremePoints[] = { pointList[0], pointList[0], pointList[0], pointList[0] };

	for (int i = 0; i < pointList.size(); i++) {
		if (pointList[i].y > extremePoints[0].y)
			extremePoints[0] = pointList[i];
		if (pointList[i].x > extremePoints[1].x)
			extremePoints[1] = pointList[i];
		if (pointList[i].y < extremePoints[2].y)
			extremePoints[2] = pointList[i];
		if (pointList[i].x < extremePoints[3].x)
			extremePoints[3] = pointList[i];
	}

	for (int i = 0; i < 4; i++)
		hull->push_back(extremePoints[i]);

	// printPoints(stdout, hull, "Extreme points");

	for (int i = 1; i <= hull->size(); i++) {
		int farthestPoint;
		if (i < hull->size())
			farthestPoint = getPointFarthestFromEdge(hull->at(i - 1), hull->at(i), &pointList);
		else
			farthestPoint = getPointFarthestFromEdge(hull->at(i - 1), hull->at(0), &pointList);
		bool inHull = false;
		for (int j = 0; j < hull->size(); j++) {
			if (pointList[farthestPoint].x == hull->at(j).x && pointList[farthestPoint].y == hull->at(j).y) {
				inHull = true;
				break;
			}
		}

		if (!inHull) {
			hull->insert(hull->begin() + i, pointList[farthestPoint]);
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