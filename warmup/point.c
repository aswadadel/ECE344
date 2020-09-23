#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>
void
point_translate(struct point *p, double x, double y)
{
	double pointx = point_X(p) + x;
	double pointy = point_Y(p) + y;
	
	p = point_set(p, pointx, pointy);
	return;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	//return -1.0;
	double p1x, p2x, p1y, p2y, dist;
	p1x = point_X(p1);
	p2x = point_X(p2);
	p1y = point_Y(p1);
	p2y = point_Y(p2);
	dist = pow(p1x-p2x, 2) + pow(p1y-p2y, 2);
	dist = sqrt(dist);
	if(dist < 0) dist = 0-dist;
	return dist;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
	double length1, length2;
	length1 = sqrt(pow(point_X(p1), 2) + pow(point_Y(p1), 2));
	length2 = sqrt(pow(point_X(p2), 2) + pow(point_Y(p2), 2));
	if(length1 < length2){
		return -1;
	} else if (length1 > length2){
		return 1;
	}
	return 0;
}
