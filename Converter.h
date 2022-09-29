#pragma once

#include <vector>
#include "DataTypes.h"

class Converter
{
private:
	int screenWidth;
	int screenHeight;
	struct point origin;
	double scale;


public:
	Converter(int width, int height);
	struct point convertPointToScreen(struct point p);
	std::vector<struct point> *convertPointsToScreen(std::vector<struct point> *points);
	struct point convertPointToGrid(struct point p);
	std::vector<struct point> *convertPointsToGrid(std::vector<struct point> *points);
	void setOrigin(double x, double y);
	void moveOrigin(double dx, double dy);
	void setScale(double newScale);
	void reset();
};

