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

bool ConvexHull::isPointInside(struct point p1, struct point p2, struct point testPoint) {
	struct vector v = makeVectorFromPoints(p1, p2);
	struct vector vPerp = { v.y * -1, v.x};

	if (v.y == 0)
		vPerp.x = 0;

	if (v.x == 0)
		vPerp.y = 0;

	struct vector vectorPointToTestPoint = makeVectorFromPoints(p1, testPoint);
	struct vector normalizedVectorPointToTestPoint = normalize(vectorPointToTestPoint);
	struct vector normalizedPerpendicular = normalize(vPerp);

	if (dotProduct(normalizedPerpendicular, vectorPointToTestPoint) >= 0)
		return true;
	return false;
}

/* Returns true if the point p is inside this convex hull */
bool ConvexHull::containsPoint(struct point p) {
	for (int i = 1; i < hull->size(); i++) {
		if ((*hull)[i - 1].x != (*hull)[i].x || (*hull)[i - 1].y != (*hull)[i].y) {
			if (!isPointInside((*hull)[i - 1], (*hull)[i], p)) {
				return false;
			}
		}
	}

	if ((*hull)[hull->size() - 1].x != (*hull)[0].x || (*hull)[hull->size() - 1].y != (*hull)[0].y) {
		if (!isPointInside((*hull)[hull->size() - 1], (*hull)[0], p)) {
			return false;
		}
	}

	return true;
}

static ConvexHull *minkowskiAux(ConvexHull *hull1, ConvexHull *hull2, bool sum, Converter *conv) {
	std::vector<struct point> *hull1Points = hull1->getHull();
	std::vector<struct point> *hull2Points = hull2->getHull();
	std::vector<struct point> *sumPoints = new std::vector<struct point>();

	hull1Points = conv->convertPointsToGrid(hull1Points);
	hull2Points = conv->convertPointsToGrid(hull2Points);

	for (int i = 0; i < hull1Points->size(); i++) {
		struct point point1 = hull1Points->at(i);
		for (int j = 0; j < hull2Points->size(); j++) {
			struct point point2 = hull2Points->at(j);
			struct point newPoint;
			if(sum)
				newPoint = { point1.x + point2.x, point1.y + point2.y };
			else
				newPoint = { point1.x - point2.x, point1.y - point2.y };
			sumPoints->push_back(newPoint);
		}
	}

	std::vector<struct point> *newSumPoints = conv->convertPointsToScreen(sumPoints);

	ConvexHull *newHull = new ConvexHull(*newSumPoints);
	newHull->getHull();
	delete hull1Points;
	delete hull2Points;
	delete sumPoints;
	delete newSumPoints;

	return newHull;
}

ConvexHull *ConvexHull::minkowskiSum(ConvexHull *hull1, ConvexHull *hull2, Converter *conv) {
	return minkowskiAux(hull1, hull2, true, conv);
}

ConvexHull *ConvexHull::minkowskiDifference(ConvexHull *hull1, ConvexHull *hull2, Converter *conv) {
	return minkowskiAux(hull1, hull2, false, conv);
}