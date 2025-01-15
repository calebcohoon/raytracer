#include <dos.h>
#include <i86.h>
#include <conio.h>
#include <math.h>

void set_mode(unsigned char mode) {
     union REGS regs;
     regs.h.ah = 0;
     regs.h.al = mode;
     int86(0x10, &regs, &regs);
}

void set_pixel(int x, int y, char color) {
     char far *screen = MK_FP(0xA000, 0);
     screen[y * 320 + x] = color;
}

struct vector2 {
       int x;
       int y;
};

struct vector3 {
       int x;
       int y;
       int z;
};

struct sphere {
       int radius;
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

int math_dot(struct vector3 *a, struct vector3 *b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

struct vector2 intersect_ray_sphere(struct vector3 *o,
                                    struct vector3 *d,
                                    struct sphere *s) {
     struct vector2 inf, t;
     int r = s->radius;
     struct vector3 co = vec3_sub(o, &s->center);
     int a = math_dot(d, d);
     int b = 2 * math_dot(&co, d);
     int c = math_dot(&co, &co) - (r * r);
     int discr = (b * b) - (4 * a * c);

     inf.x = 32567;
     inf.y = 32567;

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
     int closest_t = 32567;
     struct sphere *closest_sphere = 0;
     int i = 0;
     struct vector2 t;

     for (i = 0; i < sphere_count; i++) {
         t = intersect_ray_sphere(o, d, &spheres[i]);

         if ((t.x >= t_min && t.x <= t_max) && t.x < closest_t) {
            closest_t = t.x;
            closest_sphere = &spheres[i];
         }

         if ((t.y >= t_min && t.y <= t_max) && t.y < closest_t) {
            closest_t = t.y;
            closest_sphere = &spheres[i];
         }
     }

     if (closest_sphere == 0) {
        return 15;
     }

     return closest_sphere->color;
}

struct vector3 canvas_to_viewport(int x, int y) {
       struct vector3 pos;

       pos.x = 320/2 + x;
       pos.y = 200/2 - y;
       pos.z = 1;

       return pos;
}

int main(void) {
    struct vector3 o; // camera position
    struct sphere spheres[3];
    int x, y;

    spheres[0].radius = 1;
    spheres[0].center.x = 0;
    spheres[0].center.y = 0;
    spheres[0].center.z = 5;
    spheres[0].color = 4;

    set_mode(0x13);

    o.x = 0;
    o.y = 0;
    o.z = 0;

    for (y = 200/2; y >= -200/2; y--) {
        for (x = -320/2; x <= 320/2; x++) {
            struct vector3 d = canvas_to_viewport(x, y);
            char color = trace_ray(&o, &d, 1, 32567, 1, spheres);
            set_pixel(d.x, d.y, color);
        }
    }

    getch();

    set_mode(0x03);

    return 0;
}
