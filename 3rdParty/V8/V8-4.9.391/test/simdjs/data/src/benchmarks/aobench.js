// AOBench
// ambient occlusion renderer
// See full demo at https://github.com/wahbahdoo/aobench

(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "AOBench",
    kernelInit:       initAobench,
    kernelCleanup:    cleanupAobench,
    kernelSimd:       simdAobench,
    kernelNonSimd:    nonSimdAobench
  };

  // Hook up to the harness
  benchmarks.add (new Benchmark (kernelConfig));

  // Global variables
  var NAO_SAMPLES = 8;
  var spheres;
  var plane;
  var rands1;
  var rands2;
  var isect0;

  // Initialization and verification
  function initAobench () {
    init_scene();
    var A = ambient_occlusion(isect0);
    var B = ambient_occlusion_simd(isect0);
    return ((A.x == B.x) && (A.y == B.y) && (A.z == B.z));
  }

  function cleanupAobench() {
    return initAobench();
  }

  // Non SIMD version of the kernel
  function nonSimdAobench (n) {
    for (var i = 0; i < n; i++) {
      ambient_occlusion(isect0);
    }
  }

  // SIMD version of the kernel
  function simdAobench (n) {
    for (var i = 0; i < n; i++) {
      ambient_occlusion_simd(isect0);
    }
  }

  // AOBench initialization of objects and pseudorand numbers (for benchmark predictability)
  function init_scene() {
    spheres = new Array();
    spheres[0] = {
      center: {
        x: -2.0,
        y: 0.0,
        z: -3.5
      },
      radius: 0.5
    };
    spheres[1] = {
      center: {
        x: -0.5,
        y: 0.0,
        z: -3.0
      },
      radius: 0.5
    };
    spheres[2] = {
      center: {
        x: 1.0,
        y: 0.0,
        z: -2.2
      },
      radius: 0.5
    };
    plane = {
      p: {
        x: 0.0,
        y: -0.5,
        z: 0.0
      },
      n: {
        x: 0.0,
        y: 1.0,
        z: 0.0
      }
    };
    rands1 = new Array(0.1352356830611825,  0.288015044759959,   0.7678821850568056,  0.2686317905317992,
                       0.3331136927008629,  0.8684257145505399,  0.781927386065945,   0.5896540696267039,
                       0.44623699225485325, 0.9686877066269517,  0.07219804194755852, 0.32867410429753363,
                       0.25455036014318466, 0.6900878311134875,  0.32115139183588326, 0.8623794671148062,
                       0.41069260938093066, 0.999176808167249,   0.31144002149812877, 0.21190544497221708,
                       0.589751492254436,   0.618399447761476,   0.7838233797810972,  0.22662024036981165,
                       0.5274769144598395,  0.8913978524506092,  0.2461202829144895,  0.575232774252072,
                       0.20723191439174116, 0.15211533522233367, 0.5140219402965158,  0.695398824987933,
                       0.7201623972505331,  0.1737971710972488,  0.3138047114480287,  0.09142904286272824,
                       0.15824169223196805, 0.11588017432950437, 0.4076798539608717,  0.06385629274882376,
                       0.9907234299462289,  0.1742915315553546,  0.9236432255711406,  0.8344372694846243,
                       0.05793144227936864, 0.35464465571567416, 0.3937969475518912,  0.8209003841038793,
                       0.6443945677019656,  0.15443599177524447, 0.8957053178455681,  0.4145913925021887,
                       0.4667414356954396,  0.42764953384175897, 0.03486692951992154, 0.13391495239920914,
                       0.6122364429756999,  0.7934473238419741,  0.13505113637074828, 0.7279673060402274,
                       0.3638722419273108,  0.30750402715057135, 0.8705337035935372,  0.3060465627349913);

    rands2 = new Array(0.6100146626122296,  0.8141843967605382,  0.7538463387172669,  0.538857217412442,
                       0.7884696905966848,  0.2656198723707348,  0.3280213042162359,  0.25133296218700707,
                       0.18718935316428542, 0.7374026740435511,  0.8333564973436296,  0.22081619454547763,
                       0.08140448946505785, 0.7737920694053173,  0.9531879865098745,  0.385226191021502,
                       0.8437968089710921,  0.45293551217764616, 0.11351405014283955, 0.6402874339837581,
                       0.9657228307332844,  0.5241556512191892,  0.9501411342062056,  0.7991736396215856,
                       0.7572617880068719,  0.6777111298870295,  0.19950113398954272, 0.09956562682054937,
                       0.03746219468303025, 0.18719390942715108, 0.1519025124143809,  0.8241845818702132,
                       0.9609565436840057,  0.7231316142715514,  0.26712060417048633, 0.7414182834327221,
                       0.4706993775907904,  0.9619642498437315,  0.14598079677671194, 0.1517641346435994,
                       0.5583144023548812,  0.7664180144201964,  0.8109071112703532,  0.4008640209212899,
                       0.10891564912162721, 0.8558103002142161,  0.03816548571921885, 0.4263107746373862,
                       0.280488790711388,   0.915016517508775,   0.8379701666999608,  0.5821647725533694,
                       0.3671900019980967,  0.6120628621429205,  0.5861144624650478,  0.5639409353025258,
                       0.4884668991435319,  0.9718172331340611,  0.4438377188052982,  0.9853541473858058,
                       0.021908782655373216,0.6144221667200327,  0.11301262397319078, 0.17565111187286675);
    isect0 =  {
      t: 0.7907924036719444,
      hit: 1,
      p: {
        x: 0.3484251968503937,
        y: -0.49999999999999994,
        z: -0.5039370078740157
      },
      n: {
        x: 0,
        y: 1,
        z: 0
      }
    };
  }

  // Sequential AO calculation functions ----------------------------------------------

  function ambient_occlusion(isect) {
    var col = {};

    var ntheta = NAO_SAMPLES;
    var nphi = NAO_SAMPLES;
    var eps = 0.0001;

    var p = {
      x: isect.p.x + eps * isect.n.x,
      y: isect.p.y + eps * isect.n.y,
      z: isect.p.z + eps * isect.n.z
    };

    var basis = new Array({}, {}, {});
    orthoBasis(basis, isect.n);

    var occlusion = 0;

    for (var j = 0; j < ntheta; j++) {
      for (var i = 0; i < nphi; i++) {
        var theta = Math.sqrt(rands1[j * ntheta + i]);
        var phi = 2 * Math.PI * rands2[j * ntheta + i];

        var x = Math.cos(phi) * theta;
        var y = Math.sin(phi) * theta;
        var z = Math.sqrt(1 - theta * theta);

        var rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
        var ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
        var rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

        var ray = {
          org: p,
          dir: {
            x: rx,
            y: ry,
            z: rz
          }
        };

        var occIsectA = {
          t: 1e17,
          hit: 0
        }
        var occIsectB = {
          p: { x:0, y:0, z:0 },
          n: { x:0, y:0, z:0 }
        };

        ray_sphere_intersect(occIsectA, occIsectB, ray, spheres[0]);
        ray_sphere_intersect(occIsectA, occIsectB, ray, spheres[1]);
        ray_sphere_intersect(occIsectA, occIsectB, ray, spheres[2]);
        ray_plane_intersect(occIsectA, occIsectB, ray, plane);

        if (occIsectA.hit) occlusion += 1.0;

      }
    }

    occlusion = (ntheta * nphi - occlusion) / (ntheta * nphi);

    col.x = occlusion;
    col.y = occlusion;
    col.z = occlusion;

    return col;
  }

  function ray_sphere_intersect(isectA, isectB, ray, sphere) {
    var rs = {
      x: ray.org.x - sphere.center.x,
      y: ray.org.y - sphere.center.y,
      z: ray.org.z - sphere.center.z
    };

    var B = vdot(rs, ray.dir);
    var C = vdot(rs, rs) - sphere.radius * sphere.radius;
    var D = B * B - C;

    if (D > 0) {
      var t = -B - Math.sqrt(D);
      if ((t > 0) && (t < isectA.t)) {

        isectA.t = t;
        isectA.hit = 1;

        isectB.p.x = ray.org.x + ray.dir.x * t;
        isectB.p.y = ray.org.y + ray.dir.y * t;
        isectB.p.z = ray.org.z + ray.dir.z * t;

        isectB.n.x = isectB.p.x - sphere.center.x;
        isectB.n.y = isectB.p.y - sphere.center.y;
        isectB.n.z = isectB.p.z - sphere.center.z;

        vnormalize(isectB.n);
      }
    }

  }

  function ray_plane_intersect(isectA, isectB, ray, plane) {
    var d = -vdot(plane.p, plane.n);
    var v = vdot(ray.dir, plane.n);

    if (Math.abs(v) < 1e-17) return;

    var t = -(vdot(ray.org, plane.n) + d) / v;

    if ((t > 0) && (t < isectA.t)) {
      isectA.t = t;
      isectA.hit = 1;
      isectB.p.x = ray.org.x + ray.dir.x * t;
      isectB.p.y = ray.org.y + ray.dir.y * t;
      isectB.p.z = ray.org.z + ray.dir.z * t;
      isectB.n = plane.n;
    }
  }

  // SIMD AO calculation functions ----------------------------------------------------

  function ambient_occlusion_simd(isect) {
    var col = {};

    var i, j;
    var ntheta = NAO_SAMPLES;
    var nphi = NAO_SAMPLES;
    var eps = 0.0001;

    var p = {
      x: isect.p.x + eps * isect.n.x,
      y: isect.p.y + eps * isect.n.y,
      z: isect.p.z + eps * isect.n.z
    };

    var basis = new Array({}, {}, {});
    orthoBasis(basis, isect.n);

    var occlusion = 0;
    var occlusionx4 = SIMD.Float32x4.splat(0.0);

    for (j = 0; j < ntheta; j++) {
      for (i = 0; i < nphi; i += 4) {
        var theta = SIMD.Float32x4.sqrt(SIMD.Float32x4(rands1[j * ntheta + i], rands1[j * ntheta + i + 1], rands1[j * ntheta + i + 2], rands1[j * ntheta + i + 3]));
        var phi0 = 2 * Math.PI * rands2[j * ntheta + i];
        var phi1 = 2 * Math.PI * rands2[j * ntheta + i + 1];
        var phi2 = 2 * Math.PI * rands2[j * ntheta + i + 2];
        var phi3 = 2 * Math.PI * rands2[j * ntheta + i + 3];
        var sinphi = SIMD.Float32x4(Math.sin(phi0), Math.sin(phi1), Math.sin(phi2), Math.sin(phi3));
        var cosphi = SIMD.Float32x4(Math.cos(phi0), Math.cos(phi1), Math.cos(phi2), Math.cos(phi3));

        var x = SIMD.Float32x4.mul(cosphi, theta);
        var y = SIMD.Float32x4.mul(sinphi, theta);
        var z = SIMD.Float32x4.sqrt(SIMD.Float32x4.sub(SIMD.Float32x4.splat(1), SIMD.Float32x4.mul(theta, theta)));

        var dirx = SIMD.Float32x4.add(SIMD.Float32x4.mul(x, SIMD.Float32x4.splat(basis[0].x)),
                            SIMD.Float32x4.add(SIMD.Float32x4.mul(y, SIMD.Float32x4.splat(basis[1].x)),
                                     SIMD.Float32x4.mul(z, SIMD.Float32x4.splat(basis[2].x))));
        var diry = SIMD.Float32x4.add(SIMD.Float32x4.mul(x, SIMD.Float32x4.splat(basis[0].y)),
                            SIMD.Float32x4.add(SIMD.Float32x4.mul(y, SIMD.Float32x4.splat(basis[1].y)),
                                     SIMD.Float32x4.mul(z, SIMD.Float32x4.splat(basis[2].y))));
        var dirz = SIMD.Float32x4.add(SIMD.Float32x4.mul(x, SIMD.Float32x4.splat(basis[0].z)),
                            SIMD.Float32x4.add(SIMD.Float32x4.mul(y, SIMD.Float32x4.splat(basis[1].z)),
                                     SIMD.Float32x4.mul(z, SIMD.Float32x4.splat(basis[2].z))));

        var orgx = SIMD.Float32x4.splat(p.x);
        var orgy = SIMD.Float32x4.splat(p.y);
        var orgz = SIMD.Float32x4.splat(p.z);

        var occIsectA = {
          t: SIMD.Float32x4.splat(1e17),
          hit: SIMD.Int32x4.splat(0)
        };
        var occIsectB = {
          p: {
            x: SIMD.Float32x4.splat(0.0),
            y: SIMD.Float32x4.splat(0.0),
            z: SIMD.Float32x4.splat(0.0)
          },
          n: {
            x: SIMD.Float32x4.splat(0.0),
            y: SIMD.Float32x4.splat(0.0),
            z: SIMD.Float32x4.splat(0.0)
          }
        };

        ray_sphere_intersect_simd(occIsectA, occIsectB, dirx, diry, dirz, orgx, orgy, orgz, spheres[0]);
        ray_sphere_intersect_simd(occIsectA, occIsectB, dirx, diry, dirz, orgx, orgy, orgz, spheres[1]);
        ray_sphere_intersect_simd(occIsectA, occIsectB, dirx, diry, dirz, orgx, orgy, orgz, spheres[2]);
        ray_plane_intersect_simd (occIsectA, occIsectB, dirx, diry, dirz, orgx, orgy, orgz, plane);

        occlusionx4 = SIMD.Float32x4.add(
                        occlusionx4,
                        SIMD.Float32x4.fromInt32x4Bits(
                          SIMD.Int32x4.and(
                            occIsectA.hit, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.splat(1)))));

      }
    }

    occlusion = SIMD.Float32x4.extractLane(occlusionx4, 0) +
        SIMD.Float32x4.extractLane(occlusionx4, 1) +
        SIMD.Float32x4.extractLane(occlusionx4, 2) +
        SIMD.Float32x4.extractLane(occlusionx4, 3);

    occlusion = (ntheta * nphi - occlusion) / (ntheta * nphi);

    col.x = occlusion;
    col.y = occlusion;
    col.z = occlusion;

    return col;
  }

  function ray_sphere_intersect_simd(isectA, isectB, dirx, diry, dirz, orgx, orgy, orgz, sphere) {

    var rsx = SIMD.Float32x4.sub(orgx, SIMD.Float32x4.splat(sphere.center.x));
    var rsy = SIMD.Float32x4.sub(orgy, SIMD.Float32x4.splat(sphere.center.y));
    var rsz = SIMD.Float32x4.sub(orgz, SIMD.Float32x4.splat(sphere.center.z));

    var B = SIMD.Float32x4.add(SIMD.Float32x4.mul(rsx, dirx),
                     SIMD.Float32x4.add(SIMD.Float32x4.mul(rsy, diry), SIMD.Float32x4.mul(rsz, dirz)));
    var C = SIMD.Float32x4.sub(SIMD.Float32x4.add(SIMD.Float32x4.mul(rsx, rsx),
                              SIMD.Float32x4.add(SIMD.Float32x4.mul(rsy, rsy), SIMD.Float32x4.mul(rsz, rsz))),
                     SIMD.Float32x4.splat(sphere.radius * sphere.radius));
    var D = SIMD.Float32x4.sub(SIMD.Float32x4.mul(B, B), C);

    var cond1 = SIMD.Float32x4.greaterThan(D, SIMD.Float32x4.splat(0.0));
    if (cond1.signMask) {
      var t2 = SIMD.Float32x4.fromInt32x4Bits(SIMD.Int32x4.and(cond1, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.sub(SIMD.Float32x4.neg(B), SIMD.Float32x4.sqrt(D)))));
      var cond2 = SIMD.Int32x4.and(SIMD.Float32x4.greaterThan(t2, SIMD.Float32x4.splat(0.0)),
                                   SIMD.Float32x4.lessThan(t2, isectA.t));
      if (cond2.signMask) {
        isectA.t = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(
                       SIMD.Int32x4.and(
                         cond2,
                         SIMD.Int32x4.fromFloat32x4Bits(t2)),
                       SIMD.Int32x4.and(
                         SIMD.Int32x4.not(cond2),
                         SIMD.Int32x4.fromFloat32x4Bits(isectA.t))));
        isectA.hit = SIMD.Int32x4.or(cond2, isectA.hit);
        isectB.p.x = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.add(orgx, SIMD.Float32x4.mul(dirx, isectA.t)))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.p.x))));
        isectB.p.y = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.add(orgx, SIMD.Float32x4.mul(diry, isectA.t)))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.p.y))));
        isectB.p.z = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.add(orgx, SIMD.Float32x4.mul(dirz, isectA.t)))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.p.z))));

        isectB.n.x = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.sub(isectB.p.x, SIMD.Float32x4.splat(sphere.center.x)))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.x))));
        isectB.n.y = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.sub(isectB.p.y, SIMD.Float32x4.splat(sphere.center.y)))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.y))));
        isectB.n.z = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.sub(isectB.p.z, SIMD.Float32x4.splat(sphere.center.z)))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.z))));

        var lengths = SIMD.Float32x4.sqrt(SIMD.Float32x4.add(SIMD.Float32x4.mul(isectB.n.x, isectB.n.x),
                                          SIMD.Float32x4.add(SIMD.Float32x4.mul(isectB.n.y, isectB.n.y),
                                                             SIMD.Float32x4.mul(isectB.n.z, isectB.n.z))));
        var cond3 = SIMD.Float32x4.greaterThan(SIMD.Float32x4.abs(lengths), SIMD.Float32x4.splat(1e-17));
        isectB.n.x = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond3, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.div(isectB.n.x, lengths))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond3), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.x))));
        isectB.n.y = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond3, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.div(isectB.n.y, lengths))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond3), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.y))));
        isectB.n.z = SIMD.Float32x4.fromInt32x4Bits(
                       SIMD.Int32x4.or(SIMD.Int32x4.and(cond3, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.div(isectB.n.z, lengths))),
                       SIMD.Int32x4.and(SIMD.Int32x4.not(cond3), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.z))));
      }
    }
  }

  function ray_plane_intersect_simd(isectA, isectB, dirx, diry, dirz, orgx, orgy, orgz, plane) {
    var d = SIMD.Float32x4.neg(SIMD.Float32x4.add(SIMD.Float32x4.mul(SIMD.Float32x4.splat(plane.p.x), SIMD.Float32x4.splat(plane.n.x)),
                               SIMD.Float32x4.add(SIMD.Float32x4.mul(SIMD.Float32x4.splat(plane.p.y), SIMD.Float32x4.splat(plane.n.y)),
                                        SIMD.Float32x4.mul(SIMD.Float32x4.splat(plane.p.z), SIMD.Float32x4.splat(plane.n.z)))));
    var v = SIMD.Float32x4.add(SIMD.Float32x4.mul(dirx, SIMD.Float32x4.splat(plane.n.x)),
                     SIMD.Float32x4.add(SIMD.Float32x4.mul(diry, SIMD.Float32x4.splat(plane.n.y)),
                              SIMD.Float32x4.mul(dirz, SIMD.Float32x4.splat(plane.n.z))));

    var cond1 = SIMD.Float32x4.greaterThan(SIMD.Float32x4.abs(v), SIMD.Float32x4.splat(1e-17));
    var dp = SIMD.Float32x4.add(SIMD.Float32x4.mul(orgx, SIMD.Float32x4.splat(plane.n.x)),
                      SIMD.Float32x4.add(SIMD.Float32x4.mul(orgy, SIMD.Float32x4.splat(plane.n.y)),
                               SIMD.Float32x4.mul(orgz, SIMD.Float32x4.splat(plane.n.z))));
    var t2 = SIMD.Float32x4.fromInt32x4Bits(SIMD.Int32x4.and(cond1, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.div(SIMD.Float32x4.neg(SIMD.Float32x4.add(dp, d)), v))));
    var cond2 = SIMD.Int32x4.and(SIMD.Float32x4.greaterThan(t2, SIMD.Float32x4.splat(0.0)), SIMD.Float32x4.lessThan(t2, isectA.t));
    if (cond2.signMask) {
      isectA.t = SIMD.Float32x4.fromInt32x4Bits(SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(t2)),
                                             SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectA.t))));
      isectA.hit = SIMD.Int32x4.or(cond2, isectA.hit);
      isectB.p.x = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.add(orgx, SIMD.Float32x4.mul(dirx, isectA.t)))),
                     SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.p.x))));
      isectB.p.y = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.add(orgx, SIMD.Float32x4.mul(diry, isectA.t)))),
                     SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.p.y))));
      isectB.p.z = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.add(orgx, SIMD.Float32x4.mul(dirz, isectA.t)))),
                     SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.p.z))));

      isectB.n.x = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(Float32x4.splat(plane.n.x))),
                     SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.x))));
      isectB.n.y = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(Float32x4.splat(plane.n.y))),
                     SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.y))));
      isectB.n.z = SIMD.Float32x4.fromInt32x4Bits(
                     SIMD.Int32x4.or(SIMD.Int32x4.and(cond2, SIMD.Int32x4.fromFloat32x4Bits(Float32x4.splat(plane.n.z))),
                     SIMD.Int32x4.and(SIMD.Int32x4.not(cond2), SIMD.Int32x4.fromFloat32x4Bits(isectB.n.z))));
    }
  }

  // Utility calculation functions ----------------------------------------------------

  function vdot(v0, v1) {
    return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
  }

  function vcross(v0, v1) {
    return {
      x: v0.y * v1.z - v0.z * v1.y,
      y: v0.z * v1.x - v0.x * v1.z,
      z: v0.x * v1.y - v0.y * v1.x
    };
  }

  function vnormalize(c) {
    var length = Math.sqrt(vdot(c, c));
    if (Math.abs(length) > 1e-17) {
      c.x /= length;
      c.y /= length;
      c.z /= length;
    }
  }

  function orthoBasis(basis, n) {
    basis[2] = n;
    basis[1] = { x: 0, y: 0, z: 0 };

    if ((n.x < 0.6) && (n.x > -0.6)) {
      basis[1].x = 1.0;
    }
    else if ((n.y < 0.6) && (n.y > -0.6)) {
      basis[1].y = 1.0;
    }
    else if ((n.z < 0.6) && (n.z > -0.6)) {
      basis[1].z = 1.0;
    }
    else {
      basis[1].x = 1.0;
    }

    basis[0] = vcross(basis[1], basis[2]);
    vnormalize(basis[0]);

    basis[1] = vcross(basis[2], basis[0]);
    vnormalize(basis[1]);
  }

} ());
