#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "configuration.h"
#include "curve.h"

static void calibrate_center_point(void* data, size_t data_len, uint16_t center_index, uint8_t typesize);


void generate_axis_curve(const configuration_t *config, void *curve, uint8_t bitcount) {
  size_t bits = 0;
  for (int i = bitcount; i>0; i--) {
    bits <<=1;
    bits |=1;
  }
  // uint8_t=1, uint16_t=2
  uint8_t typesize = (uint8_t)ceil((double)((float)bitcount/8.0));

  size_t curve_len = bits + 1;
  size_t buf_size = curve_len * (typesize == 1 ? sizeof(uint8_t) : sizeof(uint16_t));

  // Allocate temp buffer for curvature data
  uint8_t *temp_curve = (uint8_t*)malloc(buf_size);
  if (!temp_curve) return;

  // in C# this is all in GetResponseCurve
  if (config->profile == JCURVE)
    generate_axis_j_curve(config, temp_curve, bits, typesize);
  else
    generate_axis_s_curve(config, temp_curve, bits, typesize);



  // Apply center point calibration if needed
//  if (center_index > 0 && center_index < (uint16_t)curve_len) {
//    calibrate_center_point(temp_curve, curve_len, 0, typesize);
//    if (calibrated != temp_curve) {
//      memcpy(curve, calibrated, buf_size);
//      free(calibrated);
//    } else {
//      memcpy(curve, temp_curve, buf_size);
//    }
//  } else {
//    memcpy(curve, temp_curve, buf_size);
//  }
  memcpy(curve, temp_curve, buf_size);
  free(temp_curve);
}

void generate_axis_j_curve(const configuration_t *config, void *curvature, size_t curvesize, uint8_t typesize) {
  int x = (int)curvesize;
  int y = (int)curvesize;
  x++;
  y++;

  int num3 = (int)((double)x * (config->deadband / 1000.0));
  int num4 = (int)((double)x * (config->xsat / 1000.0));
  int num5 = 0;
  int num6 = (int)((double)y * (config->ysat / 1000.0));

  int num7 = (num3 < num4) ? num3 : num4;
  int num8 = (num3 < num4) ? num4 : num3;
  int num9 = (num3 < num4) ? num5 : num6;
  int num10 = (num3 < num4) ? num6 : num5;

  int i = 0;
  while (i < num7) {
    if(typesize == 1)
      ((uint8_t*)curvature)[i++]=(uint8_t)num9;
    else
      ((uint16_t*)curvature)[i++]=htobe16(num9);
  }
  int j = x;
  while (j > num8) {
    if(typesize == 1)
      ((uint8_t*)curvature)[--j]=(uint8_t)num10;
    else
      ((uint16_t*)curvature)[--j]=htobe16(num10);
  }

  int curve_len = num8 - num7;
  if (curve_len > 0) {
    int subcurve_len = curve_len * (typesize == 1 ? (int)sizeof(uint8_t) : (int)sizeof(uint16_t));
    uint8_t *subcurve = (uint8_t*)malloc(subcurve_len);
    if (subcurve) {
      generate_axis_curve_data(point_t(0.0, (double)num9), point_t((double)curve_len, (double)num10), config, subcurve, typesize);

      int idx = 0;
      int k = num7;
      while (k < num8) {
        if(typesize == 1)
          ((uint8_t*)curvature)[k]=((uint8_t*)subcurve)[idx];
        else
          ((uint16_t*)curvature)[k]=((uint16_t*)subcurve)[idx];
        k++;
        idx++;
      }
      free(subcurve);
    }
  }
}

void generate_axis_s_curve(const configuration_t *config, void *curvature, size_t curvesize, uint8_t typesize) {
  int x = (int)curvesize;
  int y = (int)curvesize;
  x++;
  int s = (y / 2);  // C# uses: (yMin < yMax) ? ((num2 + yMin) / 2) : ((num2 + yMax) / 2)
  y++;

  int num4 = (int)((double)x * (config->xsat / 1000.0));
  int num5 = x - num4;
  int num6 = (int)((double)y * (config->ysat / 1000.0));
  int num7 = y - num6;
  int num8 = (x - (int)((double)x * (config->deadband / 1000.0))) / 2;
  int num9 = x - num8;

  int num10 = (num4 < num5) ? num4 : num5;
  int num11 = (num4 < num5) ? num5 : num4;
  int num12 = (num4 < num5) ? num6 : num7;
  int num13 = (num4 < num5) ? num7 : num6;

  int i = num8;
  while (i < num9) {
    if(typesize == 1)
      ((uint8_t*)curvature)[i++]=s;
    else
      ((uint16_t*)curvature)[i++]=htobe16(s);
  }

  int j = 0;
  while (j < num10) {
    if(typesize == 1)
      ((uint8_t*)curvature)[j++]=(uint8_t)num12;
    else
      ((uint16_t*)curvature)[j++]=htobe16(num12);
  }

  int k = x;
  while (k > num11) {
    if(typesize == 1)
      ((uint8_t*)curvature)[--k]=(uint8_t)num13;
    else
      ((uint16_t*)curvature)[--k]=htobe16(num13);
  }

  int curve_len = num11 - num10 - (num9 - num8);
  if (curve_len > 0) {
    int subcurve_len = curve_len * (typesize == 1 ? (int)sizeof(uint8_t) : (int)sizeof(uint16_t));
    uint8_t *subcurve = (uint8_t*)malloc(subcurve_len);
    if (subcurve) {
      generate_axis_curve_data(point_t(0.0, (double)num12), point_t((double)curve_len, (double)num13), config, subcurve, typesize);

      int idx = 0;
      int l = num10;
      while (l < num8) {
        if(typesize == 1)
          ((uint8_t*)curvature)[l]=((uint8_t*)subcurve)[idx];
        else
          ((uint16_t*)curvature)[l]=((uint16_t*)subcurve)[idx];
        l++;
        idx++;
      }
      int m = num9;
      while (m < num11) {
        if(typesize == 1)
          ((uint8_t*)curvature)[m]=((uint8_t*)subcurve)[idx];
        else
          ((uint16_t*)curvature)[m]=((uint16_t*)subcurve)[idx];
        m++;
        idx++;
      }
      free(subcurve);
    }
  }
}

void generate_axis_curve_data(point_t start, point_t end, const configuration_t *config, void* curvature, uint8_t typesize) {
  uint8_t curvetype = config->profile;

  int num = (int)((start.X < end.X) ? (end.X - start.X) : (start.X - end.X));
  double num3 = end.X - start.X;
  double num4 = end.Y - start.Y;
  double num5 = num3 / 2.0;
  double num6 = num4 / 2.0;
  double curvature_val = ((double)config->curvature - 500.0) / 500.0;
  double num7 = num5 * curvature_val;
  double num8 = num6 * curvature_val;
  point_t p(start.X + num5 - num7, start.Y + num6 + num8);
  point_t point(start.X + num5 + num7, num6 + start.Y - num8);

  if (curvetype == JCURVE) {
    p = point;
  }
  for (int i = 0; i < num; i++) {
    if(typesize == 1)
      ((uint8_t*)curvature)[i] = bezierPoint<uint8_t>(start, p, point, end, (int)(start.X) + i);
    else
      ((uint16_t*)curvature)[i] = htobe16(bezierPoint<uint16_t>(start, p, point, end, (int)(start.X) + i));
  }
  if(typesize == 1)
    ((uint8_t*)curvature)[num - 1] = (uint8_t)(end.Y - 1.0);
  else
    ((uint16_t*)curvature)[num - 1] = htobe16((uint16_t)(end.Y - 1.0));
}

static void calibrate_center_point(void* data, size_t data_len, uint16_t center_index, uint8_t typesize) {
  if (center_index == 0 || center_index >= data_len) {
    return; // return if past array length or 0
  }

  void* result = malloc(data_len * (typesize == 1 ? sizeof(uint8_t) : sizeof(uint16_t)));
  if (!result) return;

  int center_point = data_len / 2 - 1;
  int max_index = data_len - 1;

  double num6 = (double)center_point / (double)center_index;
  double num7 = 0.0;

  for (int i = 0; i <= max_index; i++) {
    int idx = (int)floor(num7);
    if (idx >= (int)data_len) idx = data_len - 1;
    if (typesize == 1)
      ((uint8_t*)result)[i] = ((uint8_t*)data)[idx];
    else
      ((uint16_t*)result)[i] = ((uint16_t*)data)[idx];

    if (i == (int)center_index) {
      num6 = (double)center_point / (double)(max_index - center_index);
    }
    num7 += num6;
  }
  if (typesize == 1)
    ((uint8_t*)result)[max_index] = ((uint8_t*)data)[max_index];
  else
    ((uint16_t*)result)[max_index] = ((uint16_t*)data)[max_index];

  memcpy(data,result,data_len*(typesize == 1 ? sizeof(uint8_t) : sizeof(uint16_t)));
  return;
}
