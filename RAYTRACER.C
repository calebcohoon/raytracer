#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <math.h>

#define INF 32767 // Infinity for 16 bit compilers
#define SW 320    // Screen width
#define SH 200    // Screen height
#define VPS 1.0   // Viewport size
#define PPZ 1.0   // Viewport depth
#define AMBIENT_LIGHT 0
#define POINT_LIGHT 1
#define DIRECTIONAL_LIGHT 2
#define SCREEN_CENTER_X (SW / 2)
#define SCREEN_CENTER_Y (SH / 2)
#define VPS_SW (VPS / SW)
#define VPS_SH (VPS / SH)
#define ASPECT_RATIO 1.2f

typedef struct {
  float x;
  float y;
} vector2;

typedef struct {
  float x;
  float y;
  float z;
} vector3;

typedef struct {
  float radius;
  int specular;
  char color;
  vector3 center;
} sphere;

typedef struct {
  int type;
  float intensity;
  vector3 position;
  vector3 direction;
} light;

// Build an optimized palette for the four main colors being used
void init_palette() {
  int color, shade;
  unsigned char index;
  float intensity;
  unsigned char base_colors[4][3] = {
      {0, 0, 63},  // Blue
      {0, 63, 0},  // Green
      {63, 0, 0},  // Red
      {63, 63, 32} // Yellow
  };

  for (color = 0; color < 4; color++) {
    for (shade = 0; shade < 64; shade++) {
      index = color * 64 + shade;
      intensity = shade / 63.0f;

      outp(0x3C8, index);

      // For the very last shade of yellow, just make it white
      // so we have a color for the background
      if (color == 3 && shade == 63) {
        outp(0x3C9, (unsigned char)63);
        outp(0x3C9, (unsigned char)63);
        outp(0x3C9, (unsigned char)63);
      } else {
        outp(0x3C9, (unsigned char)(base_colors[color][0] * intensity));
        outp(0x3C9, (unsigned char)(base_colors[color][1] * intensity));
        outp(0x3C9, (unsigned char)(base_colors[color][2] * intensity));
      }
    }
  }
}

void set_mode(unsigned char mode) {
  union REGS regs;

  regs.h.ah = 0;
  regs.h.al = mode;

  int86(0x10, &regs, &regs);
}

void set_pixel(int x, int y, char color) {
  char far *screen = MK_FP(0xA000, 0);
  int ax, ay;

  ax = SCREEN_CENTER_X + x;
  ay = SCREEN_CENTER_Y - y;

  if (ax < 0 || ax >= SW || ay < 0 || ay >= SH) {
    return;
  }

  screen[ay * SW + ax] = color;
}

unsigned char shade_color(unsigned char color, float intensity) {
  unsigned char shade;
  if (intensity > 1.0f)
    intensity = 1.0f;
  if (intensity < 0.0f)
    intensity = 0.0f;

  shade = (unsigned char)(intensity * 63.0f);
  return color * 64 + shade;
}

vector3 vec3_add(vector3 *a, vector3 *b) {
  vector3 result;

  result.x = a->x + b->x;
  result.y = a->y + b->y;
  result.z = a->z + b->z;

  return result;
}

vector3 vec3_sub(vector3 *a, vector3 *b) {
  vector3 result;

  result.x = a->x - b->x;
  result.y = a->y - b->y;
  result.z = a->z - b->z;

  return result;
}

vector3 vec3_mul(vector3 *v, float s) {
  vector3 result;

  result.x = v->x * s;
  result.y = v->y * s;
  result.z = v->z * s;

  return result;
}

vector3 vec3_div(vector3 *v, float d) {
  vector3 result;
  float recip = 1.0f / d;

  result.x = v->x * recip;
  result.y = v->y * recip;
  result.z = v->z * recip;

  return result;
}

float vec3_len(vector3 *v) {
  return sqrt((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

float vec3_dot(vector3 *a, vector3 *b) {
  return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

vector2 intersect_ray_sphere(vector3 *o, vector3 *d, sphere *s) {
  vector2 inf, t;
  float r = s->radius;
  vector3 co = vec3_sub(o, &s->center);
  float a = vec3_dot(d, d);
  float b = 2 * vec3_dot(&co, d);
  float c = vec3_dot(&co, &co) - (r * r);
  float discr = (b * b) - (4 * a * c);
  float sqrt_discr;

  inf.x = INF;
  inf.y = INF;

  if (discr < 0) {
    return inf;
  }

  sqrt_discr = sqrt(discr);
  t.x = (-b + sqrt_discr) / (2 * a);
  t.y = (-b - sqrt_discr) / (2 * a);

  return t;
}

sphere *closest_intersection(vector3 *o, vector3 *d, float t_min, float t_max,
                             float *closest_t, int sphere_count,
                             sphere *spheres) {
  sphere *closest_sphere = 0;
  int i = 0;
  vector2 t;

  *closest_t = INF;

  for (i = 0; i < sphere_count; i++) {
    t = intersect_ray_sphere(o, d, &spheres[i]);

    if ((t.x > t_min && t.x < t_max) && t.x < *closest_t) {
      *closest_t = t.x;
      closest_sphere = &spheres[i];
    }

    if ((t.y > t_min && t.y < t_max) && t.y < *closest_t) {
      *closest_t = t.y;
      closest_sphere = &spheres[i];
    }
  }

  return closest_sphere;
}

vector3 reflect_ray(vector3 *r, vector3 *n) {
  vector3 R;

  R = vec3_mul(n, 2);
  R = vec3_mul(&R, vec3_dot(n, r));
  R = vec3_sub(&R, r);

  return R;
}

float compute_lighting(vector3 *p, vector3 *n, vector3 *v, int s,
                       int light_count, light *lights, int sphere_count,
                       sphere *spheres) {
  int li;
  float i = 0.0f;
  float n_dot_l, r_dot_v, t_max, closest_t;
  float n_len = vec3_len(n);
  float v_len = vec3_len(v);
  vector3 L, R;
  sphere *hit_sphere;

  for (li = 0; li < light_count; li++) {
    if (lights[li].type == AMBIENT_LIGHT) {
      i += lights[li].intensity;
    } else {
      if (lights[li].type == POINT_LIGHT) {
        L = vec3_sub(&lights[li].position, p);
        t_max = 1.0f;
      } else {
        L = lights[li].direction;
        t_max = INF;
      }

      hit_sphere = closest_intersection(p, &L, 0.001f, t_max, &closest_t,
                                        sphere_count, spheres);

      // Cast shadow
      if (hit_sphere != 0) {
        continue;
      }

      // Diffuse
      n_dot_l = vec3_dot(n, &L);
      if (n_dot_l > 0) {
        i += lights[li].intensity * n_dot_l / (n_len * vec3_len(&L));
      }

      // Specular
      if (s != -1) {
        R = reflect_ray(&L, n);
        r_dot_v = vec3_dot(&R, v);
        if (r_dot_v > 0) {
          i += lights[li].intensity * pow(r_dot_v / (vec3_len(&R) * v_len), s);
        }
      }
    }
  }

  return i;
}

unsigned char trace_ray(vector3 *o, vector3 *d, float t_min, float t_max,
                        int sphere_count, sphere *spheres, int light_count,
                        light *lights) {
  sphere *hit_sphere;
  vector3 p, n, inter, d_neg;
  float light_inten = 0, closest_t;

  hit_sphere = closest_intersection(o, d, t_min, t_max, &closest_t,
                                    sphere_count, spheres);

  if (hit_sphere == 0) {
    // Full white
    return shade_color(3, 1);
  }

  inter = vec3_mul(d, closest_t);
  p = vec3_add(o, &inter);
  n = vec3_sub(&p, &hit_sphere->center);
  n = vec3_div(&n, vec3_len(&n));
  d_neg = vec3_mul(d, -1);

  light_inten = compute_lighting(&p, &n, &d_neg, hit_sphere->specular,
                                 light_count, lights, sphere_count, spheres);

  return shade_color(hit_sphere->color, light_inten);
}

vector3 canvas_to_viewport(int x, int y) {
  vector3 pos;

  pos.x = x * VPS_SW * ASPECT_RATIO;
  pos.y = y * VPS_SH;
  pos.z = PPZ;

  return pos;
}

int main(void) {
  vector3 o = {0, 0, 0};
  sphere spheres[] = {
      {1.0f, 500, 2, {0, -1, 3}},       /* Red sphere */
      {1.0f, 10, 1, {-2, 0, 4}},        /* Green sphere */
      {1.0f, 500, 0, {2, 0, 4}},        /* Blue sphere */
      {5000.0f, 1000, 3, {0, -5001, 0}} /* Yellow ground */
  };
  light lights[] = {{AMBIENT_LIGHT, 0.2f, {0, 0, 0}, {0, 0, 0}},
                    {POINT_LIGHT, 0.6f, {2, 1, 0}, {0, 0, 0}},
                    {DIRECTIONAL_LIGHT, 0.2f, {0, 0, 0}, {1, 4, 4}}};
  int x, y;

  set_mode(0x13);

  // Important that this runs AFTER setting the VGA subsystem
  // into graphics mode 13H
  init_palette();

  for (y = SH / 2; y >= -SH / 2; y--) {
    for (x = -SW / 2; x <= SW / 2; x++) {
      vector3 d = canvas_to_viewport(x, y);
      unsigned char color = trace_ray(&o, &d, 1, INF, 4, spheres, 3, lights);
      set_pixel(x, y, color);
    }
  }

  getch();

  set_mode(0x03);

  return 0;
}
