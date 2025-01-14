#include <dos.h>
#include <i86.h>
#include <conio.h>

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

struct color {
       int r;
       int g;
       int b;
};

struct sphere {
       int radius;
       struct vector3 center;
       struct color color;
};

struct vector3 vec3_sub(struct vector3 a, struct vector3 b) {
       struct vector3 result;

       result.x = b.x - a.x;
       result.y = b.y - a.y;
       result.z = b.z - a.z;

       return result;
}


int math_dot(struct vector3 a, struct vector3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

struct vector2 intersect_ray_sphere(struct vector3 o,
                          struct vector3 d,
                          struct sphere s) {
     struct vector2 inf;
     int r = s.radius;
     struct vector3 co = vec3_sub(o, s.center);
     int a = math_dot(d, d);
     int b = 2 * math_dot(co, d);
     int c = math_dot(co, co) - (r * r);
     int discr = (b * b) - (4 * a * c);

     inf.x = 32567;
     inf.y = 32567;

     if (discr < 0) {
        return inf;
     }

     return inf;
}

int main(void) {
    struct vector3 o; // camera position
    struct vector3 d;
    struct sphere spheres[3];

    spheres[0].radius = 1;
    spheres[0].center.x = 0;
    spheres[0].center.y = -1;
    spheres[0].center.z = 3;
    spheres[0].color.r = 255;
    spheres[0].color.g = 0;
    spheres[0].color.b = 0;

    intersect_ray_sphere(o, d, spheres[0]);

    set_mode(0x13);

    getch();

    set_mode(0x03);

    return 0;
}
