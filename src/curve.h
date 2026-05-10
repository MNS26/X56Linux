#ifndef CURVE_H
#include "configuration.h"

struct point_t {
  double X;
  double Y;
  point_t(double x = 0, double y = 0) : X(x), Y(y) {}
};

//void generate_axis_curve(const configuration_t *config, void *curve, uint8_t bitcount, uint16_t center_index);
//void generate_axis_curve(const configuration_t *config, void* curve, size_t curvesize, uint8_t typesize);
void generate_axis_curve(const configuration_t *config, void *curve, uint8_t bitcount);
void generate_axis_j_curve(const configuration_t *config, void *curvature, size_t curvesize, uint8_t typesize);
void generate_axis_s_curve(const configuration_t *config, void *curvature, size_t curvesize, uint8_t typesize);
void generate_axis_curve_data(point_t start, point_t end, const configuration_t *config, void* curvature, uint8_t typesize);

template <typename T>
T bezierPoint(point_t start, point_t p1, point_t p2, point_t end, int targetx) {
  double step = 0.5;
  double t = 0.0;
  T y = 0;
  int x = 0;
  while (step > 0.0 && step < 1.0) {
    double num2  = 3.0 * (p1.X - start.X);
    double num3  = 3.0 * (p2.X - p1.X) - num2;
    double num4  = end.X - start.X - num2 - num3;

    double num5  = 3.0 * (p1.Y - start.Y);
    double num6  = 3.0 * (p2.Y - p1.Y) - num5;
    double num7  = end.Y - start.Y - num5 - num6;

    double num8  = t * t * t;
    double num9  = t * t;
    x = (int)(num4 * num8 + num3 * num9 + num2 * t + start.X);
    y = (T)(num7 * num8 + num6 * num9 + num5 * t + start.Y);

    if (x == targetx) {
      break;
    }
    if (x < targetx) {
      t += step;
    } else {
      t -= step;
      step /= 2.0;
    }
  }
  return y;
}

#endif //CURVE_H