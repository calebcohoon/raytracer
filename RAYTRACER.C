#include <dos.h>
#include <i86.h>
#include <conio.h>
#include <math.h>

#define INF                 32767   // Infinity for 16 bit compilers
#define SW                  320      // Screen width
#define SH                  200      // Screen height
#define VPS                 1.0     // Viewport size
#define PPZ                 1.0     // Viewport depth
#define AMBIENT_LIGHT       0
#define POINT_LIGHT         1
#define DIRECTIONAL_LIGHT   2

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
    int specular;
    char color;
    struct vector3 center;
};

struct closest_sphere_ret {
    struct sphere* sphere;
    float closest_t;
};

struct light {
    int type;
    float intensity;
    struct vector3 position;
    struct vector3 direction;
};

// Build an optimized palette for the four main colors being used
void init_palette() {
    int color, shade;
    unsigned char index;
    float intensity;
    unsigned char base_colors[4][3] = {
        {0, 0, 63},   // Blue
        {0, 63, 0},   // Green
        {63, 0, 0},   // Red
        {63, 63, 32}  // Yellow
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
            }
            else {
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
    char far* screen = MK_FP(0xA000, 0);
    int ax, ay;

    ax = SW / 2 + x;
    ay = SH / 2 - y;

    if (ax < 0 || ax >= SW || ay < 0 || ay >= SH) {
        return;
    }

    screen[ay * SW + ax] = color;
}

unsigned char shade_color(unsigned char color, float intensity) {
    unsigned char shade;
    if (intensity > 1.0f) intensity = 1.0f;
    if (intensity < 0.0f) intensity = 0.0f;

    shade = (unsigned char)(intensity * 63.0f);
    return color * 64 + shade;
}

struct vector3 vec3_add(struct vector3* a, struct vector3* b) {
    struct vector3 result;

    result.x = a->x + b->x;
    result.y = a->y + b->y;
    result.z = a->z + b->z;

    return result;
}

struct vector3 vec3_sub(struct vector3* a, struct vector3* b) {
    struct vector3 result;

    result.x = b->x - a->x;
    result.y = b->y - a->y;
    result.z = b->z - a->z;

    return result;
}

struct vector3 vec3_mul(struct vector3* v, float s) {
    struct vector3 result;

    result.x = v->x * s;
    result.y = v->y * s;
    result.z = v->z * s;

    return result;
}

struct vector3 vec3_div(struct vector3* v, float d) {
    struct vector3 result;

    result.x = v->x / d;
    result.y = v->y / d;
    result.z = v->z / d;

    return result;
}

float vec3_len(struct vector3* v) {
    return sqrt((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

float vec3_dot(struct vector3* a, struct vector3* b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

struct vector2 intersect_ray_sphere(struct vector3* o,
    struct vector3* d,
    struct sphere* s) {
    struct vector2 inf, t;
    float r = s->radius;
    struct vector3 co = vec3_sub(&s->center, o);
    float a = vec3_dot(d, d);
    float b = 2 * vec3_dot(&co, d);
    float c = vec3_dot(&co, &co) - (r * r);
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

struct closest_sphere_ret closest_intersection(struct vector3* o,
    struct vector3* d,
    float t_min,
    float t_max,
    int sphere_count,
    struct sphere* spheres) {
    float closest_t = INF;
    struct sphere* closest_sphere = 0;
    struct closest_sphere_ret result;
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

    result.sphere = closest_sphere;
    result.closest_t = closest_t;

    return result;
}

struct vector3 reflect_ray(struct vector3* r, struct vector3* n) {
    struct vector3 R;

    R = vec3_mul(n, 2);
    R = vec3_mul(&R, vec3_dot(n, r));
    R = vec3_sub(r, &R);

    return R;
}

float compute_lighting(struct vector3* p,
    struct vector3* n,
    struct vector3* v,
    int s,
    int light_count,
    struct light* lights,
    int sphere_count,
    struct sphere* spheres) {
    float i = 0.0f;
    int li;
    struct vector3 L, R;
    float n_dot_l, r_dot_v, t_max;
    struct closest_sphere_ret result;

    for (li = 0; li < light_count; li++) {
        if (lights[li].type == AMBIENT_LIGHT) {
            i += lights[li].intensity;
        }
        else
        {
            if (lights[li].type == POINT_LIGHT) {
                L = vec3_sub(p, &lights[li].position);
                t_max = 1.0f;
            }
            else
            {
                L = lights[li].direction;
                t_max = INF;
            }

            result = closest_intersection(p, &L, 0.001f,
                t_max, sphere_count, spheres);

            // Cast shadow
            if (result.sphere != 0) {
                continue;
            }

            // Diffuse
            n_dot_l = vec3_dot(n, &L);
            if (n_dot_l > 0) {
                i += lights[li].intensity * n_dot_l / (vec3_len(n)
                    * vec3_len(&L));
            }

            // Specular
            if (s != -1) {
                R = reflect_ray(&L, n);
                r_dot_v = vec3_dot(&R, v);
                if (r_dot_v > 0) {
                    i += lights[li].intensity
                        * pow(r_dot_v / (vec3_len(&R) * vec3_len(v)), s);
                }
            }
        }
    }

    return i;
}

unsigned char trace_ray(struct vector3* o,
    struct vector3* d,
    float t_min,
    float t_max,
    int sphere_count,
    struct sphere* spheres,
    int light_count,
    struct light* lights) {
    struct closest_sphere_ret result;
    struct vector3 p, n, inter, d_neg;
    float light_inten = 0;

    result = closest_intersection(o, d, t_min, t_max,
        sphere_count, spheres);

    if (result.sphere == 0) {
        // Full white
        return shade_color(3, 1);
    }

    inter = vec3_mul(d, result.closest_t);
    p = vec3_add(o, &inter);
    n = vec3_sub(&result.sphere->center, &p);
    n = vec3_div(&n, vec3_len(&n));
    d_neg = vec3_mul(d, -1);

    light_inten = compute_lighting(&p, &n, &d_neg, result.sphere->specular,
        light_count, lights, sphere_count, spheres);

    return shade_color(result.sphere->color, light_inten);
}

struct vector3 canvas_to_viewport(int x, int y) {
    struct vector3 pos;

    pos.x = x * VPS / SW;
    pos.y = y * VPS / SH;
    pos.z = PPZ;

    return pos;
}

int main(void) {
    struct vector3 o;
    struct sphere spheres[4];
    struct light lights[3];
    int x, y;

    // Set camera position
    o.x = 0;
    o.y = 0;
    o.z = 0;

    // Configure colored spheres
    spheres[0].radius = 1;
    spheres[0].center.x = 0;
    spheres[0].center.y = -1;
    spheres[0].center.z = 3;
    spheres[0].specular = 500;  // Shiny
    spheres[0].color = 2;       // Red

    spheres[1].radius = 1;
    spheres[1].center.x = -2;
    spheres[1].center.y = 0;
    spheres[1].center.z = 4;
    spheres[1].specular = 10;   // Somewhat shiny
    spheres[1].color = 1;       // Green

    spheres[2].radius = 1;
    spheres[2].center.x = 2;
    spheres[2].center.y = 0;
    spheres[2].center.z = 4;
    spheres[2].specular = 500;  // Shiny
    spheres[2].color = 0;       // Blue

    spheres[3].radius = 5000;
    spheres[3].center.x = 0;
    spheres[3].center.y = -5001;
    spheres[3].center.z = 0;
    spheres[3].specular = 1000; // Very shiny
    spheres[3].color = 3;       // Yellow

    // Configure lights
    lights[0].type = AMBIENT_LIGHT;
    lights[0].intensity = 0.2f;

    lights[1].type = POINT_LIGHT;
    lights[1].intensity = 0.6f;
    lights[1].position.x = 2;
    lights[1].position.y = 1;
    lights[1].position.z = 0;

    lights[2].type = DIRECTIONAL_LIGHT;
    lights[2].intensity = 0.2f;
    lights[2].direction.x = 1;
    lights[2].direction.y = 4;
    lights[2].direction.z = 4;

    set_mode(0x13);

    // Important that this runs AFTER setting the VGA subsystem
    // into graphics mode 13H
    init_palette();

    for (y = SH / 2; y >= -SH / 2; y--) {
        for (x = -SW / 2; x <= SW / 2; x++) {
            struct vector3 d;
            unsigned char color;

            d = canvas_to_viewport(x, y);
            color = trace_ray(&o, &d, 1,
                INF, 4, spheres, 3, lights);
            set_pixel(x, y, color);
        }
    }

    getch();

    set_mode(0x03);

    return 0;
}
