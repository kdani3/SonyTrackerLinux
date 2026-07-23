#ifndef QUAT_H
#define QUAT_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { double x, y, z; } vec3;
typedef struct { double w, x, y, z; } quat;

typedef struct {
    /* source[i] = which input axis feeds output axis i; sign[i] flips it.
       Default: YXZ order with X and Z inverted (matches WH-1000XM5) */
    int source[3];
    double sign[3];
} axis_map;

static const axis_map AXIS_MAP_DEFAULT = { {1, 0, 2}, {-1.0, 1.0, -1.0} };

static inline double vec3_get(vec3 v, int i) {
    return i == 0 ? v.x : (i == 1 ? v.y : v.z);
}

static inline vec3 remap_vec3(vec3 v, axis_map m) {
    vec3 out;
    out.x = vec3_get(v, m.source[0]) * m.sign[0];
    out.y = vec3_get(v, m.source[1]) * m.sign[1];
    out.z = vec3_get(v, m.source[2]) * m.sign[2];
    return out;
}

static inline quat quat_normalize(quat q) {
    double n = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (n < 1e-12) { quat id = {1, 0, 0, 0}; return id; }
    quat r = { q.w/n, q.x/n, q.y/n, q.z/n };
    return r;
}

static inline quat quat_conjugate(quat q) {
    quat r = { q.w, -q.x, -q.y, -q.z };
    return r;
}

static inline quat quat_multiply(quat a, quat b) {
    quat r;
    r.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    r.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    r.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    r.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
    return quat_normalize(r);
}

// Axis-angle rotation vector (radians) -> quaternion
static inline quat rotvec_to_quat(vec3 v) {
    double angle = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (angle < 1e-12) { quat id = {1, 0, 0, 0}; return id; }
    double s = sin(angle / 2.0) / angle;
    quat r = { cos(angle / 2.0), v.x*s, v.y*s, v.z*s };
    return quat_normalize(r);
}

// Head axes: X=right, Y=forward, Z=up. Yaw is rotation around Z 
typedef struct { double yaw, pitch, roll; } euler_deg;

static inline euler_deg quat_to_euler_deg(quat q) {
    q = quat_normalize(q);

    double sinr = 2.0 * (q.w*q.x + q.y*q.z);
    double cosr = 1.0 - 2.0 * (q.x*q.x + q.y*q.y);

    double sinp = 2.0 * (q.w*q.y - q.z*q.x);
    if (sinp > 1.0) sinp = 1.0;
    if (sinp < -1.0) sinp = -1.0;

    double siny = 2.0 * (q.w*q.z + q.x*q.y);
    double cosy = 1.0 - 2.0 * (q.y*q.y + q.z*q.z);

    const double rad2deg = 180.0 / M_PI;

    euler_deg e;
    e.yaw   = atan2(siny, cosy) * rad2deg;
    e.pitch = asin(sinp) * rad2deg;
    e.roll  = atan2(sinr, cosr) * rad2deg;
    return e;
}

/* Recentering: relative = conjugate(reference) * current.
   Call quat_set_reference() once (f.e. on startup, or on a recenter hotkey),
   then quat_relative_to_reference() for every subsequent sample */
static inline quat quat_relative_to_reference(quat current, quat reference) {
    return quat_multiply(quat_conjugate(reference), current);
}

#endif