#pragma once

#include <vector>
#include "DataTypes.h"

class ConvexHull
{
private:
	std::vector<struct point> pointList;
	std::vector<struct point> *hull;
public:
	ConvexHull(std::vector<struct point> points);

	std::vector<struct point> *getHull();
	bool containsPoint(struct point p);
	bool contains(std::vector<struct point>* hull, struct point p);
	bool isPointInside(struct point p1, struct point p2, struct point testPoint);

	static ConvexHull *minkowskiSum(ConvexHull *hull1, ConvexHull *hull2);
	static ConvexHull *minkowskiDifference(ConvexHull *hull1, ConvexHull *hull2);
};