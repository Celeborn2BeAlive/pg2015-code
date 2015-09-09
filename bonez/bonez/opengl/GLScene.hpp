#pragma once

#include <unordered_map>
#include <vector>
#include <bonez/scene/SceneGeometry.hpp>
#include "utils/GLutils.hpp"

namespace BnZ {

struct GLMaterialUniforms {
    GLUniform<Vec3f> uKd;
    GLUniform<Vec3f> uKs;
    GLUniform<float> uShininess;
    GLUniform<GLSLSampler2Df> uKdSampler;
    GLUniform<GLSLSampler2Df> uKsSampler;
    GLUniform<GLSLSampler2Df> uShininessSampler;

    GLMaterialUniforms(const GLProgram& program):
        uKd(program, "uMaterial.Kd"),
        uKs(program, "uMaterial.Ks"),
        uShininess(program, "uMaterial.shininess"),
        uKdSampler(program, "uMaterial.KdSampler"),
        uKsSampler(program, "uMaterial.KsSampler"),
        uShininessSampler(program, "uMaterial.shininessSampler") {
    }
};

//#define USEVERTEXBUFFERUNIFIEDMEMORY
#ifndef USEVERTEXBUFFERUNIFIEDMEMORY

//#define USEONEBUFFER
#ifdef USEONEBUFFER

#define USEMULTIDRAWINDIRECT
#ifdef USEMULTIDRAWINDIRECT

class GLScene {
public:
    GLScene(const SceneGeometry& geometry);

    void render(uint32_t numInstances = 1) const;

    void render(GLMaterialUniforms& uniforms, uint32_t numInstances = 1) const;
private:
    GLImmutableBuffer<TriangleMesh::Vertex> m_VBO;
    GLImmutableBuffer<TriangleMesh::Triangle> m_IBO;
    GLVertexArray m_VAO;

    struct DrawElementsIndirectCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint firstIndex;
        GLuint baseVertex;
        GLuint baseInstance;
    };

    mutable GLImmutableBuffer<DrawElementsIndirectCommand> m_DrawCommands;

    std::vector<GLTexture2D> m_Textures;
    std::unordered_map<Shared<Image>, int> m_TextureCache;

    struct GLMaterial {
        Vec3f m_Kd;
        Vec3f m_Ks;
        float m_Shininess;

        int m_KdTextureID = -1;
        int m_KsTextureID = -1;
        int m_ShininessTextureID = -1;

        GLMaterial(const Material& material, int kdTextureID, int ksTextureID, int shininessTextureID);
    };

    std::vector<GLMaterial> m_Materials;
};

#else

class GLScene {
public:
    GLScene(const SceneGeometry& geometry);

    void render(uint32_t numInstances = 1) const;

    void render(GLMaterialUniforms& uniforms, uint32_t numInstances = 1) const;
private:
    GLImmutableBuffer<TriangleMesh::Vertex> m_VBO;
    GLImmutableBuffer<TriangleMesh::Triangle> m_IBO;
    GLVertexArray m_VAO;

    struct GLTriangleMesh {
        uint32_t m_nBaseVertex;
        uint32_t m_nIndexOffset;
        uint32_t m_nIndexCount;
        uint32_t m_MaterialID;

        GLTriangleMesh() = default;

        GLTriangleMesh(uint32_t baseVertex, uint32_t indexOffset,
                       uint32_t indexCount, uint32_t materialID);

        void render(uint32_t numInstances) const;
    };

    std::vector<GLTexture2D> m_Textures;
    std::unordered_map<Shared<Image>, int> m_TextureCache;

    struct GLMaterial {
        Vec3f m_Kd;
        Vec3f m_Ks;
        float m_Shininess;

        int m_KdTextureID = -1;
        int m_KsTextureID = -1;
        int m_ShininessTextureID = -1;

        GLMaterial(const Material& material, int kdTextureID, int ksTextureID, int shininessTextureID);
    };

    std::vector<GLTriangleMesh> m_TriangleMeshs;
    std::vector<GLMaterial> m_Materials;
};

#endif

#else

class GLScene {
public:
    GLScene(const SceneGeometry& geometry);

    void render(uint32_t numInstances = 1) const;

    void render(GLMaterialUniforms& uniforms, uint32_t numInstances = 1) const;
private:
    struct GLTriangleMesh {
        GLBufferStorage<TriangleMesh::Vertex> m_VBO;
        GLBufferStorage<TriangleMesh::Triangle> m_IBO;
        GLVertexArray m_VAO;
        uint32_t m_MaterialID;

        GLTriangleMesh() = default;

        GLTriangleMesh(GLTriangleMesh&& rvalue) : m_VBO(std::move(rvalue.m_VBO)), m_IBO(std::move(rvalue.m_IBO)),
            m_VAO(std::move(rvalue.m_VAO)), m_MaterialID(rvalue.m_MaterialID) {
        }

        GLTriangleMesh(const TriangleMesh& mesh);

        void render(uint32_t numInstances) const;
    };

    std::vector<GLTexture2D> m_Textures;
    std::unordered_map<Shared<Image>, int> m_TextureCache;

    struct GLMaterial {
        Vec3f m_Kd;
        Vec3f m_Ks;
        float m_Shininess;

        int m_KdTextureID = -1;
        int m_KsTextureID = -1;
        int m_ShininessTextureID = -1;

        GLMaterial(const Material& material, int kdTextureID, int ksTextureID, int shininessTextureID);
    };

    std::vector<GLTriangleMesh> m_TriangleMeshs;
    std::vector<GLMaterial> m_Materials;
};

#endif

#else

class GLScene {
public:
    GLScene(const SceneGeometry& geometry);

    void render(uint32_t numInstances = 1) const;

    void render(GLMaterialUniforms& uniforms, uint32_t numInstances = 1) const;
private:
    struct GLTriangleMesh {
        GLImmutableBuffer<TriangleMesh::Vertex> m_VBO;
        GLImmutableBuffer<TriangleMesh::Triangle> m_IBO;
        uint32_t m_MaterialID;

        GLTriangleMesh() = default;

        GLTriangleMesh(GLTriangleMesh&& rvalue) :
            m_VBO(std::move(rvalue.m_VBO)),
            m_IBO(std::move(rvalue.m_IBO)),
            m_MaterialID(rvalue.m_MaterialID) {
        }

        GLTriangleMesh(const TriangleMesh& mesh);

        void render(uint32_t numInstances) const;
    };

    std::vector<GLTexture2D> m_Textures;
    std::unordered_map<Shared<Image>, int> m_TextureCache;

    struct GLMaterial {
        Vec3f m_Kd;
        Vec3f m_Ks;
        float m_Shininess;

        int m_KdTextureID = -1;
        int m_KsTextureID = -1;
        int m_ShininessTextureID = -1;

        GLMaterial(const Material& material, int kdTextureID, int ksTextureID, int shininessTextureID);
    };

    std::vector<GLTriangleMesh> m_TriangleMeshs;
    std::vector<GLMaterial> m_Materials;
};

#endif

}
