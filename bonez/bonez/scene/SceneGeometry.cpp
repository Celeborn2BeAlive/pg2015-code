#include "SceneGeometry.hpp"

#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <bonez/sys/time.hpp>

#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "bonez/common.hpp"
#include "bonez/sys/files.hpp"
#include "bonez/maths/maths.hpp"


#include <bonez/parsing/parsing.hpp>

namespace BnZ {

Sample<TriangleMesh::Vertex> sampleTriangle(const TriangleMesh& mesh, size_t triangleIdx, const Vec2f& s2D) {
    const auto& triangle = mesh.getTriangle(triangleIdx);

    const auto& v0 = mesh.m_Vertices[triangle.v0];
    const auto& v1 = mesh.m_Vertices[triangle.v1];
    const auto& v2 = mesh.m_Vertices[triangle.v2];

    auto sampledUV = uniformSampleTriangleUVs(s2D.x, s2D.y,
                                              v0.position,
                                              v1.position,
                                              v2.position);

    Sample<TriangleMesh::Vertex> vertex;

    auto u = sampledUV.x;
    auto v = sampledUV.y;
    auto w = 1.f - u - v;

    vertex.value.position = w * v0.position + u * v1.position + v * v2.position;
    vertex.value.normal = normalize(w * v0.normal + u * v1.normal + v * v2.normal);
    vertex.value.texCoords = w * v0.texCoords + u * v1.texCoords + v * v2.texCoords;

    auto area = mesh.getTriangleArea(triangleIdx);
    vertex.pdf = 1.f / area;

    return vertex;
}

TriangleMesh::Vertex computeTriangleCenter(const TriangleMesh& mesh, size_t triangleIdx) {
    const auto& triangle = mesh.getTriangle(triangleIdx);

    const auto& v0 = mesh.m_Vertices[triangle.v0];
    const auto& v1 = mesh.m_Vertices[triangle.v1];
    const auto& v2 = mesh.m_Vertices[triangle.v2];

    TriangleMesh::Vertex vertex;

    vertex.position = (1.f / 3.f) * (v0.position + v1.position + v2.position);
    vertex.normal = normalize((1.f / 3.f) * (v0.normal + v1.normal + v2.normal));
    vertex.texCoords = (1.f / 3.f) * (v0.texCoords + v1.texCoords + v2.texCoords);

    return vertex;
}

Sample<TriangleMesh::Vertex> sample(const TriangleMesh& mesh, float s1D, const Vec2f& s2D, std::size_t* pTriangleID) {
    auto triangleIdx = clamp(uint32_t(s1D * mesh.getTriangleCount()),
                             0u, uint32_t(mesh.getTriangleCount() - 1));

    if(pTriangleID) {
        *pTriangleID = triangleIdx;
    }

    const auto& triangle = mesh.getTriangle(triangleIdx);

    const auto& v0 = mesh.m_Vertices[triangle.v0];
    const auto& v1 = mesh.m_Vertices[triangle.v1];
    const auto& v2 = mesh.m_Vertices[triangle.v2];

    auto sampledUV = uniformSampleTriangleUVs(s2D.x, s2D.y,
                                              v0.position,
                                              v1.position,
                                              v2.position);

    Sample<TriangleMesh::Vertex> vertex;

    auto u = sampledUV.x;
    auto v = sampledUV.y;
    auto w = 1.f - u - v;

    vertex.value.position = w * v0.position + u * v1.position + v * v2.position;
    vertex.value.normal = normalize(w * v0.normal + u * v1.normal + v * v2.normal);
    vertex.value.texCoords = w * v0.texCoords + u * v1.texCoords + v * v2.texCoords;

    auto area = mesh.getTriangleArea(triangleIdx);
    vertex.pdf = 1.f / (mesh.getTriangleCount() * area);

    return vertex;
}

static const aiVector3D aiZERO(0.f, 0.f, 0.f);

static void loadMaterial(const aiMaterial* aimaterial, const FilePath& basePath, SceneGeometry& geometry) {
    auto pLogger = el::Loggers::getLogger("SceneLoader");

    aiColor3D color;

    aiString ainame;
    aimaterial->Get(AI_MATKEY_NAME, ainame);
    std::string name = ainame.C_Str();

    pLogger->verbose(1, "Load material %v", name);

    Material material(name);

    if (AI_SUCCESS == aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
        material.m_DiffuseReflectance = Vec3f(color.r, color.g, color.b);
    }

    aiString path;

    if (AI_SUCCESS == aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path,
                                             nullptr, nullptr, nullptr, nullptr, nullptr)) {
        pLogger->verbose(1, "Load texture %v", (basePath + path.data));
        material.m_DiffuseReflectanceTexture = loadImage(basePath + path.data, true);
    }

    if (AI_SUCCESS == aimaterial->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
        material.m_GlossyReflectance = Vec3f(color.r, color.g, color.b);
    }

    if (AI_SUCCESS == aimaterial->GetTexture(aiTextureType_SPECULAR, 0, &path,
                                             nullptr, nullptr, nullptr, nullptr, nullptr)) {
        pLogger->verbose(1, "Load texture %v", (basePath + path.data));
        material.m_GlossyReflectanceTexture = loadImage(basePath + path.data, true);
    }

    aimaterial->Get(AI_MATKEY_SHININESS, material.m_Shininess);

    if (AI_SUCCESS == aimaterial->GetTexture(aiTextureType_SHININESS, 0, &path,
                                             nullptr, nullptr, nullptr, nullptr, nullptr)) {
        pLogger->verbose(1, "Load texture %v", (basePath + path.data));
        material.m_ShininessTexture = loadImage(basePath + path.data, true);
    }

    geometry.addMaterial(std::move(material));
}

static void loadMesh(const aiMesh* aimesh, uint32_t materialOffset, SceneGeometry& geometry) {
    TriangleMesh mesh;

#ifdef _DEBUG
    mesh.m_MaterialID = 0;
#else
    mesh.m_MaterialID = materialOffset + aimesh->mMaterialIndex;
#endif

    mesh.m_Vertices.reserve(aimesh->mNumVertices);
    for (size_t vertexIdx = 0; vertexIdx < aimesh->mNumVertices; ++vertexIdx) {
        const aiVector3D* pPosition = &aimesh->mVertices[vertexIdx];
        const aiVector3D* pNormal = &aimesh->mNormals[vertexIdx];
        const aiVector3D* pTexCoords = aimesh->HasTextureCoords(0) ? &aimesh->mTextureCoords[0][vertexIdx] : &aiZERO;
        mesh.m_Vertices.emplace_back(
                    Vec3f(pPosition->x, pPosition->y, pPosition->z),
                    Vec3f(pNormal->x, pNormal->y, pNormal->z),
                    Vec2f(pTexCoords->x, pTexCoords->y));

        if(vertexIdx == 0) {
            mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
        } else {
            mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
        }
    }

    mesh.m_Triangles.reserve(aimesh->mNumFaces);
    for (size_t triangleIdx = 0; triangleIdx < aimesh->mNumFaces; ++triangleIdx) {
        const aiFace& face = aimesh->mFaces[triangleIdx];
        mesh.m_Triangles.emplace_back(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
    }

    geometry.append(mesh);
}

void loadAssimpScene(const aiScene* aiscene, const std::string& filepath, SceneGeometry& geometry) {
    auto pLogger = el::Loggers::getLogger("SceneLoader");

    auto materialOffset = geometry.getMaterialCount();

    FilePath path(filepath);

    //geometry.m_Materials.reserve(materialOffset + aiscene->mNumMaterials);
    for (size_t materialIdx = 0; materialIdx < aiscene->mNumMaterials; ++materialIdx) {
        loadMaterial(aiscene->mMaterials[materialIdx], path.directory(), geometry);
    }

    //geometry.m_TriangleMeshs.reserve(geometry.m_TriangleMeshs.size() + aiscene->mNumMeshes);
    for (size_t meshIdx = 0u; meshIdx < aiscene->mNumMeshes; ++meshIdx) {
        loadMesh(aiscene->mMeshes[meshIdx], materialOffset, geometry);
    }
}

SceneGeometry loadModel(const std::string& filepath) {
    auto pLogger = el::Loggers::getLogger("SceneLoader");

    Assimp::Importer importer;

    pLogger->info("Loading geometry of %v with assimp.", filepath);

    //importer.SetExtraVerbose(true); // TODO: add logger and check for sponza
    const aiScene* aiscene = importer.ReadFile(filepath.c_str(),
                                               aiProcess_Triangulate |
                                               aiProcess_GenNormals |
                                               aiProcess_FlipUVs);
    if (aiscene) {
        pLogger->info("Number of meshes = %v", aiscene->mNumMeshes);
        try {
            SceneGeometry geometry;
            loadAssimpScene(aiscene, filepath, geometry);

            return geometry;
        }
        catch (const std::runtime_error& e) {
            throw std::runtime_error("Assimp loading error on file " + filepath + ": " + e.what());
        }
    }
    else {
        throw std::runtime_error("Assimp loading error on file " + filepath + ": " + importer.GetErrorString());
    }
}

void SceneGeometry::postIntersect(const Ray& ray, Intersection& I) const {
    float u = I.uv.x;
    float v = I.uv.y;
    float w = 1.f - u - v;

    const auto& mesh = m_TriangleMeshs[I.meshID];
    const auto& triangle = mesh.m_Triangles[I.triangleID];

    const auto& v0 = mesh.m_Vertices[triangle.v0];
    const auto& v1 = mesh.m_Vertices[triangle.v1];
    const auto& v2 = mesh.m_Vertices[triangle.v2];

    I.Ns = normalize(w * v0.normal + u * v1.normal + v * v2.normal);

    if(glm::dot(-ray.dir, I.Ns) > 0.f) {
        I.Le = m_Materials[mesh.m_MaterialID].m_EmittedRadiance;
        if(m_Materials[mesh.m_MaterialID].m_EmittedRadianceTexture) {
            I.Le *= Vec3f(texture(*m_Materials[mesh.m_MaterialID].m_EmittedRadianceTexture, I.texCoords));
        }
    }

    I.texCoords = w * v0.texCoords + u * v1.texCoords + v * v2.texCoords;
}

void SceneGeometry::getSurfacePoint(uint32_t meshID, uint32_t triangleID,
                                    float u, float v, SurfacePoint& point) const {
    float w = 1.f - u - v;
    const auto& mesh = m_TriangleMeshs[meshID];
    const auto& triangle = mesh.m_Triangles[triangleID];

    const auto& v0 = mesh.m_Vertices[triangle.v0];
    const auto& v1 = mesh.m_Vertices[triangle.v1];
    const auto& v2 = mesh.m_Vertices[triangle.v2];

    auto e0 = v1.position - v0.position;
    auto e1 = v2.position - v0.position;

    point.meshID = meshID;
    point.triangleID = triangleID;
    point.P = w * v0.position + u * v1.position + v * v2.position;
    point.Ng = normalize(cross(e0, e1));
    point.uv = Vec2f(u, v);
    point.Ns = normalize(w * v0.normal + u * v1.normal + v * v2.normal);
    faceForward(point.Ns, point.Ng);
    point.texCoords = w * v0.texCoords + u * v1.texCoords + v * v2.texCoords;
}

static Mat4f loadMatrix(const tinyxml2::XMLElement& description) {
    auto matrix = Mat4f(1.f);
    for(auto pElement = description.FirstChildElement(); pElement; pElement = pElement->NextSiblingElement()) {
        if(std::string(pElement->Name()) == "Rotate") {
            auto angle = 0.f;
            getAttribute(*pElement, "angle", angle);
            auto axis = Vec3f(0, 1, 0);
            getAttribute(*pElement, "x", axis.x);
            getAttribute(*pElement, "y", axis.y);
            getAttribute(*pElement, "z", axis.z);
            axis = normalize(axis);

            std::string angleUnit = "degree";
            getAttribute(*pElement, "angleUnit", angleUnit);
            if(angleUnit == "degree") {
                angle = radians(angle);
            } else if(angleUnit != "radians") {
                CLOG(ERROR, "SceneLoader") << "Unknow angle unit for Rotate, using degree instead";
            }

            matrix = rotate(matrix, angle, axis);
        }
    }
    return matrix;
}

static void updateMaterial(const FilePath& path, const tinyxml2::XMLElement& materialDescription,
                           Material& material) {
    auto pDiffuse = materialDescription.FirstChildElement("Diffuse");
    if(pDiffuse) {
        material.m_DiffuseReflectance = loadColor(*pDiffuse, material.m_DiffuseReflectance);
        std::string texturePath;
        if(getAttribute(*pDiffuse, "textureDiffuse", texturePath)) {
            material.m_DiffuseReflectanceTexture = loadImage(path + texturePath);
        }
    }

    auto pGlossy = materialDescription.FirstChildElement("Glossy");
    if(pGlossy) {
        material.m_GlossyReflectance = loadColor(*pGlossy, material.m_GlossyReflectance);
        getAttribute(*pGlossy, "shininess", material.m_Shininess);
        std::string texturePath;
        if(getAttribute(*pGlossy, "textureGlossy", texturePath)) {
            material.m_GlossyReflectanceTexture = loadImage(path + texturePath);
        }
        if(getAttribute(*pGlossy, "textureShininess", texturePath)) {
            material.m_ShininessTexture = loadImage(path + texturePath);
        }
    }

    auto pEmission = materialDescription.FirstChildElement("Emission");
    if(pEmission) {
        material.m_EmittedRadiance = loadColor(*pEmission, material.m_EmittedRadiance);
        std::string texturePath;
        if(getAttribute(*pEmission, "textureEmission", texturePath)) {
            material.m_EmittedRadianceTexture = loadImage(path + texturePath);
        }
    }

    auto pSpecular = materialDescription.FirstChildElement("Specular");
    if(pSpecular) {
        getAttribute(*pSpecular, "indexOfRefraction", material.m_fIndexOfRefraction);
        std::string texturePath;
        if(getAttribute(*pSpecular, "textureIndexOfRefraction", texturePath)) {
            material.m_IndexOfRefractionTexture = loadImage(path + texturePath);
        }

        auto pReflection = pSpecular->FirstChildElement("Reflection");

        if(pReflection) {
            material.m_SpecularReflectance = loadColor(*pReflection, material.m_SpecularReflectance);
            if(getAttribute(*pReflection, "textureReflection", texturePath)) {
                material.m_SpecularReflectanceTexture = loadImage(path + texturePath);
            }

            getAttribute(*pSpecular, "absorption", material.m_fSpecularAbsorption);
            if(getAttribute(*pSpecular, "textureAbsorption", texturePath)) {
                material.m_SpecularAbsorptionTexture = loadImage(path + texturePath);
            }
        }

        auto pTransmission = pSpecular->FirstChildElement("Transmission");

        if(pTransmission) {
            material.m_SpecularTransmittance = loadColor(*pTransmission, material.m_SpecularTransmittance);
            if(getAttribute(*pTransmission, "textureTransmission", texturePath)) {
                material.m_SpecularTransmittanceTexture = loadImage(path + texturePath);
            }
        }
    }
}

static Material loadMaterial(const FilePath& path, const tinyxml2::XMLElement& materialDescription) {
    std::string name;
    if(!getAttribute(materialDescription, "name", name)) {
        name = getUniqueName();
    }

    Material material(name);
    updateMaterial(path, materialDescription, material);

    return material;
}

SceneGeometry loadMesh(const FilePath& path, const tinyxml2::XMLElement& meshDescription) {
    SceneGeometry geometry;
    auto materialID = geometry.getMaterialCount();

    auto pMaterial = meshDescription.FirstChildElement("Material");
    if(!pMaterial) {
        geometry.addMaterial(Material(getUniqueName()));
    } else {
        geometry.m_Materials.emplace_back(loadMaterial(path, *pMaterial));
    }

    auto pVertexList = meshDescription.FirstChildElement("VertexList");
    if(!pVertexList) {
        throw std::runtime_error("No VertexList in a mesh description.");
    }

    auto pTriangleList = meshDescription.FirstChildElement("TriangleList");
    if(!pTriangleList) {
        throw std::runtime_error("No TriangleList in a mesh description.");
    }

    TriangleMesh mesh;

    mesh.m_MaterialID = materialID;

    auto pVertex = pVertexList->FirstChildElement("Vertex");
    auto vertexIdx = 0u;
    while(pVertex) {
        Vec3f position(0.f), normal(0.f);
        Vec2f texCoords(0.f);

        getAttribute(*pVertex, "x", position.x);
        getAttribute(*pVertex, "y", position.y);
        getAttribute(*pVertex, "z", position.z);
        getAttribute(*pVertex, "nx", normal.x);
        getAttribute(*pVertex, "ny", normal.y);
        getAttribute(*pVertex, "nz", normal.z);
        getAttribute(*pVertex, "u", texCoords.x);
        getAttribute(*pVertex, "v", texCoords.y);

        mesh.m_Vertices.emplace_back(
                    position,
                    normal,
                    texCoords);

        if(vertexIdx == 0) {
            mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
        } else {
            mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
        }
        ++vertexIdx;

        pVertex = pVertex->NextSiblingElement("Vertex");
    }

    auto pTriangle = pTriangleList->FirstChildElement("Triangle");
    while(pTriangle) {
        uint32_t v0, v1, v2;
        getAttribute(*pTriangle, "a", v0);
        getAttribute(*pTriangle, "b", v1);
        getAttribute(*pTriangle, "c", v2);
        if(v0 >= vertexIdx || v1 >= vertexIdx || v2 >= vertexIdx) {
            throw std::runtime_error("Vertex index out of range");
        }
        mesh.m_Triangles.emplace_back(v0, v1, v2);

        pTriangle = pTriangle->NextSiblingElement("Triangle");
    }

    geometry.append(mesh);

    return geometry;
}

static TriangleMesh buildSphere(const Vec3f& center, float r, uint32_t discLat, uint32_t discLong) {
    // Equation paramétrique en (r, phi, theta) de la sphère
    // avec r >= 0, -PI / 2 <= theta <= PI / 2, 0 <= phi <= 2PI
    //
    // x(r, phi, theta) = r sin(phi) cos(theta)
    // y(r, phi, theta) = r sin(theta)
    // z(r, phi, theta) = r cos(phi) cos(theta)
    //
    // Discrétisation:
    // dPhi = 2PI / discLat, dTheta = PI / discLong
    //
    // x(r, i, j) = r * sin(i * dPhi) * cos(-PI / 2 + j * dTheta)
    // y(r, i, j) = r * sin(-PI / 2 + j * dTheta)
    // z(r, i, j) = r * cos(i * dPhi) * cos(-PI / 2 + j * dTheta)

    float rcpLat = 1.f / discLat, rcpLong = 1.f / discLong;
    float dPhi = 2 * pi<float>() * rcpLat, dTheta = pi<float>() * rcpLong;

    TriangleMesh mesh;

    // Construit l'ensemble des vertex
    for(uint32_t j = 0; j <= discLong; ++j) {
        float cosTheta = cos(-pi<float>() * 0.5f + j * dTheta);
        float sinTheta = sin(-pi<float>() * 0.5f + j * dTheta);

        for(uint32_t i = 0; i <= discLat; ++i) {
            TriangleMesh::Vertex vertex;

            vertex.texCoords.x = i * rcpLat;
            vertex.texCoords.y = 1.f - j * rcpLong;

            vertex.normal.x = sin(i * dPhi) * cosTheta;
            vertex.normal.y = sinTheta;
            vertex.normal.z = cos(i * dPhi) * cosTheta;

            vertex.position = center + r * vertex.normal;

            mesh.m_Vertices.emplace_back(vertex);

            if(mesh.m_Vertices.size() == 1u) {
                mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
            } else {
                mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
            }
        }
    }

    // Construit les vertex finaux en regroupant les données en triangles:
    // Pour une longitude donnée, les deux triangles formant une face sont de la forme:
    // (i, i + 1, i + discLat + 1), (i, i + discLat + 1, i + discLat)
    // avec i sur la bande correspondant à la longitude
    for(uint32_t j = 0; j < discLong; ++j) {
        uint32_t offset = j * (discLat + 1);
        for(uint32_t i = 0; i < discLat; ++i) {
            mesh.m_Triangles.emplace_back(offset + i, offset + (i + 1), offset + discLat + 1 + (i + 1));
            mesh.m_Triangles.emplace_back(offset + i, offset + discLat + 1 + (i + 1), offset + i + discLat + 1);
        }
    }

    return mesh;
}

static TriangleMesh buildQuad() {
    TriangleMesh mesh;

    {
        TriangleMesh::Vertex vertex;

        vertex.texCoords.x = 0;
        vertex.texCoords.y = 0;

        vertex.normal.x = 0;
        vertex.normal.y = -1;
        vertex.normal.z = 0;

        vertex.position = Vec3f(0.25, 1.28002, 0.25);

        mesh.m_Vertices.emplace_back(vertex);

        if(mesh.m_Vertices.size() == 1u) {
            mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
        } else {
            mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
        }
    }

    {
        TriangleMesh::Vertex vertex;

        vertex.texCoords.x = 1;
        vertex.texCoords.y = 0;

        vertex.normal.x = 0;
        vertex.normal.y = -1;
        vertex.normal.z = 0;

        vertex.position = Vec3f(0.25, 1.28002, -0.25);

        mesh.m_Vertices.emplace_back(vertex);

        if(mesh.m_Vertices.size() == 1u) {
            mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
        } else {
            mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
        }
    }

    {
        TriangleMesh::Vertex vertex;

        vertex.texCoords.x = 1;
        vertex.texCoords.y = 1;

        vertex.normal.x = 0;
        vertex.normal.y = -1;
        vertex.normal.z = 0;

        vertex.position = Vec3f(-0.25, 1.28002, -0.25);

        mesh.m_Vertices.emplace_back(vertex);

        if(mesh.m_Vertices.size() == 1u) {
            mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
        } else {
            mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
        }
    }

    {
        TriangleMesh::Vertex vertex;

        vertex.texCoords.x = 0;
        vertex.texCoords.y = 1;

        vertex.normal.x = 0;
        vertex.normal.y = -1;
        vertex.normal.z = 0;

        vertex.position = Vec3f(-0.25, 1.28002, 0.25);

        mesh.m_Vertices.emplace_back(vertex);

        if(mesh.m_Vertices.size() == 1u) {
            mesh.m_BBox = BBox3f(Vec3f(mesh.m_Vertices.back().position));
        } else {
            mesh.m_BBox.grow(Vec3f(mesh.m_Vertices.back().position));
        }
    }

    mesh.m_Triangles.emplace_back(2, 1, 0);
    mesh.m_Triangles.emplace_back(2, 0, 3);

    return mesh;
}

SceneGeometry loadQuad(const FilePath& path, const tinyxml2::XMLElement& quadDescription) {
    SceneGeometry geometry;
    auto materialID = geometry.getMaterialCount();

    auto pMaterial = quadDescription.FirstChildElement("Material");
    if(!pMaterial) {
        geometry.addMaterial(Material(getUniqueName()));
    } else {
        geometry.m_Materials.emplace_back(loadMaterial(path, *pMaterial));
    }

    TriangleMesh mesh = buildQuad();

    mesh.m_MaterialID = materialID;

    geometry.append(mesh);

    return geometry;
}

SceneGeometry loadDisk(const FilePath& path, const tinyxml2::XMLElement& diskDescription) {
    SceneGeometry geometry;
    auto materialID = geometry.getMaterialCount();

    auto pMaterial = diskDescription.FirstChildElement("Material");
    if(!pMaterial) {
        geometry.addMaterial(Material(getUniqueName()));
    } else {
        geometry.m_Materials.emplace_back(loadMaterial(path, *pMaterial));
    }

    Vec3f center = zero<Vec3f>(), normal = Vec3f(0, 1, 0);
    float radius = 1.f;
    std::size_t discStepCount = 1024;

    getChildAttribute(diskDescription, "Center", center);
    getChildAttribute(diskDescription, "Normal", normal);
    getChildAttribute(diskDescription, "Radius", radius);
    getChildAttribute(diskDescription, "DiscStepCount", discStepCount);

    normal = normalize(normal);
    auto frame = frameZ(normal);

    TriangleMesh mesh;
    mesh.m_MaterialID = materialID;

    float dAngle = two_pi<float>() / discStepCount;

    mesh.m_Vertices.emplace_back(center, normal, Vec2f(0.5f));
    mesh.m_BBox.grow(center);

    for(auto i: range(discStepCount)) {
        Vec2f xy = Vec2f(cos(i * dAngle), sin(i * dAngle));
        TriangleMesh::Vertex vertex;
        vertex.texCoords = xy;
        vertex.position = center + frame * Vec3f(radius * xy, 0);
        vertex.normal = normal;
        mesh.m_Vertices.emplace_back(vertex);
        mesh.m_BBox.grow(vertex.position);
    }

    for(auto i: range(discStepCount)) {
        mesh.m_Triangles.emplace_back(0, 1 + i, 1 + (i + 1) % discStepCount);
    }

    geometry.append(mesh);

    return geometry;
}

SceneGeometry loadSphere(const FilePath& path, const tinyxml2::XMLElement& sphereDescription) {
    SceneGeometry geometry;
    auto materialID = geometry.getMaterialCount();

    auto pMaterial = sphereDescription.FirstChildElement("Material");
    if(!pMaterial) {
        geometry.addMaterial(Material(getUniqueName()));
    } else {
        geometry.m_Materials.emplace_back(loadMaterial(path, *pMaterial));
    }

    float radius = 1;
    uint32_t discLat = 8, discLong = 4;
    Vec3f center;

    center = loadVector(sphereDescription);

    if(!getAttribute(sphereDescription, "radius", radius)) {
        CLOG(ERROR, "SceneLoader") << "Sphere description error: no radius specified";
    }
    if(!getAttribute(sphereDescription, "discLat", discLat)) {
        CLOG(ERROR, "SceneLoader") << "Sphere description error: no discLat specified";
    }
    if(!getAttribute(sphereDescription, "discLong", discLong)) {
        CLOG(ERROR, "SceneLoader") << "Sphere description error: no discLong specified";
    }

    TriangleMesh mesh = buildSphere(center, radius, discLat, discLong);

    mesh.m_MaterialID = materialID;

    geometry.append(mesh);

    return geometry;
}

SceneGeometry loadGeometry(const FilePath& path, const tinyxml2::XMLElement& geometryDescription) {
    auto pLogger = el::Loggers::getLogger("SceneLoader");

    SceneGeometry geometry;

    auto pMaterials = geometryDescription.FirstChildElement("Materials");
    if(pMaterials) {
        for(auto pMaterial = pMaterials->FirstChildElement("Material"); pMaterial; pMaterial = pMaterial->NextSiblingElement("Material")) {
            std::string name;
            if(getAttribute(*pMaterial, "name", name)) {
                pLogger->verbose(1, "Load material %v", name);
                auto materialID = geometry.addMaterial(Material(name));
                auto& material = geometry.getMaterial(materialID);
                material.m_DiffuseReflectance = zero<Vec3f>();
                updateMaterial(path, *pMaterial, material);
            } else {
                pLogger->error("Material with no name declared in scene file");
            }
        }
    }

    auto localToWorldMatrix = Mat4f(1.f);

    auto pLocalToWorldMatrix = geometryDescription.FirstChildElement("LocalToWorldMatrix");
    if(pLocalToWorldMatrix) {
        localToWorldMatrix = loadMatrix(*pLocalToWorldMatrix);
    }

    auto pModel = geometryDescription.FirstChildElement("Model");
    while(pModel) {
        auto offset = geometry.getMeshCount();
        geometry.append(loadModel(path + pModel->Attribute("path")));

        std::string materialName;
        if(getAttribute(*pModel, "material", materialName)) {
            auto materialID = geometry.getMaterialID(materialName);
            if(materialID < 0) {
                pLogger->error("Error in geometry description: material %v not found", materialName);
            } else {
                for(auto i = offset; i < geometry.getMeshCount(); ++i) {
                    geometry.getMesh(i).m_MaterialID = materialID;
                }
            }
        }

        pModel = pModel->NextSiblingElement("Model");
    }

    auto pMesh = geometryDescription.FirstChildElement("Mesh");
    while(pMesh) {
        geometry.append(loadMesh(path, *pMesh));
        pMesh = pMesh->NextSiblingElement("Mesh");
    }

    for(auto pShape = geometryDescription.FirstChildElement("Shape");
        pShape; pShape = pShape->NextSiblingElement("Shape")) {
        std::string type;
        if(!getAttribute(*pShape, "type", type)) {
            pLogger->error("Error in geometry description: Shape specified without type");
            continue;
        }

        if(type == "sphere") {
            pLogger->verbose(1, "Load a sphere");
            geometry.append(loadSphere(path, *pShape));
        } else if(type == "quad") {
            pLogger->verbose(1, "Load a quad");
            geometry.append(loadQuad(path, *pShape));
        } else if(type == "disk") {
            pLogger->verbose(1, "Load a disk");
            geometry.append(loadDisk(path, *pShape));
        } else {
            pLogger->error("Error in geometry description: unknown shape type %v", type);
            continue;
        }
    }

    auto pMaterialUpdates = geometryDescription.FirstChildElement("MaterialUpdates");
    if(pMaterialUpdates) {
        for(auto pMaterial = pMaterialUpdates->FirstChildElement("Material"); pMaterial; pMaterial = pMaterial->NextSiblingElement("Material")) {
            std::string name;
            if(!getAttribute(*pMaterial, "name", name)) {
                pLogger->error("Error in geometry description: material update with no name provided.");
                continue;
            }
            auto materialID = geometry.getMaterialID(name);
            if(materialID < 0) {
                pLogger->error("Error in geometry description: material %v not found for update", name);
                continue;
            }

            auto& material = geometry.getMaterial(materialID);
            updateMaterial(path, *pMaterial, material);
        }
    }

    geometry.transform(localToWorldMatrix);
    geometry.extractEmissiveMeshes();

    return geometry;
}

}
