#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <RE/N/NiPoint3.h>
#include <RE/N/NiTransform.h>

namespace body
{
    // Column-vector rigid transform with uniform scale: x -> r * (s * x) + t.
    struct Xf
    {
        glm::mat3 r{ 1.0f };
        glm::vec3 t{ 0.0f };
        float     s{ 1.0f };
    };

    // Stored rotations are row-vector matrices; glm column a equals stored row a.
    inline Xf FromNi(const RE::NiTransform& n)
    {
        Xf x;
        for (int a = 0; a < 3; ++a) {
            for (int b = 0; b < 3; ++b) {
                x.r[a][b] = n.rotate.entry[a].pt[b];
            }
        }
        x.t = { n.translate.x, n.translate.y, n.translate.z };
        x.s = n.scale;
        return x;
    }

    inline void ToNi(const Xf& in, RE::NiTransform& n)
    {
        Xf x = in;
        x.r[0] = glm::normalize(x.r[0]);
        x.r[1] = glm::normalize(x.r[1] - glm::dot(x.r[1], x.r[0]) * x.r[0]);
        x.r[2] = glm::cross(x.r[0], x.r[1]);
        for (int a = 0; a < 3; ++a) {
            for (int b = 0; b < 3; ++b) {
                n.rotate.entry[a].pt[b] = x.r[a][b];
            }
            n.rotate.entry[a].pt[3] = 0.0f;
        }
        n.translate = { x.t.x, x.t.y, x.t.z };
        n.scale     = x.s;
    }

    inline Xf Compose(const Xf& parent, const Xf& local)
    {
        return { parent.r * local.r, parent.t + parent.s * (parent.r * local.t), parent.s * local.s };
    }

    inline Xf Inverse(const Xf& x)
    {
        const auto rt = glm::transpose(x.r);
        const auto s  = x.s != 0.0f ? 1.0f / x.s : 1.0f;
        return { rt, -(rt * x.t) * s, s };
    }

    inline glm::mat3 RotationBetween(glm::vec3 from, glm::vec3 to)
    {
        from = glm::normalize(from);
        to   = glm::normalize(to);
        return glm::mat3_cast(glm::rotation(from, to));
    }

    // Right-handed frame (right, forward, up) from a forward direction and an approximate up.
    inline glm::mat3 FrameOf(glm::vec3 forward, glm::vec3 up)
    {
        forward         = glm::normalize(forward);
        glm::vec3 right = glm::cross(forward, up);
        if (glm::length(right) < 1e-4f) {
            right = glm::cross(forward, glm::vec3{ 0.0f, 0.0f, 1.0f });
        }
        right = glm::normalize(right);
        return glm::mat3{ right, forward, glm::cross(right, forward) };
    }

    inline glm::vec3 ToVec(const RE::NiPoint3& p) { return { p.x, p.y, p.z }; }
}
