#include <dos.h>
#include <i86.h>
#include <conio.h>
#include <math.h>

#define INF 32767
#define SW 320
#define SH 200
#define VPS 1.0
#define PPZ 1.0
#define BACKGROUND 15

void set_mode(unsigned char mode) {
     union REGS regs;
     regs.h.ah = 0;
     regs.h.al = mode;
     int86(0x10, &regs, &regs);
}

void set_pixel(int x, int y, char color) {
     char far *screen = MK_FP(0xA000, 0);
     int ax, ay;

     ax = SW / 2 + x;
     ay = SH / 2 - y;

     if (ax < 0 || ax >= SW || ay < 0 || ay >= SH) {
        return;
     }

     screen[ay * SW + ax] = color;
}

struct vector2 {
       float x;
       float y;
};

struct vector3 {
       float x;
       float y;
       float z;
};

struct sphere {
       float radius;
       char color;
       struct vector3 center;
};

struct vector3 vec3_sub(struct vector3 *a, struct vector3 *b) {
       struct vector3 result;

       result.x = b->x - a->x;
       result.y = b->y - a->y;
       result.z = b->z - a->z;

       return result;
}

float math_dot(struct vector3 *a, struct vector3 *b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

struct vector2 intersect_ray_sphere(struct vector3 *o,
                                    struct vector3 *d,
                                    struct sphere *s) {
     struct vector2 inf, t;
     float r = s->radius;
     struct vector3 co = vec3_sub(&s->center, o);
     float a = math_dot(d, d);
     float b = 2 * math_dot(&co, d);
     float c = math_dot(&co, &co) - (r * r);
     float discr = (b * b) - (4 * a * c);

     inf.x = INF;
     inf.y = INF;

     if (discr < 0) {
        return inf;
     }

     t.x = (-b + sqrt(discr)) / (2 * a);
     t.y = (-b - sqrt(discr)) / (2 * a);

     return t;
}

char trace_ray(struct vector3 *o,
               struct vector3 *d,
               int t_min,
               int t_max,
               int sphere_count,
               struct sphere *spheres) {
     float closest_t = INF;
     struct sphere *closest_sphere = 0;
     int i = 0;
     struct vector2 t;

     for (i = 0; i < sphere_count; i++) {
         t = intersect_ray_sphere(o, d, &spheres[i]);

         if ((t.x > t_min && t.x < t_max) && t.x < closest_t) {
            closest_t = t.x;
            closest_sphere = &spheres[i];
         }

         if ((t.y > t_min && t.y < t_max) && t.y < closest_t) {
            closest_t = t.y;
            closest_sphere = &spheres[i];
         }
     }

     if (closest_sphere == 0) {
        return BACKGROUND;
     }

     return closest_sphere->color;
}

struct vector3 canvas_to_viewport(int x, int y) {
       struct vector3 pos;

       pos.x = x * VPS / SW;
       pos.y = y * VPS / SH;
       pos.z = PPZ;

       return pos;
}

int main(void) {
    struct vector3 o; // camera position
    struct sphere spheres[4];
    int x, y;

    spheres[0].radius = 1;
    spheres[0].center.x = 0;
    spheres[0].center.y = -1;
    spheres[0].center.z = 3;
    spheres[0].color = 4;

    spheres[1].radius = 1;
    spheres[1].center.x = -2;
    spheres[1].center.y = 0;
    spheres[1].center.z = 4;
    spheres[1].color = 2;

    spheres[2].radius = 1;
    spheres[2].center.x = 2;
    spheres[2].center.y = 0;
    spheres[2].center.z = 4;
    spheres[2].color = 1;

    spheres[3].radius = 5000;
    spheres[3].center.x = 0;
    spheres[3].center.y = -5001;
    spheres[3].center.z = 0;
    spheres[3].color = 14;

    set_mode(0x13);

    o.x = 0;
    o.y = 0;
    o.z = 0;

    for (y = SH/2; y >= -SH/2; y--) {
        for (x = -SW/2; x <= SW/2; x++) {
            struct vector3 d = canvas_to_viewport(x, y);
            char color = trace_ray(&o, &d, 1, INF, 4, spheres);
            set_pixel(x, y, color);
        }
    }

    getch();

    set_mode(0x03);

    return 0;
}
