#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "vulkaninja/buffer.hpp"

namespace vulkaninja
{
    struct VertexAttributeDescription
    {
        uint32_t   offset;
        vk::Format format;
    };

    struct Vertex
    {
        glm::vec3 pos {0.0};
        glm::vec3 normal {0.0};
        glm::vec2 texCoord {0.0};

        auto operator==(const Vertex& other) const -> bool
        {
            return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
        }

        static auto getAttributeDescriptions() -> std::vector<VertexAttributeDescription>;
    };

    enum class MeshUsage
    {
        eGraphics,
        eRayTracing,
        eHybrid,
    };

    struct SphereMeshCreateInfo
    {
        int         numSlices = 32;
        int         numStacks = 32;
        float       radius    = 1.0;
        MeshUsage   usage     = MeshUsage::eGraphics;
        std::string name      = "Sphere";
    };

    struct CubeMeshCreateInfo
    {
        MeshUsage   usage = MeshUsage::eGraphics;
        std::string name  = "Cube";
    };

    struct CubeLineMeshCreateInfo
    {
        MeshUsage   usage = MeshUsage::eGraphics;
        std::string name  = "CubeLine";
    };

    struct PlaneMeshCreateInfo
    {
        float       width          = 1.0f;
        float       height         = 1.0f;
        uint32_t    widthSegments  = 1u;
        uint32_t    heightSegments = 1u;
        MeshUsage   usage          = MeshUsage::eGraphics;
        std::string name           = "Plane";
    };

    struct PlaneLineMeshCreateInfo
    {
        float       width          = 1;
        float       height         = 1;
        uint32_t    widthSegments  = 1;
        uint32_t    heightSegments = 1;
        MeshUsage   usage          = MeshUsage::eGraphics;
        std::string name           = "PlaneLine";
    };

    class Mesh
    {
    public:
        Mesh() = default;
        Mesh(const Context&          contextIn,
             MeshUsage               usage,
             vk::MemoryPropertyFlags memoryProps,
             std::vector<Vertex>     verticesIn,
             std::vector<uint32_t>   indicesIn,
             std::string             nameIn);
        static auto createSphereMesh(const Context& context, SphereMeshCreateInfo createInfo) -> Mesh;
        static auto createPlaneMesh(const Context& context, PlaneMeshCreateInfo createInfo) -> Mesh;
        static auto createCubeMesh(const Context& context, CubeMeshCreateInfo createInfo) -> Mesh;
        static auto createPlaneLineMesh(const Context& context, PlaneLineMeshCreateInfo createInfo) -> Mesh;
        static auto createCubeLineMesh(const Context& context, CubeLineMeshCreateInfo createInfo) -> Mesh;

        auto getVertexCount() const -> uint32_t { return static_cast<uint32_t>(vertices.size()); }
        auto getIndicesCount() const -> uint32_t { return static_cast<uint32_t>(indices.size()); }
        auto getTriangleCount() const -> uint32_t { return static_cast<uint32_t>(indices.size() / 3); }

        const Context* context = nullptr;

        std::string name;

        BufferHandle vertexBuffer;
        BufferHandle indexBuffer;

        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
    };
} // namespace vulkaninja

namespace std
{
    template<>
    struct hash<vulkaninja::Vertex>
    {
        auto operator()(const vulkaninja::Vertex& vertex) const -> size_t
        {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
} // namespace std