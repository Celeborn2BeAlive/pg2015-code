#include "GLScene.hpp"

namespace BnZ {

#ifndef USEVERTEXBUFFERUNIFIEDMEMORY

#ifdef USEONEBUFFER

GLScene::GLScene(const SceneGeometry& geometry) {
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    for (const auto& mesh : geometry.m_TriangleMeshs) {
        vertexCount += mesh.m_Vertices.size();
        triangleCount += mesh.m_Triangles.size();
    }

    m_VBO = genBufferStorage<TriangleMesh::Vertex>(vertexCount, nullptr, GL_MAP_WRITE_BIT);
    m_IBO = genBufferStorage<TriangleMesh::Triangle>(triangleCount, nullptr, GL_MAP_WRITE_BIT);

    m_DrawCommands = genBufferStorage<DrawElementsIndirectCommand>(geometry.m_TriangleMeshs.size(),
                                                                   nullptr, GL_MAP_WRITE_BIT);

    auto pDrawCommand = m_DrawCommands.map(GL_MAP_WRITE_BIT);
    auto pVertex = m_VBO.map(GL_MAP_WRITE_BIT);
    auto pTriangle = m_IBO.map(GL_MAP_WRITE_BIT);

    uint32_t baseVertex = 0u;
    uint32_t indexOffset = 0u;
    for (const auto& mesh : geometry.m_TriangleMeshs) {
        std::copy(begin(mesh.m_Vertices), end(mesh.m_Vertices), pVertex);
        pVertex += mesh.m_Vertices.size();

        std::copy(begin(mesh.m_Triangles), end(mesh.m_Triangles), pTriangle);
        pTriangle += mesh.m_Triangles.size();

        pDrawCommand->count = mesh.m_Triangles.size() * 3;
        pDrawCommand->firstIndex = indexOffset;
        pDrawCommand->baseVertex = baseVertex;
        pDrawCommand->baseInstance = 0;
        pDrawCommand->instanceCount = 1;

        baseVertex += mesh.m_Vertices.size();
        indexOffset += mesh.m_Triangles.size() * 3;

        ++pDrawCommand;
    }

    m_VBO.unmap();
    m_IBO.unmap();
    m_DrawCommands.unmap();

    m_VAO.enableVertexAttrib(0);
    m_VAO.enableVertexAttrib(1);
    m_VAO.enableVertexAttrib(2);
    m_VAO.vertexAttribOffset(m_VBO.glId(), 0, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex),
        BNZ_OFFSETOF(TriangleMesh::Vertex, position));
    m_VAO.vertexAttribOffset(m_VBO.glId(), 1, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex),
        BNZ_OFFSETOF(TriangleMesh::Vertex, normal));
    m_VAO.vertexAttribOffset(m_VBO.glId(), 2, 2, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex),
        BNZ_OFFSETOF(TriangleMesh::Vertex, texCoords));

    m_VAO.bind();
        m_IBO.bind(GL_ELEMENT_ARRAY_BUFFER);
    glBindVertexArray(0);

    m_Materials.reserve(geometry.m_Materials.size() + 1);

    m_Textures.emplace_back();
    Image white(1, 1);
    white.fill(Vec4f(1));
    fillTexture(m_Textures.back(), white);
    m_Textures.back().generateMipmap();
    m_Textures.back().setMinFilter(GL_NEAREST_MIPMAP_NEAREST);
    m_Textures.back().setMagFilter(GL_LINEAR);
    m_Textures.back().makeTextureHandleResident();

    static auto getTextureID = [&](const Shared<Image>& image) -> int {
        if(!image) {
            return 0;
        }

        auto it = m_TextureCache.find(image);
        if(it != end(m_TextureCache)) {
            return (*it).second;
        }

        uint32_t id = m_Textures.size();
        m_Textures.emplace_back();
        fillTexture(m_Textures.back(), *image);
        m_Textures.back().generateMipmap();
        m_Textures.back().setMinFilter(GL_NEAREST_MIPMAP_NEAREST);
        m_Textures.back().setMagFilter(GL_LINEAR);
        m_Textures.back().makeTextureHandleResident();

        if(id == 1) {
            std::cerr << GLuint64(m_Textures.back().getTextureHandle()) << std::endl;
        }

        m_TextureCache[image] = id;

        return id;
    };
    for (const auto& material: geometry.m_Materials) {
        m_Materials.emplace_back(material, getTextureID(material.m_KdTexture),
                                 getTextureID(material.m_KsTexture),  getTextureID(material.m_ShininessTexture));
    }
}

GLScene::GLMaterial::GLMaterial(const Material &material, int kdTextureID, int ksTextureID, int shininessTextureID):
    m_Kd(material.m_Kd), m_Ks(material.m_Ks), m_Shininess(material.m_Shininess),
    m_KdTextureID(kdTextureID), m_KsTextureID(ksTextureID), m_ShininessTextureID(shininessTextureID) {
}

void GLScene::render(uint32_t numInstances) const {
    m_VAO.bind();

    auto pDrawCommand = m_DrawCommands.map(GL_MAP_WRITE_BIT);
    for(auto i = 0u; i < m_DrawCommands.size(); ++i) {
        pDrawCommand->instanceCount = numInstances;
        ++pDrawCommand;
    }
    m_DrawCommands.unmap();

    m_DrawCommands.bind(GL_DRAW_INDIRECT_BUFFER);

    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                0, m_DrawCommands.size(), 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
}

void GLScene::render(GLMaterialUniforms& uniforms, uint32_t numInstances) const {
    m_VAO.bind();

    auto pDrawCommand = m_DrawCommands.map(GL_MAP_WRITE_BIT);
    for(auto i = 0u; i < m_DrawCommands.size(); ++i) {
        pDrawCommand->instanceCount = numInstances;
        ++pDrawCommand;
    }
    m_DrawCommands.unmap();

    m_DrawCommands.bind(GL_DRAW_INDIRECT_BUFFER);

    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                0, m_DrawCommands.size(), 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
}

#else

GLScene::GLScene(const SceneGeometry& geometry) {
    m_TriangleMeshs.reserve(geometry.getMeshCount());
    for (const auto& mesh : geometry.getMeshs()) {
        m_TriangleMeshs.emplace_back(mesh);
    }

    m_Materials.reserve(geometry.getMaterialCount() + 1);

    m_Textures.emplace_back();
    Image white(1, 1);
    white.fill(Vec4f(1));
    fillTexture(m_Textures.back(), white);
    m_Textures.back().generateMipmap();
    m_Textures.back().setMinFilter(GL_NEAREST_MIPMAP_NEAREST);
    m_Textures.back().setMagFilter(GL_LINEAR);

    auto getTextureID = [&](const Shared<Image>& image) -> int {
        if(!image) {
            return 0;
        }

        auto it = m_TextureCache.find(image);
        if(it != end(m_TextureCache)) {
            return (*it).second;
        }

        uint32_t id = m_Textures.size();
        m_Textures.emplace_back();
        fillTexture(m_Textures.back(), *image);
        m_Textures.back().generateMipmap();
        m_Textures.back().setMinFilter(GL_NEAREST_MIPMAP_NEAREST);
        m_Textures.back().setMagFilter(GL_LINEAR);

        m_TextureCache[image] = id;

        return id;
    };
    for (const auto& material: geometry.getMaterials()) {
        m_Materials.emplace_back(material, getTextureID(material.m_DiffuseReflectanceTexture),
                                 getTextureID(material.m_GlossyReflectanceTexture),  getTextureID(material.m_ShininessTexture));
    }
}

GLScene::GLTriangleMesh::GLTriangleMesh(const TriangleMesh& mesh):
    m_VBO(mesh.m_Vertices),
    m_IBO(mesh.m_Triangles),
    m_MaterialID(mesh.m_MaterialID) {

    m_VAO.enableVertexAttrib(0);
    m_VAO.enableVertexAttrib(1);
    m_VAO.enableVertexAttrib(2);
    m_VAO.vertexAttribOffset(m_VBO.glId(), 0, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex),
        BNZ_OFFSETOF(TriangleMesh::Vertex, position));
    m_VAO.vertexAttribOffset(m_VBO.glId(), 1, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex),
        BNZ_OFFSETOF(TriangleMesh::Vertex, normal));
    m_VAO.vertexAttribOffset(m_VBO.glId(), 2, 2, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex),
        BNZ_OFFSETOF(TriangleMesh::Vertex, texCoords));

    m_VAO.bind();
    m_IBO.bind(GL_ELEMENT_ARRAY_BUFFER);
    glBindVertexArray(0);
}

void GLScene::GLTriangleMesh::render(uint32_t numInstances) const {
    m_VAO.bind();
    glDrawElementsInstanced(GL_TRIANGLES, m_IBO.size() * 3,
                            GL_UNSIGNED_INT, 0, numInstances);
}

GLScene::GLMaterial::GLMaterial(const Material &material, int kdTextureID, int ksTextureID, int shininessTextureID):
    m_Kd(material.m_DiffuseReflectance), m_Ks(material.m_GlossyReflectance), m_Shininess(material.m_Shininess),
    m_KdTextureID(kdTextureID), m_KsTextureID(ksTextureID), m_ShininessTextureID(shininessTextureID) {
}

void GLScene::render(uint32_t numInstances) const {
    for (const auto& mesh : m_TriangleMeshs) {
        mesh.render(numInstances);
    }
    glBindVertexArray(0);
}

void GLScene::render(GLMaterialUniforms& uniforms, uint32_t numInstances) const {
    for (const auto& mesh : m_TriangleMeshs) {
        const auto& material = m_Materials[mesh.m_MaterialID];
        uniforms.uKd.set(material.m_Kd);
        uniforms.uKs.set(material.m_Ks);
        uniforms.uShininess.set(material.m_Shininess);

        if(material.m_KdTextureID >= 0) {
            m_Textures[material.m_KdTextureID].bind(0u);
            uniforms.uKdSampler.set(0u);
        }

        if(material.m_KsTextureID >= 0) {
            m_Textures[material.m_KsTextureID].bind(1u);
            uniforms.uKsSampler.set(1u);
        }

        if(material.m_ShininessTextureID >= 0) {
            m_Textures[material.m_ShininessTextureID].bind(2u);
            uniforms.uShininessSampler.set(2u);
        }

        mesh.render(numInstances);
    }
    glBindVertexArray(0);
}

#endif

#else

GLScene::GLScene(const SceneGeometry& geometry) {
    m_TriangleMeshs.reserve(geometry.m_TriangleMeshs.size());
    for (const auto& mesh : geometry.m_TriangleMeshs) {
        m_TriangleMeshs.emplace_back(mesh);
    }

    m_Materials.reserve(geometry.m_Materials.size() + 1);

    m_Textures.emplace_back();
    Image white(16, 16);
    white.fill(Vec4f(1));
    fillTexture(m_Textures.back(), white);
    m_Textures.back().generateMipmap();
    m_Textures.back().setMinFilter(GL_NEAREST_MIPMAP_NEAREST);
    m_Textures.back().setMagFilter(GL_LINEAR);
    m_Textures.back().makeTextureHandleResident();

    static auto getTextureID = [&](const Shared<Image>& image) -> int {
        if(!image) {
            return 0;
        }

        auto it = m_TextureCache.find(image);
        if(it != end(m_TextureCache)) {
            return (*it).second;
        }

        uint32_t id = m_Textures.size();
        m_Textures.emplace_back();
        fillTexture(m_Textures.back(), *image);
        m_Textures.back().generateMipmap();
        m_Textures.back().setMinFilter(GL_NEAREST_MIPMAP_NEAREST);
        m_Textures.back().setMagFilter(GL_LINEAR);
        m_Textures.back().makeTextureHandleResident();

        m_TextureCache[image] = id;

        return id;
    };
    for (const auto& material: geometry.m_Materials) {
        m_Materials.emplace_back(material, getTextureID(material.m_KdTexture),
                                 getTextureID(material.m_KsTexture),  getTextureID(material.m_ShininessTexture));
    }
}

GLScene::GLTriangleMesh::GLTriangleMesh(const TriangleMesh& mesh):
    m_MaterialID(mesh.m_MaterialID),
    m_VBO(mesh.m_Vertices, 0, GL_READ_ONLY),
    m_IBO(mesh.m_Triangles, 0, GL_READ_ONLY) {
}

void GLScene::GLTriangleMesh::render(uint32_t numInstances) const {
    glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0,
                           GLuint64(m_VBO.getGPUAddress()) + BNZ_OFFSETOF(TriangleMesh::Vertex, position),
                           m_VBO.byteSize() - BNZ_OFFSETOF(TriangleMesh::Vertex, position));

    glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 1,
                           GLuint64(m_VBO.getGPUAddress()) + BNZ_OFFSETOF(TriangleMesh::Vertex, normal),
                           m_VBO.byteSize() - BNZ_OFFSETOF(TriangleMesh::Vertex, normal));

    glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 2,
                           GLuint64(m_VBO.getGPUAddress()) + BNZ_OFFSETOF(TriangleMesh::Vertex, texCoords),
                           m_VBO.byteSize() - BNZ_OFFSETOF(TriangleMesh::Vertex, texCoords));

    glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV, 0,
                           GLuint64(m_IBO.getGPUAddress()),
                           m_IBO.byteSize());

    glDrawElementsInstanced(GL_TRIANGLES, m_IBO.size() * 3, GL_UNSIGNED_INT, 0, numInstances);
}

GLScene::GLMaterial::GLMaterial(const Material &material, int kdTextureID, int ksTextureID, int shininessTextureID):
    m_Kd(material.m_Kd), m_Ks(material.m_Ks), m_Shininess(material.m_Shininess),
    m_KdTextureID(kdTextureID), m_KsTextureID(ksTextureID), m_ShininessTextureID(shininessTextureID) {
}

void GLScene::render(uint32_t numInstances) const {
    glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);

    glVertexAttribFormatNV(0, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex));
    glVertexAttribFormatNV(1, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex));
    glVertexAttribFormatNV(2, 2, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex));

    for (const auto& mesh : m_TriangleMeshs) {
        mesh.render(numInstances);
    }

    glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
}

void GLScene::render(GLMaterialUniforms& uniforms, uint32_t numInstances) const {
    glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);

    glVertexAttribFormatNV(0, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex));
    glVertexAttribFormatNV(1, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex));
    glVertexAttribFormatNV(2, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleMesh::Vertex));

    for (const auto& mesh : m_TriangleMeshs) {
        const auto& material = m_Materials[mesh.m_MaterialID];
        uniforms.uKd.set(material.m_Kd);
        uniforms.uKs.set(material.m_Ks);
        uniforms.uShininess.set(material.m_Shininess);

        if(material.m_KdTextureID >= 0) {
            uniforms.uKdSampler.set(m_Textures[material.m_KdTextureID].getTextureHandle());
        }

        if(material.m_KsTextureID >= 0) {
            uniforms.uKsSampler.set(m_Textures[material.m_KsTextureID].getTextureHandle());
        }

        if(material.m_ShininessTextureID >= 0) {
            uniforms.uShininessSampler.set(m_Textures[material.m_ShininessTextureID].getTextureHandle());
        }

        mesh.render(numInstances);
    }

    glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
}

#endif

}
