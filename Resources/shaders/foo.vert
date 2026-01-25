#version 310 es

precision highp float;
precision highp int;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

struct BasisVectors {
    vec3 S;
    vec3 T;
};
const float TOF = 2.3283064e-10;
const float TWO_PI = 6.2831855;

layout(rgba32f) writeonly uniform highp image2DArray _group_0_binding_0_cs;

uniform highp samplerCube _group_0_binding_1_cs;


vec3 getCubeMapTexCoord(vec2 imageSize_, uvec3 id_1) {
    vec3 ret = vec3(0.0);
    vec2 st = (vec2(id_1.xy) / imageSize_);
    vec2 uv = ((2.0 * vec2(st.x, (1.0 - st.y))) - vec2(1.0));
    if ((id_1.z == 0u)) {
        ret = vec3(1.0, uv.y, -(uv.x));
    } else {
        if ((id_1.z == 1u)) {
            ret = vec3(-1.0, uv.y, uv.x);
        } else {
            if ((id_1.z == 2u)) {
                ret = vec3(uv.x, 1.0, -(uv.y));
            } else {
                if ((id_1.z == 3u)) {
                    ret = vec3(uv.x, -1.0, uv.y);
                } else {
                    if ((id_1.z == 4u)) {
                        ret = vec3(uv.x, uv.y, 1.0);
                    } else {
                        if ((id_1.z == 5u)) {
                            ret = vec3(-(uv.x), uv.y, -1.0);
                        }
                    }
                }
            }
        }
    }
    vec3 _e61 = ret;
    return normalize(_e61);
}

BasisVectors computeBasisVectors(vec3 N) {
    vec3 up = ((abs(N.y) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));
    vec3 S_1 = normalize(cross(up, N));
    vec3 T_1 = cross(N, S_1);
    return BasisVectors(S_1, T_1);
}

vec2 hammersley(uint i_1, uint numSamples) {
    uint bits = 0u;
    bits = i_1;
    uint _e3 = bits;
    uint _e6 = bits;
    bits = ((_e3 << 16u) | (_e6 >> 16u));
    uint _e10 = bits;
    uint _e15 = bits;
    bits = (((_e10 & 1431655765u) << 1u) | ((_e15 & 2863311530u) >> 1u));
    uint _e21 = bits;
    uint _e26 = bits;
    bits = (((_e21 & 858993459u) << 2u) | ((_e26 & 3435973836u) >> 2u));
    uint _e32 = bits;
    uint _e37 = bits;
    bits = (((_e32 & 252645135u) << 4u) | ((_e37 & 4042322160u) >> 4u));
    uint _e43 = bits;
    uint _e48 = bits;
    bits = (((_e43 & 16711935u) << 8u) | ((_e48 & 4278255360u) >> 8u));
    uint _e57 = bits;
    return vec2((float(i_1) / float(numSamples)), (float(_e57) * TOF));
}

vec3 sampleHemisphere(float u1_, float u2_) {
    float u1p = sqrt(max(0.0, (1.0 - (u1_ * u1_))));
    return vec3((cos((TWO_PI * u2_)) * u1p), (sin((TWO_PI * u2_)) * u1p), u1_);
}

vec3 tangentToWorld(vec3 v, vec3 N_1, vec3 S, vec3 T) {
    return (((S * v.x) + (T * v.y)) + (N_1 * v.z));
}

void main() {
    uvec3 id = gl_GlobalInvocationID;
    vec3 irradiance = vec3(0.0);
    uint i = 0u;
    uvec2 outputDimensions = uvec2(imageSize(_group_0_binding_0_cs).xy).xy;
    if (((id.x >= outputDimensions.x) || (id.y >= outputDimensions.y))) {
        return;
    }
    vec3 _e12 = getCubeMapTexCoord(vec2(outputDimensions), id);
    BasisVectors _e13 = computeBasisVectors(_e12);
    bool loop_init = true;
    while(true) {
        if (!loop_init) {
            uint _e44 = i;
            i = (_e44 + 1u);
        }
        loop_init = false;
        uint _e20 = i;
        if ((_e20 < 2048u)) {
        } else {
            break;
        }
        {
            uint _e22 = i;
            vec2 _e23 = hammersley(_e22, 2048u);
            vec3 _e26 = sampleHemisphere(_e23.x, _e23.y);
            vec3 _e29 = tangentToWorld(_e26, _e12, _e13.S, _e13.T);
            float cosTheta = max(0.0, dot(_e29, _e12));
            vec4 _e36 = textureLod(_group_0_binding_1_cs, vec3(_e29), 0.0);
            vec3 radianceSample = _e36.xyz;
            vec3 _e41 = irradiance;
            irradiance = (_e41 + ((2.0 * radianceSample) * cosTheta));
        }
    }
    vec3 _e48 = irradiance;
    irradiance = (_e48 / vec3(float(2048u)));
    vec3 _e53 = irradiance;
    imageStore(_group_0_binding_0_cs, ivec3(id.xy, id.z), vec4(_e53, 1.0));
    return;
}

