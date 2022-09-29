#include "Converter.h"

Converter::Converter(int width, int height) {
	this->screenWidth = width;
	this->screenHeight = height;
	this->origin = { screenWidth / 2.0, screenHeight / 2.0 };
	this->scale = 1.0;
}

struct point Converter::convertPointToScreen(struct point p) {
	return { origin.x + (p.x * scale), origin.y - (p.y * scale) };
}

/* Takes a list of points with grid coordinates and returns
 * a list of the screen coordinates for these points */
std::vector<struct point> *Converter::convertPointsToScreen(std::vector<struct point> *points) {
	std::vector<struct point> *newPoints = new std::vector<struct point>(points->size());

	for (int i = 0; i < points->size(); i++) {
		struct point p = points->at(i);
		p = convertPointToScreen(p);
		(*newPoints)[i] = p;
	}

	return newPoints;
}

struct point Converter::convertPointToGrid(struct point p) {
	return { (p.x - origin.x) / scale, (p.y - origin.y) / -scale };
}

std::vector<struct point> *Converter::convertPointsToGrid(std::vector<struct point> *points) {
	std::vector<struct point> *newPoints = new std::vector<struct point>(points->size());

	for (int i = 0; i < points->size(); i++) {
		struct point p = points->at(i);
		p = convertPointToGrid(p);
		(*newPoints)[i] = p;
	}

	return newPoints;
}

void Converter::setOrigin(double x, double y) {
	origin = { x, y };
}

void Converter::moveOrigin(double dx, double dy) {
	origin.x += dx;
	origin.y += dy;
}

void Converter::setScale(double newScale) {
	scale = newScale;
}

void Converter::reset() {
	origin = { screenWidth / 2.0, screenHeight / 2.0 };
	scale = 1.0;
}